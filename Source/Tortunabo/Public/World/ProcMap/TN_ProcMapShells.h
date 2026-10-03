#pragma once

#include "CoreMinimal.h"
#include "World/TN_ScoreShells.h"
#include "World/ProcMap/TN_ProcMapLayout.h"
#include "World/ProcMap/TN_ProcMapPath.h"

/**
 * Reparto de las conchas de puntos del mapa procedural. Lógica pura y determinista (misma semilla, mismo reparto); la
 * prueba Tortunabo.ProcMap.Shells y la usa ATN_ProcMapGenerator::SpawnShells en el servidor.
 *
 *  - Conchitas de 1 (TNScoreShells::ETier::Small):
 *    - rachas de 5-7, a 1,8 m una de otra y serpenteando un poco, por el camino principal (una cada 220-340 m) y, más
 *      seguidas, por las ramas y los desvíos (una cada 120-190 m): guían y premian explorar;
 *    - un arco sobre los huecos de salto de labios y de panzazo (5 o 6 conchitas que dibujan el salto; como mucho uno
 *      cada 380 m de cada camino, o más separados si no caben en su tope a lo largo de todo el mapa);
 *    - una fila por el medio de las cornisas del adarve roto de las murallas.
 *  - Especiales de 50 (Big) y 100 (Grand), pocas y en retos o escondidas: en el aire sobre una brecha del adarve (se
 *    coge saltándola), tras el último hueco de una rama arriesgada, en lo alto de una ruta alta, en el punto más
 *    apartado de un desvío, a media rama tranquila y, si faltan sitios, al otro lado del salto más largo del camino
 *    principal. Las del medio de los tramos hundidos de los puentes colosales las añade la capa UE, porque dependen de
 *    su malla (ver ATN_ProcMapGenerator::SpawnShells).
 *  - Las normales de 25 no salen de aquí: siguen como antes (peligros por bioma y BonusPickup de atalayas y torres).
 *
 * Nada pisa huecos, estructuras, obstáculos, formaciones, huevos, torres, pozas ni lo que el servidor ya ha puesto
 * (Occupied: peligros, conchas normales, medusas...). Todo lo del suelo va dentro del cauce del camino.
 */
namespace TNProcMap
{
	/** Dónde va una concha del plan (para el registro, la prueba y TNShells). */
	enum class EShellSpot : uint8
	{
		Trail,        ///< Racha de conchitas por el camino principal.
		Detour,       ///< Racha de conchitas por una rama o un desvío.
		JumpArc,      ///< Arco de conchitas sobre un hueco de salto (dibuja el salto).
		WallLedge,    ///< Fila de conchitas por una cornisa del adarve roto.
		WallBreach,   ///< Especial en el aire sobre una brecha del adarve.
		RiskyBranch,  ///< Especial tras el último hueco de una rama arriesgada.
		HighRoute,    ///< Especial en lo alto de una ruta alta, antes del tobogán.
		FarTrail,     ///< Especial en el punto más apartado de un desvío por un módulo vacío.
		ScenicBranch, ///< Especial a media rama tranquila.
		BigJump,      ///< Especial al otro lado del salto más largo del camino principal.
		BrokenSpan,   ///< Especial en el medio de un tramo hundido de un puente colosal (la pone la capa UE).
		Count
	};

	inline const TCHAR* ShellSpotName(EShellSpot Spot)
	{
		switch (Spot)
		{
			case EShellSpot::Trail:        return TEXT("racha del camino");
			case EShellSpot::Detour:       return TEXT("racha de desvío");
			case EShellSpot::JumpArc:      return TEXT("arco de salto");
			case EShellSpot::WallLedge:    return TEXT("cornisa de muralla");
			case EShellSpot::WallBreach:   return TEXT("brecha de muralla");
			case EShellSpot::RiskyBranch:  return TEXT("rama arriesgada");
			case EShellSpot::HighRoute:    return TEXT("ruta alta");
			case EShellSpot::FarTrail:     return TEXT("desvío apartado");
			case EShellSpot::ScenicBranch: return TEXT("rama tranquila");
			case EShellSpot::BigJump:      return TEXT("salto largo");
			case EShellSpot::BrokenSpan:   return TEXT("tramo hundido de puente");
			default:                       return TEXT("?");
		}
	}

	/** Si el sitio es de una especial (50 o 100). */
	inline bool IsSpecialShellSpot(EShellSpot Spot) { return Spot >= EShellSpot::WallBreach && Spot < EShellSpot::Count; }

