#pragma once

#include "CoreMinimal.h"
#include "World/ProcMap/TN_ProcMapLayout.h"

/**
 * Altura del terreno a partir del layout, evaluada sobre una malla regular de
 * vértices (la que luego se convierte en tiles de terreno con colisión).
 *
 * Composición (en este orden) para cada punto:
 *   1. Base: nivel suavizado de módulos + ondulación por bioma (pesos de bioma
 *      con deformación de dominio para transiciones naturales).
 *   2. Fuera del camino el terreno sube (valle) o baja al lecho (biomas de agua).
 *   3. Cauce del camino: plano a la cota de la muestra más cercana, con arcén.
 *   4. Estructuras colosales: holgura bajo puentes, torres, mesas y túnel.
 *   5. Zanjas de los huecos, pozas de lava, islas y río.
 *   6. Muros del borde (sur/este/oeste), costa al norte y claro de salida.
 *
 * ComputeRows es thread-safe (filas disjuntas): el actor lo reparte con ParallelFor.
 */

namespace TNProcMap
{
	struct FBiomeTerrain
	{
		double UndAmp = 300.0;
		double RiseMax = 1500.0;
		double RiseDist = 3500.0;
		double Shoulder = 900.0;
		double Rough = 150.0;
		/** 0 = ruido suave, 1 = crestas (dunas, roca). */
		double Ridge = 0.0;
		double CoastWidth = 3000.0;
		/** Cota del lecho en biomas húmedos. */
		double BedZ = 0.0;
		bool bWet = false;
	};

	inline FBiomeTerrain GetBiomeTerrain(ETNProcBiome B)
	{
		FBiomeTerrain T;
		switch (B)
		{
			case ETNProcBiome::Jungle:   T.UndAmp = 350; T.RiseMax = 1900; T.RiseDist = 3500; T.Shoulder = 900;  T.Rough = 160; T.Ridge = 0.0; T.CoastWidth = 2500; break;
			case ETNProcBiome::Beach:    T.UndAmp = 110; T.RiseMax = 380;  T.RiseDist = 5000; T.Shoulder = 1600; T.Rough = 190; T.Ridge = 0.8; T.CoastWidth = 6500; break;
			case ETNProcBiome::Desert:   T.UndAmp = 300; T.RiseMax = 1500; T.RiseDist = 4200; T.Shoulder = 1200; T.Rough = 380; T.Ridge = 1.0; T.CoastWidth = 3500; break;
			case ETNProcBiome::Volcanic: T.UndAmp = 450; T.RiseMax = 2900; T.RiseDist = 2600; T.Shoulder = 600;  T.Rough = 300; T.Ridge = 0.6; T.CoastWidth = 1200; break;
			case ETNProcBiome::Water:    T.UndAmp = 60;  T.RiseMax = 0;    T.RiseDist = 3000; T.Shoulder = 500;  T.Rough = 60;  T.Ridge = 0.0; T.CoastWidth = 4000; T.BedZ = -380; T.bWet = true; break;
			case ETNProcBiome::Rocky:    T.UndAmp = 400; T.RiseMax = 3300; T.RiseDist = 1900; T.Shoulder = 400;  T.Rough = 360; T.Ridge = 1.0; T.CoastWidth = 800;  break;
			case ETNProcBiome::Mangrove: T.UndAmp = 30;  T.RiseMax = 0;    T.RiseDist = 3000; T.Shoulder = 400;  T.Rough = 25;  T.Ridge = 0.0; T.CoastWidth = 4000; T.BedZ = -60; T.bWet = true; break;
			case ETNProcBiome::Human:    T.UndAmp = 150; T.RiseMax = 700;  T.RiseDist = 3000; T.Shoulder = 800;  T.Rough = 60;  T.Ridge = 0.0; T.CoastWidth = 3000; break;
			default: break;
		}
		return T;
	}

	/** Nivel del agua de todo el mapa (mar, lagunas, río). */
	constexpr double SeaLevel = 0.0;

