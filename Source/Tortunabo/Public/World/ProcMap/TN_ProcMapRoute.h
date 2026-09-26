#pragma once

#include "CoreMinimal.h"
#include "World/ProcMap/TN_ProcMapLayout.h"
#include "World/ProcMap/TN_ProcMapModules.h"

/**
 * Ruta a nivel de módulo, biomas por regiones, niveles y portales.
 *
 * Ruta: búsqueda en profundidad aleatorizada (heurística de Warnsdorff + poda por
 * alcanzabilidad) desde un módulo del borde sur hasta uno del borde norte que
 * recorre ~Coverage de los módulos. Además de los movimientos normales admite
 * "pasadas de cruce": A → B → C por un módulo B ya recorrido, siempre que las
 * dos pasadas por B se crucen (portales intercalados alrededor de B). Esa segunda
 * pasada es la que luego se convierte en puente o cueva colosal.
 */

namespace TNProcMap
{
	namespace RouteDetail
	{
		struct FSearch
		{
			const FLayout* L = nullptr;
			int32 N = 0;
			int32 TargetMin = 0;
			int32 TargetMax = 0;
			int32 TargetCrossings = 0;
			int64 Budget = 0;
			FRng Rng = FRng(0);

			TArray<int32> Steps;
			TArray<uint8> IsCross;
			TArray<int32> Visits;
			/** Módulos que ya albergan géiser/torre/tobogán o son módulo de cruce. */
			TArray<uint8> Reserved;
			int32 Unique = 0;
			int32 Crossings = 0;
			int32 LastCrossStep = -100;
		};

		inline bool IsNorth(const FSearch& S, int32 M) { return S.L->Modules[M].GridCoord.Y == S.N - 1; }

		inline int32 StepIndexOf(const FSearch& S, int32 M)
		{
			for (int32 k = 0; k < S.Steps.Num(); ++k)
			{
				if (S.Steps[k] == M && !S.IsCross[k]) { return k; }
			}
			return INDEX_NONE;
		}

		/** Módulos no visitados alcanzables desde From; false si no se llega al norte. */
		inline bool ReachableCount(const FSearch& S, int32 From, int32& OutCount)
		{
			TArray<uint8> Seen;
			Seen.Init(0, S.L->Modules.Num());
			TArray<int32> Queue;
			Queue.Add(From);
			Seen[From] = 1;
			bool bNorth = IsNorth(S, From);
			OutCount = 0;
			for (int32 h = 0; h < Queue.Num(); ++h)
			{
				for (const int32 Nb : S.L->Modules[Queue[h]].Neighbors)
				{
					if (Seen[Nb] || S.Visits[Nb] != 0) { continue; }
					Seen[Nb] = 1;
					++OutCount;
					bNorth |= IsNorth(S, Nb);
					Queue.Add(Nb);
				}
			}
			return bNorth;
		}

		inline double AngleAround(const FLayout& L, int32 Center, int32 Nb)
		{
			const FModule& M = L.Modules[Center];
			for (int32 i = 0; i < M.Neighbors.Num(); ++i)
			{
				if (M.Neighbors[i] == Nb) { return AngleOf(M.BorderMid[i] - M.Centroid); }
			}
			return AngleOf(L.Modules[Nb].Centroid - M.Centroid);
		}

		/** true si el ángulo X está en el arco antihorario (From, To). */
		inline bool InArcCCW(double From, double To, double X)
		{
			const double Span = WrapAngle(To - From) < 0.0 ? WrapAngle(To - From) + TwoPi : WrapAngle(To - From);
			const double Off = WrapAngle(X - From) < 0.0 ? WrapAngle(X - From) + TwoPi : WrapAngle(X - From);
			return Off > 0.0 && Off < Span;
		}

		/** Las cuerdas P-Q y A-C alrededor de B se cruzan (portales intercalados). */
		inline bool Interleaved(const FLayout& L, int32 B, int32 P, int32 Q, int32 A, int32 C)
		{
			const double AP = AngleAround(L, B, P);
			const double AQ = AngleAround(L, B, Q);
			const bool bA = InArcCCW(AP, AQ, AngleAround(L, B, A));
			const bool bC = InArcCCW(AP, AQ, AngleAround(L, B, C));
			return bA != bC;
		}

		inline bool IsNeighbor(const FLayout& L, int32 A, int32 B)
		{
			return L.Modules[A].Neighbors.Contains(B);
		}

