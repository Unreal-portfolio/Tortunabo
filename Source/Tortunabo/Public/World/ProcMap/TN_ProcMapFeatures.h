#pragma once

#include "CoreMinimal.h"
#include "World/ProcMap/TN_ProcMapLayout.h"
#include "World/ProcMap/TN_ProcMapPath.h"

/**
 * Elementos colocados sobre el camino y el terreno: estructura de los cruces
 * colosales, huecos de salto, isletas, pasarelas, pilas de huevos, puzles 2vs2,
 * río opcional, pozas de lava e islas. Todo determinista a partir del FRng.
 */

namespace TNProcMap
{
	namespace FeatureDetail
	{
		inline bool AnyFlag(const TArray<FPathSample>& Samples, int32 From, int32 To, uint32 Mask)
		{
			for (int32 i = FMath::Max(0, From); i <= FMath::Min(Samples.Num() - 1, To); ++i)
			{
				if ((Samples[i].Flags & Mask) != 0) { return true; }
			}
			return false;
		}

		inline FFeature MakeAtSample(EFeature Type, const FPathSample& Sm, int32 PathIndex, int32 BranchIndex)
		{
			FFeature F;
			F.Type = Type;
			F.Location = FVector(Sm.P, Sm.Z);
			F.Dir = Sm.Dir;
			F.Width = Sm.Width;
			F.PathIndex = PathIndex;
			F.BranchIndex = BranchIndex;
			F.Biome = Sm.Biome;
			return F;
		}

		/** Desenfoque de caja separable, Channels valores por celda, in-place. */
		inline void BoxBlur(TArray<float>& Data, int32 W, int32 H, int32 Channels, int32 Radius)
		{
			if (Radius <= 0) { return; }
			TArray<float> Tmp;
			Tmp.SetNum(Data.Num());
			for (int32 y = 0; y < H; ++y)
			{
				for (int32 x = 0; x < W; ++x)
				{
					for (int32 c = 0; c < Channels; ++c)
					{
						double Sum = 0.0;
						int32 N = 0;
						for (int32 k = FMath::Max(0, x - Radius); k <= FMath::Min(W - 1, x + Radius); ++k) { Sum += Data[(y * W + k) * Channels + c]; ++N; }
						Tmp[(y * W + x) * Channels + c] = static_cast<float>(Sum / N);
					}
				}
			}
			for (int32 y = 0; y < H; ++y)
			{
				for (int32 x = 0; x < W; ++x)
				{
					for (int32 c = 0; c < Channels; ++c)
					{
						double Sum = 0.0;
						int32 N = 0;
						for (int32 k = FMath::Max(0, y - Radius); k <= FMath::Min(H - 1, y + Radius); ++k) { Sum += Tmp[(k * W + x) * Channels + c]; ++N; }
						Data[(y * W + x) * Channels + c] = static_cast<float>(Sum / N);
					}
				}
			}
		}
	}

	/** Campos suaves de bioma, nivel y "módulo elevado" en un raster grueso. */
	inline void BuildBiomeFields(FLayout& L)
	{
		using namespace FeatureDetail;
		L.BiomeCell = 800.0;
		L.BiomeW = FMath::Max(2, FMath::CeilToInt(L.WorldSize / L.BiomeCell));
		L.BiomeH = L.BiomeW;
		const int32 Cells = L.BiomeW * L.BiomeH;
		L.BiomeWeights.Init(0.0f, Cells * NumBiomes);
		L.LevelField.Init(0.0f, Cells);
		L.ElevatedField.Init(0.0f, Cells);
		for (int32 y = 0; y < L.BiomeH; ++y)
		{
			for (int32 x = 0; x < L.BiomeW; ++x)
			{
				const FVector2D C((x + 0.5) * L.BiomeCell, (y + 0.5) * L.BiomeCell);
				int32 Mod = L.ModuleAt(C);
				if (Mod == INDEX_NONE) { Mod = L.ModuleAt(FVector2D(FMath::Min(C.X, L.WorldSize - 1.0), FMath::Min(C.Y, L.WorldSize - 1.0))); }
				if (Mod == INDEX_NONE) { continue; }
				const FModule& M = L.Modules[Mod];
				const int32 Idx = y * L.BiomeW + x;
				L.BiomeWeights[Idx * NumBiomes + BiomeIndex(M.Biome)] = 1.0f;
				L.LevelField[Idx] = static_cast<float>(M.Level);
				// Todo módulo sin camino principal es un macizo: fuera del camino nada es transitable.
				L.ElevatedField[Idx] = M.VisitCount > 0 ? 0.0f : (M.bHasBranch && M.EmptyKind != ETNProcEmptyModuleMode::Elevated ? 0.6f : 1.0f);
			}
		}
		BoxBlur(L.BiomeWeights, L.BiomeW, L.BiomeH, NumBiomes, 4);
		BoxBlur(L.BiomeWeights, L.BiomeW, L.BiomeH, NumBiomes, 3);
		BoxBlur(L.LevelField, L.BiomeW, L.BiomeH, 1, 8);
		BoxBlur(L.LevelField, L.BiomeW, L.BiomeH, 1, 5);
		// Desenfoque amplio: el macizo culmina en el centro del módulo y baja en ladera hacia sus bordes.
		BoxBlur(L.ElevatedField, L.BiomeW, L.BiomeH, 1, 9);
		BoxBlur(L.ElevatedField, L.BiomeW, L.BiomeH, 1, 7);
	}