	/** Pendiente (tan) de las laderas que bajan desde el borde de un cauce elevado. */
	constexpr double FlankSlope = 1.15;
	/** Altura mínima de las orillas de laguna y de los acantilados de costa sobre el mar. */
	constexpr double ShoreCliffHeight = 650.0;
	/** Anchura de la meseta a la cota del borde del talud antes de que baje la ladera. */
	constexpr double RimPlateau = 2500.0;

	/**
	 * Diagonal de cada quad del mallado (A=(x,y) B=(x+1,y) C=(x,y+1) D=(x+1,y+1)):
	 * true = partir por A-D, false = por B-C. Se parte por la diagonal de menor
	 * desnivel, que sigue la curva de nivel: sin ella un talud en diagonal a la
	 * rejilla sale en dientes de sierra con peldaños casi planos por los que trepar.
	 */
	inline bool SplitAlongAD(double HA, double HB, double HC, double HD)
	{
		return FMath::Abs(HA - HD) < FMath::Abs(HB - HC);
	}

	class FTerrainBuilder
	{
	public:
		/** Mallado: vértice (ix, iy) en Origin + (ix, iy) * Spacing (espacio del mapa). */
		FVector2D Origin = FVector2D::ZeroVector;
		double Spacing = 200.0;
		int32 NX = 0;
		int32 NY = 0;

		void Build(const FLayout& InLayout, const FVector2D& InOrigin, double InSpacing, int32 InNX, int32 InNY)
		{
			L = &InLayout;
			Origin = InOrigin;
			Spacing = InSpacing;
			NX = InNX;
			NY = InNY;
			Seed = InLayout.Params.Seed ^ 0x7E44Au;
			for (int32 b = 0; b < NumBiomes; ++b) { Biomes[b] = GetBiomeTerrain(BiomeFromIndex(b)); }
			BuildSamples();
			StampPathField();
			BuildInfluences();
			for (const FFeature& F : L->Features)
			{
				if (F.Type == EFeature::StartArea) { StartZ = F.Location.Z; }
			}
		}

		/** Rellena Height (cm) y PathMask (0..255) para las filas [RowBegin, RowEnd). */
		void ComputeRows(int32 RowBegin, int32 RowEnd, TArray<float>& OutHeight, TArray<uint8>& OutPathMask) const
		{
			for (int32 iy = RowBegin; iy < RowEnd; ++iy)
			{
				for (int32 ix = 0; ix < NX; ++ix)
				{
					const int32 Idx = iy * NX + ix;
					uint8 Mask = 0;
					OutHeight[Idx] = static_cast<float>(HeightAtVertex(ix, iy, Mask));
					OutPathMask[Idx] = Mask;
				}
			}
		}

		/** Distancia (cm) de cada vértice al BORDE del camino más cercano; 1e9 si está lejos de todos. */
		void ExportPathEdgeDistance(TArray<float>& Out) const
		{
			Out.SetNum(PathDist.Num());
			for (int32 Idx = 0; Idx < PathDist.Num(); ++Idx)
			{
				Out[Idx] = PathSeg[Idx] == INDEX_NONE ? 1e9f : static_cast<float>(FMath::Max(0.0, PathDist[Idx] - EffHalfWidth(Idx)));
			}
		}

		double HeightAtVertex(int32 ix, int32 iy, uint8& OutMask) const
		{
			const FVector2D P = Origin + FVector2D(ix * Spacing, iy * Spacing);
			const int32 Idx = iy * NX + ix;
			return Evaluate(P, PathDist[Idx], PathSeg[Idx], PathT[Idx], OutMask);
		}

	private:
		const FLayout* L = nullptr;
		uint32 Seed = 0;
		FBiomeTerrain Biomes[NumBiomes];
		double StartZ = 0.0;

		// ── Campo del camino ────────────────────────────────────────────────
		TArray<FPathSample> Samples;
		TArray<int32> NextOf;
		TArray<uint8> Carves;
		TArray<float> PathDist;
		TArray<int32> PathSeg;
		TArray<float> PathT;

		// ── Influencias localizadas (cubos) ─────────────────────────────────
		enum class EInf : uint8 { Tower, Mesa, Tunnel, DeckClear, Gap, Lava, Island, River };
		struct FInf
		{
			EInf Type = EInf::Tower;
			int32 A = INDEX_NONE;   ///< feature / cruce / muestra
			int32 B = INDEX_NONE;   ///< segmento
		};
		double BinCell = 4000.0;
		FVector2D BinOrigin = FVector2D::ZeroVector;
		int32 BinW = 0;
		int32 BinH = 0;
		TArray<TArray<FInf>> Bins;