	/** Una concha del plan, en espacio del mapa. */
	struct FShellSpawn
	{
		/**
		 * Con bOnGround, XY y la cota de referencia del camino (la capa UE la asienta en el terreno y le suma
		 * TNScoreShells::Hover). Sin él, el centro exacto de la concha (ya con Hover): arcos, murallas y puentes.
		 */
		FVector Location = FVector::ZeroVector;
		TNScoreShells::ETier Tier = TNScoreShells::ETier::Small;
		EShellSpot Spot = EShellSpot::Trail;
		bool bOnGround = true;
		/** Especiales: dónde ponerse para ir a por ella (en el suelo, unos metros antes) y hacia dónde mirar (TNShells). */
		FVector Approach = FVector::ZeroVector;
		FVector2D Facing = FVector2D(1.0, 0.0);
		int32 PathIndex = INDEX_NONE;
		int32 BranchIndex = INDEX_NONE;
	};

	namespace ShellDims
	{
		/** Rachas del camino principal: una cada 220-340 m (se espacian más si no caben en el tope). */
		constexpr double TrailEveryMin = 22000.0;
		constexpr double TrailEveryMax = 34000.0;
		/** Rachas de ramas y desvíos, más seguidas: una cada 120-190 m. */
		constexpr double DetourEveryMin = 12000.0;
		constexpr double DetourEveryMax = 19000.0;
		/** Conchitas por racha y distancia entre ellas (cm). */
		constexpr int32 StreakMin = 5;
		constexpr int32 StreakMax = 7;
		constexpr double StreakStep = 180.0;
		/** Margen de las rachas con el borde del cauce (cm). */
		constexpr double EdgeMargin = 130.0;
		/**
		 * Como mucho un arco de salto cada tantos cm de cada camino (más, si con eso no caben en su tope a lo largo de todo
		 * el mapa: ArcSpacing), y cuánto pasa de cada labio.
		 */
		constexpr double ArcEvery = 38000.0;
		constexpr double ArcOverLip = 70.0;
		/** Lo que sube el arco sobre el labio: un 75 % de lo que sube el salto (1,2 m); el centro de la tortuga pasa por él. */
		inline double ArcRise() { return 0.75 * TurtleJump::Apex(); }
		/** Topes de conchitas por mapa: en total, por cornisas, en arcos y en ramas y desvíos (el principal, lo que quede). */
		constexpr int32 MaxSmall = 600;
		constexpr int32 MaxLedgeSmall = 40;
		constexpr int32 MaxArcSmall = 120;
		constexpr int32 MaxDetourSmall = 200;
		/** Hueco alrededor de cada conchita, de cada especial y distancia mínima entre dos especiales (cm). */
		constexpr double SmallSpacing = 110.0;
		constexpr double SpecialClear = 350.0;
		constexpr double SpecialSpacing = 6000.0;
		/** Especiales en el suelo: nada a menos de esto más su propio hueco (cm). */
		constexpr double SpecialRoom = 100.0;
		/** Puntuación mínima de un sitio para llevar una reina de 100 (murallas y ramas arriesgadas). */
		constexpr double GrandMinScore = 2.0;

		/** Reinas de 100 del plan (sin contar los puentes): una; dos si el camino principal pasa de 15 km. */
		inline int32 GrandBudget(const FLayout& L) { return L.MainLength() > 1500000.0 ? 2 : 1; }
		/** Grandes de 50: una cada ~3 km de camino principal, entre 1 y 5. */
		inline int32 BigBudget(const FLayout& L) { return FMath::Clamp(FMath::RoundToInt32(L.MainLength() / 300000.0), 1, 5); }

		/** Largo de todos los caminos con conchas (principal, ramas y desvíos; sin los carriles del 2vs2), en cm. */
		inline double WalkableLength(const FLayout& L)
		{
			double Length = L.MainLength();
			for (const FBranch& Br : L.Branches)
			{
				if (Br.Kind != EBranchKind::Lane && Br.Samples.Num() > 1) { Length += Br.Samples.Last().S - Br.Samples[0].S; }
			}
			return Length;
		}

		/** Distancia mínima entre dos arcos de un mismo camino: ArcEvery o la que reparte MaxArcSmall por todo el mapa. */
		inline double ArcSpacing(const FLayout& L) { return FMath::Max(ArcEvery, WalkableLength(L) / (MaxArcSmall / 5.5)); }
	}

	/** Discos prohibidos en planta (x, y, radio) con una rejilla de 20 m para consultar deprisa. */
	struct FShellKeepOut
	{
		static constexpr double Cell = 2000.0;
		TArray<FVector> Discs;
		TMap<FIntPoint, TArray<int32>> Grid;

		static FIntPoint CellOf(const FVector2D& P)
		{
			return FIntPoint(FMath::FloorToInt32(P.X / Cell), FMath::FloorToInt32(P.Y / Cell));
		}

		void Add(const FVector2D& P, double Radius)
		{
			if (Radius <= 0.0) { return; }
			const int32 Index = Discs.Add(FVector(P.X, P.Y, Radius));
			const FIntPoint A = CellOf(P - FVector2D(Radius, Radius));
			const FIntPoint B = CellOf(P + FVector2D(Radius, Radius));
			for (int32 Cy = A.Y; Cy <= B.Y; ++Cy)
			{
				for (int32 Cx = A.X; Cx <= B.X; ++Cx)
				{
					Grid.FindOrAdd(FIntPoint(Cx, Cy)).Add(Index);
				}
			}
		}