	/** Salida, meta, géiseres, toboganes, torres, tablero/mesa y techo de cueva. */
	inline void BuildStructuralFeatures(FLayout& L)
	{
		using namespace FeatureDetail;
		const FGenParams& P = L.Params;
		const TArray<FPathSample>& M = L.Main;

		{
			FFeature F = MakeAtSample(EFeature::StartArea, M[0], 0, INDEX_NONE);
			F.Location = FVector(L.StartPoint, M[0].Z);
			F.Radius = P.StartClearingRadius;
			L.Features.Add(F);
		}
		for (int32 i = 0; i < M.Num(); ++i)
		{
			if ((M[i].Flags & PathFlags::Shore) != 0)
			{
				FFeature F = MakeAtSample(EFeature::Finish, M[i], i, INDEX_NONE);
				F.Location = FVector(M[i].P, 0.0);
				F.Width = 9000.0;
				F.Length = 5000.0;
				F.Dir = FVector2D(0.0, 1.0);
				L.Features.Add(F);
				break;
			}
		}

		// Géiseres: aterrizan en la torre (cruce colosal) o pasado el escalón.
		for (int32 i = 0; i < M.Num(); ++i)
		{
			if ((M[i].Flags & PathFlags::GeyserBase) == 0) { continue; }
			FFeature F = MakeAtSample(EFeature::Geyser, M[i], i, INDEX_NONE);
			F.Target = F.Location;
			for (int32 j = i + 1; j < M.Num(); ++j)
			{
				if ((M[j].Flags & PathFlags::UnderTower) != 0)
				{
					// Centro de la torre = muestra de portal (última de la zona bajo torre).
					int32 k = j;
					while (k + 1 < M.Num() && (M[k + 1].Flags & PathFlags::UnderTower) != 0) { ++k; }
					F.Target = FVector(M[k].P, M[k].Z);
					break;
				}
				if ((M[j].Flags & PathFlags::CliffUp) != 0)
				{
					int32 k = j;
					while (k + 1 < M.Num() && M[k].S - M[j].S < 900.0) { ++k; }
					F.Target = FVector(M[k].P, M[k].Z);
					break;
				}
				if (M[j].S - M[i].S > 6000.0) { break; }
			}
			F.Height = F.Target.Z - F.Location.Z;
			L.Features.Add(F);
		}

		// Toboganes: tramos contiguos con Slide (principal y rutas altas de las ramas).
		auto AddSlides = [&L](const TArray<FPathSample>& S, int32 BranchIndex)
		{
			for (int32 i = 0; i < S.Num(); ++i)
			{
				if ((S[i].Flags & PathFlags::Slide) == 0) { continue; }
				int32 j = i;
				while (j + 1 < S.Num() && (S[j + 1].Flags & PathFlags::Slide) != 0) { ++j; }
				const int32 Top = FMath::Max(0, i - 1);
				FFeature F = MakeAtSample(EFeature::SlideZone, S[Top], Top, BranchIndex);
				F.Aux = j;
				F.Target = FVector(S[j].P, S[j].Z);
				F.Height = S[Top].Z - S[j].Z;
				F.Length = S[j].S - S[Top].S;
				for (int32 k = Top; k <= j; ++k) { F.Width = FMath::Max(F.Width, S[k].Width); }
				L.Features.Add(F);
				i = j;
			}
		};
		AddSlides(M, INDEX_NONE);
		for (int32 b = 0; b < L.Branches.Num(); ++b) { AddSlides(L.Branches[b].Samples, b); }

		// Cruces colosales.
		for (int32 c = 0; c < L.Crossings.Num(); ++c)
		{
			const FCrossing& C = L.Crossings[c];
			const FRouteStep& High = L.Route[C.HighStep];
			for (int32 End = 0; End < 2; ++End)
			{
				const int32 Si = End == 0 ? High.FirstSample : High.LastSample;
				FFeature T = MakeAtSample(EFeature::Tower, M[Si], Si, INDEX_NONE);
				T.Location = FVector(M[Si].P, C.TopZ);
				T.Radius = P.TowerRadius;
				T.Height = C.TopZ;
				T.Aux = c;
				L.Features.Add(T);
			}
			// Puente colgante: pilares de roca cada 70-100 m en los vanos largos, lejos de cualquier
			// otro camino de abajo (si no cabe en su sitio se desplaza hasta 30 m; si no, se omite).
			if (C.Type == ETNProcCrossingType::Bridge)
			{
				const double PillarR = 450.0;
				const double S0 = M[High.FirstSample].S + P.TowerRadius + 2500.0;
				const double S1 = M[High.LastSample].S - P.TowerRadius - 2500.0;
				const int32 NumPillars = S1 - S0 > 9000.0 ? FMath::FloorToInt((S1 - S0) / 8500.0) : 0;
				auto Clear = [&](const FVector2D& Pt)
				{
					auto Far = [&](const TArray<FPathSample>& Arr, int32 SkipFrom, int32 SkipTo)
					{
						for (int32 i = 0; i < Arr.Num(); ++i)
						{
							if (i >= SkipFrom && i <= SkipTo) { continue; }
							if (FVector2D::Distance(Arr[i].P, Pt) < PillarR + 600.0 + Arr[i].Width * 0.5 + 1500.0) { return false; }
						}
						return true;
					};
					if (!Far(M, High.FirstSample, High.LastSample)) { return false; }
					for (const FBranch& B : L.Branches) { if (!Far(B.Samples, INDEX_NONE, INDEX_NONE)) { return false; } }
					return true;
				};
				for (int32 k = 1; k <= NumPillars; ++k)
				{
					const double Target = S0 + (S1 - S0) * k / (NumPillars + 1);
					for (const double Off : { 0.0, 1500.0, -1500.0, 3000.0, -3000.0 })
					{
						int32 Idx = INDEX_NONE;
						PathDetail::MainPointAt(M, Target + Off, nullptr, &Idx);
						if (Idx == INDEX_NONE || !Clear(M[Idx].P)) { continue; }
						FFeature Pl = MakeAtSample(EFeature::DeckPillar, M[Idx], Idx, INDEX_NONE);
						Pl.Location = FVector(M[Idx].P, C.TopZ - 30.0);
						Pl.Radius = PillarR;
						Pl.Height = C.TopZ - 30.0;
						Pl.Aux = c;
						L.Features.Add(Pl);
						break;
					}
				}
			}
			// Caerse del puente colgante mata: cajas desde 60 cm bajo el tablero hasta 12,5 m, en tramos
			// de <= 20 m desde el borde de un pilar de torre hasta el del otro. Nunca sobre el vuelo del
			// géiser (tramo de llegada a la torre de entrada) ni sobre el tobogán de la de salida.
			if (C.Type == ETNProcCrossingType::Bridge)
			{
				const FVector2D TowerIn = M[High.FirstSample].P;
				const FVector2D TowerOut = M[High.LastSample].P;
				TArray<FVector> Avoid;
				if (C.HighStep > 0)
				{
					const FRouteStep& Prev = L.Route[C.HighStep - 1];
					for (int32 i = Prev.FirstSample; i <= Prev.LastSample; ++i)
					{
						if (FVector2D::Distance(M[i].P, TowerIn) < P.TowerRadius + 3500.0) { Avoid.Add(FVector(M[i].P, M[i].Z)); }
					}
				}
				if (C.HighStep + 1 < L.Route.Num())
				{
					const FRouteStep& Next = L.Route[C.HighStep + 1];
					for (int32 i = Next.FirstSample; i <= Next.LastSample; ++i)
					{
						if (FVector2D::Distance(M[i].P, TowerOut) < P.TowerRadius + 6000.0) { Avoid.Add(FVector(M[i].P, M[i].Z)); }
					}
				}
				auto Emit = [&](int32 From, int32 To)
				{
					const FVector2D A = M[From].P;
					const FVector2D B = M[To].P;
					FKillBox K;
					K.Dir = (B - A).GetSafeNormal();
					if (K.Dir.IsNearlyZero()) { return; }
					double HalfW = 0.0;
					for (int32 i = From; i <= To; ++i) { HalfW = FMath::Max(HalfW, M[i].Width * 0.5); }
					K.Center = FVector((A + B) * 0.5, C.TopZ - 655.0);
					K.Half = FVector(FVector2D::Distance(A, B) * 0.5 + 150.0, HalfW + 800.0, 595.0);
					for (const FVector& Q : Avoid)
					{
						const FVector2D Rel(Q.X - K.Center.X, Q.Y - K.Center.Y);
						const bool bIn = FMath::Abs(FVector2D::DotProduct(Rel, K.Dir)) <= K.Half.X + 400.0
							&& FMath::Abs(FVector2D::DotProduct(Rel, FVector2D(-K.Dir.Y, K.Dir.X))) <= K.Half.Y + 400.0;
						if (bIn) { return; }
					}
					L.KillBoxes.Add(K);
				};
				int32 From = INDEX_NONE;
				for (int32 i = High.FirstSample; i <= High.LastSample; ++i)
				{
					const bool bFree = FVector2D::Distance(M[i].P, TowerIn) > P.TowerRadius + 50.0 && FVector2D::Distance(M[i].P, TowerOut) > P.TowerRadius + 50.0;
					if (bFree && From == INDEX_NONE) { From = i; }
					const bool bFlush = From != INDEX_NONE && (!bFree || i == High.LastSample || M[i].S - M[From].S >= 2000.0);
					if (!bFlush) { continue; }
					const int32 To = bFree ? i : i - 1;
					if (To > From) { Emit(From, To); }
					From = bFree ? i : INDEX_NONE;
				}
			}
			FFeature S = MakeAtSample(C.Type == ETNProcCrossingType::Bridge ? EFeature::Deck : EFeature::Mesa, M[High.FirstSample], High.FirstSample, INDEX_NONE);
			S.Aux = c;
			S.Aux2 = High.LastSample;
			S.Height = C.TopZ;
			S.Location = FVector(C.CrossPoint, C.TopZ);
			L.Features.Add(S);

			if (C.Type == ETNProcCrossingType::Cave)
			{
				const FRouteStep& Low = L.Route[C.LowStep];
				for (int32 i = Low.FirstSample; i <= Low.LastSample; ++i)
				{
					if ((M[i].Flags & PathFlags::Tunnel) == 0) { continue; }
					int32 j = i;
					while (j + 1 <= Low.LastSample && (M[j + 1].Flags & PathFlags::Tunnel) != 0) { ++j; }
					FFeature R = MakeAtSample(EFeature::TunnelRoof, M[(i + j) / 2], i, INDEX_NONE);
					R.Aux = c;
					R.Aux2 = j;
					R.Height = C.TopZ;
					R.Length = M[j].S - M[i].S;
					L.Features.Add(R);
					i = j;
				}
			}
		}
	}