		void BuildSamples()
		{
			Samples.Reset();
			NextOf.Reset();
			Carves.Reset();
			auto AddPolyline = [&](const TArray<FPathSample>& In)
			{
				const int32 Base = Samples.Num();
				for (int32 i = 0; i < In.Num(); ++i)
				{
					Samples.Add(In[i]);
					NextOf.Add(i + 1 < In.Num() ? Base + i + 1 : INDEX_NONE);
					const uint32 F = In[i].Flags;
					const bool bCarve = (F & (PathFlags::NotTerrain | PathFlags::Colossal | PathFlags::UnderTower)) == 0;
					Carves.Add(bCarve ? 1 : 0);
				}
			};
			AddPolyline(L->Main);
			for (const FBranch& B : L->Branches) { AddPolyline(B.Samples); }
		}

		void StampPathField()
		{
			const int32 N = NX * NY;
			PathDist.Init(1e9f, N);
			PathSeg.Init(INDEX_NONE, N);
			PathT.Init(0.0f, N);
			for (int32 i = 0; i < Samples.Num(); ++i)
			{
				const int32 j = NextOf[i];
				if (j == INDEX_NONE || !Carves[i] || !Carves[j]) { continue; }
				const FVector2D A = Samples[i].P;
				const FVector2D B = Samples[j].P;
				const double Reach = FMath::Max(Samples[i].Width, Samples[j].Width) * 0.5 + 5600.0;
				const int32 X0 = FMath::Max(0, FMath::FloorToInt((FMath::Min(A.X, B.X) - Reach - Origin.X) / Spacing));
				const int32 X1 = FMath::Min(NX - 1, FMath::CeilToInt((FMath::Max(A.X, B.X) + Reach - Origin.X) / Spacing));
				const int32 Y0 = FMath::Max(0, FMath::FloorToInt((FMath::Min(A.Y, B.Y) - Reach - Origin.Y) / Spacing));
				const int32 Y1 = FMath::Min(NY - 1, FMath::CeilToInt((FMath::Max(A.Y, B.Y) + Reach - Origin.Y) / Spacing));
				for (int32 y = Y0; y <= Y1; ++y)
				{
					for (int32 x = X0; x <= X1; ++x)
					{
						const FVector2D P = Origin + FVector2D(x * Spacing, y * Spacing);
						double T = 0.0;
						const double D = DistPointSegment(P, A, B, T);
						const int32 Idx = y * NX + x;
						// Distancia "efectiva": al borde del camino, para que el más ancho gane.
						const double Hw = LerpD(Samples[i].Width, Samples[j].Width, T) * 0.5;
						const double Eff = D - Hw;
						const double Cur = PathSeg[Idx] == INDEX_NONE ? 1e18 : PathDist[Idx] - EffHalfWidth(Idx);
						if (D <= Reach && Eff < Cur)
						{
							PathDist[Idx] = static_cast<float>(D);
							PathSeg[Idx] = i;
							PathT[Idx] = static_cast<float>(T);
						}
					}
				}
			}
		}

		double EffHalfWidth(int32 Idx) const
		{
			const int32 i = PathSeg[Idx];
			if (i == INDEX_NONE) { return 0.0; }
			const int32 j = NextOf[i];
			return LerpD(Samples[i].Width, Samples[j == INDEX_NONE ? i : j].Width, PathT[Idx]) * 0.5;
		}

		void AddInf(const FInf& Inf, const FVector2D& Min, const FVector2D& Max)
		{
			const int32 X0 = FMath::Clamp(FMath::FloorToInt((Min.X - BinOrigin.X) / BinCell), 0, BinW - 1);
			const int32 X1 = FMath::Clamp(FMath::FloorToInt((Max.X - BinOrigin.X) / BinCell), 0, BinW - 1);
			const int32 Y0 = FMath::Clamp(FMath::FloorToInt((Min.Y - BinOrigin.Y) / BinCell), 0, BinH - 1);
			const int32 Y1 = FMath::Clamp(FMath::FloorToInt((Max.Y - BinOrigin.Y) / BinCell), 0, BinH - 1);
			for (int32 y = Y0; y <= Y1; ++y)
			{
				for (int32 x = X0; x <= X1; ++x) { Bins[y * BinW + x].Add(Inf); }
			}
		}

