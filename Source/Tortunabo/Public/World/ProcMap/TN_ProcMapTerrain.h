#pragma once

#include "CoreMinimal.h"
#include "World/ProcMap/TN_ProcMapLayout.h"
#include "World/ProcMap/TN_ProcMapCaves.h"

/**
 * Altura del terreno a partir del layout, evaluada sobre una malla regular de
 * vértices (la que luego se convierte en tiles de terreno con colisión).
 *
 * Composición (en este orden) para cada punto:
 *   1. Paisaje exterior: nivel suavizado de módulos + ondulación por bioma (pesos
 *      con deformación de dominio), montañas lejos de los caminos, volcanes,
 *      mesetas de los módulos elevados, lagunas con orillas escarpadas, muros del
 *      borde y acantilados de costa (salvo la playa de la meta).
 *   2. Cauce de cada camino: suelo a la cota de la muestra más cercana y un talud
 *      infranqueable (más empinado que la pendiente andable de la tortuga) hasta
 *      el borde; más allá el terreno sube hacia el paisaje o cae en ladera.
 *   3. Claro de salida (con borde escarpado).
 *   4. Estructuras colosales: holgura bajo puentes, torres, mesas y túnel.
 *   5. Zanjas de los huecos, pozas de lava, islas y río.
 *
 * Regla de diseño: fuera de los cauces nada es alcanzable a pie (no se puede salir
 * del camino ni atajar entre dos tramos); solo el agua de las lagunas es nadable.
 *
 * ComputeRows es thread-safe (filas disjuntas): el actor lo reparte con ParallelFor.
 */

namespace TNProcMap
{
	struct FBiomeTerrain
	{
		double UndAmp = 300.0;
		/** Subida suave del terreno más allá del borde del talud. */
		double RiseMax = 1500.0;
		double RiseDist = 3500.0;
		/** Pie del talud: franja casi plana entre el camino y el talud. */
		double Shoulder = 250.0;
		double Rough = 150.0;
		/** 0 = ruido suave, 1 = crestas (dunas, roca). */
		double Ridge = 0.0;
		double CoastWidth = 3000.0;
		/** Cota del lecho en biomas húmedos. */
		double BedZ = 0.0;
		bool bWet = false;
		/** Altura del talud del cauce (cm): mínimo y máximo, variando con ruido. */
		double BankMin = 500.0;
		double BankMax = 900.0;
		/**
		 * Pendiente media del talud (grados); la tortuga anda hasta ~45°. Con el perfil redondeado la
		 * parte central llega a 1,5 veces esa pendiente: 56-62° de media son paredes de 65-70°, que dejan
		 * ver el paisaje sin dejar de ser infranqueables.
		 */
		double BankAngle = 58.0;
		/** Relieve (montañas) lejos de los caminos (cm). */
		double MountainAmp = 3000.0;
		/** Paredes de cañón: cuánto sube el paisaje en los primeros ~35 m desde el cauce (cm). */
		double WallAmp = 1500.0;
	};

	inline FBiomeTerrain GetBiomeTerrain(ETNProcBiome B)
	{
		FBiomeTerrain T;
		switch (B)
		{
			case ETNProcBiome::Jungle:
				T.UndAmp = 350; T.RiseMax = 900;  T.RiseDist = 2200; T.Shoulder = 250; T.Rough = 160; T.Ridge = 0.0; T.CoastWidth = 2500;
				T.BankMin = 550; T.BankMax = 1100; T.BankAngle = 58; T.MountainAmp = 8400; T.WallAmp = 800; break;
			case ETNProcBiome::Beach:
				T.UndAmp = 110; T.RiseMax = 300;  T.RiseDist = 3500; T.Shoulder = 300; T.Rough = 190; T.Ridge = 0.8; T.CoastWidth = 6500;
				T.BankMin = 480; T.BankMax = 850;  T.BankAngle = 56; T.MountainAmp = 3000; T.WallAmp = 400; break;
			case ETNProcBiome::Desert:
				T.UndAmp = 300; T.RiseMax = 800;  T.RiseDist = 2200; T.Shoulder = 200; T.Rough = 380; T.Ridge = 1.0; T.CoastWidth = 3500;
				T.BankMin = 650; T.BankMax = 1400; T.BankAngle = 60; T.MountainAmp = 7200; T.WallAmp = 1000; break;
			case ETNProcBiome::Volcanic:
				T.UndAmp = 450; T.RiseMax = 1200; T.RiseDist = 2000; T.Shoulder = 200; T.Rough = 300; T.Ridge = 0.6; T.CoastWidth = 1200;
				T.BankMin = 750; T.BankMax = 1500; T.BankAngle = 60; T.MountainAmp = 9600; T.WallAmp = 1000; break;
			case ETNProcBiome::Water:
				T.UndAmp = 60;  T.RiseMax = 0;    T.RiseDist = 3000; T.Shoulder = 200; T.Rough = 60;  T.Ridge = 0.0; T.CoastWidth = 4000; T.BedZ = -1000; T.bWet = true;
				T.BankMin = 550; T.BankMax = 950;  T.BankAngle = 58; T.MountainAmp = 6600; T.WallAmp = 600; break;
			case ETNProcBiome::Rocky:
				T.UndAmp = 400; T.RiseMax = 1500; T.RiseDist = 1500; T.Shoulder = 150; T.Rough = 360; T.Ridge = 1.0; T.CoastWidth = 800;
				T.BankMin = 900; T.BankMax = 1800; T.BankAngle = 62; T.MountainAmp = 14400; T.WallAmp = 1400; break;
			case ETNProcBiome::Mangrove:
				T.UndAmp = 30;  T.RiseMax = 0;    T.RiseDist = 3000; T.Shoulder = 200; T.Rough = 25;  T.Ridge = 0.0; T.CoastWidth = 4000; T.BedZ = -350; T.bWet = true;
				T.BankMin = 500; T.BankMax = 850;  T.BankAngle = 56; T.MountainAmp = 3000; T.WallAmp = 500; break;
			case ETNProcBiome::Human:
				T.UndAmp = 150; T.RiseMax = 400;  T.RiseDist = 2500; T.Shoulder = 250; T.Rough = 60;  T.Ridge = 0.0; T.CoastWidth = 3000;
				T.BankMin = 480; T.BankMax = 750;  T.BankAngle = 58; T.MountainAmp = 3000; T.WallAmp = 400; break;
			default: break;
		}
		return T;
	}

	/** Nivel del agua de todo el mapa (mar, lagunas, río). */
	constexpr double SeaLevel = 0.0;

	/** Fondo de la zanja de un hueco de salto: 12 m bajo el camino, siempre por encima del agua. */
	inline double GapFloorZ(const FFeature& F)
	{
		// El río de lava corre 1,5 m bajo el suelo de la cueva; su cauce, 3 m más abajo.
		return IsLavaGap(F) ? F.Location.Z - 450.0 : FMath::Max(F.Location.Z - 1200.0, SeaLevel + 150.0);
	}