	/** true si la zanja de un hueco (rectángulo con sus orillas) alcanza muestras de otra parte de algún camino. */
	inline bool TrenchHitsOtherPath(const FLayout& L, const TArray<FPathSample>& Own, const FPathSample& Sm, const FFeature& F)
	{
		const FVector2D C(F.Location.X, F.Location.Y);
		const FVector2D N = LeftNormal(F.Dir);
		auto Hits = [&](const TArray<FPathSample>& Arr)
		{
			for (const FPathSample& Q : Arr)
			{
				if (&Arr == &Own && FMath::Abs(Q.S - Sm.S) <= F.Height * 0.5 + 2000.0) { continue; }
				const FVector2D Rel = Q.P - C;
				const double Hq = Q.Width * 0.5 + 300.0;
				if (FMath::Abs(FVector2D::DotProduct(Rel, F.Dir)) <= F.Height * 0.5 + Hq
					&& FMath::Abs(FVector2D::DotProduct(Rel, N)) <= F.Width * 0.5 + GapTrenchSide + Hq)
				{
					return true;
				}
			}
			return false;
		};
		if (Hits(L.Main)) { return true; }
		for (const FBranch& B : L.Branches) { if (Hits(B.Samples)) { return true; } }
		return false;
	}

	/** Huecos de salto sobre un array de muestras (principal o rama). */
	inline void PlaceGapsOn(FLayout& L, TArray<FPathSample>& Samples, int32 BranchIndex, FRng& Rng)
	{
		using namespace FeatureDetail;
		const FGenParams& P = L.Params;
		const double GapMaxD = LerpD(FMath::Min(P.GapMax, 200.0), P.GapMax, Saturate(P.Difficulty01));
		const double Chance = P.GapsPerKm * P.SampleSpacing / 100000.0;
		double LastS = -1e9;

		TArray<int32> Forks;
		if (BranchIndex == INDEX_NONE)
		{
			for (const FBranch& B : L.Branches) { Forks.Add(B.ForkSample); Forks.Add(B.RejoinSample); }
		}

		for (int32 i = 8; i < Samples.Num() - 8; ++i)
		{
			const FPathSample& Sm = Samples[i];
			if (Sm.S - LastS < 7000.0 || IsWetBiome(Sm.Biome)) { continue; }
			// En explanadas no: la zanja cruzaría toda la plaza. Ni a ras de agua: la zanja
			// (con el fondo sobre el agua) tiene que tener al menos 3 m de hondo.
			if (Sm.Width > 2600.0 || Sm.Z < 450.0) { continue; }
			if (AnyFlag(Samples, i - 6, i + 6, PathFlags::Special | PathFlags::Lane)) { continue; }
			bool bFlat = true;
			for (int32 j = i - 5; j <= i + 5; ++j) { if (FMath::Abs(Samples[j].Z - Sm.Z) > 110.0) { bFlat = false; break; } }
			if (!bFlat) { continue; }
			bool bNearFork = false;
			for (const int32 Fk : Forks) { if (FMath::Abs(Fk - i) < 14) { bNearFork = true; break; } }
			if (bNearFork || !Rng.Chance(Chance)) { continue; }

			FFeature F = MakeAtSample(EFeature::Gap, Sm, i, BranchIndex);
			F.Length = Rng.Range(P.GapMin, GapMaxD);
			F.Width = Sm.Width + 500.0;
			// Zanja del terreno más larga que el hueco: los labios (mallas) la estrechan al valor exacto.
			F.Height = FMath::Max(F.Length + 500.0, 800.0);
			// La zanja no puede pisar otra parte de ningún camino (curvas que vuelven, ramas, horquillas).
			if (TrenchHitsOtherPath(L, Samples, Sm, F)) { continue; }
			L.Features.Add(F);
			for (int32 j = 0; j < Samples.Num(); ++j)
			{
				if (FMath::Abs(Samples[j].S - Sm.S) <= F.Height * 0.5 + 100.0) { Samples[j].Flags |= PathFlags::Gap; }
			}
			LastS = Sm.S;
		}
	}

	inline void BuildGaps(FLayout& L, FRng Rng)
	{
		PlaceGapsOn(L, L.Main, INDEX_NONE, Rng);
		for (int32 b = 0; b < L.Branches.Num(); ++b)
		{
			// Las ramas "arriesgadas" llevan el doble de huecos.
			FGenParams Saved = L.Params;
			if (L.Branches[b].Kind == EBranchKind::Risky) { L.Params.GapsPerKm *= 2.0; }
			if (L.Branches[b].Kind != EBranchKind::Lane) { PlaceGapsOn(L, L.Branches[b].Samples, b, Rng); }
			L.Params = Saved;
		}
	}