		void AddSegmentInf(EInf Type, int32 A, int32 SegIdx, const FVector2D& P0, const FVector2D& P1, double Reach)
		{
			FInf Inf;
			Inf.Type = Type;
			Inf.A = A;
			Inf.B = SegIdx;
			AddInf(Inf, FVector2D(FMath::Min(P0.X, P1.X) - Reach, FMath::Min(P0.Y, P1.Y) - Reach),
				FVector2D(FMath::Max(P0.X, P1.X) + Reach, FMath::Max(P0.Y, P1.Y) + Reach));
		}

		void BuildInfluences()
		{
			BinOrigin = Origin;
			BinW = FMath::Max(1, FMath::CeilToInt(NX * Spacing / BinCell) + 1);
			BinH = FMath::Max(1, FMath::CeilToInt(NY * Spacing / BinCell) + 1);
			Bins.Reset();
			Bins.SetNum(BinW * BinH);

			const TArray<FPathSample>& M = L->Main;
			for (int32 c = 0; c < L->Crossings.Num(); ++c)
			{
				const FCrossing& C = L->Crossings[c];
				const FRouteStep& High = L->Route[C.HighStep];
				const FRouteStep& Low = L->Route[C.LowStep];
				const double Rise = C.TopZ - L->Modules[C.Module].Level;
				for (int32 i = High.FirstSample; i < High.LastSample; ++i)
				{
					if (C.Type == ETNProcCrossingType::Cave)
					{
						AddSegmentInf(EInf::Mesa, c, i, M[i].P, M[i + 1].P, 1200.0 + Rise / 2.5 + 400.0);
					}
					else
					{
						AddSegmentInf(EInf::DeckClear, c, i, M[i].P, M[i + 1].P, 3500.0);
					}
				}
				if (C.Type == ETNProcCrossingType::Cave)
				{
					for (int32 j = Low.FirstSample; j < Low.LastSample; ++j)
					{
						if ((M[j].Flags & PathFlags::Tunnel) != 0 || (M[j + 1].Flags & PathFlags::Tunnel) != 0)
						{
							AddSegmentInf(EInf::Tunnel, c, j, M[j].P, M[j + 1].P, 1200.0);
						}
					}
				}
			}

			for (int32 f = 0; f < L->Features.Num(); ++f)
			{
				const FFeature& F = L->Features[f];
				const FVector2D C(F.Location.X, F.Location.Y);
				FInf Inf;
				Inf.A = f;
				double Reach = 0.0;
				switch (F.Type)
				{
					case EFeature::Tower:    Inf.Type = EInf::Tower;  Reach = F.Radius + 700.0; break;
					case EFeature::Gap:      Inf.Type = EInf::Gap;    Reach = FMath::Max(F.Height, F.Width) * 0.5 + 3000.0; break;
					case EFeature::LavaPool: Inf.Type = EInf::Lava;   Reach = F.Radius + 1000.0; break;
					case EFeature::Island:   Inf.Type = EInf::Island; Reach = F.Radius + 200.0; break;
					default: continue;
				}
				AddInf(Inf, C - FVector2D(Reach, Reach), C + FVector2D(Reach, Reach));
			}

			for (int32 r = 0; r + 1 < L->River.Num(); ++r)
			{
				const double Reach = L->RiverWidth[r] * 0.5 + 4500.0;
				AddSegmentInf(EInf::River, r, r, L->River[r], L->River[r + 1], Reach);
			}
		}