		/** Si un círculo de radio Extra en P toca algún disco. */
		bool Blocked(const FVector2D& P, double Extra = 0.0) const
		{
			const FIntPoint A = CellOf(P - FVector2D(Extra, Extra));
			const FIntPoint B = CellOf(P + FVector2D(Extra, Extra));
			for (int32 Cy = A.Y; Cy <= B.Y; ++Cy)
			{
				for (int32 Cx = A.X; Cx <= B.X; ++Cx)
				{
					const TArray<int32>* List = Grid.Find(FIntPoint(Cx, Cy));
					if (!List) { continue; }
					for (const int32 Index : *List)
					{
						const FVector& D = Discs[Index];
						if (FVector2D::DistSquared(P, FVector2D(D.X, D.Y)) < FMath::Square(D.Z + Extra)) { return true; }
					}
				}
			}
			return false;
		}
	};

	namespace ShellDetail
	{
		inline bool FlagsClear(const TArray<FPathSample>& S, int32 From, int32 To, uint32 Mask)
		{
			for (int32 i = FMath::Max(0, From); i <= FMath::Min(S.Num() - 1, To); ++i)
			{
				if ((S[i].Flags & Mask) != 0) { return false; }
			}
			return true;
		}

		/** Punto del centro de la polilínea a la distancia Sq, con su dirección, ancho, cota y muestra (interpolados). */
		inline FVector2D PointAt(const TArray<FPathSample>& S, double Sq, FVector2D& OutDir, double& OutWidth, double& OutZ, int32& OutIndex)
		{
			OutIndex = INDEX_NONE;
			OutWidth = 0.0;
			OutZ = 0.0;
			OutDir = FVector2D(1.0, 0.0);
			if (S.Num() == 0) { return FVector2D::ZeroVector; }
			int32 Lo = INDEX_NONE;
			const FVector2D P = PathDetail::MainPointAt(S, Sq, &OutDir, &Lo);
			Lo = FMath::Clamp(Lo, 0, S.Num() - 1);
			const int32 Hi = FMath::Min(Lo + 1, S.Num() - 1);
			const double Span = S[Hi].S - S[Lo].S;
			const double T = Span > 1e-3 ? FMath::Clamp((Sq - S[Lo].S) / Span, 0.0, 1.0) : 0.0;
			OutWidth = LerpD(S[Lo].Width, S[Hi].Width, T);
			OutZ = LerpD(S[Lo].Z, S[Hi].Z, T);
			OutIndex = Lo;
			if (OutDir.IsNearlyZero()) { OutDir = S[Lo].Dir; }
			return P;
		}

		/** Muestras de un camino (principal si BranchIndex no es válido). */
		inline const TArray<FPathSample>& SamplesOf(const FLayout& L, int32 BranchIndex)
		{
			return L.Branches.IsValidIndex(BranchIndex) ? L.Branches[BranchIndex].Samples : L.Main;
		}

		/**
		 * Lo que las conchas no deben pisar (radio de la huella más un margen): obstáculos, torres de escalada y sus
		 * medusas y recompensas, huevos, géiseres, puzles 2vs2, claro de salida, torres colosales, formaciones del
		 * camino, secuoyas, lava y pozas de las cascadas.
		 */
		inline void AddFeatureKeepOut(const FLayout& L, FShellKeepOut& Keep)
		{
			for (const FFeature& F : L.Features)
			{
				const FVector2D C(F.Location.X, F.Location.Y);
				switch (F.Type)
				{
					case EFeature::Boulder:
					case EFeature::RockSpire:      Keep.Add(C, F.Radius + 150.0); break;
					case EFeature::PathProp:       Keep.Add(C, FMath::Max(F.Radius, F.Length * 0.5) + 150.0); break;
					case EFeature::Log:            Keep.Add(C, F.Length * 0.5 + 150.0); break;
					case EFeature::ClimbTower:     Keep.Add(C, F.Radius + 250.0); break;
					case EFeature::BonusPickup:
					case EFeature::Bouncer:        Keep.Add(C, 250.0); break;
					case EFeature::EggNest:
					case EFeature::Geyser:         Keep.Add(C, 900.0); break;
					case EFeature::ThrowWall:
					case EFeature::SabotageGate:
					case EFeature::SabotageSwitch: Keep.Add(C, 800.0); break;
					case EFeature::StartArea:      Keep.Add(C, F.Radius + 300.0); break;
					case EFeature::Tower:          Keep.Add(C, F.Radius + 300.0); break;
					case EFeature::GiantTree:      Keep.Add(C, F.Radius * 3.0 + 300.0); break;
					case EFeature::LavaPool:       Keep.Add(C, F.Radius + 400.0); break;
					case EFeature::Formation:
					{
						const EFormation Kind = static_cast<EFormation>(F.Aux);
						if (IsLandmarkFormation(Kind)) { break; }
						Keep.Add(C, (IsArchFormation(Kind) ? FMath::Max(F.Length * 0.5, F.Radius) : F.Radius) + 300.0);
						break;
					}
					case EFeature::SlideZone:
					{
						FVector2D PoolC, Land, Flow;
						double PoolR = 0.0, PoolZ = 0.0;
						if (SlidePoolOf(L, F, PoolC, PoolR, PoolZ, Land, Flow)) { Keep.Add(PoolC, PoolR + 200.0); }
						break;
					}
					default:
						break;
				}
			}
		}