		struct FMove
		{
			int32 Via = INDEX_NONE;   ///< Módulo de cruce (INDEX_NONE = movimiento normal).
			int32 To = INDEX_NONE;
			double Score = 0.0;
		};

		inline bool Search(FSearch& S)
		{
			if (--S.Budget < 0) { return false; }

			const int32 Cur = S.Steps.Last();
			if (IsNorth(S, Cur) && S.Unique >= S.TargetMin && S.Crossings >= S.TargetCrossings) { return true; }
			if (S.Unique >= S.TargetMax) { return false; }

			TArray<FMove> Moves;
			const double ExpectedRow = (static_cast<double>(S.Unique) / FMath::Max(1, S.TargetMin)) * (S.N - 1);
			const bool bWantNorth = S.Unique + 1 >= S.TargetMin;

			for (const int32 Nb : S.L->Modules[Cur].Neighbors)
			{
				if (S.Visits[Nb] != 0) { continue; }
				int32 Onward = 0;
				for (const int32 Nb2 : S.L->Modules[Nb].Neighbors) { if (S.Visits[Nb2] == 0 && Nb2 != Nb) { ++Onward; } }
				const double Row = S.L->Modules[Nb].GridCoord.Y;
				FMove M;
				M.To = Nb;
				M.Score = S.Rng.Unit() * 0.9 - 0.22 * Onward
					- 0.18 * FMath::Max(0.0, Row - ExpectedRow - 1.0)
					+ (bWantNorth ? 0.6 * (Row - S.L->Modules[Cur].GridCoord.Y) : 0.0);
				Moves.Add(M);
			}

			// Pasadas de cruce: A(=Cur) → B (ya recorrido) → C (nuevo).
			const bool bCanCross = S.Crossings < S.TargetCrossings && S.Steps.Num() - S.LastCrossStep >= 4
				&& S.Unique >= 3 && !S.Reserved[Cur];
			if (bCanCross)
			{
				for (const int32 B : S.L->Modules[Cur].Neighbors)
				{
					if (S.Visits[B] != 1 || S.Reserved[B]) { continue; }
					const int32 KB = StepIndexOf(S, B);
					if (KB <= 0 || KB >= S.Steps.Num() - 1) { continue; }
					if (S.IsCross[KB - 1] || S.IsCross[KB + 1]) { continue; }
					const int32 PM = S.Steps[KB - 1];
					const int32 QM = S.Steps[KB + 1];
					if (PM == Cur || QM == Cur || S.Reserved[PM] || S.Reserved[QM]) { continue; }
					for (const int32 C : S.L->Modules[B].Neighbors)
					{
						if (S.Visits[C] != 0 || C == Cur || C == PM || C == QM || S.Reserved[C]) { continue; }
						if (!Interleaved(*S.L, B, PM, QM, Cur, C)) { continue; }
						FMove M;
						M.Via = B;
						M.To = C;
						M.Score = 1.4 + S.Rng.Unit();
						Moves.Add(M);
					}
				}
			}

			Moves.Sort([](const FMove& A, const FMove& B) { return A.Score > B.Score; });

			for (const FMove& Mv : Moves)
			{
				// Poda: el norte debe seguir alcanzable y quedar módulos suficientes.
				S.Visits[Mv.To] += 1;
				int32 Reach = 0;
				const bool bNorth = ReachableCount(S, Mv.To, Reach);
				const bool bEnough = S.Unique + 1 + Reach >= S.TargetMin;
				S.Visits[Mv.To] -= 1;
				if (!bNorth || !bEnough) { continue; }

				if (Mv.Via == INDEX_NONE)
				{
					S.Steps.Add(Mv.To);
					S.IsCross.Add(0);
					S.Visits[Mv.To] = 1;
					++S.Unique;
					if (Search(S)) { return true; }
					--S.Unique;
					S.Visits[Mv.To] = 0;
					S.Steps.Pop();
					S.IsCross.Pop();
				}
				else
				{
					const int32 KB = StepIndexOf(S, Mv.Via);
					const int32 PM = S.Steps[KB - 1];
					const int32 QM = S.Steps[KB + 1];
					const int32 PrevLast = S.LastCrossStep;

					S.Steps.Add(Mv.Via);
					S.IsCross.Add(1);
					S.Visits[Mv.Via] = 2;
					S.LastCrossStep = S.Steps.Num() - 1;
					S.Steps.Add(Mv.To);
					S.IsCross.Add(0);
					S.Visits[Mv.To] = 1;
					++S.Unique;
					++S.Crossings;
					const int32 ReservedList[5] = { Mv.Via, PM, QM, Cur, Mv.To };
					uint8 PrevReserved[5];
					for (int32 r = 0; r < 5; ++r) { PrevReserved[r] = S.Reserved[ReservedList[r]]; S.Reserved[ReservedList[r]] = 1; }

					if (Search(S)) { return true; }

					for (int32 r = 4; r >= 0; --r) { S.Reserved[ReservedList[r]] = PrevReserved[r]; }
					--S.Crossings;
					--S.Unique;
					S.Visits[Mv.To] = 0;
					S.Visits[Mv.Via] = 1;
					S.LastCrossStep = PrevLast;
					S.Steps.Pop(); S.IsCross.Pop();
					S.Steps.Pop(); S.IsCross.Pop();
				}
				if (S.Budget < 0) { return false; }
			}
			return false;
		}
	}