	/** Isletas (bioma agua) y pasarelas con tablones rotos (manglar). */
	inline void BuildWetFeatures(FLayout& L, FRng Rng)
	{
		using namespace PathDetail;
		const FGenParams& P = L.Params;
		const double Diff = Saturate(P.Difficulty01);
		const double GapMaxW = LerpD(FMath::Min(P.IsletGapMax, 220.0), P.IsletGapMax, Diff);
		const TArray<FPathSample>& M = L.Main;

		for (int32 i = 0; i < M.Num(); ++i)
		{
			const bool bIslet = (M[i].Flags & PathFlags::Islet) != 0;
			const bool bBoard = (M[i].Flags & PathFlags::Boardwalk) != 0;
			if (!bIslet && !bBoard) { continue; }
			const uint32 Flag = bIslet ? PathFlags::Islet : PathFlags::Boardwalk;
			int32 j = i;
			while (j + 1 < M.Num() && (M[j + 1].Flags & Flag) != 0) { ++j; }
			const double S0 = M[FMath::Max(0, i - 1)].S;
			const double S1 = M[FMath::Min(M.Num() - 1, j + 1)].S;

			if (bIslet)
			{
				const double LiMin = LerpD(900.0, 500.0, Diff);
				const double LiMax = LerpD(1600.0, 1000.0, Diff);
				double Cursor = S0;
				double PrevTop = M[FMath::Max(0, i - 1)].Z;
				int32 Guard = 0;
				while (Cursor < S1 && Guard++ < 400)
				{
					double Gap = Rng.Range(P.IsletGapMin, GapMaxW);
					double Len = Rng.Range(LiMin, LiMax);
					const double Remaining = S1 - Cursor;
					if (Remaining <= GapMaxW) { break; }
					if (Remaining < Gap + Len + P.IsletGapMin)
					{
						// Última isleta: reparte el resto para que el salto final a tierra sea válido.
						Gap = FMath::Min(GapMaxW, FMath::Max(P.IsletGapMin, (Remaining - LiMin) * 0.5));
						Len = FMath::Max(LiMin * 0.7, Remaining - 2.0 * Gap);
					}
					const double Center = Cursor + Gap + Len * 0.5;
					FVector2D Dir;
					const FVector2D C = MainPointAt(M, Center, &Dir);
					int32 Near = INDEX_NONE;
					MainPointAt(M, Center, nullptr, &Near);
					const double HalfAcross = FMath::Clamp(M[Near].Width * 0.5, 280.0, 750.0);
					double Top = M[Near].Z + Rng.Range(-35.0, 35.0);
					Top = FMath::Clamp(Top, PrevTop - 60.0, PrevTop + 60.0);
					Top = FMath::Max(Top, 45.0);

					FFeature F;
					F.Type = EFeature::Islet;
					F.Location = FVector(C, Top);
					F.Dir = Dir;
					F.Length = Len;
					F.Width = HalfAcross * 2.0;
					F.Height = Top + 450.0;
					F.PathIndex = Near;
					F.Biome = M[Near].Biome;
					const FVector2D N = LeftNormal(Dir);
					const int32 Verts = 12;
					for (int32 v = 0; v < Verts; ++v)
					{
						const double A = TwoPi * v / Verts;
						const double Rn = 1.0 + 0.16 * FMath::Abs(FMath::Sin(A)) * Noise1(P.Seed ^ 0x15137u, Center / 1000.0 + v);
						F.Polygon.Add(C + Dir * (FMath::Cos(A) * Len * 0.5) + N * (FMath::Sin(A) * HalfAcross * Rn));
					}
					L.Features.Add(F);
					PrevTop = Top;
					Cursor = Center + Len * 0.5;
				}
			}
			else
			{
				// Pasarela: tablones a trozos; los huecos crecen con la dificultad.
				double Cursor = S0;
				int32 Guard = 0;
				while (Cursor < S1 - 200.0 && Guard++ < 200)
				{
					const double PieceLen = FMath::Min(S1 - Cursor, Rng.Range(1500.0, 4000.0));
					int32 A = INDEX_NONE, B = INDEX_NONE;
					MainPointAt(M, Cursor, nullptr, &A);
					MainPointAt(M, Cursor + PieceLen, nullptr, &B);
					FFeature F;
					F.Type = EFeature::Boardwalk;
					F.PathIndex = A;
					F.Aux2 = FMath::Max(A, B);
					F.Location = FVector(M[A].P, M[A].Z);
					F.Height = M[A].Z;
					F.Width = M[A].Width;
					F.Length = PieceLen;
					F.Biome = M[A].Biome;
					F.Target = FVector(M[F.Aux2].P, M[F.Aux2].Z);
					L.Features.Add(F);
					Cursor += PieceLen;
					if (Rng.Chance(0.35 + 0.4 * Diff)) { Cursor += Rng.Range(P.GapMin, LerpD(220.0, P.GapMax, Diff)); }
				}
			}
			i = j;
		}
	}

	/** Pilas de huevos de respawn al cruzar a otro módulo (cada N portales). */
	inline void BuildEggNests(FLayout& L, FRng Rng)
	{
		using namespace FeatureDetail;
		const FGenParams& P = L.Params;
		const TArray<FPathSample>& M = L.Main;
		const uint32 Bad = PathFlags::Elevated | PathFlags::Colossal | PathFlags::UnderTower | PathFlags::TowerTop | PathFlags::Slide
			| PathFlags::GeyserBase | PathFlags::Tunnel | PathFlags::Islet | PathFlags::Boardwalk | PathFlags::Gap | PathFlags::Shore;

		int32 Order = 0;
		{
			FFeature F = MakeAtSample(EFeature::EggNest, M[0], 0, INDEX_NONE);
			F.Location = FVector(L.StartPoint + LeftNormal(M[0].Dir) * (P.StartClearingRadius * 0.55), M[0].Z);
			F.Aux = Order++;
			L.Features.Add(F);
		}

		int32 PortalCount = 0;
		const int32 Every = FMath::Max(1, P.EggNestEveryNPortals);
		for (int32 k = 1; k < L.Route.Num(); ++k)
		{
			const int32 Pi0 = L.Route[k].FirstSample;
			if (AnyFlag(M, Pi0 - 6, Pi0 + 6, PathFlags::Elevated | PathFlags::Colossal | PathFlags::UnderTower | PathFlags::TowerTop)) { continue; }
			++PortalCount;
			if (PortalCount % Every != 0) { continue; }
			for (int32 i = Pi0 + 1; i < FMath::Min(M.Num(), Pi0 + 40); ++i)
			{
				if (M[i].S - M[Pi0].S < 1500.0) { continue; }
				if ((M[i].Flags & Bad) != 0 || AnyFlag(M, i - 3, i + 3, Bad)) { continue; }
				const double Side = Rng.Chance(0.5) ? 1.0 : -1.0;
				FFeature F = MakeAtSample(EFeature::EggNest, M[i], i, INDEX_NONE);
				F.Location = FVector(M[i].P + LeftNormal(M[i].Dir) * Side * FMath::Max(0.0, M[i].Width * 0.5 - 300.0), M[i].Z);
				F.Aux = Order++;
				L.Features.Add(F);
				break;
			}
		}
	}