		/**
		 * Sitio para una especial en el centro de la polilínea S cerca de Sq: prueba Sq y, a saltos de 2 m hacia delante y
		 * hacia atrás, hasta 10 m; la muestra y sus vecinas no pueden ser especiales (huecos, estructuras, uniones...) ni de
		 * agua, y no puede pisar nada. Rellena Location (cota del camino), Approach (4 m antes) y Facing.
		 */
		inline bool FindGroundSpot(const TArray<FPathSample>& S, double Sq, const FShellKeepOut& Keep, FShellSpawn& Out)
		{
			if (S.Num() < 8) { return false; }
			const uint32 Avoid = PathFlags::Special | PathFlags::Lane;
			for (int32 Try = 0; Try <= 10; ++Try)
			{
				const double Shift = ((Try % 2) ? -1.0 : 1.0) * 200.0 * static_cast<double>((Try + 1) / 2);
				const double At = FMath::Clamp(Sq + Shift, S[0].S, S.Last().S);
				FVector2D Dir;
				double Width = 0.0, Z = 0.0;
				int32 Index = INDEX_NONE;
				const FVector2D P = PointAt(S, At, Dir, Width, Z, Index);
				if (Index == INDEX_NONE || IsWetBiome(S[Index].Biome) || !FlagsClear(S, Index - 2, Index + 3, Avoid)
					|| Keep.Blocked(P, ShellDims::SpecialRoom))
				{
					continue;
				}
				FVector2D BackDir;
				double BackWidth = 0.0, BackZ = 0.0;
				int32 BackIndex = INDEX_NONE;
				const FVector2D Back = PointAt(S, FMath::Max(S[0].S, At - 400.0), BackDir, BackWidth, BackZ, BackIndex);
				Out.Location = FVector(P, Z);
				Out.bOnGround = true;
				Out.PathIndex = Index;
				Out.Approach = FVector(Back, BackZ);
				Out.Facing = Dir;
				return true;
			}
			return false;
		}

		/** Candidata a especial: el sitio, lo difícil o escondido que es (más, mejor) y su grupo (uno por muralla o rama). */
		struct FShellCandidate
		{
			FShellSpawn Spawn;
			double Score = 0.0;
			int32 Group = INDEX_NONE;
		};