	/** Busca la ruta de módulos. Rellena L.Route y L.Crossings. */
	inline bool BuildRoute(FLayout& L, FRng Rng)
	{
		using namespace RouteDetail;
		const FGenParams& P = L.Params;
		const int32 N = FMath::Max(1, P.GridSize);
		const int32 NumModules = L.Modules.Num();

		TArray<int32> Starts;
		for (const FModule& M : L.Modules) { if (M.GridCoord.Y == 0) { Starts.Add(M.Id); } }
		Rng.Shuffle(Starts);

		const int32 Desired = FMath::Clamp(FMath::RoundToInt(P.Coverage * NumModules), FMath::Min(N, NumModules), NumModules);
		const int32 MaxCross = FMath::Max(0, P.NumCrossings);

		// De más exigente a menos: primero cruces y cobertura pedidas, luego relajando.
		for (int32 Relax = 0; Relax < 12; ++Relax)
		{
			const int32 WantCross = FMath::Max(0, MaxCross - Relax / 3);
			const int32 WantMin = FMath::Max(FMath::Min(N, NumModules), Desired - (Relax % 3) * FMath::Max(1, NumModules / 12) - (Relax / 6) * 2);
			for (int32 s = 0; s < Starts.Num(); ++s)
			{
				FSearch S;
				S.L = &L;
				S.N = N;
				S.TargetMin = WantMin;
				S.TargetMax = FMath::Min(NumModules, Desired + FMath::Max(2, NumModules / 8));
				S.TargetCrossings = WantCross;
				S.Budget = 60000;
				S.Rng = Rng.Fork(static_cast<uint64>(Relax * 131 + s));
				S.Visits.Init(0, NumModules);
				S.Reserved.Init(0, NumModules);
				S.Steps.Add(Starts[s]);
				S.IsCross.Add(0);
				S.Visits[Starts[s]] = 1;
				S.Reserved[Starts[s]] = 1;
				S.Unique = 1;
				if (N == 1)
				{
					// Mapa de un solo módulo: salida y playa en el mismo.
				}
				else if (!Search(S))
				{
					continue;
				}

				L.Route.Reset();
				L.Crossings.Reset();
				for (int32 k = 0; k < S.Steps.Num(); ++k)
				{
					FRouteStep Step;
					Step.Module = S.Steps[k];
					Step.bCrossingPass = S.IsCross[k] != 0;
					L.Route.Add(Step);
				}
				for (int32 k = 0; k < L.Route.Num(); ++k)
				{
					if (!L.Route[k].bCrossingPass) { continue; }
					FCrossing C;
					C.Module = L.Route[k].Module;
					C.SecondPassStep = k;
					for (int32 q = 0; q < k; ++q)
					{
						if (L.Route[q].Module == C.Module && !L.Route[q].bCrossingPass) { C.FirstPassStep = q; break; }
					}
					const bool bSecondHigh = Rng.Chance(0.5);
					C.HighStep = bSecondHigh ? C.SecondPassStep : C.FirstPassStep;
					C.LowStep = bSecondHigh ? C.FirstPassStep : C.SecondPassStep;
					C.Type = Rng.Chance(0.5) ? ETNProcCrossingType::Bridge : ETNProcCrossingType::Wall;
					L.Route[k].CrossingIndex = L.Crossings.Num();
					L.Route[C.FirstPassStep].CrossingIndex = L.Crossings.Num();
					L.Route[C.HighStep].bHigh = true;
					L.Crossings.Add(C);
				}
				for (FModule& M : L.Modules) { M.VisitCount = 0; }
				for (const FRouteStep& St : L.Route) { ++L.Modules[St.Module].VisitCount; }
				L.UniqueModulesOnRoute = S.Unique;
				return true;
			}
		}
		L.FailReason = "Sin ruta de modulos: revisa GridSize/Coverage";
		return false;
	}