		void BlendBiomes(const FVector2D& P, FBiomeTerrain& Out, double& OutWet, double W[NumBiomes]) const
		{
			const FVector2D Warp(Noise2(Seed + 1u, P.X / 6000.0, P.Y / 6000.0), Noise2(Seed + 2u, P.X / 6000.0, P.Y / 6000.0));
			L->BiomeWeightsAt(P + Warp * 2600.0, W);
			Out = FBiomeTerrain();
			Out.UndAmp = Out.RiseMax = Out.RiseDist = Out.Shoulder = Out.Rough = Out.Ridge = Out.CoastWidth = Out.BedZ = 0.0;
			double WetSum = 0.0, Total = 0.0, BedAcc = 0.0;
			for (int32 b = 0; b < NumBiomes; ++b)
			{
				const double Wb = W[b];
				if (Wb <= 0.0) { continue; }
				const FBiomeTerrain& T = Biomes[b];
				Total += Wb;
				Out.UndAmp += Wb * T.UndAmp;
				Out.RiseMax += Wb * T.RiseMax;
				Out.RiseDist += Wb * T.RiseDist;
				Out.Shoulder += Wb * T.Shoulder;
				Out.Rough += Wb * T.Rough;
				Out.Ridge += Wb * T.Ridge;
				Out.CoastWidth += Wb * T.CoastWidth;
				if (T.bWet) { WetSum += Wb; BedAcc += Wb * T.BedZ; }
			}
			if (Total <= 0.0) { Out = Biomes[0]; OutWet = 0.0; return; }
			Out.UndAmp /= Total; Out.RiseMax /= Total; Out.RiseDist /= Total; Out.Shoulder /= Total;
			Out.Rough /= Total; Out.Ridge /= Total; Out.CoastWidth /= Total;
			Out.BedZ = WetSum > 0.0 ? BedAcc / WetSum : -300.0;
			OutWet = WetSum / Total;
		}

		double Evaluate(const FVector2D& P, float InPathDist, int32 InSeg, float InT, uint8& OutMask) const
		{
			OutMask = 0;
			FBiomeTerrain Bt;
			double Wet = 0.0;
			double W[NumBiomes];
			BlendBiomes(P, Bt, Wet, W);

			const double NLarge = Fbm2(Seed + 3u, P.X / 12000.0, P.Y / 12000.0, 3);
			const double NMed = Fbm2(Seed + 4u, P.X / 4000.0, P.Y / 4000.0, 2);
			const double NRidge = Ridged2(Seed + 5u, P.X / 5000.0, P.Y / 5000.0, 3);
			const double Detail = LerpD(NMed, NRidge * 2.0 - 1.0, Bt.Ridge);

			const double Level = L->SampleCoarse(L->LevelField, P);
			const double Elev = L->SampleCoarse(L->ElevatedField, P);
			const double LandBase = Level + Bt.UndAmp * NLarge + Bt.Rough * Detail + Elev * (2600.0 + 1200.0 * NLarge);

			// ── Camino ──────────────────────────────────────────────────────
			double H = 0.0;
			const bool bOnField = InSeg != INDEX_NONE;
			double PathZ = 0.0, Hw = 0.0, Shoulder = Bt.Shoulder;
			bool bLane = false;
			if (bOnField)
			{
				const FPathSample& A = Samples[InSeg];
				const int32 J = NextOf[InSeg];
				const FPathSample& B = Samples[J == INDEX_NONE ? InSeg : J];
				PathZ = LerpD(A.Z, B.Z, InT);
				Hw = LerpD(A.Width, B.Width, InT) * 0.5;
				bLane = ((A.Flags | B.Flags) & PathFlags::Lane) != 0;
				if (bLane) { Shoulder = 350.0; }
				if (((A.Flags | B.Flags) & (PathFlags::Slide | PathFlags::TowerTop)) != 0) { Shoulder = FMath::Min(Shoulder, 500.0); }
			}
			const double Beyond = bOnField ? FMath::Max(0.0, static_cast<double>(InPathDist) - Hw) : 1e9;
			double RiseMax = Bt.RiseMax;
			if (bLane) { RiseMax = FMath::Max(RiseMax, 1100.0); }
			const double LandOff = LandBase + RiseMax * SmoothStep(0.0, FMath::Max(bLane ? 900.0 : Bt.RiseDist, 1.0), Beyond);
			const double WetOff = Bt.BedZ + 70.0 * NMed;
			const double Off = LerpD(LandOff, WetOff, SmoothStep(0.35, 0.65, Wet));

			if (bOnField && Beyond <= 0.0)
			{
				H = PathZ + 10.0 * NMed;
				OutMask = 255;
			}
			else if (bOnField && Beyond < Shoulder)
			{
				const double T = SmoothStep(0.0, 1.0, Beyond / Shoulder);
				H = LerpD(PathZ, Off, T);
				OutMask = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(255.0 * (1.0 - SmoothStep(0.0, 0.6, Beyond / Shoulder))), 0, 255));
			}
			else
			{
				H = Off;
			}