		/** Especiales de 50 y 100: candidatas por orden de dificultad, una por grupo y a 60 m como mínimo entre sí. */
		inline void PlanSpecials(const FLayout& L, FShellKeepOut& Keep, TArray<FShellSpawn>& Out)
		{
			TArray<FShellCandidate> Cands;
			const double SprintReach = TurtleJump::Reach(TurtleJump::SprintSpeed);

			// Brechas del adarve (de lado a lado): en el aire, en su centro, a la altura de un salto. Se coge saltándola.
			for (int32 c = 0; c < L.Crossings.Num(); ++c)
			{
				const FCrossing& Cr = L.Crossings[c];
				if (Cr.Type != ETNProcCrossingType::Wall) { continue; }
				const FRouteStep& High = L.Route[Cr.HighStep];
				FWallAxis Axis;
				Axis.Build(L.Main, High.FirstSample, High.LastSample);
				for (const FFeature& F : L.Features)
				{
					if (F.Type != EFeature::WallBreach || F.Aux != c || WallBreachDims::KindOf(F) != EWallBreach::Gap) { continue; }
					if (F.Length > 0.75 * SprintReach) { continue; }
					// Dónde ponerse para ir a por ella: en el adarve entero antes del grupo de mordiscos (no en otro hueco).
					double StandS = F.Target.X - 250.0;
					for (int32 Guard = 0; Guard < 16; ++Guard)
					{
						bool bInBreach = false;
						for (const FFeature& G : L.Features)
						{
							if (G.Type == EFeature::WallBreach && G.Aux == c && StandS >= G.Target.X - 150.0 && StandS <= G.Target.Y + 150.0)
							{
								bInBreach = true;
								StandS = G.Target.X - 250.0;
								break;
							}
						}
						if (!bInBreach) { break; }
					}
					FVector2D Pm, Tm, Nm, Pa, Ta, Na;
					double Hw = 0.0, HwA = 0.0;
					Axis.At(0.5 * (F.Target.X + F.Target.Y), Pm, Tm, Nm, Hw);
					Axis.At(FMath::Max(0.0, StandS), Pa, Ta, Na, HwA);
					FShellCandidate Cand;
					Cand.Spawn.Location = FVector(Pm, F.Location.Z + TNScoreShells::Hover + ShellDims::ArcRise());
					Cand.Spawn.bOnGround = false;
					Cand.Spawn.Spot = EShellSpot::WallBreach;
					Cand.Spawn.Approach = FVector(Pa, F.Location.Z);
					Cand.Spawn.Facing = Tm;
					Cand.Spawn.PathIndex = F.PathIndex;
					Cand.Score = 3.0 + F.Length / 1000.0;
					Cand.Group = 1000 + c;
					Cands.Add(Cand);
				}
			}

			// Ramas: tras el último hueco de las arriesgadas, en lo alto de las rutas altas, en el punto más apartado de los
			// desvíos y a media rama en las tranquilas.
			PathDetail::FSampleGrid MainGrid;
			MainGrid.Build(L.Main, L.MaxExtent());
			for (int32 b = 0; b < L.Branches.Num(); ++b)
			{
				const FBranch& Br = L.Branches[b];
				const TArray<FPathSample>& S = Br.Samples;
				if (S.Num() < 24) { continue; }
				FShellCandidate Cand;
				Cand.Group = 2000 + b;
				Cand.Spawn.BranchIndex = b;
				double Sq = -1.0;
				switch (Br.Kind)
				{
					case EBranchKind::Risky:
					{
						int32 LastGap = INDEX_NONE;
						double GapHalf = 0.0;
						for (const FFeature& F : L.Features)
						{
							if (F.Type == EFeature::Gap && F.BranchIndex == b && F.PathIndex > LastGap) { LastGap = F.PathIndex; GapHalf = F.Height * 0.5; }
						}
						Sq = S.IsValidIndex(LastGap) ? S[LastGap].S + GapHalf + 400.0 : 0.5 * (S[0].S + S.Last().S);
						if (Sq > S.Last().S - 800.0) { Sq = 0.5 * (S[0].S + S.Last().S); }
						Cand.Spawn.Spot = EShellSpot::RiskyBranch;
						Cand.Score = 2.2;
						break;
					}
					case EBranchKind::High:
					{
						int32 SlideAt = INDEX_NONE;
						for (int32 i = 0; i < S.Num(); ++i) { if ((S[i].Flags & PathFlags::Slide) != 0) { SlideAt = i; break; } }
						if (SlideAt == INDEX_NONE) { break; }
						Sq = S[SlideAt].S - 900.0;
						Cand.Spawn.Spot = EShellSpot::HighRoute;
						Cand.Score = 1.6;
						break;
					}
					case EBranchKind::Trail:
					{
						double Farthest = -1.0;
						for (int32 i = 0; i < S.Num(); i += 3)
						{
							double D = 0.0;
							if (MainGrid.Nearest(S[i].P, 30000.0, D) == INDEX_NONE) { D = 30000.0; }
							if (D > Farthest) { Farthest = D; Sq = S[i].S; }
						}
						Cand.Spawn.Spot = EShellSpot::FarTrail;
						Cand.Score = 1.3 + FMath::Min(Farthest, 30000.0) / 60000.0;
						break;
					}
					case EBranchKind::Scenic:
						Sq = 0.5 * (S[0].S + S.Last().S);
						Cand.Spawn.Spot = EShellSpot::ScenicBranch;
						Cand.Score = 1.0;
						break;
					default:
						break;
				}
				if (Sq < 0.0 || !FindGroundSpot(S, Sq, Keep, Cand.Spawn)) { continue; }
				Cands.Add(Cand);
			}

			// Por si faltan sitios: al otro lado del salto más largo del camino principal (los de panzazo primero).
			{
				const FFeature* Best = nullptr;
				for (const FFeature& F : L.Features)
				{
					if (F.Type != EFeature::Gap || F.BranchIndex != INDEX_NONE || IsLavaGap(F)) { continue; }
					const EGapStyle Style = GapStyleOf(F);
					if (Style != EGapStyle::Dive && Style != EGapStyle::Lips) { continue; }
					const bool bDive = Style == EGapStyle::Dive;
					const bool bBestDive = Best && GapStyleOf(*Best) == EGapStyle::Dive;
					if (!Best || (bDive && !bBestDive) || (bDive == bBestDive && F.Length > Best->Length)) { Best = &F; }
				}
				if (Best && L.Main.IsValidIndex(Best->PathIndex) && (GapStyleOf(*Best) == EGapStyle::Dive || Best->Length >= 250.0))
				{
					FShellCandidate Cand;
					Cand.Group = 3000;
					Cand.Spawn.Spot = EShellSpot::BigJump;
					Cand.Score = GapStyleOf(*Best) == EGapStyle::Dive ? 1.4 : 0.9;
					if (FindGroundSpot(L.Main, L.Main[Best->PathIndex].S + Best->Height * 0.5 + 300.0, Keep, Cand.Spawn))
					{
						Cands.Add(Cand);
					}
				}
			}

			Cands.StableSort([](const FShellCandidate& A, const FShellCandidate& B) { return A.Score > B.Score; });
			int32 GrandsLeft = ShellDims::GrandBudget(L);
			int32 BigsLeft = ShellDims::BigBudget(L);
			TSet<int32> UsedGroups;
			TArray<FVector2D> Placed;
			for (const FShellCandidate& Cand : Cands)
			{
				if (GrandsLeft <= 0 && BigsLeft <= 0) { break; }
				if (UsedGroups.Contains(Cand.Group)) { continue; }
				const FVector2D P(Cand.Spawn.Location.X, Cand.Spawn.Location.Y);
				bool bCrowded = false;
				for (const FVector2D& Other : Placed)
				{
					if (FVector2D::DistSquared(P, Other) < FMath::Square(ShellDims::SpecialSpacing)) { bCrowded = true; break; }
				}
				if (bCrowded) { continue; }
				FShellSpawn Spawn = Cand.Spawn;
				if (GrandsLeft > 0 && Cand.Score >= ShellDims::GrandMinScore)
				{
					Spawn.Tier = TNScoreShells::ETier::Grand;
					--GrandsLeft;
				}
				else if (BigsLeft > 0)
				{
					Spawn.Tier = TNScoreShells::ETier::Big;
					--BigsLeft;
				}
				else
				{
					continue;
				}
				Out.Add(Spawn);
				Keep.Add(P, ShellDims::SpecialClear);
				Placed.Add(P);
				UsedGroups.Add(Cand.Group);
			}
		}