	/** Regiones de bioma contiguas (varios módulos cada una) y tipo de módulo vacío. */
	inline void AssignBiomes(FLayout& L, FRng Rng)
	{
		const FGenParams& P = L.Params;
		const int32 NumModules = L.Modules.Num();
		int32 R = P.NumBiomeRegions > 0 ? P.NumBiomeRegions : FMath::RoundToInt(static_cast<double>(NumModules) / 6.5);
		R = FMath::Clamp(R, FMath::Min(2, NumModules), FMath::Min(NumBiomes, NumModules));

		// Semillas separadas: muestreo por punto más lejano sobre los centroides.
		TArray<int32> Seeds;
		Seeds.Add(Rng.RangeInt(0, NumModules - 1));
		while (Seeds.Num() < R)
		{
			int32 Best = INDEX_NONE;
			double BestD = -1.0;
			for (const FModule& M : L.Modules)
			{
				double D = 1e300;
				for (const int32 Sd : Seeds) { D = FMath::Min(D, FVector2D::Distance(M.Centroid, L.Modules[Sd].Centroid)); }
				D *= Rng.Range(0.8, 1.0);
				if (D > BestD) { BestD = D; Best = M.Id; }
			}
			Seeds.Add(Best);
		}

		for (FModule& M : L.Modules) { M.Region = INDEX_NONE; }
		TArray<int32> Size;
		Size.Init(0, R);
		for (int32 r = 0; r < R; ++r) { L.Modules[Seeds[r]].Region = r; Size[r] = 1; }

		int32 Assigned = R;
		while (Assigned < NumModules)
		{
			// La región más pequeña con frontera libre crece un módulo.
			int32 BestRegion = INDEX_NONE;
			for (int32 r = 0; r < R; ++r)
			{
				bool bHasFrontier = false;
				for (const FModule& M : L.Modules)
				{
					if (M.Region != r) { continue; }
					for (const int32 Nb : M.Neighbors) { if (L.Modules[Nb].Region == INDEX_NONE) { bHasFrontier = true; break; } }
					if (bHasFrontier) { break; }
				}
				if (bHasFrontier && (BestRegion == INDEX_NONE || Size[r] < Size[BestRegion])) { BestRegion = r; }
			}
			if (BestRegion == INDEX_NONE)
			{
				// Módulo aislado sin vecinos "buenos": se une a la región del vecino más cercano.
				for (FModule& M : L.Modules)
				{
					if (M.Region != INDEX_NONE) { continue; }
					double BestD = 1e300;
					for (const FModule& O : L.Modules)
					{
						if (O.Region == INDEX_NONE) { continue; }
						const double D = FVector2D::Distance(M.Centroid, O.Centroid);
						if (D < BestD) { BestD = D; M.Region = O.Region; }
					}
					++Assigned;
				}
				break;
			}
			TArray<int32> Frontier;
			for (const FModule& M : L.Modules)
			{
				if (M.Region != BestRegion) { continue; }
				for (const int32 Nb : M.Neighbors)
				{
					if (L.Modules[Nb].Region == INDEX_NONE && !Frontier.Contains(Nb)) { Frontier.Add(Nb); }
				}
			}
			const int32 Pick = Frontier[Rng.RangeInt(0, Frontier.Num() - 1)];
			L.Modules[Pick].Region = BestRegion;
			++Size[BestRegion];
			++Assigned;
		}

		// Bioma por región, sin repetir mientras queden.
		TArray<int32> Pool;
		for (int32 b = 0; b < NumBiomes; ++b) { Pool.Add(b); }
		Rng.Shuffle(Pool);
		TArray<ETNProcBiome> RegionBiome;
		for (int32 r = 0; r < R; ++r) { RegionBiome.Add(BiomeFromIndex(Pool[r % Pool.Num()])); }

		if (L.Route.Num() > 0)
		{
			const int32 EndRegion = L.Modules[L.Route.Last().Module].Region;
			const int32 StartRegion = L.Modules[L.Route[0].Module].Region;
			if (P.bForceFinalBeach && RegionBiome[EndRegion] != ETNProcBiome::Beach)
			{
				int32 BeachRegion = INDEX_NONE;
				for (int32 r = 0; r < R; ++r) { if (RegionBiome[r] == ETNProcBiome::Beach) { BeachRegion = r; } }
				if (BeachRegion != INDEX_NONE) { Swap(RegionBiome[BeachRegion], RegionBiome[EndRegion]); }
				else { RegionBiome[EndRegion] = ETNProcBiome::Beach; }
			}
			if (StartRegion != EndRegion && IsWetBiome(RegionBiome[StartRegion]))
			{
				for (int32 r = 0; r < R; ++r)
				{
					if (r != EndRegion && r != StartRegion && !IsWetBiome(RegionBiome[r]))
					{
						Swap(RegionBiome[r], RegionBiome[StartRegion]);
						break;
					}
				}
				if (IsWetBiome(RegionBiome[StartRegion])) { RegionBiome[StartRegion] = ETNProcBiome::Jungle; }
			}
		}

		for (FModule& M : L.Modules)
		{
			M.Biome = RegionBiome[FMath::Max(0, M.Region)];
			if (M.VisitCount == 0)
			{
				ETNProcEmptyModuleMode Kind = P.EmptyMode;
				if (Kind == ETNProcEmptyModuleMode::Mixed)
				{
					const int32 K = Rng.RangeInt(0, 2);
					Kind = K == 0 ? ETNProcEmptyModuleMode::Elevated : (K == 1 ? ETNProcEmptyModuleMode::BranchesAndScenery : ETNProcEmptyModuleMode::Explorable);
				}
				M.EmptyKind = Kind;
			}
		}

		// Los cruces colosales no hacen cueva en biomas de agua: sería un túnel de isletas.
		for (FCrossing& C : L.Crossings)
		{
			if (IsWetBiome(L.Modules[C.Module].Biome)) { C.Type = ETNProcCrossingType::Bridge; }
		}
	}