			// ── Influencias localizadas ─────────────────────────────────────
			const int32 BX = FMath::Clamp(FMath::FloorToInt((P.X - BinOrigin.X) / BinCell), 0, BinW - 1);
			const int32 BY = FMath::Clamp(FMath::FloorToInt((P.Y - BinOrigin.Y) / BinCell), 0, BinH - 1);
			const TArray<FInf>& Bin = Bins[BY * BinW + BX];
			if (Bin.Num() > 0)
			{
				H = ApplyInfluences(P, H, Bin, OutMask);
			}

			// ── Muros del borde (sur, este, oeste) ──────────────────────────
			{
				const double ES = P.Y;
				const double EW = P.X;
				const double EE = L->WorldSize - P.X;
				double E = ES;
				double Along = P.X;
				if (EW < E) { E = EW; Along = P.Y + 100000.0; }
				if (EE < E) { E = EE; Along = P.Y + 200000.0; }
				// El muro no llega al mar: se desvanece al acercarse a la costa.
				const double CoastFade = 1.0 - SmoothStep(L->CoastY(P.X) - 9000.0, L->CoastY(P.X) + 1000.0, P.Y);
				const double Inset = L->WallInset(Along);
				const double T = SmoothStep(Inset + 3500.0, Inset, E) * CoastFade;
				if (T > 0.0)
				{
					const double WallTop = FMath::Max(Level, 800.0) + L->Params.WallHeight * (0.75 + 0.35 * NLarge) + 600.0 * NRidge;
					H = FMath::Max(H, LerpD(H, WallTop, T));
					if (T > 0.5) { OutMask = 0; }
				}
			}

			// ── Costa norte y mar abierto ───────────────────────────────────
			{
				const double Coast = L->CoastY(P.X);
				const double T = SmoothStep(Coast - Bt.CoastWidth, Coast + 1500.0, P.Y);
				if (T > 0.0)
				{
					const double SeaBed = FMath::Max(-2600.0, -450.0 - (P.Y - Coast) * 0.035);
					H = LerpD(H, FMath::Min(H, SeaBed), T);
				}
			}