		/** Fila de conchitas por el medio de cada cornisa del adarve roto (a lo largo del eje de verdad de la muralla). */
		inline void PlanLedgeLines(const FLayout& L, FShellKeepOut& Keep, int32& Budget, TArray<FShellSpawn>& Out)
		{
			int32 Used = 0;
			for (int32 c = 0; c < L.Crossings.Num(); ++c)
			{
				const FCrossing& Cr = L.Crossings[c];
				if (Cr.Type != ETNProcCrossingType::Wall) { continue; }
				const FRouteStep& High = L.Route[Cr.HighStep];
				FWallAxis Axis;
				Axis.Build(L.Main, High.FirstSample, High.LastSample);
				for (const FFeature& F : L.Features)
				{
					if (F.Type != EFeature::WallBreach || F.Aux != c) { continue; }
					const EWallBreach Kind = WallBreachDims::KindOf(F);
					if (Kind == EWallBreach::Gap) { continue; }
					const int32 Count = FMath::Min(FMath::Clamp(FMath::FloorToInt32(F.Length / 130.0), 2, 6), FMath::Min(Budget, ShellDims::MaxLedgeSmall - Used));
					if (Count < 2) { return; }
					int32 Placed = 0;
					for (int32 k = 1; k <= Count; ++k)
					{
						const double Sq = LerpD(F.Target.X, F.Target.Y, static_cast<double>(k) / (Count + 1));
						FVector2D P, T, N;
						double Hw = 0.0;
						Axis.At(Sq, P, T, N, Hw);
						const double Lat = Kind == EWallBreach::LedgeLeft ? Hw - F.Radius * 0.5 : -Hw + F.Radius * 0.5;
						const FVector2D OnLedge = P + N * Lat;
						// Junto a una especial (la de la brecha que sigue a la cornisa), esa conchita sobra.
						if (Keep.Blocked(OnLedge)) { continue; }
						FShellSpawn Spawn;
						Spawn.Location = FVector(OnLedge, F.Location.Z + TNScoreShells::Hover);
						Spawn.bOnGround = false;
						Spawn.Spot = EShellSpot::WallLedge;
						Spawn.PathIndex = F.PathIndex;
						Out.Add(Spawn);
						Keep.Add(OnLedge, ShellDims::SmallSpacing);
						++Placed;
					}
					Budget -= Placed;
					Used += Placed;
				}
			}
		}