	/** Muro de lanzamiento, compuerta e interruptor de sabotaje en cada carril 2vs2. */
	inline void BuildLanePuzzles(FLayout& L, FRng Rng)
	{
		using namespace FeatureDetail;
		for (int32 b = 0; b < L.Branches.Num(); ++b)
		{
			const FBranch& Br = L.Branches[b];
			if (Br.Kind != EBranchKind::Lane) { continue; }

			// Carril A = tramo del principal; carril B = la rama.
			TArray<FPathSample> LaneA;
			for (int32 i = Br.ForkSample; i <= Br.RejoinSample; ++i) { LaneA.Add(L.Main[i]); }
			const TArray<FPathSample>* Lanes[2] = { &LaneA, &Br.Samples };
			const int32 BranchIdx[2] = { INDEX_NONE, b };
			const int32 IndexOffset[2] = { Br.ForkSample, 0 };
			int32 GateIdx[2] = { INDEX_NONE, INDEX_NONE };
			int32 SwitchIdx[2] = { INDEX_NONE, INDEX_NONE };

			for (int32 Lane = 0; Lane < 2; ++Lane)
			{
				const TArray<FPathSample>& S = *Lanes[Lane];
				auto At = [&](double Frac) { return FMath::Clamp(FMath::RoundToInt(Frac * (S.Num() - 1)), 1, S.Num() - 2); };

				const int32 WallI = At(Rng.Range(0.36, 0.44));
				FFeature Wall = MakeAtSample(EFeature::ThrowWall, S[WallI], IndexOffset[Lane] + WallI, BranchIdx[Lane]);
				Wall.Width = S[WallI].Width + 1800.0;
				Wall.Height = 480.0;
				Wall.Length = 300.0;
				Wall.Aux = b;
				L.Features.Add(Wall);

				const int32 SwitchI = At(Rng.Range(0.54, 0.6));
				FFeature Sw = MakeAtSample(EFeature::SabotageSwitch, S[SwitchI], IndexOffset[Lane] + SwitchI, BranchIdx[Lane]);
				const double Side = Rng.Chance(0.5) ? 1.0 : -1.0;
				Sw.Location = FVector(S[SwitchI].P + LeftNormal(S[SwitchI].Dir) * Side * (S[SwitchI].Width * 0.5 - 150.0), S[SwitchI].Z);
				SwitchIdx[Lane] = L.Features.Add(Sw);

				const int32 GateI = At(Rng.Range(0.74, 0.82));
				FFeature Gate = MakeAtSample(EFeature::SabotageGate, S[GateI], IndexOffset[Lane] + GateI, BranchIdx[Lane]);
				Gate.Width = S[GateI].Width + 1800.0;
				Gate.Height = 350.0;
				Gate.Length = 200.0;
				Gate.Aux = b;
				GateIdx[Lane] = L.Features.Add(Gate);
			}
			// Cada interruptor levanta la compuerta del OTRO carril.
			L.Features[SwitchIdx[0]].Aux = GateIdx[1];
			L.Features[SwitchIdx[1]].Aux = GateIdx[0];
		}
	}

	/** Río opcional: de la costa hacia el interior, con puentes donde cruza caminos. */
	inline void BuildRiver(FLayout& L, FRng Rng)
	{
		using namespace PathDetail;
		const FGenParams& P = L.Params;
		L.River.Reset();
		L.RiverWidth.Reset();

		TArray<FVector2D> Avoid;
		TArray<double> AvoidR;
		for (const FFeature& F : L.Features)
		{
			if (F.Type == EFeature::Tower || F.Type == EFeature::Geyser || F.Type == EFeature::EggNest || F.Type == EFeature::SlideZone || F.Type == EFeature::StartArea)
			{
				Avoid.Add(FVector2D(F.Location.X, F.Location.Y));
				AvoidR.Add(FMath::Max(F.Radius, 2500.0) + 3500.0);
			}
		}

		for (int32 Attempt = 0; Attempt < 30; ++Attempt)
		{
			const double X0 = Rng.Range(0.2, 0.8) * L.WorldSize;
			if (FMath::Abs(X0 - L.EndPoint.X) < 12000.0) { continue; }
			TArray<FVector2D> Pts;
			TArray<double> Widths;
			FVector2D Pos(X0, L.CoastY(X0) + 2500.0);
			double Heading = -Pi * 0.5;
			const double Target = Rng.Range(0.35, 0.6) * L.WorldSize;
			double Len = 0.0;
			bool bOk = true;
			const uint32 RSeed = P.Seed ^ (0x51BE5u + static_cast<uint32>(Attempt));
			while (Len < Target)
			{
				Pts.Add(Pos);
				Widths.Add(LerpD(2400.0, 1200.0, Len / Target));
				Heading = -Pi * 0.5 + 0.8 * Fbm1(RSeed, Len / 20000.0, 2);
				Pos = Pos + DirFromAngle(Heading) * 1000.0;
				Len += 1000.0;
				if (Pos.X < P.MapEdgeClearance || Pos.X > L.WorldSize - P.MapEdgeClearance || Pos.Y < P.MapEdgeClearance * 1.5) { break; }
				const int32 Mod = L.ModuleAt(Pos);
				if (Mod != INDEX_NONE && IsCrossingModule(L, Mod)) { bOk = false; break; }
				for (int32 a = 0; a < Avoid.Num(); ++a) { if (FVector2D::Distance(Avoid[a], Pos) < AvoidR[a]) { bOk = false; break; } }
				if (!bOk) { break; }
			}
			if (!bOk || Pts.Num() < 12) { continue; }

			// Cruces con caminos: solo en tramos normales, con cota suficiente para un puente.
			struct FHit { int32 Branch; int32 Index; FVector2D Point; double Width; };
			TArray<FHit> Hits;
			auto Check = [&](const TArray<FPathSample>& S, int32 BranchIndex)
			{
				for (int32 i = 0; i + 1 < S.Num() && bOk; ++i)
				{
					for (int32 r = 0; r + 1 < Pts.Num(); ++r)
					{
						FVector2D X;
						if (!SegmentsIntersect(S[i].P, S[i + 1].P, Pts[r], Pts[r + 1], &X)) { continue; }
						if (FeatureDetail::AnyFlag(S, i - 8, i + 8, PathFlags::Special | PathFlags::Lane) || S[i].Z < 250.0 || IsWetBiome(S[i].Biome))
						{
							bOk = false;
							break;
						}
						FHit H;
						H.Branch = BranchIndex;
						H.Index = i;
						H.Point = X;
						H.Width = Widths[r];
						Hits.Add(H);
					}
				}
			};
			Check(L.Main, INDEX_NONE);
			for (int32 b = 0; b < L.Branches.Num() && bOk; ++b) { Check(L.Branches[b].Samples, b); }
			if (!bOk) { continue; }

			L.River = Pts;
			L.RiverWidth = Widths;
			for (const FHit& H : Hits)
			{
				TArray<FPathSample>& S = H.Branch == INDEX_NONE ? L.Main : L.Branches[H.Branch].Samples;
				FFeature F = FeatureDetail::MakeAtSample(EFeature::RiverBridge, S[H.Index], H.Index, H.Branch);
				F.Location = FVector(H.Point, S[H.Index].Z);
				F.Length = H.Width + 2.0 * (S[H.Index].Z + 300.0) / 1.2 + 800.0;
				F.Width = S[H.Index].Width + 200.0;
				L.Features.Add(F);
				for (FPathSample& Sm : S)
				{
					if (FVector2D::Distance(Sm.P, H.Point) < F.Length * 0.5 + 200.0) { Sm.Flags |= PathFlags::RiverCross; }
				}
			}
			return;
		}
	}

