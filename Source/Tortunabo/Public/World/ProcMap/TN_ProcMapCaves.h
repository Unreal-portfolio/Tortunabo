#pragma once

#include "CoreMinimal.h"
#include "World/ProcMap/TN_ProcMapLayout.h"
#include "World/ProcMap/TN_ProcMapFeatures.h"

/**
 * Cuevas (lógica pura): tramos del camino principal que atraviesan una loma por un túnel de roca.
 * El tramo se marca como túnel (suelo llano, paredes a plomo, sin huecos ni obstáculos normales),
 * se estrecha en pasos de 4-7 m y se abre en una cámara de 16-26 m en medio; en las del volcán,
 * un río de lava cruza la cámara (hueco que se salta: caer en él mata). El terreno levanta la loma
 * sobre el túnel (TN_ProcMapTerrain) y el actor pone el techo de roca, las estalactitas y la luz.
 */
namespace TNProcMap
{
	namespace CaveDetail
	{
		inline bool CaveBiome(ETNProcBiome B)
		{
			return B == ETNProcBiome::Volcanic || B == ETNProcBiome::Rocky || B == ETNProcBiome::Jungle || B == ETNProcBiome::Desert;
		}

		/** Altura libre (suelo a clave) de la cueva para un ancho de camino. */
		inline double Clearance(double Width, double Factor)
		{
			return FMath::Max(420.0, 0.45 * Width) * Factor;
		}
	}

	inline void BuildCaves(FLayout& L, FRng Rng)
	{
		using namespace FeatureDetail;
		using namespace CaveDetail;
		TArray<FPathSample>& M = L.Main;
		if (M.Num() < 120) { return; }
		const FGenParams& P = L.Params;
		const int32 MaxCaves = FMath::Clamp(FMath::RoundToInt(L.MainLength() / 350000.0), 1, 5);
		TArray<int32> Forks;
		for (const FBranch& B : L.Branches) { Forks.Add(B.ForkSample); Forks.Add(B.RejoinSample); }
		const double GapMaxD = LerpD(FMath::Min(P.GapMax, 200.0), P.GapMax, Saturate(P.Difficulty01));

		double NextS = Rng.Range(15000.0, 40000.0);
		int32 Count = 0;
		for (int32 i = 20; i < M.Num() - 40 && Count < MaxCaves; ++i)
		{
			if (M[i].S < NextS || !CaveBiome(M[i].Biome)) { continue; }
			const double Len = Rng.Range(6000.0, 15000.0);
			int32 j = i;
			while (j < M.Num() - 20 && M[j].S - M[i].S < Len) { ++j; }
			if (j >= M.Num() - 20) { break; }

			// Sin tramos especiales ni horquillas cerca, casi todo en biomas de cueva, sin curvas cerradas
			// ni rampas fuertes.
			bool bOk = !AnyFlag(M, i - 8, j + 8, PathFlags::Special | PathFlags::Lane);
			for (const int32 Fk : Forks) { if (Fk >= i - 24 && Fk <= j + 24) { bOk = false; break; } }
			int32 InBiome = 0;
			double Turn = 0.0;
			for (int32 k = i; k <= j && bOk; ++k)
			{
				if (CaveBiome(M[k].Biome)) { ++InBiome; }
				if (k > i) { Turn += FMath::Acos(FMath::Clamp(FVector2D::DotProduct(M[k - 1].Dir, M[k].Dir), -1.0, 1.0)); }
				if (M[k].Width > 3200.0 || M[k].Z < 450.0) { bOk = false; }
			}
			if (!bOk || InBiome < (j - i + 1) * 0.7 || Turn > FMath::DegreesToRadians(110.0) || FMath::Abs(M[j].Z - M[i].Z) > 0.1 * (M[j].S - M[i].S)) { continue; }

			// Pasos estrechos y una cámara ancha en medio; en las bocas, el ancho de fuera.
			const double Span = M[j].S - M[i].S;
			const double Narrow = Rng.Range(400.0, 700.0);
			const double Chamber = Rng.Range(1600.0, 2600.0);
			const double Uc = Rng.Range(0.38, 0.62);
			const double HalfChamber = Rng.Range(0.12, 0.2);
			int32 Mid = i;
			for (int32 k = i; k <= j; ++k)
			{
				const double U = (M[k].S - M[i].S) / Span;
				const double Bump = SmoothStep(HalfChamber, HalfChamber * 0.35, FMath::Abs(U - Uc));
				const double Shaped = LerpD(FMath::Min(M[k].Width, Narrow), Chamber, Bump);
				const double Mouth = SmoothStep(0.0, 1200.0, FMath::Min(M[k].S - M[i].S, M[j].S - M[k].S));
				M[k].Width = LerpD(M[k].Width, Shaped, Mouth);
				M[k].Flags |= PathFlags::Tunnel;
				if (FMath::Abs(U - Uc) < FMath::Abs((M[Mid].S - M[i].S) / Span - Uc)) { Mid = k; }
			}

			FFeature C = MakeAtSample(EFeature::Cave, M[Mid], i, INDEX_NONE);
			C.Aux = j;
			C.Aux2 = Rng.RangeInt(0, 1 << 20);
			C.Radius = Rng.Range(350.0, 520.0);
			C.Height = Rng.Range(1.0, 1.25);
			C.Width = Span;
			L.Features.Add(C);

			// Río de lava en la cámara de las cuevas del volcán: un hueco que se salta.
			if (M[Mid].Biome == ETNProcBiome::Volcanic)
			{
				FFeature G = MakeAtSample(EFeature::Gap, M[Mid], Mid, INDEX_NONE);
				G.Aux2 = GapLava;
				G.Length = Rng.Range(P.GapMin, GapMaxD);
				G.Width = M[Mid].Width + 200.0;
				G.Height = FMath::Max(G.Length + 300.0, 600.0);
				L.Features.Add(G);
				for (int32 k = i; k <= j; ++k)
				{
					if (FMath::Abs(M[k].S - M[Mid].S) <= G.Height * 0.5 + 100.0) { M[k].Flags |= PathFlags::Gap; }
				}
			}
			NextS = M[j].S + Rng.Range(150000.0, 300000.0);
			++Count;
			i = j;
		}
	}
}