		/**
		 * Arco de conchitas sobre los huecos de labios y de panzazo que se saltan (hasta un salto esprintando y 1 m): de
		 * labio a labio (70 cm más allá de cada uno) y subiendo ArcRise en el medio. Como mucho uno cada ArcSpacing de cada
		 * camino.
		 */
		inline void PlanJumpArcs(const FLayout& L, FShellKeepOut& Keep, int32& Budget, TArray<FShellSpawn>& Out)
		{
			TMap<int32, double> LastArc;
			int32 Used = 0;
			const double SprintReach = TurtleJump::Reach(TurtleJump::SprintSpeed);
			const double Spacing = ShellDims::ArcSpacing(L);
			for (const FFeature& F : L.Features)
			{
				if (Budget < 3 || Used >= ShellDims::MaxArcSmall) { break; }
				if (F.Type != EFeature::Gap || IsLavaGap(F)) { continue; }
				const EGapStyle Style = GapStyleOf(F);
				if ((Style != EGapStyle::Lips && Style != EGapStyle::Dive) || F.Length > SprintReach + 100.0) { continue; }
				const TArray<FPathSample>& S = SamplesOf(L, F.BranchIndex);
				if (!S.IsValidIndex(F.PathIndex)) { continue; }
				const double Sg = S[F.PathIndex].S;
				double& Last = LastArc.FindOrAdd(F.BranchIndex, -1e18);
				if (Sg - Last < Spacing) { continue; }
				const int32 Count = FMath::Min3(Style == EGapStyle::Dive ? 6 : 5, Budget, ShellDims::MaxArcSmall - Used);
				if (Count < 3) { break; }
				const FVector2D Dir = F.Dir.GetSafeNormal();
				const FVector2D C(F.Location.X, F.Location.Y);
				const double Half = F.Length * 0.5 + ShellDims::ArcOverLip;
				TArray<FShellSpawn, TInlineAllocator<8>> Arc;
				bool bClear = true;
				for (int32 k = 0; k < Count; ++k)
				{
					const double T = static_cast<double>(k) / (Count - 1);
					const FVector2D P = C + Dir * (-Half + 2.0 * Half * T);
					if (Keep.Blocked(P)) { bClear = false; break; }
					FShellSpawn Spawn;
					Spawn.Location = FVector(P, F.Location.Z + TNScoreShells::Hover + ShellDims::ArcRise() * 4.0 * T * (1.0 - T));
					Spawn.bOnGround = false;
					Spawn.Spot = EShellSpot::JumpArc;
					Spawn.PathIndex = F.PathIndex;
					Spawn.BranchIndex = F.BranchIndex;
					Arc.Add(Spawn);
				}
				if (!bClear) { continue; }
				for (const FShellSpawn& Spawn : Arc)
				{
					Out.Add(Spawn);
					Keep.Add(FVector2D(Spawn.Location.X, Spawn.Location.Y), ShellDims::SmallSpacing);
				}
				Budget -= Count;
				Used += Count;
				Last = Sg;
			}
		}

		/**
		 * Cuánto hay que espaciar las rachas (factor >= 1) para que Length de camino no pida más de Budget conchitas con
		 * una racha cada EveryMin-EveryMax (6 conchitas y 9 m de racha de media).
		 */
		inline double StreakSpacingScale(double Length, double EveryMin, double EveryMax, int32 Budget)
		{
			const double AvgCount = 0.5 * (ShellDims::StreakMin + ShellDims::StreakMax);
			const double AvgLen = (AvgCount - 1.0) * ShellDims::StreakStep;
			const double AvgEvery = 0.5 * (EveryMin + EveryMax);
			if (Budget <= 0 || Length <= 0.0) { return 1.0; }
			const double Needed = Length / (AvgEvery + AvgLen) * AvgCount;
			if (Needed <= Budget) { return 1.0; }
			return FMath::Max(1.0, (AvgCount * Length / Budget - AvgLen) / AvgEvery);
		}