	/**
	 * Un volcán enorme por región volcánica, en el punto más alejado de los caminos:
	 * cono con cráter y lago de lava. Sus laderas pueden cubrir caminos cercanos, que
	 * lo atraviesan encajonados en su cauce.
	 */
	inline void BuildLandmarks(FLayout& L, FRng Rng)
	{
		using namespace PathDetail;
		TArray<FPathSample> All = L.Main;
		for (const FBranch& B : L.Branches) { All.Append(B.Samples); }
		FSampleGrid Grid;
		Grid.Build(All, L.WorldSize);

		TArray<int32> Regions;
		for (const FModule& M : L.Modules)
		{
			if (M.Biome == ETNProcBiome::Volcanic && !Regions.Contains(M.Region)) { Regions.Add(M.Region); }
		}
		for (const int32 Region : Regions)
		{
			double BestScore = -1e300;
			FVector2D Best = FVector2D::ZeroVector;
			double BestClear = 0.0;
			for (int32 y = 0; y < L.RasterH; y += 3)
			{
				for (int32 x = 0; x < L.RasterW; x += 3)
				{
					const int32 Mod = L.ModuleOfCell[L.CellIndex(x, y)];
					if (Mod < 0 || L.Modules[Mod].Region != Region) { continue; }
					const FVector2D C = L.CellCenter(x, y);
					const double Edge = FMath::Min(FMath::Min(C.X, L.WorldSize - C.X), FMath::Min(C.Y, L.CoastY(C.X) - C.Y));
					if (Edge < 12000.0) { continue; }
					double D = 0.0;
					const int32 Near = Grid.Nearest(C, 40000.0, D);
					const double Clear = Near == INDEX_NONE ? 40000.0 : D - All[Near].Width * 0.5;
					const double Score = FMath::Min(Clear, 30000.0) + 0.15 * FMath::Min(Edge, 30000.0) + Rng.Range(0.0, 800.0);
					if (Score > BestScore) { BestScore = Score; Best = C; BestClear = Clear; }
				}
			}
			if (BestClear < 6000.0) { continue; }

			FFeature V;
			V.Type = EFeature::Volcano;
			V.Biome = ETNProcBiome::Volcanic;
			V.Radius = FMath::Clamp(BestClear * 2.0 + 6000.0, 24000.0, 42000.0);
			V.Height = V.Radius * Rng.Range(0.38, 0.48);
			const double CraterR = V.Radius * Rng.Range(0.11, 0.14);
			V.Width = CraterR * 2.0;
			V.Length = Rng.Range(1800.0, 2800.0);
			const double Base = L.SampleCoarse(L.LevelField, Best);
			V.Location = FVector(Best, Base);
			L.Features.Add(V);

			// Lago de lava a media altura del cráter (mismo perfil que TN_ProcMapTerrain).
			const double Rim = V.Height * FMath::Pow(1.0 - CraterR / V.Radius, 1.35);
			FFeature Lava;
			Lava.Type = EFeature::LavaPool;
			Lava.Biome = ETNProcBiome::Volcanic;
			Lava.Radius = CraterR * 0.72;
			Lava.Location = FVector(Best, Base + Rim - V.Length * 0.45);
			L.Features.Add(Lava);
		}
	}

	/**
	 * Volcanes pequeños (volcánico: 60-140 m de base, 18-77 m de alto, con lago de lava en el
	 * cráter) e islas decorativas (agua), lejos de los caminos.
	 */
	inline void BuildDecor(FLayout& L, FRng Rng)
	{
		using namespace PathDetail;
		TArray<FPathSample> All = L.Main;
		for (const FBranch& B : L.Branches) { All.Append(B.Samples); }
		FSampleGrid Grid;
		Grid.Build(All, L.WorldSize);

		for (const FModule& M : L.Modules)
		{
			const bool bVolcanic = M.Biome == ETNProcBiome::Volcanic;
			const bool bWater = M.Biome == ETNProcBiome::Water;
			if (!bVolcanic && !bWater) { continue; }
			const int32 Count = bVolcanic ? Rng.RangeInt(1, 3) : Rng.RangeInt(2, 5);
			for (int32 n = 0; n < Count; ++n)
			{
				for (int32 Try = 0; Try < 25; ++Try)
				{
					const int32 X = Rng.RangeInt(0, L.RasterW - 1);
					const int32 Y = Rng.RangeInt(0, L.RasterH - 1);
					if (L.ModuleOfCell[L.CellIndex(X, Y)] != M.Id || L.BorderDist[L.CellIndex(X, Y)] < 3000.0f) { continue; }
					const FVector2D C = L.CellCenter(X, Y);
					const double Radius = bVolcanic ? Rng.Range(6000.0, 14000.0) : Rng.Range(700.0, 2200.0);
					double D = 0.0;
					const int32 Near = Grid.Nearest(C, 20000.0, D);
					// Un cono solo necesita libre su parte alta: los cauces cortan sus faldas.
					const double Clear = (bVolcanic ? Radius * 0.45 : Radius) + (Near != INDEX_NONE ? All[Near].Width * 0.5 : 0.0) + 3500.0;
					if (Near != INDEX_NONE && D < Clear) { continue; }
					bool bOverlap = false;
					for (const FFeature& F : L.Features)
					{
						if ((F.Type == EFeature::LavaPool || F.Type == EFeature::Island || F.Type == EFeature::Volcano)
							&& FVector2D::Distance(FVector2D(F.Location.X, F.Location.Y), C) < (F.Radius + Radius) * (bVolcanic ? 0.7 : 1.0) + 1500.0)
						{
							bOverlap = true;
							break;
						}
					}
					if (bOverlap) { continue; }
					FFeature F;
					if (bVolcanic)
					{
						// Cono con cráter y lago de lava a media altura del cráter (mismo perfil que el terreno).
						FFeature V;
						V.Type = EFeature::Volcano;
						V.Biome = ETNProcBiome::Volcanic;
						V.Radius = Radius;
						V.Height = Radius * Rng.Range(0.3, 0.55);
						const double CraterR = Radius * Rng.Range(0.12, 0.18);
						V.Width = CraterR * 2.0;
						V.Length = Rng.Range(700.0, 1400.0);
						const double Base = L.SampleCoarse(L.LevelField, C);
						V.Location = FVector(C, Base);
						L.Features.Add(V);
						const double Rim = V.Height * FMath::Pow(1.0 - CraterR / V.Radius, 1.35);
						FFeature Lava;
						Lava.Type = EFeature::LavaPool;
						Lava.Biome = ETNProcBiome::Volcanic;
						Lava.Radius = CraterR * 0.72;
						Lava.Location = FVector(C, Base + Rim - V.Length * 0.45);
						L.Features.Add(Lava);
						break;
					}
					F.Type = EFeature::Island;
					F.Radius = Radius;
					F.Biome = M.Biome;
					F.Location = FVector(C, Rng.Range(150.0, 520.0));
					L.Features.Add(F);
					break;
				}
			}
		}
	}