	/** Rango de altura base (cm) por bioma para el nivel de cada módulo. */
	inline void BiomeLevelRange(ETNProcBiome B, double& OutMin, double& OutMax)
	{
		switch (B)
		{
			case ETNProcBiome::Jungle:   OutMin = 400.0;  OutMax = 2600.0; break;
			case ETNProcBiome::Beach:    OutMin = 150.0;  OutMax = 450.0;  break;
			case ETNProcBiome::Desert:   OutMin = 300.0;  OutMax = 2200.0; break;
			case ETNProcBiome::Volcanic: OutMin = 800.0;  OutMax = 3400.0; break;
			case ETNProcBiome::Water:    OutMin = 80.0;   OutMax = 120.0;  break;
			case ETNProcBiome::Rocky:    OutMin = 600.0;  OutMax = 3000.0; break;
			case ETNProcBiome::Mangrove: OutMin = 55.0;   OutMax = 70.0;   break;
			case ETNProcBiome::Human:    OutMin = 200.0;  OutMax = 1200.0; break;
			default:                     OutMin = 300.0;  OutMax = 1500.0; break;
		}
	}

	inline void AssignLevels(FLayout& L, FRng Rng)
	{
		for (FModule& M : L.Modules)
		{
			double Min = 0.0, Max = 0.0;
			BiomeLevelRange(M.Biome, Min, Max);
			M.Level = Rng.Range(Min, Max);
		}
		if (L.Route.Num() > 0)
		{
			L.Modules[L.Route.Last().Module].Level = 150.0;
		}
	}