		/**
		 * Rachas de conchitas por la polilínea S: cada EveryMin-EveryMax (por Scale), de StreakMin a StreakMax a StreakStep,
		 * serpenteando suave dentro del cauce (a EdgeMargin del borde). Solo en tramos sin muestras especiales (huecos,
		 * estructuras, uniones, portales, agua...) y si ninguna pisa nada; si no cabe, prueba en la muestra siguiente.
		 */
		inline void PlanStreaks(const TArray<FPathSample>& S, int32 BranchIndex, double EveryMin, double EveryMax, EShellSpot Spot,
			FShellKeepOut& Keep, FRng& Rng, int32& Budget, TArray<FShellSpawn>& Out)
		{
			if (S.Num() < 24 || Budget <= 0) { return; }
			const uint32 Avoid = PathFlags::Special | PathFlags::Lane;
			double NextS = S[0].S + Rng.Range(0.35, 0.8) * EveryMin;
			for (int32 i = 6; i < S.Num() - 6 && Budget > 0; ++i)
			{
				if (S[i].S < NextS || IsWetBiome(S[i].Biome)) { continue; }
				const int32 Count = FMath::Min(Rng.RangeInt(ShellDims::StreakMin, ShellDims::StreakMax), Budget);
				const double Len = (Count - 1) * ShellDims::StreakStep;
				int32 j = i;
				while (j + 1 < S.Num() && S[j].S - S[i].S < Len) { ++j; }
				if (j >= S.Num() - 3) { break; }
				if (!FlagsClear(S, i - 2, j + 2, Avoid)) { continue; }
				const double Phase = Rng.Range(0.0, TwoPi);
				const double Amp = Rng.Range(0.12, 0.3);
				const double Base = Rng.Range(-0.15, 0.15);
				TArray<FShellSpawn, TInlineAllocator<8>> Streak;
				bool bClear = true;
				for (int32 k = 0; k < Count; ++k)
				{
					FVector2D Dir;
					double Width = 0.0, Z = 0.0;
					int32 Index = INDEX_NONE;
					const FVector2D C = PointAt(S, S[i].S + k * ShellDims::StreakStep, Dir, Width, Z, Index);
					const double Room = FMath::Max(0.0, Width * 0.5 - ShellDims::EdgeMargin);
					const double Lat = FMath::Clamp((Base + Amp * FMath::Sin(Phase + 0.55 * k)) * Width * 0.5, -Room, Room);
					const FVector2D P = C + LeftNormal(Dir) * Lat;
					if (Index == INDEX_NONE || IsWetBiome(S[Index].Biome) || Keep.Blocked(P)) { bClear = false; break; }
					FShellSpawn Spawn;
					Spawn.Location = FVector(P, Z);
					Spawn.bOnGround = true;
					Spawn.Spot = Spot;
					Spawn.PathIndex = Index;
					Spawn.BranchIndex = BranchIndex;
					Streak.Add(Spawn);
				}
				if (!bClear) { continue; }
				for (const FShellSpawn& Spawn : Streak)
				{
					Out.Add(Spawn);
					Keep.Add(FVector2D(Spawn.Location.X, Spawn.Location.Y), ShellDims::SmallSpacing);
				}
				Budget -= Streak.Num();
				NextS = S[i].S + Len + Rng.Range(EveryMin, EveryMax);
			}
		}
	}

	/**
	 * Plan completo de conchas de 1, 50 y 100 del mapa L. Occupied: lo que el servidor ya ha puesto (x, y, radio en cm);
	 * nada del plan cae dentro. Orden: especiales, cornisas, arcos de salto, rachas de ramas y desvíos y rachas del
	 * camino principal (estas reparten lo que quede del tope a lo largo de todo el camino).
	 */
	inline void PlanShells(const FLayout& L, const TArray<FVector>& Occupied, TArray<FShellSpawn>& Out)
	{
		Out.Reset();
		if (!L.bValid || L.Main.Num() < 24) { return; }
		const FRng Root(static_cast<uint64>(L.Params.Seed) * 0x5E11C0DEull + 0xC0C4ull);

		FShellKeepOut Keep;
		ShellDetail::AddFeatureKeepOut(L, Keep);
		for (const FVector& O : Occupied) { Keep.Add(FVector2D(O.X, O.Y), O.Z); }

		ShellDetail::PlanSpecials(L, Keep, Out);
		int32 SmallLeft = ShellDims::MaxSmall;
		ShellDetail::PlanLedgeLines(L, Keep, SmallLeft, Out);
		ShellDetail::PlanJumpArcs(L, Keep, SmallLeft, Out);

		// Ramas y desvíos: premian explorar, con su tope y más seguidas.
		{
			FRng Rng = Root.Fork(3);
			int32 DetourLeft = FMath::Min(SmallLeft, ShellDims::MaxDetourSmall);
			const int32 DetourStart = DetourLeft;
			const double BranchLength = ShellDims::WalkableLength(L) - L.MainLength();
			const double Scale = ShellDetail::StreakSpacingScale(BranchLength, ShellDims::DetourEveryMin, ShellDims::DetourEveryMax, DetourLeft);
			for (int32 b = 0; b < L.Branches.Num() && DetourLeft > 0; ++b)
			{
				if (L.Branches[b].Kind == EBranchKind::Lane) { continue; }
				ShellDetail::PlanStreaks(L.Branches[b].Samples, b, ShellDims::DetourEveryMin * Scale, ShellDims::DetourEveryMax * Scale,
					EShellSpot::Detour, Keep, Rng, DetourLeft, Out);
			}
			SmallLeft -= DetourStart - DetourLeft;
		}

		// Camino principal: lo que quede, repartido a lo largo de todo el recorrido.
		{
			FRng Rng = Root.Fork(4);
			const double Scale = ShellDetail::StreakSpacingScale(L.MainLength(), ShellDims::TrailEveryMin, ShellDims::TrailEveryMax, SmallLeft);
			ShellDetail::PlanStreaks(L.Main, INDEX_NONE, ShellDims::TrailEveryMin * Scale, ShellDims::TrailEveryMax * Scale,
				EShellSpot::Trail, Keep, Rng, SmallLeft, Out);
		}
	}
}