	/**
	 * Vida y obstáculos del camino (principal y ramas, salvo carriles): peñascos en grupos de
	 * 1-3 que siempre dejan un carril libre de al menos 3,5 m, agujas y mogotes de roca en las
	 * explanadas y troncos caídos que se saltan (<= 1,1 m) en selva, manglar y volcán. Nunca
	 * junto a huecos, géiseres, toboganes, torres, portales, horquillas ni sobre el agua.
	 * Además, secuoyas gigantes con raíces zancudas en los módulos de manglar.
	 */
	inline void BuildObstacles(FLayout& L, FRng Rng)
	{
		using namespace FeatureDetail;
		const uint32 Avoid = PathFlags::Special | PathFlags::Lane;
		auto OnPolyline = [&](const TArray<FPathSample>& S, int32 BranchIndex)
		{
			if (S.Num() < 20) { return; }
			double NextS = Rng.Range(2000.0, 5000.0);
			for (int32 i = 8; i < S.Num() - 8; ++i)
			{
				const FPathSample& Sm = S[i];
				if (Sm.S < NextS) { continue; }
				NextS = Sm.S + Rng.Range(2200.0, 5500.0);
				if (IsWetBiome(Sm.Biome) || AnyFlag(S, i - 6, i + 6, Avoid)) { continue; }
				// Cerca de una horquilla o unión de rama tampoco (dos cauces se cruzan ahí).
				bool bFork = false;
				for (const FBranch& B : L.Branches)
				{
					if (BranchIndex == INDEX_NONE && (FMath::Abs(B.ForkSample - i) < 10 || FMath::Abs(B.RejoinSample - i) < 10)) { bFork = true; break; }
				}
				if (bFork || (BranchIndex != INDEX_NONE && (i < 14 || i > S.Num() - 14))) { continue; }

				const double W = Sm.Width;
				const FVector2D N = LeftNormal(Sm.Dir);
				const bool bForest = Sm.Biome == ETNProcBiome::Jungle || Sm.Biome == ETNProcBiome::Mangrove || Sm.Biome == ETNProcBiome::Volcanic;
				const double U = Rng.Unit();
				if (W >= 2000.0 && U < 0.35)
				{
					// Aguja (alta y fina) o mogote (bajo y ancho) en mitad de la explanada, con carriles a ambos lados.
					const bool bSpire = Rng.Chance(0.6);
					const double R = bSpire ? Rng.Range(150.0, 320.0) : Rng.Range(380.0, 700.0);
					const double Room = W * 0.5 - R - 500.0;
					if (Room < 0.0) { continue; }
					FFeature F = MakeAtSample(EFeature::RockSpire, Sm, i, BranchIndex);
					F.Location = FVector(Sm.P + N * Rng.Range(-Room, Room) * 0.6, Sm.Z);
					F.Radius = R;
					F.Height = bSpire ? Rng.Range(500.0, 1400.0) : Rng.Range(220.0, 600.0);
					F.Aux = static_cast<int32>(Rng.RangeInt(0, 1 << 20));
					L.Features.Add(F);
					continue;
				}
				if (bForest && W >= 500.0 && W <= 2600.0 && U < 0.6)
				{
					// Tronco caído atravesado: se salta (radio 35-55 cm); deja hueco en un extremo o no.
					FFeature F = MakeAtSample(EFeature::Log, Sm, i, BranchIndex);
					const double Ang = FMath::DegreesToRadians(Rng.Range(60.0, 90.0)) * (Rng.Chance(0.5) ? 1.0 : -1.0);
					F.Dir = (Sm.Dir * FMath::Cos(Ang) + N * FMath::Sin(Ang)).GetSafeNormal();
					F.Length = W * Rng.Range(0.55, 0.9) / FMath::Max(0.3, FMath::Abs(FMath::Sin(Ang)));
					F.Radius = Rng.Range(35.0, 55.0);
					F.Location = FVector(Sm.P + N * Rng.Range(-0.15, 0.15) * W, Sm.Z);
					F.Aux = static_cast<int32>(Rng.RangeInt(0, 1 << 20));
					L.Features.Add(F);
					continue;
				}
				// Grupo de 1-3 peñascos a un lado, dejando libre el otro (>= 3,5 m y >= 40 % del ancho).
				const double Lane = FMath::Max(350.0, 0.4 * W);
				const double Side = Rng.Chance(0.5) ? 1.0 : -1.0;
				const int32 Count = Rng.RangeInt(1, 3);
				for (int32 k = 0; k < Count; ++k)
				{
					const double R = Rng.Range(70.0, 220.0);
					const double MinOff = R + Lane - W * 0.5;
					const double MaxOff = W * 0.5 - R * 0.4;
					if (MaxOff <= FMath::Max(0.0, MinOff)) { break; }
					FFeature F = MakeAtSample(EFeature::Boulder, Sm, i, BranchIndex);
					const double Along = Rng.Range(-250.0, 250.0) * k;
					F.Location = FVector(Sm.P + Sm.Dir * Along + N * (Side * Rng.Range(FMath::Max(0.0, MinOff), MaxOff)), Sm.Z);
					F.Radius = R;
					F.Height = R * Rng.Range(0.8, 1.7);
					F.Aux = static_cast<int32>(Rng.RangeInt(0, 1 << 20));
					L.Features.Add(F);
				}
			}
		};
		OnPolyline(L.Main, INDEX_NONE);
		for (int32 b = 0; b < L.Branches.Num(); ++b)
		{
			if (L.Branches[b].Kind != EBranchKind::Lane) { OnPolyline(L.Branches[b].Samples, b); }
		}

		// Secuoyas del manglar: fuera de los cauces (en tierra o en las pozas), con raíces zancudas.
		TArray<FPathSample> All = L.Main;
		for (const FBranch& B : L.Branches) { All.Append(B.Samples); }
		PathDetail::FSampleGrid Grid;
		Grid.Build(All, L.WorldSize);
		for (const FModule& M : L.Modules)
		{
			if (M.Biome != ETNProcBiome::Mangrove) { continue; }
			const int32 Count = Rng.RangeInt(8, 14);
			for (int32 n = 0; n < Count; ++n)
			{
				for (int32 Try = 0; Try < 20; ++Try)
				{
					const int32 X = Rng.RangeInt(0, L.RasterW - 1);
					const int32 Y = Rng.RangeInt(0, L.RasterH - 1);
					if (L.ModuleOfCell[L.CellIndex(X, Y)] != M.Id) { continue; }
					const FVector2D C = L.CellCenter(X, Y) + FVector2D(Rng.Range(-150.0, 150.0), Rng.Range(-150.0, 150.0));
					const double R = Rng.Range(150.0, 260.0);
					double D = 0.0;
					const int32 Near = Grid.Nearest(C, 20000.0, D);
					if (Near != INDEX_NONE && D < All[Near].Width * 0.5 + R * 3.0 + 900.0) { continue; }
					bool bClose = false;
					for (const FFeature& F : L.Features)
					{
						if (F.Type == EFeature::GiantTree && FVector2D::Distance(FVector2D(F.Location.X, F.Location.Y), C) < 2500.0) { bClose = true; break; }
					}
					if (bClose) { continue; }
					FFeature T;
					T.Type = EFeature::GiantTree;
					T.Biome = ETNProcBiome::Mangrove;
					T.Location = FVector(C, 0.0);
					T.Radius = R;
					T.Height = Rng.Range(3000.0, 5500.0);
					T.Aux = static_cast<int32>(Rng.RangeInt(0, 1 << 20));
					L.Features.Add(T);
					break;
				}
			}
		}
	}

