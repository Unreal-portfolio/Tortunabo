#pragma once

#include "CoreMinimal.h"
#include "World/ProcMap/TN_ProcMapLayout.h"
#include "World/ProcMap/TN_ProcMapFeatures.h"

/**
 * Cuevas (lógica pura): tramos del camino principal de 120-260 m que atraviesan una montaña por un
 * túnel de roca, preferentemente junto a un macizo (el terreno ya es alto a un lado). El tramo se marca
 * como túnel (suelo llano, paredes a plomo, sin huecos ni obstáculos normales), se estrecha en pasos
 * de 4-7 m y se abre en una o dos cámaras de 16-26 m; en las del volcán, un río de lava cruza la primera
 * (hueco que se salta: caer en él mata). El terreno levanta la montaña sobre el túnel y un desfiladero
 * que lleva a cada boca (TN_ProcMapTerrain); el actor pone el techo, la decoración y la luz.
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

		/** Valor fijo en [0, 1] de la cueva F para el rasgo Salt (desfiladero, cima...). */
		inline double CaveHash01(const FFeature& F, int32 Salt)
		{
			return static_cast<double>(HashCell(0xCA7Eu, F.Aux2, Salt)) / 4294967295.0;
		}

		/** Muestras de camino antes y después de las bocas que cubren el desfiladero (y el margen de su frente). */
		constexpr int32 GorgeSamples = 14;

		/** Largo (cm) del desfiladero que lleva a cada boca de la cueva F. */
		inline double GorgeLength(const FFeature& F)
		{
			return 2500.0 + 1500.0 * CaveHash01(F, 1);
		}

		/** Altura extra (cm) de la montaña en el centro de la cueva F: las largas se ven como un monte. */
		inline double Bulk(const FFeature& F)
		{
			return (600.0 + 1400.0 * CaveHash01(F, 2)) * SmoothStep(8000.0, 20000.0, F.Width);
		}

		/**
		 * Cuánto del tramo [I, J] cruza terreno alto: fracción de muestras con el paisaje a 30 m de los dos
		 * bordes al menos 12 m por encima del camino. Ahí la cueva queda metida en el relieve que ya hay.
		 * LandAt: cota del paisaje sin cauces en un punto.
		 */
		template <typename FLandAt>
		double HighGroundShare(const FLayout& L, int32 I, int32 J, const FLandAt& LandAt)
		{
			int32 N = 0, High = 0;
			for (int32 k = I; k <= J; k += 3)
			{
				const FPathSample& S = L.Main[k];
				const FVector2D Nrm = LeftNormal(S.Dir);
				const double Off = S.Width * 0.5 + 3000.0;
				High += FMath::Min(LandAt(S.P + Nrm * Off), LandAt(S.P - Nrm * Off)) - S.Z >= 1200.0 ? 1 : 0;
				++N;
			}
			return N > 0 ? static_cast<double>(High) / N : 0.0;
		}

		/**
		 * Coloca cuevas a lo largo del camino principal en los tramos [i, j] que valgan y que Accept(i, j)
		 * acepte. Devuelve cuántas puso (si ninguna, no ha tocado el layout).
		 */
		template <typename FAccept>
		int32 PlaceCaves(FLayout& L, FRng& Rng, const FAccept& Accept)
		{
			using namespace FeatureDetail;
			TArray<FPathSample>& M = L.Main;
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
				const double Len = Rng.Range(12000.0, 26000.0);
				int32 j = i;
				while (j < M.Num() - 20 && M[j].S - M[i].S < Len) { ++j; }
				if (j >= M.Num() - 20) { break; }

				// Sin tramos especiales ni horquillas cerca (tampoco en los desfiladeros de las bocas), casi
				// todo en biomas de cueva, sin curvas cerradas ni rampas fuertes.
				bool bOk = !AnyFlag(M, i - GorgeSamples, j + GorgeSamples, PathFlags::Special | PathFlags::Lane);
				for (const int32 Fk : Forks) { if (Fk >= i - 24 && Fk <= j + 24) { bOk = false; break; } }
				int32 InBiome = 0;
				double Turn = 0.0;
				for (int32 k = i; k <= j && bOk; ++k)
				{
					if (CaveBiome(M[k].Biome)) { ++InBiome; }
					if (k > i) { Turn += FMath::Acos(FMath::Clamp(FVector2D::DotProduct(M[k - 1].Dir, M[k].Dir), -1.0, 1.0)); }
					if (M[k].Width > 3200.0 || M[k].Z < 450.0) { bOk = false; }
				}
				if (!bOk || InBiome < (j - i + 1) * 0.7 || Turn > FMath::DegreesToRadians(140.0) || FMath::Abs(M[j].Z - M[i].Z) > 0.1 * (M[j].S - M[i].S)) { continue; }
				if (!Accept(i, j)) { continue; }

				// El camino se estrecha por el desfiladero hasta la boca (11-15 m como mucho); dentro, pasos
				// estrechos y una cámara ancha (dos en las largas).
				const double Span = M[j].S - M[i].S;
				const double MouthW = Rng.Range(1100.0, 1500.0);
				for (int32 k = FMath::Max(0, i - GorgeSamples); k <= FMath::Min(M.Num() - 1, j + GorgeSamples); ++k)
				{
					const double Out = FMath::Max(0.0, M[i].S - M[k].S) + FMath::Max(0.0, M[k].S - M[j].S);
					M[k].Width = LerpD(FMath::Min(M[k].Width, MouthW), M[k].Width, SmoothStep(1500.0, 4500.0, Out));
				}
				const double Narrow = Rng.Range(400.0, 700.0);
				const double Chamber = Rng.Range(1600.0, 2600.0);
				const bool bTwo = Span > 18000.0;
				const double Uc = bTwo ? Rng.Range(0.26, 0.36) : Rng.Range(0.38, 0.62);
				const double Uc2 = bTwo ? Rng.Range(0.64, 0.76) : -10.0;
				const double HalfChamber = bTwo ? Rng.Range(0.07, 0.11) : Rng.Range(0.12, 0.2);
				int32 Mid = i;
				for (int32 k = i; k <= j; ++k)
				{
					const double U = (M[k].S - M[i].S) / Span;
					const double Bump = FMath::Max(SmoothStep(HalfChamber, HalfChamber * 0.35, FMath::Abs(U - Uc)), SmoothStep(HalfChamber, HalfChamber * 0.35, FMath::Abs(U - Uc2)));
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

				// Río de lava en la (primera) cámara de las cuevas del volcán: un hueco que se salta.
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
			return Count;
		}
	}

	/**
	 * Cuevas del camino principal: donde se pueda, en tramos que cruzan terreno alto; si no hay sitio así,
	 * donde valga. LandAt: cota del paisaje sin cauces en un punto (FTerrainBuilder::LandAt).
	 */
	template <typename FLandAt>
	void BuildCaves(FLayout& L, FRng Rng, const FLandAt& LandAt)
	{
		if (L.Main.Num() < 120) { return; }
		const auto InHighGround = [&L, &LandAt](int32 I, int32 J) { return CaveDetail::HighGroundShare(L, I, J, LandAt) >= 0.6; };
		if (CaveDetail::PlaceCaves(L, Rng, InHighGround) == 0)
		{
			CaveDetail::PlaceCaves(L, Rng, [](int32, int32) { return true; });
		}
	}
}