			// ── Claro de salida ─────────────────────────────────────────────
			{
				const double R = L->Params.StartClearingRadius;
				const double Ds = FVector2D::Distance(P, L->StartPoint);
				if (Ds < R + 2500.0)
				{
					const double T = SmoothStep(R + 2500.0, R, Ds);
					H = LerpD(H, StartZ, T);
					if (Ds < R) { OutMask = FMath::Max<uint8>(OutMask, 200); }
				}
			}
			return H;
		}

		double ApplyInfluences(const FVector2D& P, double H, const TArray<FInf>& Bin, uint8& OutMask) const
		{
			const TArray<FPathSample>& M = L->Main;

			// Holgura bajo puentes colosales (antes que las torres, que la sobrescriben).
			for (const FInf& Inf : Bin)
			{
				if (Inf.Type != EInf::DeckClear) { continue; }
				const FCrossing& C = L->Crossings[Inf.A];
				double T = 0.0;
				const double D = DistPointSegment(P, M[Inf.B].P, M[Inf.B + 1].P, T);
				if (D < 3500.0) { H = FMath::Min(H, C.TopZ - 1700.0); }
			}

			// Mesas: cima plana a TopZ y laderas empinadas (pendiente 2.5).
			for (const FInf& Inf : Bin)
			{
				if (Inf.Type != EInf::Mesa) { continue; }
				const FCrossing& C = L->Crossings[Inf.A];
				double T = 0.0;
				const double D = DistPointSegment(P, M[Inf.B].P, M[Inf.B + 1].P, T);
				const double MesaH = C.TopZ - FMath::Max(0.0, D - 1200.0) * 2.5;
				if (MesaH > H) { H = MesaH; if (D <= 1200.0) { OutMask = 180; } else { OutMask = 0; } }
			}

			// Torres (pilares de roca) en los extremos de las pasadas altas.
			for (const FInf& Inf : Bin)
			{
				if (Inf.Type != EInf::Tower) { continue; }
				const FFeature& F = L->Features[Inf.A];
				const double D = FVector2D::Distance(P, FVector2D(F.Location.X, F.Location.Y));
				if (D <= F.Radius) { H = FMath::Max(H, F.Height); OutMask = 150; }
				else if (D < F.Radius + 600.0) { H = FMath::Max(H, LerpD(F.Height, H, (D - F.Radius) / 600.0)); }
			}

			// Túnel: el tramo bajo atraviesa la mesa a su propia cota.
			for (const FInf& Inf : Bin)
			{
				if (Inf.Type != EInf::Tunnel) { continue; }
				const FPathSample& A = M[Inf.B];
				const FPathSample& B = M[Inf.B + 1];
				double T = 0.0;
				const double D = DistPointSegment(P, A.P, B.P, T);
				const double Hw = LerpD(A.Width, B.Width, T) * 0.5 + 150.0;
				if (D < Hw) { H = LerpD(A.Z, B.Z, T); OutMask = 255; }
			}

			// Zanjas de los huecos de salto.
			for (const FInf& Inf : Bin)
			{
				if (Inf.Type != EInf::Gap) { continue; }
				const FFeature& F = L->Features[Inf.A];
				const FVector2D Rel = P - FVector2D(F.Location.X, F.Location.Y);
				const double Along = FVector2D::DotProduct(Rel, F.Dir);
				const double Across = FVector2D::DotProduct(Rel, LeftNormal(F.Dir));
				if (FMath::Abs(Along) <= F.Height * 0.5 && FMath::Abs(Across) <= F.Width * 0.5 + 2500.0)
				{
					H = FMath::Min(H, F.Location.Z - 1200.0);
					OutMask = 0;
				}
			}

			// Pozas de lava: cuenco bajo la superficie de lava, con borde.
			for (const FInf& Inf : Bin)
			{
				if (Inf.Type != EInf::Lava) { continue; }
				const FFeature& F = L->Features[Inf.A];
				const double D = FVector2D::Distance(P, FVector2D(F.Location.X, F.Location.Y));
				if (D < F.Radius)
				{
					const double U = D / F.Radius;
					H = FMath::Min(H, F.Location.Z - 120.0 - 180.0 * (1.0 - U * U));
				}
				else if (D < F.Radius + 900.0)
				{
					H = FMath::Max(H, LerpD(F.Location.Z + 80.0, H, (D - F.Radius) / 900.0));
				}
			}

			// Islas decorativas: cúpula suave sobre el lecho.
			for (const FInf& Inf : Bin)
			{
				if (Inf.Type != EInf::Island) { continue; }
				const FFeature& F = L->Features[Inf.A];
				const double D = FVector2D::Distance(P, FVector2D(F.Location.X, F.Location.Y));
				if (D < F.Radius)
				{
					const double U = D / F.Radius;
					H = FMath::Max(H, F.Location.Z - (1.0 - FMath::Cos(U * Pi * 0.5)) * (F.Location.Z + 420.0));
				}
			}

			// Río: cauce bajo el nivel del mar y orillas a 45°.
			double RiverH = 1e18;
			for (const FInf& Inf : Bin)
			{
				if (Inf.Type != EInf::River) { continue; }
				double T = 0.0;
				const double D = DistPointSegment(P, L->River[Inf.B], L->River[Inf.B + 1], T);
				const double Hw = LerpD(L->RiverWidth[Inf.B], L->RiverWidth[Inf.B + 1], T) * 0.5;
				RiverH = FMath::Min(RiverH, -330.0 + FMath::Max(0.0, D - Hw) * 1.1);
			}
			if (RiverH < H) { H = RiverH; OutMask = 0; }

			return H;
		}
	};
}