	// ─────────────────────────────────────────────────────────────────────────
	// Planificación de peligros (enemigos, spawners) — determinista y pura
	// ─────────────────────────────────────────────────────────────────────────

	enum class EHazardPlacement : uint8
	{
		OnPath,
		PathEdge,
		NearPath,
		InWater,
		AbovePath,
		OffPathFar
	};

	struct FHazardRule
	{
		int32 Id = INDEX_NONE;
		double PerKm = 1.0;
		EHazardPlacement Placement = EHazardPlacement::OnPath;
		/** Bit b = bioma b permitido. */
		uint32 BiomeMask = 0xFFFFFFFFu;
		double MinDifficulty01 = 0.0;
		/** Distancia mínima entre dos del mismo tipo. */
		double Clearance = 3000.0;
		bool bMainOnly = false;
	};

	struct FHazardSpawn
	{
		int32 RuleId = INDEX_NONE;
		FVector2D P = FVector2D::ZeroVector;
		/** Cota de referencia (suelo del camino); el actor hace su propio trazado. */
		double RefZ = 0.0;
		double Yaw = 0.0;
		int32 PathIndex = INDEX_NONE;
		int32 BranchIndex = INDEX_NONE;
	};

	inline TArray<FHazardSpawn> PlanHazards(const FLayout& L, const TArray<FHazardRule>& Rules, double DensityMul, uint64 Salt)
	{
		TArray<FHazardSpawn> Out;
		FRng Rng = FRng(static_cast<uint64>(L.Params.Seed) * 0x9E37ull + Salt);

		// Zonas reservadas (x, y, radio): estructuras del recorrido y obstáculos del camino.
		TArray<FVector> Keep;
		for (const FFeature& F : L.Features)
		{
			if (F.Type == EFeature::Geyser || F.Type == EFeature::EggNest || F.Type == EFeature::Gap || F.Type == EFeature::ThrowWall
				|| F.Type == EFeature::SabotageGate || F.Type == EFeature::SabotageSwitch || F.Type == EFeature::StartArea || F.Type == EFeature::Finish)
			{
				Keep.Add(FVector(F.Location.X, F.Location.Y, 1600.0));
			}
			else if (F.Type == EFeature::Boulder || F.Type == EFeature::RockSpire)
			{
				Keep.Add(FVector(F.Location.X, F.Location.Y, F.Radius + 250.0));
			}
			else if (F.Type == EFeature::Log)
			{
				Keep.Add(FVector(F.Location.X, F.Location.Y, F.Length * 0.5 + 200.0));
			}
		}

		auto Visit = [&](const TArray<FPathSample>& Samples, int32 BranchIndex)
		{
			for (const FHazardRule& R : Rules)
			{
				if (R.bMainOnly && BranchIndex != INDEX_NONE) { continue; }
				if (L.Params.Difficulty01 + 1e-6 < R.MinDifficulty01) { continue; }
				const double Chance = R.PerKm * DensityMul * L.Params.SampleSpacing / 100000.0;
				TArray<FVector2D> Placed;
				for (int32 i = 4; i < Samples.Num() - 4; ++i)
				{
					const FPathSample& Sm = Samples[i];
					if ((R.BiomeMask & (1u << BiomeIndex(Sm.Biome))) == 0) { continue; }
					const bool bWet = (Sm.Flags & (PathFlags::Islet | PathFlags::Boardwalk)) != 0;
					if (R.Placement == EHazardPlacement::InWater)
					{
						if (!bWet) { continue; }
					}
					else if ((Sm.Flags & (PathFlags::Special & ~PathFlags::Portal)) != 0 && !(bWet && R.Placement != EHazardPlacement::OnPath))
					{
						continue;
					}
					if (!Rng.Chance(Chance)) { continue; }

					const FVector2D N = LeftNormal(Sm.Dir);
					const double Side = Rng.Chance(0.5) ? 1.0 : -1.0;
					FVector2D Pos = Sm.P;
					switch (R.Placement)
					{
						case EHazardPlacement::OnPath:     Pos += N * (Rng.Range(-0.35, 0.35) * Sm.Width); break;
						case EHazardPlacement::PathEdge:   Pos += N * Side * FMath::Max(0.0, Sm.Width * 0.5 - 150.0); break;
						case EHazardPlacement::NearPath:   Pos += N * Side * (Sm.Width * 0.5 + Rng.Range(400.0, 1800.0)); break;
						case EHazardPlacement::InWater:    Pos += N * Side * (Sm.Width * 0.5 + Rng.Range(700.0, 2600.0)); break;
						case EHazardPlacement::AbovePath:  break;
						case EHazardPlacement::OffPathFar: Pos += N * Side * (Sm.Width * 0.5 + Rng.Range(2500.0, 6000.0)); break;
					}

					bool bOk = true;
					for (const FVector2D& Pl : Placed) { if (FVector2D::DistSquared(Pl, Pos) < R.Clearance * R.Clearance) { bOk = false; break; } }
					for (const FVector2D& Kp : Keep) { if (bOk && FVector2D::DistSquared(Kp, Pos) < 1600.0 * 1600.0) { bOk = false; } }
					if (!bOk) { continue; }

					FHazardSpawn H;
					H.RuleId = R.Id;
					H.P = Pos;
					H.RefZ = Sm.Z;
					H.Yaw = FMath::RadiansToDegrees(AngleOf(Sm.Dir));
					H.PathIndex = i;
					H.BranchIndex = BranchIndex;
					Out.Add(H);
					Placed.Add(Pos);
				}
			}
		};

		Visit(L.Main, INDEX_NONE);
		for (int32 b = 0; b < L.Branches.Num(); ++b) { Visit(L.Branches[b].Samples, b); }
		return Out;
	}
}