	/**
	 * Coloca un portal en la frontera From→To. Evita la zona de los extremos (cruces
	 * triples) y respeta una separación mínima con los portales ya puestos en esos
	 * dos módulos (importante en los módulos de cruce, que tienen cuatro).
	 */
	inline int32 PlacePortal(FLayout& L, int32 From, int32 To, FRng& Rng)
	{
		const FGenParams& P = L.Params;
		TArray<FVector2D> Pts, Dirs;
		CollectBorderPoints(L, From, To, Pts, Dirs);

		FPortal Portal;
		Portal.From = From;
		Portal.To = To;
		Portal.Width = Rng.Range(P.PortalWidthMin, P.PortalWidthMax);

		if (Pts.Num() == 0)
		{
			Portal.Point = (L.Modules[From].Centroid + L.Modules[To].Centroid) * 0.5;
			Portal.Dir = (L.Modules[To].Centroid - L.Modules[From].Centroid).GetSafeNormal();
			return L.Portals.Add(Portal);
		}

		// Eje principal de la frontera (PCA 2x2) para ordenar los puntos a lo largo.
		FVector2D Mean = FVector2D::ZeroVector;
		for (const FVector2D& Pt : Pts) { Mean += Pt; }
		Mean = Mean / static_cast<double>(Pts.Num());
		double Sxx = 0.0, Sxy = 0.0, Syy = 0.0;
		for (const FVector2D& Pt : Pts)
		{
			const FVector2D D = Pt - Mean;
			Sxx += D.X * D.X; Sxy += D.X * D.Y; Syy += D.Y * D.Y;
		}
		const double Theta = 0.5 * FMath::Atan2(2.0 * Sxy, Sxx - Syy);
		const FVector2D Axis = DirFromAngle(Theta);

		TArray<int32> Order;
		for (int32 i = 0; i < Pts.Num(); ++i) { Order.Add(i); }
		Order.Sort([&](int32 A, int32 B) { return FVector2D::DotProduct(Pts[A], Axis) < FVector2D::DotProduct(Pts[B], Axis); });

		// Tramo contiguo más largo (huecos de proyección < 3 celdas).
		int32 RunStart = 0, BestStart = 0, BestLen = 1;
		for (int32 i = 1; i <= Order.Num(); ++i)
		{
			const bool bBreak = i == Order.Num()
				|| FVector2D::DotProduct(Pts[Order[i]] - Pts[Order[i - 1]], Axis) > 3.0 * P.CellSize;
			if (bBreak)
			{
				if (i - RunStart > BestLen) { BestLen = i - RunStart; BestStart = RunStart; }
				RunStart = i;
			}
		}

		// Separación con portales existentes de los dos módulos implicados.
		auto TooClose = [&](const FVector2D& Pt)
		{
			for (const FPortal& Other : L.Portals)
			{
				const bool bShares = Other.From == From || Other.To == From || Other.From == To || Other.To == To;
				if (bShares && FVector2D::Distance(Other.Point, Pt) < 9000.0) { return true; }
			}
			return false;
		};

		int32 Chosen = BestStart + BestLen / 2;
		for (int32 Try = 0; Try < 12; ++Try)
		{
			const double F = Try == 0 ? Rng.Range(0.35, 0.65) : Rng.Range(0.18, 0.82);
			const int32 Cand = BestStart + FMath::Clamp(FMath::RoundToInt(F * (BestLen - 1)), 0, BestLen - 1);
			Chosen = Cand;
			if (!TooClose(Pts[Order[Cand]])) { break; }
		}

		Portal.Point = Pts[Order[Chosen]];
		FVector2D DirSum = FVector2D::ZeroVector;
		for (int32 k = FMath::Max(BestStart, Chosen - 6); k <= FMath::Min(BestStart + BestLen - 1, Chosen + 6); ++k)
		{
			DirSum += Dirs[Order[k]];
		}
		Portal.Dir = DirSum.GetSafeNormal();
		if (Portal.Dir.IsNearlyZero()) { Portal.Dir = (L.Modules[To].Centroid - L.Modules[From].Centroid).GetSafeNormal(); }
		Portal.Width = FMath::Min(Portal.Width, FMath::Max(P.PathWidthMin * 2.0, 0.45 * BestLen * P.CellSize));
		return L.Portals.Add(Portal);
	}

	inline void BuildPortals(FLayout& L, FRng Rng)
	{
		L.Portals.Reset();
		// Primero los portales de los módulos de cruce (4 cada uno), que son los más
		// exigentes en separación; luego el resto en orden de ruta.
		TArray<int32> Order;
		for (int32 k = 0; k + 1 < L.Route.Num(); ++k)
		{
			const bool bCross = L.Route[k].CrossingIndex != INDEX_NONE || L.Route[k + 1].CrossingIndex != INDEX_NONE;
			if (bCross) { Order.Add(k); }
		}
		for (int32 k = 0; k + 1 < L.Route.Num(); ++k)
		{
			if (!Order.Contains(k)) { Order.Add(k); }
		}
		for (const int32 k : Order)
		{
			const int32 Idx = PlacePortal(L, L.Route[k].Module, L.Route[k + 1].Module, Rng);
			L.Route[k].ExitPortal = Idx;
			L.Route[k + 1].EntryPortal = Idx;
		}
	}
}