	/**
	 * Si P (en el borde de la plataforma de una torre) queda en una abertura del pretil: sobre el
	 * arranque de la pasada alta (puente o adarve) o, en la torre de salida, del tobogán. En las
	 * torres de muralla la abertura es justo la del adarve, que sigue entre sus parapetos.
	 */
	inline bool TowerOpeningAt(const FLayout& L, const FFeature& F, const FVector2D& P)
	{
		if (F.Aux == INDEX_NONE || !L.Crossings.IsValidIndex(F.Aux)) { return true; }
		const FCrossing& C = L.Crossings[F.Aux];
		const FRouteStep& High = L.Route[C.HighStep];
		const TArray<FPathSample>& M = L.Main;
		const double Margin = C.Type == ETNProcCrossingType::Wall ? 0.0 : 150.0;
		auto Near = [&](int32 From, int32 To)
		{
			for (int32 i = FMath::Max(0, From); i < FMath::Min(To, M.Num() - 1); ++i)
			{
				double T = 0.0;
				const double D = DistPointSegment(P, M[i].P, M[i + 1].P, T);
				if (D <= LerpD(M[i].Width, M[i + 1].Width, T) * 0.5 + Margin) { return true; }
			}
			return false;
		};
		if (F.PathIndex == High.FirstSample) { return Near(High.FirstSample, High.FirstSample + 10); }
		if (Near(High.LastSample - 10, High.LastSample)) { return true; }
		return C.HighStep + 1 < L.Route.Num() && Near(L.Route[C.HighStep + 1].FirstSample, L.Route[C.HighStep + 1].FirstSample + 10);
	}

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
			Init(InLayout, InOrigin, InSpacing, InNX, InNY);
			StampPathField();
			BuildCorridorDistance();
			BuildDivides();
			BuildInfluences();
		}

		/**
		 * Solo lo que necesita LandAt (biomas y distancia gruesa a los cauces), sin mallado: para consultar
		 * el relieve mientras se traza el mapa.
		 */
		void BuildCoarse(const FLayout& InLayout, const FVector2D& InOrigin, double InSpacing, int32 InNX, int32 InNY)
		{
			Init(InLayout, InOrigin, InSpacing, InNX, InNY);
			BuildCorridorDistance();
		}

		/** Cota del paisaje exterior en P, sin volcanes, agua, bordes ni cauces. */
		double LandAt(const FVector2D& P) const
		{
			FBiomeTerrain Bt;
			double Wet = 0.0;
			double W[NumBiomes];
			BlendBiomes(P, Bt, Wet, W);
			const double NLarge = Fbm2(Seed + 3u, P.X / 12000.0, P.Y / 12000.0, 3);
			const double NMed = Fbm2(Seed + 4u, P.X / 4000.0, P.Y / 4000.0, 2);
			const double NRidge = Ridged2(Seed + 5u, P.X / 5000.0, P.Y / 5000.0, 3);
			return RawLand(P, Bt, NLarge, LerpD(NMed, NRidge * 2.0 - 1.0, Bt.Ridge), L->SampleCoarse(L->LevelField, P));
		}

		/** Distancia aproximada (cm) de un punto al borde del cauce más cercano (campo grueso). */
		double CorridorDistance(const FVector2D& P) const { return SampleCoarseField(CorrDist, P); }

		/** Distancia aproximada (cm) a la divisoria más cercana entre tramos alejados del camino. */
		double DivideDistance(const FVector2D& P) const { return SampleCoarseField(DivideDist, P); }

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
			return Evaluate(P, PathDist[Idx], PathSeg[Idx], PathT[Idx], GuardZ[Idx], OtherEff[Idx], DeckTop[Idx], OutMask);
		}

	private:
		const FLayout* L = nullptr;
		uint32 Seed = 0;
		FBiomeTerrain Biomes[NumBiomes];
		double StartZ = 0.0;
		/** Y de la orilla del agua en la playa de la meta. */
		double ShoreWaterY = 0.0;
		/** Meta: X del eje, Y de la línea y semiancho de la boca (0 si no hay meta). */
		double FinishX = 0.0;
		double FinishLineAt = 0.0;
		double CoveHalf = 0.0;

		// ── Campo del camino ────────────────────────────────────────────────
		TArray<FPathSample> Samples;
		TArray<int32> NextOf;
		/** Cauce (polilínea) de cada muestra: 0 el principal, luego las ramas. */
		TArray<int32> SamplePoly;
		TArray<uint8> Carves;
		/** Cuánto va el camino por agua en cada muestra (0 tierra, 1 laguna): sin taludes y con orilla suave. */
		TArray<float> SampleWet;
		/** Cota del suelo del cauce: la del camino, o el fondo del canal bajo isletas y pasarelas. */
		TArray<float> SampleFloor;
		TArray<float> PathDist;
		TArray<int32> PathSeg;
		TArray<float> PathT;
		/** Puntuación del segmento ganador (distancia al borde, con la prioridad del túnel). */
		TArray<float> PathScore;
		/** Altura mínima fuera de los suelos: la del suelo + talud mínimo de cualquier cauce cercano. */
		TArray<float> GuardZ;
		/** Cota del tablero de un puente colosal sobre cada vértice de su huella (-1e9 fuera). */
		TArray<float> DeckTop;
		static constexpr double GuardBandMin = 300.0;
		static constexpr double GuardBandMax = 3500.0;
		/** Pendiente (tan) de la guarda: sube en rampa de 62° (infranqueable) en vez de en escalón. */
		static constexpr double GuardSlope = 1.88;
		/**
		 * Distancia al borde del otro cauce más cercano (otro camino, o el mismo tras una curva cerrada);
		 * 1e9 si no hay ninguno a menos de OtherReach. Entre dos cauces próximos la pared es más empinada.
		 */
		TArray<float> OtherEff;
		static constexpr double OtherReach = 1600.0;

		// ── Distancia gruesa a los cauces (para el relieve lejano) ──────────
		double CorrCell = 1000.0;
		FVector2D CorrOrigin = FVector2D::ZeroVector;
		int32 CorrW = 0;
		int32 CorrH = 0;
		TArray<float> CorrDist;
		/** Progreso del cauce más cercano en cada celda gruesa (-1 si ninguno). */
		TArray<float> CorrLabel;
		/** Distancia (cm) a la divisoria más cercana entre tramos alejados del camino. */
		TArray<float> DivideDist;
		/** Progreso de cada muestra (S del principal). */
		TArray<float> SampleProgress;
		/** Diferencia de progreso a partir de la cual dos tramos van separados por una divisoria. */
		static constexpr double DivideGap = 9000.0;
		TArray<int32> Volcanoes;

		// ── Influencias localizadas (cubos) ─────────────────────────────────
		enum class EInf : uint8 { Tower, Tunnel, DeckClear, Gate, Gap, Lava, Island, River, CaveMass };
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

		void Init(const FLayout& InLayout, const FVector2D& InOrigin, double InSpacing, int32 InNX, int32 InNY)
		{
			L = &InLayout;
			Origin = InOrigin;
			Spacing = InSpacing;
			NX = InNX;
			NY = InNY;
			Seed = InLayout.Params.Seed ^ 0x7E44Au;
			for (int32 b = 0; b < NumBiomes; ++b) { Biomes[b] = GetBiomeTerrain(BiomeFromIndex(b)); }
			Volcanoes.Reset();
			for (int32 f = 0; f < L->Features.Num(); ++f)
			{
				const FFeature& F = L->Features[f];
				if (F.Type == EFeature::StartArea) { StartZ = F.Location.Z; }
				if (F.Type == EFeature::Volcano) { Volcanoes.Add(f); }
				if (F.Type == EFeature::Finish) { FinishX = F.Location.X; FinishLineAt = F.Location.Y; CoveHalf = 0.5 * F.Width; }
			}
			ShoreWaterY = FinishWaterY(*L);
			BuildSamples();
		}

		void BuildSamples()
		{
			Samples.Reset();
			NextOf.Reset();
			SamplePoly.Reset();
			Carves.Reset();
			SampleWet.Reset();
			SampleFloor.Reset();
			SampleProgress.Reset();
			// Progreso: S del principal; en las ramas, interpolado entre su horquilla y su unión.
			int32 Poly = 0;
			auto AddPolyline = [&](const TArray<FPathSample>& In, double S0, double S1)
			{
				const int32 Base = Samples.Num();
				const double Len = In.Num() > 0 ? FMath::Max(1.0, In.Last().S) : 1.0;
				for (int32 i = 0; i < In.Num(); ++i)
				{
					SampleProgress.Add(static_cast<float>(S0 < 0.0 ? In[i].S : LerpD(S0, S1, In[i].S / Len)));
					Samples.Add(In[i]);
					NextOf.Add(i + 1 < In.Num() ? Base + i + 1 : INDEX_NONE);
					SamplePoly.Add(Poly);
					// Todo lo que pisa suelo es cauce, también la cima de las torres (aterrizaje del géiser).
					// Isletas y pasarelas van sobre un canal de agua: su cauce es agua bajo su cota.
					const bool bOverWater = (In[i].Flags & (PathFlags::Islet | PathFlags::Boardwalk)) != 0;
					const bool bCarve = bOverWater || (In[i].Flags & PathFlags::NotTerrain) == 0;
					Carves.Add(bCarve ? 1 : 0);
					SampleFloor.Add(static_cast<float>(bOverWater ? FMath::Min(In[i].Z, SeaLevel) - 250.0 : In[i].Z));
					FBiomeTerrain Bt;
					double Wet = 0.0;
					double W[NumBiomes];
					BlendBiomes(In[i].P, Bt, Wet, W);
					// Solo es camino "de agua" el que va a ras de agua; una torre en un manglar lleva sus taludes.
					SampleWet.Add(static_cast<float>(SmoothStep(0.49, 0.51, Wet) * SmoothStep(400.0, 150.0, In[i].Z)));
				}
				++Poly;
			};
			AddPolyline(L->Main, -1.0, -1.0);
			for (const FBranch& B : L->Branches) { AddPolyline(B.Samples, L->Main[B.ForkSample].S, L->Main[B.RejoinSample].S); }
		}

		void StampPathField()
		{
			const int32 N = NX * NY;
			PathDist.Init(1e9f, N);
			PathSeg.Init(INDEX_NONE, N);
			PathT.Init(0.0f, N);
			PathScore.Init(1e18f, N);
			GuardZ.Init(-1e9f, N);
			// Huella de los tableros de los puentes colosales: bajo ellos no se levantan las guardas de
			// otros cauces (el tobogán de la torre de salida arranca justo al lado del tablero).
			DeckTop.Init(-1e9f, N);
			for (const FCrossing& C : L->Crossings)
			{
				const FRouteStep& High = L->Route[C.HighStep];
				for (int32 i = High.FirstSample; i < High.LastSample; ++i)
				{
					const FPathSample& A = L->Main[i];
					const FPathSample& B = L->Main[i + 1];
					const double R = FMath::Max(A.Width, B.Width) * 0.5 + 300.0;
					const int32 X0 = FMath::Max(0, FMath::FloorToInt((FMath::Min(A.P.X, B.P.X) - R - Origin.X) / Spacing));
					const int32 X1 = FMath::Min(NX - 1, FMath::CeilToInt((FMath::Max(A.P.X, B.P.X) + R - Origin.X) / Spacing));
					const int32 Y0 = FMath::Max(0, FMath::FloorToInt((FMath::Min(A.P.Y, B.P.Y) - R - Origin.Y) / Spacing));
					const int32 Y1 = FMath::Min(NY - 1, FMath::CeilToInt((FMath::Max(A.P.Y, B.P.Y) + R - Origin.Y) / Spacing));
					for (int32 y = Y0; y <= Y1; ++y)
					{
						for (int32 x = X0; x <= X1; ++x)
						{
							double T = 0.0;
							if (DistPointSegment(Origin + FVector2D(x * Spacing, y * Spacing), A.P, B.P, T) <= R) { DeckTop[y * NX + x] = static_cast<float>(C.TopZ); }
						}
					}
				}
			}
			for (int32 i = 0; i < Samples.Num(); ++i)
			{
				const int32 j = NextOf[i];
				if (j == INDEX_NONE || !Carves[i] || !Carves[j]) { continue; }
				const FVector2D A = Samples[i].P;
				const FVector2D B = Samples[j].P;
				// Guarda: pared mínima alrededor de este suelo aunque el vértice "pertenezca" a otro cauce.
				const FBiomeTerrain& BtA = Biomes[BiomeIndex(Samples[i].Biome)];
				const FBiomeTerrain& BtB = Biomes[BiomeIndex(Samples[j].Biome)];
				const bool bWetSeg = FMath::Max(SampleWet[i], SampleWet[j]) > 0.5f;
				const double GuardH = bWetSeg ? 0.0 : FMath::Min(BtA.BankMin, BtB.BankMin);
				// Playa final (recta hacia +Y que se abre en campana): franja |x| <= Hw(y), cada vértice del
				// tramo que contiene su y. Con cápsulas, las muestras anchas del fondo "ganarían" la playa
				// entera y le pondrían su cota, ya bajo el agua.
				const bool bShoreSeg = (Samples[i].Flags & Samples[j].Flags & PathFlags::Shore) != 0 && B.Y > A.Y;
				// Alcance: talud y subida, o la ladera completa si el cauce va por encima del paisaje.
				const double Raised = FMath::Max(Samples[i].Z, Samples[j].Z) - L->SampleCoarse(L->LevelField, A);
				const double FlankReach = FMath::Clamp((Raised + 1800.0) / FlankSlope + 2000.0 + RimPlateau, 9000.0, 19000.0);
				const double Reach = FMath::Max(Samples[i].Width, Samples[j].Width) * 0.5 + FlankReach;
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
						double D = 0.0;
						if (bShoreSeg)
						{
							if (P.Y < A.Y || P.Y >= B.Y) { continue; }
							T = (P.Y - A.Y) / (B.Y - A.Y);
							D = FMath::Abs(P.X - LerpD(A.X, B.X, T));
						}
						else
						{
							D = DistPointSegment(P, A, B, T);
						}
						const int32 Idx = y * NX + x;
						// Distancia "efectiva": al borde del camino, para que el más ancho gane.
						const double Hw = LerpD(Samples[i].Width, Samples[j].Width, T) * 0.5;
						const double Eff = D - Hw;
						const double Score = Eff;
						if (D <= Reach && Score < PathScore[Idx])
						{
							PathScore[Idx] = static_cast<float>(Score);
							PathDist[Idx] = static_cast<float>(D);
							PathSeg[Idx] = i;
							PathT[Idx] = static_cast<float>(T);
						}
						if (GuardH > 0.0 && Eff >= GuardBandMin && Eff <= GuardBandMax && DeckTop[Idx] < -1e8f)
						{
							// Rampa desde el borde de la franja: pared de 62°, no un escalón a plomo. En la playa
							// final desaparece al llegar al agua, como los taludes (allí los brazos son el acantilado).
							const double Fade = bShoreSeg ? 1.0 - SmoothStep(ShoreWaterY - 800.0, ShoreWaterY + 200.0, P.Y) : 1.0;
							const double Ramp = Saturate((Eff - GuardBandMin) * GuardSlope / GuardH);
							const float G = static_cast<float>(LerpD(SampleFloor[i], SampleFloor[j], T) + GuardH * Ramp * Fade);
							GuardZ[Idx] = FMath::Max(GuardZ[Idx], G);
						}
					}
				}
			}
			StampOtherPaths();
		}

		/**
		 * OtherEff: por vértice, distancia al borde del cauce más cercano que no sea el suyo. Cuenta el punto
		 * de cada cauce más cercano al vértice (mínimo local a lo largo del cauce); del propio cauce, solo si
		 * el camino se aleja y vuelve (curva cerrada), no los tramos contiguos.
		 */
		void StampOtherPaths()
		{
			OtherEff.Init(1e9f, NX * NY);
			for (int32 i = 0; i < Samples.Num(); ++i)
			{
				const int32 j = NextOf[i];
				if (j == INDEX_NONE || !Carves[i] || !Carves[j]) { continue; }
				const FVector2D A = Samples[i].P;
				const FVector2D B = Samples[j].P;
				const int32 h = i > 0 && NextOf[i - 1] == i ? i - 1 : INDEX_NONE;
				const int32 k = NextOf[j];
				const double Reach = FMath::Max(Samples[i].Width, Samples[j].Width) * 0.5 + OtherReach;
				const int32 X0 = FMath::Max(0, FMath::FloorToInt((FMath::Min(A.X, B.X) - Reach - Origin.X) / Spacing));
				const int32 X1 = FMath::Min(NX - 1, FMath::CeilToInt((FMath::Max(A.X, B.X) + Reach - Origin.X) / Spacing));
				const int32 Y0 = FMath::Max(0, FMath::FloorToInt((FMath::Min(A.Y, B.Y) - Reach - Origin.Y) / Spacing));
				const int32 Y1 = FMath::Min(NY - 1, FMath::CeilToInt((FMath::Max(A.Y, B.Y) + Reach - Origin.Y) / Spacing));
				for (int32 y = Y0; y <= Y1; ++y)
				{
					for (int32 x = X0; x <= X1; ++x)
					{
						const int32 Idx = y * NX + x;
						const int32 o = PathSeg[Idx];
						if (o == INDEX_NONE || o == i) { continue; }
						const FVector2D P = Origin + FVector2D(x * Spacing, y * Spacing);
						double T = 0.0;
						const double D = DistPointSegment(P, A, B, T);
						const double Eff = D - LerpD(Samples[i].Width, Samples[j].Width, T) * 0.5;
						if (D > Reach || Eff >= OtherEff[Idx]) { continue; }
						// Un extremo solo cuenta si ningún tramo contiguo queda más cerca.
						if (T <= 0.0 && h != INDEX_NONE && Carves[h] && FVector2D::DotProduct(P - A, A - Samples[h].P) < 0.0) { continue; }
						if (T >= 1.0 && k != INDEX_NONE && Carves[k] && FVector2D::DotProduct(P - B, Samples[k].P - B) > 0.0) { continue; }
						if (SamplePoly[o] == SamplePoly[i])
						{
							// Mismo cauce: el recorrido entre los dos puntos ha de ser bastante más largo que su
							// distancia (el camino se alejó y ha vuelto); si no, es el mismo tramo.
							const int32 oj = NextOf[o];
							const double To = PathT[Idx];
							const FVector2D Qo = Samples[o].P + (Samples[oj].P - Samples[o].P) * To;
							const FVector2D Qs = A + (B - A) * T;
							const double Arc = FMath::Abs(LerpD(Samples[i].S, Samples[j].S, T) - LerpD(Samples[o].S, Samples[oj].S, To));
							if (Arc < 1.5 * FVector2D::Distance(Qo, Qs) + 500.0) { continue; }
						}
						OtherEff[Idx] = static_cast<float>(Eff);
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

		/** Transformada de distancia (chamfer 3x3) in situ; si Label no es null, propaga la etiqueta del origen. */
		void Chamfer(TArray<float>& D, TArray<float>* Label) const
		{
			const float Ortho = static_cast<float>(CorrCell);
			const float Diag = static_cast<float>(CorrCell * 1.41421356);
			auto Relax = [&](int32 Idx, int32 From, float Cost)
			{
				if (D[From] + Cost < D[Idx])
				{
					D[Idx] = D[From] + Cost;
					if (Label) { (*Label)[Idx] = (*Label)[From]; }
				}
			};
			for (int32 y = 0; y < CorrH; ++y)
			{
				for (int32 x = 0; x < CorrW; ++x)
				{
					const int32 Idx = y * CorrW + x;
					if (x > 0) { Relax(Idx, Idx - 1, Ortho); }
					if (y > 0)
					{
						Relax(Idx, Idx - CorrW, Ortho);
						if (x > 0) { Relax(Idx, Idx - CorrW - 1, Diag); }
						if (x < CorrW - 1) { Relax(Idx, Idx - CorrW + 1, Diag); }
					}
				}
			}
			for (int32 y = CorrH - 1; y >= 0; --y)
			{
				for (int32 x = CorrW - 1; x >= 0; --x)
				{
					const int32 Idx = y * CorrW + x;
					if (x < CorrW - 1) { Relax(Idx, Idx + 1, Ortho); }
					if (y < CorrH - 1)
					{
						Relax(Idx, Idx + CorrW, Ortho);
						if (x < CorrW - 1) { Relax(Idx, Idx + CorrW + 1, Diag); }
						if (x > 0) { Relax(Idx, Idx + CorrW - 1, Diag); }
					}
				}
			}
		}

		/** Transformada de distancia gruesa desde los bordes de todos los caminos, con el progreso del más cercano. */
		void BuildCorridorDistance()
		{
			CorrOrigin = Origin;
			CorrW = FMath::Max(2, FMath::CeilToInt(NX * Spacing / CorrCell) + 1);
			CorrH = FMath::Max(2, FMath::CeilToInt(NY * Spacing / CorrCell) + 1);
			CorrDist.Init(1e9f, CorrW * CorrH);
			CorrLabel.Init(-1.0f, CorrW * CorrH);
			for (int32 i = 0; i < Samples.Num(); ++i)
			{
				const FPathSample& S = Samples[i];
				// El tablero de un puente colosal no es suelo: bajo él no hay cauce (ni poza).
				if ((S.Flags & PathFlags::Elevated) != 0) { continue; }
				const double Hw = S.Width * 0.5;
				const int32 R = FMath::CeilToInt((Hw + 2.0 * CorrCell) / CorrCell);
				const int32 CX = FMath::RoundToInt((S.P.X - CorrOrigin.X) / CorrCell);
				const int32 CY = FMath::RoundToInt((S.P.Y - CorrOrigin.Y) / CorrCell);
				for (int32 y = FMath::Max(0, CY - R); y <= FMath::Min(CorrH - 1, CY + R); ++y)
				{
					for (int32 x = FMath::Max(0, CX - R); x <= FMath::Min(CorrW - 1, CX + R); ++x)
					{
						const FVector2D C = CorrOrigin + FVector2D(x * CorrCell, y * CorrCell);
						const float D = static_cast<float>(FMath::Max(0.0, FVector2D::Distance(C, S.P) - Hw));
						const int32 Idx = y * CorrW + x;
						if (D < CorrDist[Idx]) { CorrDist[Idx] = D; CorrLabel[Idx] = SampleProgress[i]; }
					}
				}
			}
			Chamfer(CorrDist, &CorrLabel);
		}

		/**
		 * Divisorias: celdas donde se tocan las zonas de influencia de dos tramos del camino
		 * alejados por el recorrido (más de DivideGap). Ahí el agua se corta: las pozas de dos
		 * tramos alejados nunca se tocan.
		 */
		void BuildDivides()
		{
			DivideDist.Init(1e9f, CorrW * CorrH);
			static const int32 DX[4] = { 1, -1, 0, 0 };
			static const int32 DY[4] = { 0, 0, 1, -1 };
			for (int32 y = 0; y < CorrH; ++y)
			{
				for (int32 x = 0; x < CorrW; ++x)
				{
					const int32 Idx = y * CorrW + x;
					if (CorrLabel[Idx] < 0.0f || CorrDist[Idx] < 250.0f) { continue; }
					for (int32 k = 0; k < 4; ++k)
					{
						const int32 X2 = x + DX[k];
						const int32 Y2 = y + DY[k];
						if (X2 < 0 || Y2 < 0 || X2 >= CorrW || Y2 >= CorrH) { continue; }
						const int32 N = Y2 * CorrW + X2;
						if (CorrLabel[N] < 0.0f || CorrDist[N] < 250.0f) { continue; }
						if (FMath::Abs(CorrLabel[Idx] - CorrLabel[N]) > DivideGap) { DivideDist[Idx] = 0.0f; break; }
					}
				}
			}
			Chamfer(DivideDist, nullptr);
		}

		double SampleCoarseField(const TArray<float>& F, const FVector2D& P) const
		{
			if (CorrW < 2 || CorrH < 2) { return 1e9; }
			const double Fx = FMath::Clamp((P.X - CorrOrigin.X) / CorrCell, 0.0, static_cast<double>(CorrW - 1) - 1e-6);
			const double Fy = FMath::Clamp((P.Y - CorrOrigin.Y) / CorrCell, 0.0, static_cast<double>(CorrH - 1) - 1e-6);
			const int32 X0 = FMath::FloorToInt(Fx);
			const int32 Y0 = FMath::FloorToInt(Fy);
			const double Tx = Fx - X0;
			const double Ty = Fy - Y0;
			const double A = LerpD(F[Y0 * CorrW + X0], F[Y0 * CorrW + X0 + 1], Tx);
			const double B = LerpD(F[(Y0 + 1) * CorrW + X0], F[(Y0 + 1) * CorrW + X0 + 1], Tx);
			return LerpD(A, B, Ty);
		}

		/** Forma del relieve lejano en [0, 1]: crestas deformadas sobre una base ondulada. */
		double MountainShape(const FVector2D& P) const
		{
			const FVector2D Warp(Fbm2(Seed + 21u, P.X / 22000.0, P.Y / 22000.0, 2), Fbm2(Seed + 22u, P.X / 22000.0, P.Y / 22000.0, 2));
			const FVector2D Q = P + Warp * 9000.0;
			// Crestas y picos: ruido crestado con una base ancha que decide dónde hay macizos.
			const double Ridges = Ridged2(Seed + 23u, Q.X / 24000.0, Q.Y / 24000.0, 4);
			const double Base = SmoothStep(0.25, 0.85, 0.5 + 0.5 * Fbm2(Seed + 24u, Q.X / 45000.0, Q.Y / 45000.0, 3));
			return Saturate(FMath::Pow(Ridges, 1.3) * (0.25 + 0.95 * Base));
		}

		/**
		 * Cono volcánico con cráter (sobre su cota de arranque, OutBase); 0 fuera de su radio.
		 * OutInfluence (0..1) dice cuánto sustituye el cono al relieve normal.
		 */
		double VolcanoHeight(const FVector2D& P, double& OutInfluence, double& OutBase) const
		{
			double Best = 0.0;
			double BestTop = -1e300;
			OutInfluence = 0.0;
			OutBase = 0.0;
			for (const int32 f : Volcanoes)
			{
				const FFeature& F = L->Features[f];
				const FVector2D C(F.Location.X, F.Location.Y);
				const double D = FVector2D::Distance(P, C);
				if (D >= F.Radius) { continue; }
				OutInfluence = FMath::Max(OutInfluence, SmoothStep(F.Radius, F.Radius * 0.55, D));
				// Ladera cóncava con barrancos radiales.
				const double U = D / F.Radius;
				const double Ang = AngleOf(P - C);
				const double Gully = 0.12 * Noise1(Seed + 31u + static_cast<uint32>(f), Ang * 3.0) * U;
				double H = F.Height * FMath::Pow(FMath::Max(0.0, 1.0 - U), 1.35) * (1.0 + Gully);
				const double Crater = F.Width * 0.5;
				if (D < Crater * 1.35)
				{
					// Borde del cráter y hundimiento hasta su fondo.
					const double V = D / Crater;
					const double Rim = F.Height * FMath::Pow(1.0 - Crater / F.Radius, 1.35);
					const double Bowl = LerpD(Rim - F.Length, Rim, V * V);
					H = V < 1.0 ? Bowl : LerpD(Rim, H, SmoothStep(1.0, 1.35, V));
				}
				// Donde se solapan dos conos, manda el más alto.
				if (F.Location.Z + H > BestTop)
				{
					BestTop = F.Location.Z + H;
					Best = H;
					OutBase = F.Location.Z;
				}
			}
			return Best;
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
				const FRouteStep& High = L->Route[L->Crossings[c].HighStep];
				for (int32 i = High.FirstSample; i < High.LastSample; ++i)
				{
					AddSegmentInf(EInf::DeckClear, c, i, M[i].P, M[i + 1].P, 3500.0);
				}
			}
			// Tramos bajo una estructura (puertas de muralla, cuevas): suelo a la cota del camino.
			for (int32 j = 0; j + 1 < M.Num(); ++j)
			{
				if (((M[j].Flags | M[j + 1].Flags) & PathFlags::Tunnel) != 0) { AddSegmentInf(EInf::Tunnel, INDEX_NONE, j, M[j].P, M[j + 1].P, 1200.0); }
			}

			for (int32 f = 0; f < L->Features.Num(); ++f)
			{
				const FFeature& F = L->Features[f];
				const FVector2D C(F.Location.X, F.Location.Y);
				FInf Inf;
				Inf.A = f;
				double Reach = 0.0;
				if (F.Type == EFeature::Cave)
				{
					// Loma sobre la cueva: cada tramo del túnel, con su alcance a los lados y en las bocas.
					for (int32 j = FMath::Max(0, F.PathIndex - 1); j < FMath::Min(M.Num() - 1, F.Aux + 1); ++j)
					{
						AddSegmentInf(EInf::CaveMass, f, j, M[j].P, M[j + 1].P, M[j].Width * 0.5 + 4000.0);
					}
					continue;
				}
				switch (F.Type)
				{
					case EFeature::Tower:      Inf.Type = EInf::Tower; Reach = F.Radius + 3200.0; break;
					case EFeature::DeckPillar: Inf.Type = EInf::Tower; Reach = F.Radius + 700.0; break;
					case EFeature::Gate:       Inf.Type = EInf::Gate;  Reach = FMath::Max(F.Width, F.Length) * 0.5 + 900.0; break;
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
			Out.BankMin = Out.BankMax = Out.BankAngle = Out.MountainAmp = Out.WallAmp = 0.0;
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
				Out.BankMin += Wb * T.BankMin;
				Out.BankMax += Wb * T.BankMax;
				Out.BankAngle += Wb * T.BankAngle;
				Out.MountainAmp += Wb * T.MountainAmp;
				Out.WallAmp += Wb * T.WallAmp;
				if (T.bWet) { WetSum += Wb; BedAcc += Wb * T.BedZ; }
			}
			if (Total <= 0.0) { Out = Biomes[0]; OutWet = 0.0; return; }
			Out.UndAmp /= Total; Out.RiseMax /= Total; Out.RiseDist /= Total; Out.Shoulder /= Total;
			Out.Rough /= Total; Out.Ridge /= Total; Out.CoastWidth /= Total;
			Out.BankMin /= Total; Out.BankMax /= Total; Out.BankAngle /= Total; Out.MountainAmp /= Total; Out.WallAmp /= Total;
			Out.BedZ = WetSum > 0.0 ? BedAcc / WetSum : -300.0;
			OutWet = WetSum / Total;
		}

		/** Paisaje exterior antes de volcanes, agua, bordes y cauces. */
		double RawLand(const FVector2D& P, const FBiomeTerrain& Bt, double NLarge, double Detail, double Level) const
		{
			const double Elev = L->SampleCoarse(L->ElevatedField, P);
			// País de cañones: el paisaje queda por encima del borde de los taludes (sube desde ellos en vez de
			// dejar mesetas planas) y las montañas arrancan a pocos metros de los cauces.
			const double MountMask = SmoothStep(700.0, 4500.0, CorridorDistance(P));
			const double Uplift = Bt.BankMax * (0.7 + 0.5 * (0.5 + 0.5 * NLarge));
			// Los módulos por los que no pasa el camino son macizos montañosos (cima en su centro, crestas).
			const double Shape = MountainShape(P);
			const double Massif = FMath::Pow(Elev, 1.4) * (4500.0 + 9000.0 * Shape + 3000.0 * NLarge);
			// Paredes de cañón: el paisaje sube con fuerza en los primeros ~35 m desde el cauce (unas
			// zonas son valles abiertos y otras gargantas, según un ruido lento).
			const double Walls = Bt.WallAmp * SmoothStep(300.0, 3500.0, CorridorDistance(P)) * (0.35 + 0.9 * (0.5 + 0.5 * NLarge));
			// Alrededor de los volcanes el relieve se allana (llanura volcánica): el cono destaca en vez de
			// quedar tapado por las montañas.
			double Calm = 1.0;
			for (const int32 f : Volcanoes)
			{
				const FFeature& F = L->Features[f];
				const double D = FVector2D::Distance(P, FVector2D(F.Location.X, F.Location.Y));
				Calm = FMath::Min(Calm, 1.0 - 0.85 * SmoothStep(F.Radius + 20000.0, F.Radius + 3000.0, D));
			}
			return Level + Uplift + Bt.UndAmp * NLarge + Bt.Rough * Detail + Walls + (Massif + Bt.MountainAmp * Shape * MountMask) * Calm;
		}

		double Evaluate(const FVector2D& P, float InPathDist, int32 InSeg, float InT, float InGuard, float InOtherEff, float InDeckTop, uint8& OutMask) const
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

			// ── Paisaje exterior (lo que no es cauce) ───────────────────────
			double Land = RawLand(P, Bt, NLarge, Detail, Level);
			// Dentro del cono manda el volcán, que arranca de la cota del relieve que lo rodea (así asoma
			// entre las montañas y el lago de lava cuadra con el cráter).
			double VolcanoInf = 0.0;
			double VolcanoBase = 0.0;
			const double Volcano = VolcanoHeight(P, VolcanoInf, VolcanoBase);
			Land = LerpD(Land + Volcano, VolcanoBase + Volcano, VolcanoInf);
			// Junto al agua la tierra queda siempre por encima: orillas escarpadas, sin playas por las que salir.
			Land = FMath::Max(Land, LerpD(Land, SeaLevel + ShoreCliffHeight + 250.0 * NMed, SmoothStep(0.0, 0.25, Wet)));
			// El agua de lagunas y manglares no es un lago abierto: son pozas alrededor de cada tramo
			// del camino, separadas por tierra alta en las
			// divisorias entre tramos alejados. No se puede ir nadando de un tramo a otro.
			// Anchura muy variable: una escala de ~250 m decide si hay canal estrecho (8 m) o laguna
			// amplia (hasta 150 m), con orilla irregular. La orilla es siempre un corte de 6 m (acantilado):
			// una transición proporcional al tamaño dejaría orillas andables en las lagunas grandes.
			const double Lagoon = FMath::Pow(0.5 + 0.5 * Fbm2(Seed + 51u, P.X / 25000.0, P.Y / 25000.0, 2), 1.6);
			const double PoolR = LerpD(800.0, 15000.0, Lagoon) * (1.0 + 0.25 * NMed);
			// Ni junto a la costa: una poza que llegara al mar sería un atajo nadando hasta la meta
			// (corte brusco: una transición larga dejaría una rampa andable para salir del agua).
			const double CoastCut = 1.0 - SmoothStep(L->CoastY(P.X) - 2600.0, L->CoastY(P.X) - 2000.0, P.Y);
			const double Pool = SmoothStep(PoolR, PoolR - 600.0, CorridorDistance(P)) * SmoothStep(1200.0, 1800.0, DivideDistance(P)) * CoastCut;
			// Contorno estrecho: el domain warp estira localmente la transición y una banda ancha dejaría orillas andables.
			const double WetT = SmoothStep(0.49, 0.51, Wet) * Pool;
			double Outer = LerpD(Land, Bt.BedZ + 90.0 * NMed, WetT);
			Outer = ApplyBorderWalls(P, Outer, Level, NLarge, NRidge);
			Outer = ApplyCoast(P, Outer, WetT, NMed);

			const int32 BX = FMath::Clamp(FMath::FloorToInt((P.X - BinOrigin.X) / BinCell), 0, BinW - 1);
			const int32 BY = FMath::Clamp(FMath::FloorToInt((P.Y - BinOrigin.Y) / BinCell), 0, BinH - 1);
			const TArray<FInf>& Bin = Bins[BY * BinW + BX];
			// Holgura bajo los puentes colosales: es paisaje, así los cauces cercanos conservan sus
			// taludes. Las torres quedan intactas (su cima es el cauce de aterrizaje).
			for (const FInf& Inf : Bin)
			{
				if (Inf.Type != EInf::DeckClear) { continue; }
				const FCrossing& C = L->Crossings[Inf.A];
				// Hasta el mismo pilar (que va encima, como influencia): el puente arranca limpio de la
				// plataforma de la torre.
				double T = 0.0;
				const double D = DistPointSegment(P, L->Main[Inf.B].P, L->Main[Inf.B + 1].P, T);
				if (D < 3500.0) { Outer = FMath::Min(Outer, C.TopZ - 1700.0); }
			}
			// Las torres sobresalen del paisaje: alrededor de cada una (22-30 m) el terreno queda al menos
			// 17 m bajo su cima. Si no, con paredes y montañas altas la cima quedaría a ras del paisaje y
			// se saldría de ella andando.
			for (const FInf& Inf : Bin)
			{
				if (Inf.Type != EInf::Tower) { continue; }
				const FFeature& F = L->Features[Inf.A];
				if (F.Type != EFeature::Tower) { continue; }
				const double D = FVector2D::Distance(P, FVector2D(F.Location.X, F.Location.Y));
				if (D < F.Radius + 3000.0)
				{
					Outer = FMath::Min(Outer, LerpD(F.Height - 1700.0, Outer, SmoothStep(F.Radius + 2200.0, F.Radius + 3000.0, D)));
				}
			}

			// ── Cauce del camino ────────────────────────────────────────────
			double H = Outer;
			if (InSeg != INDEX_NONE)
			{
				const FPathSample& A = Samples[InSeg];
				const int32 J = NextOf[InSeg];
				const FPathSample& B = Samples[J == INDEX_NONE ? InSeg : J];
				double PathZ = LerpD(SampleFloor[InSeg], SampleFloor[J == INDEX_NONE ? InSeg : J], InT);
				const double Hw = LerpD(A.Width, B.Width, InT) * 0.5;
				const uint32 Flags = A.Flags | B.Flags;
				const bool bLane = (Flags & PathFlags::Lane) != 0;
				double Beyond = FMath::Max(0.0, static_cast<double>(InPathDist) - Hw);
				// El claro de salida es parte del cauce: su borde también es talud.
				const double StartBeyond = FVector2D::Distance(P, L->StartPoint) - L->Params.StartClearingRadius;
				if (StartBeyond < Beyond)
				{
					Beyond = FMath::Max(0.0, StartBeyond);
					PathZ = StartZ;
				}

				// Talud infranqueable: más empinado que lo andable y más alto que un salto.
				const double BankN = 0.5 + 0.5 * Noise2(Seed + 9u, P.X / 6000.0, P.Y / 6000.0);
				// Solo el camino que va por el agua (bancos, isletas) va sin talud; un cauce de tierra junto
				// a una laguna conserva su talud por ese lado (si no, se atajaría nadando).
				// Y solo por el lado que da al agua: hacia tierra el banco de arena también lleva talud.
				const double PathWet = LerpD(SampleWet[InSeg], SampleWet[J == INDEX_NONE ? InSeg : J], InT) * WetT;
				double BankH = LerpD(Bt.BankMin, Bt.BankMax, BankN) * (1.0 - PathWet);
				if (bLane) { BankH = FMath::Max(BankH, 1100.0); }
				// El cauce se abre solo mar adentro: la meta es una playa encajada que da al agua.
				BankH *= 1.0 - SmoothStep(L->CoastY(P.X) - 300.0, L->CoastY(P.X) + 1500.0, P.Y);
				// Playa final: al llegar al agua los brazos pasan a ser el propio acantilado de la costa (pared
				// a plomo hasta él, sin meseta ni guarda), y mar adentro fuera de la franja solo queda el fondo.
				// Así no hay lenguas de tierra baja junto al mar por las que salir nadando del mapa.
				const bool bShore = (A.Flags & B.Flags & PathFlags::Shore) != 0;
				const double SeaSide = bShore ? SmoothStep(ShoreWaterY - 800.0, ShoreWaterY + 200.0, P.Y) : 0.0;
				BankH *= 1.0 - SeaSide;
				// Bajo una estructura la zanja es de paredes a plomo desde el borde del suelo (la tapa ella).
				const bool bTunnel = (Flags & PathFlags::Tunnel) != 0;
				const double Toe = (bLane || bTunnel || (Flags & (PathFlags::Slide | PathFlags::TowerTop)) != 0) ? 150.0 : Bt.Shoulder;
				const double Run = bTunnel ? 60.0 : BankH / FMath::Tan(FMath::DegreesToRadians(Bt.BankAngle));
				const double Rim = PathZ + BankH;
				// La guarda de los cauces cercanos sube en rampa de 62° desde el borde de este: si no, los
				// tramos vecinos (o el nivel alto de un tobogán) la levantan a plomo al pie del talud.
				// Entre dos cauces próximos (horquillas, curvas cerradas) sube más empinada, hasta el talud
				// mínimo en la cresta que los separa: así la cresta no nace a ras de suelo como una rampa.
				const double Crest = 0.5 * (Beyond + static_cast<double>(InOtherEff));
				const double GuardRise = FMath::Max(GuardSlope, Bt.BankMin / FMath::Max(1.0, Crest - GuardBandMin));
				const double Guard = FMath::Min(static_cast<double>(InGuard), PathZ + FMath::Max(0.0, Beyond - GuardBandMin) * GuardRise);

				if (Beyond <= 0.0)
				{
					H = PathZ + 10.0 * NMed;
					OutMask = 255;
				}
				else if (Beyond < Toe)
				{
					const double T = Beyond / Toe;
					H = PathZ + FMath::Min(25.0, BankH) * SmoothStep(0.0, 1.0, T);
					OutMask = static_cast<uint8>(FMath::RoundToInt(255.0 * (1.0 - SmoothStep(0.0, 1.0, T))));
				}
				else if (Beyond < Toe + Run)
				{
					const double T = (Beyond - Toe) / FMath::Max(1.0, Run);
					const double Base = PathZ + FMath::Min(25.0, BankH);
					H = FMath::Max(LerpD(Base, Rim, T * T * (3.0 - 2.0 * T)), Guard);
				}
				else
				{
					// Más allá del talud: sube hacia el paisaje, o si el cauce va por encima,
					// meseta a la cota del borde y luego ladera. La meseta hace que cualquier
					// muesca del talud (claro, zanjas) dé a terreno alto, nunca a una salida.
					const double X = Beyond - Toe - Run;
					if (Outer >= Rim)
					{
						// Bajo una estructura (y en la orilla de la playa final) la pared sube a plomo hasta arriba.
						const double RiseDist = bTunnel ? 150.0 : LerpD(Bt.RiseDist, 150.0, SeaSide);
						H = Rim + (Outer - Rim) * SmoothStep(0.0, FMath::Max(RiseDist, 1.0), X);
					}
					else
					{
						const double Flank = LerpD(LerpD(FlankSlope, 0.45, PathWet), 4.0, SeaSide);
						const double Plateau = RimPlateau * (1.0 - PathWet) * (1.0 - SeaSide);
						H = FMath::Max(Outer, Rim + 120.0 * NMed * (1.0 - PathWet) * (1.0 - SeaSide) - FMath::Max(0.0, X - Plateau) * Flank);
					}
					H = FMath::Max(H, Guard);
				}
			}

			// Bajo el tablero de un puente colosal: vacío (y zona de muerte) al menos 1,5 m por debajo,
			// sin taludes ni mesetas que lo tapen. Las torres y los pilares van encima (influencias).
			if (InDeckTop > -1e8f) { H = FMath::Min(H, static_cast<double>(InDeckTop) - 150.0); }

			// ── Influencias localizadas ─────────────────────────────────────
			if (Bin.Num() > 0)
			{
				H = ApplyInfluences(P, H, Bin, OutMask, InSeg);
			}
			return H;
		}

		/** Muros naturales del borde (sur, este, oeste); no llegan al mar. */
		double ApplyBorderWalls(const FVector2D& P, double H, double Level, double NLarge, double NRidge) const
		{
			const double ES = P.Y;
			const double EW = P.X;
			const double EE = L->WorldSize - P.X;
			double E = ES;
			double Along = P.X;
			if (EW < E) { E = EW; Along = P.Y + 100000.0; }
			if (EE < E) { E = EE; Along = P.Y + 200000.0; }
			// Solo se retira mar adentro: si se desvaneciera antes dejaría rampas suaves desde el agua.
			const double CoastFade = 1.0 - SmoothStep(L->CoastY(P.X), L->CoastY(P.X) + 1500.0, P.Y);
			const double Inset = L->WallInset(Along);
			const double T = SmoothStep(Inset + 1200.0, Inset, E) * CoastFade;
			if (T > 0.0)
			{
				const double WallTop = FMath::Max(Level, 800.0) + L->Params.WallHeight * (0.75 + 0.35 * NLarge) + 600.0 * NRidge;
				H = FMath::Max(H, LerpD(H, WallTop, T));
			}
			return H;
		}

		/**
		 * Costa norte: acantilado sobre el mar en toda la línea de costa. La meta es una
		 * cala: el cauce del último tramo (la playa) es la única bajada al agua, así el
		 * mar no da acceso al resto del mapa.
		 */
		double ApplyCoast(const FVector2D& P, double H, double WetT, double NMed) const
		{
			// Junto a la playa de la meta el acantilado llega al menos 7 m más allá de la línea: los brazos
			// siguen en pie al cruzarla y nadie sale al mar sin pasar por ella (la costa natural ondula
			// varios metros en pocas decenas).
			double Coast = L->CoastY(P.X);
			if (CoveHalf > 0.0)
			{
				const double W = SmoothStep(CoveHalf + 6000.0, CoveHalf + 2000.0, FMath::Abs(P.X - FinishX));
				Coast = FMath::Max(Coast, LerpD(Coast, FinishLineAt + 700.0, W));
			}
			if (P.Y < Coast - 3500.0) { return H; }
			const double SeaBed = FMath::Max(-2600.0, -450.0 - FMath::Max(0.0, P.Y - Coast) * 0.035);
			// Solo se levanta tierra: una laguna junto a la costa no puede convertirse en rampa de salida.
			const double NearEdge = WetT < 0.5 ? SmoothStep(Coast - 3000.0, Coast - 700.0, P.Y) : 0.0;
			const double Cliff = FMath::Max(H, LerpD(H, SeaLevel + ShoreCliffHeight + 300.0 * NMed, NearEdge));
			return LerpD(Cliff, SeaBed, SmoothStep(Coast - 400.0, Coast + 200.0, P.Y));
		}

		bool TowerOpening(const FFeature& F, const FVector2D& P) const { return TowerOpeningAt(*L, F, P); }

		double ApplyInfluences(const FVector2D& P, double H, const TArray<FInf>& Bin, uint8& OutMask, int32 InSeg) const
		{
			const TArray<FPathSample>& M = L->Main;

			// Loma sobre las cuevas: fuera del suelo del túnel el terreno sube hasta cubrir su techo (y baja
			// en ladera más allá); en las bocas acaba en un frente de roca. Solo los vértices de ese tramo
			// del camino (o de las muestras de justo antes y después): no tapa otros caminos cercanos.
			double Ridge = -1e18;
			for (const FInf& Inf : Bin)
			{
				if (Inf.Type != EInf::CaveMass || InSeg == INDEX_NONE) { continue; }
				const FFeature& F = L->Features[Inf.A];
				if (InSeg < F.PathIndex - 3 || InSeg > F.Aux + 2) { continue; }
				const FPathSample& A = M[Inf.B];
				const FPathSample& B = M[Inf.B + 1];
				double T = 0.0;
				const double D = DistPointSegment(P, A.P, B.P, T);
				const double Hw = LerpD(A.Width, B.Width, T) * 0.5;
				const double Eff = D - Hw;
				if (Eff < 180.0) { continue; }
				const double Floor = LerpD(A.Z, B.Z, T);
				double Z = Floor + CaveDetail::Clearance(Hw * 2.0, F.Height) + F.Radius + 150.0 - FMath::Max(0.0, Eff - 700.0) * 0.7;
				// Fuera del túnel (antes de la primera muestra o después de la última): frente casi a plomo.
				const double Out = FMath::Max(0.0, FVector2D::DotProduct(M[F.PathIndex].P - P, M[F.PathIndex].Dir))
					+ FMath::Max(0.0, FVector2D::DotProduct(P - M[F.Aux].P, M[F.Aux].Dir));
				Z -= Out * 3.0;
				Ridge = FMath::Max(Ridge, Z);
			}
			if (Ridge > H) { H = Ridge; }

			// Torres (pilares de roca) en los extremos de las pasadas altas.
			for (const FInf& Inf : Bin)
			{
				if (Inf.Type != EInf::Tower) { continue; }
				const FFeature& F = L->Features[Inf.A];
				const double D = FVector2D::Distance(P, FVector2D(F.Location.X, F.Location.Y));
				// Pilar: plataforma plana a la cota de la cima en todo su radio (sin taludes ni mesetas
				// dentro): ahí se encuentran el aterrizaje del géiser, el puente y el tobogán. Los pilares
				// bajo el tablero solo suben hasta su cima. Las torres de muralla van forradas de fábrica
				// (malla, con almenas): aquí su núcleo, de paredes a plomo, y el pretil que las cierra.
				const bool bWallTower = F.Type == EFeature::Tower && L->Crossings.IsValidIndex(F.Aux) && L->Crossings[F.Aux].Type == ETNProcCrossingType::Wall;
				if (D <= F.Radius)
				{
					// Pretil de roca de 1,8 m en el borde (no se salta), abierto hacia el puente y el tobogán.
					const bool bParapet = F.Type == EFeature::Tower && D > F.Radius - 300.0 && !TowerOpening(F, P);
					H = F.Type == EFeature::Tower ? F.Height + (bParapet ? 180.0 : 0.0) : FMath::Max(H, F.Height);
					OutMask = FMath::Max<uint8>(OutMask, 150);
				}
				else if (D < F.Radius + 600.0 && !bWallTower) { H = FMath::Max(H, LerpD(F.Height, H, (D - F.Radius) / 600.0)); }
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

			// Puertas de muralla: suelo llano a la cota del camino en todo el paso y una explanada de 6 m
			// ante cada boca que se funde con el terreno.
			for (const FInf& Inf : Bin)
			{
				if (Inf.Type != EInf::Gate) { continue; }
				const FFeature& F = L->Features[Inf.A];
				const FVector2D Rel = P - FVector2D(F.Location.X, F.Location.Y);
				if (FMath::Abs(FVector2D::DotProduct(Rel, F.Dir)) > F.Width * 0.5 + 100.0) { continue; }
				const double Out = FMath::Max(0.0, FMath::Abs(FVector2D::DotProduct(Rel, LeftNormal(F.Dir))) - F.Length * 0.5);
				if (Out >= 600.0) { continue; }
				const double T = SmoothStep(0.0, 600.0, Out);
				H = LerpD(F.Location.Z, H, T);
				OutMask = FMath::Max<uint8>(OutMask, static_cast<uint8>(FMath::RoundToInt(255.0 * (1.0 - T))));
			}

			// Zanjas de los huecos de salto.
			for (const FInf& Inf : Bin)
			{
				if (Inf.Type != EInf::Gap) { continue; }
				const FFeature& F = L->Features[Inf.A];
				const FVector2D Rel = P - FVector2D(F.Location.X, F.Location.Y);
				const double Along = FVector2D::DotProduct(Rel, F.Dir);
				const double Across = FVector2D::DotProduct(Rel, LeftNormal(F.Dir));
				if (FMath::Abs(Along) <= F.Height * 0.5 && FMath::Abs(Across) <= F.Width * 0.5 + GapTrenchSideOf(F))
				{
					H = FMath::Min(H, GapFloorZ(F));
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
