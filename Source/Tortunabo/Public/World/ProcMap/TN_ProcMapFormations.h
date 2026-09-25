#pragma once

#include "CoreMinimal.h"
#include "World/ProcMap/TN_ProcMapLayout.h"
#include "World/ProcMap/TN_ProcMapPath.h"
#include "World/ProcMap/TN_ProcMapFeatures.h"

/**
 * Formaciones temáticas del paisaje (lógica pura): arcos que cruzan el camino, piezas en sus
 * explanadas (dejando carriles libres) e hitos lejanos, según el bioma: naturaleza (arcos de roca,
 * chimeneas de hadas, basalto, farallones, mesas), entorno (barco varado, faro, palafitos, templo,
 * pirámide, molino) y guerra (sacos terreros, búnker, torre de vigía, carro de combate, cañón,
 * castillo). Las mallas las construye el actor (TN_ProcMapFormationMeshes.h).
 */
namespace TNProcMap
{
	namespace FormationDetail
	{
		/** Qué formaciones van en cada bioma, por clase de colocación. */
		inline void KindsFor(ETNProcBiome Biome, TArray<EFormation>& Arches, TArray<EFormation>& Plaza, TArray<EFormation>& Far)
		{
			Arches.Reset();
			Plaza.Reset();
			Far.Reset();
			using K = EFormation;
			switch (Biome)
			{
				case ETNProcBiome::Jungle:   Arches = { K::StoneArch, K::TempleGate, K::TempleGate }; Plaza = { K::StoneHead, K::StoneHead, K::TankWreck }; Far = { K::Pyramid }; break;
				case ETNProcBiome::Beach:    Arches = { K::WhaleRibs, K::WhaleRibs, K::StoneArch }; Plaza = { K::Shipwreck, K::Shipwreck, K::Bunker, K::Sandbags }; Far = { K::Lighthouse, K::SeaStack }; break;
				case ETNProcBiome::Desert:   Arches = { K::StoneArch }; Plaza = { K::Hoodoo, K::BalancedRock, K::Wagon, K::Sandbags }; Far = { K::Mesa }; break;
				case ETNProcBiome::Volcanic: Arches = { K::StoneArch }; Plaza = { K::BasaltColumns, K::Fumarole }; break;
				case ETNProcBiome::Water:    Far = { K::StiltHut }; break;
				case ETNProcBiome::Rocky:    Arches = { K::StoneArch }; Plaza = { K::Hoodoo, K::BalancedRock, K::Cannon, K::Bunker }; Far = { K::CastleRuin, K::SeaStack }; break;
				case ETNProcBiome::Mangrove: Arches = { K::RootArch }; Far = { K::StiltHut }; break;
				case ETNProcBiome::Human:
				default:                     Plaza = { K::Sandbags, K::Bunker, K::WatchTower, K::TankWreck }; Far = { K::Windmill }; break;
			}
		}

		/** Huella (radio) y alto de una pieza de explanada. */
		inline void PlazaSize(EFormation Kind, FRng& Rng, double& OutRadius, double& OutHeight)
		{
			using K = EFormation;
			switch (Kind)
			{
				case K::Shipwreck:     OutRadius = Rng.Range(700.0, 950.0); OutHeight = 450.0; break;
				case K::StoneHead:     OutRadius = Rng.Range(250.0, 340.0); OutHeight = OutRadius * Rng.Range(1.6, 1.9); break;
				case K::BasaltColumns: OutRadius = Rng.Range(300.0, 550.0); OutHeight = Rng.Range(350.0, 700.0); break;
				case K::Fumarole:      OutRadius = Rng.Range(250.0, 400.0); OutHeight = Rng.Range(150.0, 280.0); break;
				case K::Hoodoo:        OutRadius = Rng.Range(150.0, 260.0); OutHeight = Rng.Range(600.0, 1100.0); break;
				case K::BalancedRock:  OutRadius = Rng.Range(200.0, 300.0); OutHeight = Rng.Range(500.0, 800.0); break;
				case K::Wagon:         OutRadius = 300.0; OutHeight = 300.0; break;
				case K::Cannon:        OutRadius = 260.0; OutHeight = 200.0; break;
				case K::Sandbags:      OutRadius = Rng.Range(300.0, 450.0); OutHeight = 110.0; break;
				case K::Bunker:        OutRadius = Rng.Range(350.0, 450.0); OutHeight = 260.0; break;
				case K::WatchTower:    OutRadius = 250.0; OutHeight = Rng.Range(750.0, 950.0); break;
				case K::TankWreck:     OutRadius = Rng.Range(350.0, 420.0); OutHeight = 280.0; break;
				default:               OutRadius = 300.0; OutHeight = 300.0; break;
			}
		}

		/** Base y alto de un hito lejano. */
		inline void LandmarkSize(EFormation Kind, FRng& Rng, double& OutRadius, double& OutHeight)
		{
			using K = EFormation;
			switch (Kind)
			{
				case K::Pyramid:    OutRadius = Rng.Range(1500.0, 2500.0); OutHeight = OutRadius * Rng.Range(0.75, 0.9); break;
				case K::Lighthouse: OutRadius = 380.0; OutHeight = Rng.Range(2600.0, 3400.0); break;
				case K::Mesa:       OutRadius = Rng.Range(2500.0, 5000.0); OutHeight = Rng.Range(1800.0, 3500.0); break;
				case K::SeaStack:   OutRadius = Rng.Range(400.0, 900.0); OutHeight = Rng.Range(1500.0, 3500.0); break;
				case K::CastleRuin: OutRadius = Rng.Range(1500.0, 2200.0); OutHeight = Rng.Range(1100.0, 1500.0); break;
				case K::Windmill:   OutRadius = 420.0; OutHeight = Rng.Range(1400.0, 1800.0); break;
				case K::StiltHut:   OutRadius = 420.0; OutHeight = 650.0; break;
				default:            OutRadius = 1000.0; OutHeight = 1000.0; break;
			}
		}

		/** Lejos de lo que ya hay en el layout (huecos, géiseres, torres, obstáculos, huevos, otras formaciones). */
		inline bool ClearOfFeatures(const FLayout& L, const FVector2D& C, double R)
		{
			for (const FFeature& F : L.Features)
			{
				double Reach = 0.0;
				switch (F.Type)
				{
					case EFeature::Gap:
					case EFeature::Geyser:
					case EFeature::SlideZone:
					case EFeature::EggNest:
					case EFeature::Boulder:
					case EFeature::RockSpire:
					case EFeature::Log:
					case EFeature::ThrowWall:
					case EFeature::SabotageGate:
					case EFeature::SabotageSwitch:
					case EFeature::Gate:
					case EFeature::Tower:
					case EFeature::Formation:
					case EFeature::Volcano:
					case EFeature::GiantTree:
						Reach = FMath::Max(F.Radius, FMath::Max(F.Width, F.Length) * 0.5) + 1000.0;
						break;
					default:
						break;
				}
				if (Reach > 0.0 && FVector2D::Distance(C, FVector2D(F.Location.X, F.Location.Y)) < Reach + R) { return false; }
			}
			return true;
		}

		/** Tramo recto (menos de 20° de giro en ±3 muestras) y llano (menos de 1,5 m de desnivel). */
		inline bool StraightAndFlat(const TArray<FPathSample>& S, int32 i)
		{
			const int32 A = FMath::Max(0, i - 3);
			const int32 B = FMath::Min(S.Num() - 1, i + 3);
			return FVector2D::DotProduct(S[A].Dir, S[B].Dir) > FMath::Cos(FMath::DegreesToRadians(20.0)) && FMath::Abs(S[B].Z - S[A].Z) < 150.0;
		}
	}

	inline void BuildFormations(FLayout& L, FRng Rng)
	{
		using namespace FeatureDetail;
		using namespace FormationDetail;
		const uint32 Avoid = PathFlags::Special | PathFlags::Lane;
		TArray<EFormation> Arches, Plaza, Far;

		// ── En el camino: arcos que lo cruzan y piezas en sus explanadas ──
		auto OnPolyline = [&](const TArray<FPathSample>& S, int32 BranchIndex)
		{
			if (S.Num() < 30) { return; }
			double NextArch = Rng.Range(6000.0, 20000.0);
			double NextPlaza = Rng.Range(3000.0, 10000.0);
			for (int32 i = 10; i < S.Num() - 10; ++i)
			{
				const FPathSample& Sm = S[i];
				if (Sm.S < FMath::Min(NextArch, NextPlaza) || AnyFlag(S, i - 8, i + 8, Avoid)) { continue; }
				bool bFork = false;
				for (const FBranch& B : L.Branches)
				{
					if (BranchIndex == INDEX_NONE && (FMath::Abs(B.ForkSample - i) < 12 || FMath::Abs(B.RejoinSample - i) < 12)) { bFork = true; break; }
				}
				if (bFork || (BranchIndex != INDEX_NONE && (i < 16 || i > S.Num() - 16))) { continue; }
				KindsFor(Sm.Biome, Arches, Plaza, Far);

				// Arco: pies en los taludes a ambos lados, el camino pasa por debajo.
				if (Sm.S >= NextArch && Arches.Num() > 0 && Sm.Width >= 350.0 && Sm.Width <= 1800.0 && StraightAndFlat(S, i))
				{
					const EFormation Kind = Arches[Rng.RangeInt(0, Arches.Num() - 1)];
					FFeature F = MakeAtSample(EFeature::Formation, Sm, i, BranchIndex);
					F.Aux = static_cast<int32>(Kind);
					F.Aux2 = Rng.RangeInt(0, 1 << 20);
					F.Width = Sm.Width + 2.0 * Rng.Range(250.0, 450.0);
					F.Height = Rng.Range(650.0, 1000.0) + 0.25 * Sm.Width;
					F.Length = Kind == EFormation::WhaleRibs ? Rng.Range(900.0, 1300.0) : Rng.Range(250.0, 450.0);
					F.Radius = F.Width * 0.5;
					if (ClearOfFeatures(L, Sm.P, F.Radius + 800.0))
					{
						L.Features.Add(F);
						NextArch = Sm.S + Rng.Range(80000.0, 160000.0);
						continue;
					}
				}

				// Explanada: una pieza a un lado o en medio, con carriles de 4,5 m o más.
				if (Sm.S >= NextPlaza && Plaza.Num() > 0 && Sm.Width >= 1400.0)
				{
					const EFormation Kind = Plaza[Rng.RangeInt(0, Plaza.Num() - 1)];
					double R = 0.0, H = 0.0;
					PlazaSize(Kind, Rng, R, H);
					const double Room = Sm.Width * 0.5 - R - 450.0;
					if (Room < 0.0) { continue; }
					FFeature F = MakeAtSample(EFeature::Formation, Sm, i, BranchIndex);
					F.Location = FVector(Sm.P + LeftNormal(Sm.Dir) * Rng.Range(-Room, Room), Sm.Z);
					const double Yaw = Rng.Range(-0.6, 0.6);
					F.Dir = FVector2D(Sm.Dir.X * FMath::Cos(Yaw) - Sm.Dir.Y * FMath::Sin(Yaw), Sm.Dir.X * FMath::Sin(Yaw) + Sm.Dir.Y * FMath::Cos(Yaw));
					F.Radius = R;
					F.Height = H;
					F.Aux = static_cast<int32>(Kind);
					F.Aux2 = Rng.RangeInt(0, 1 << 20);
					if (ClearOfFeatures(L, FVector2D(F.Location.X, F.Location.Y), R + 600.0))
					{
						L.Features.Add(F);
						NextPlaza = Sm.S + Rng.Range(30000.0, 60000.0);
					}
				}
			}
		};
		OnPolyline(L.Main, INDEX_NONE);
		for (int32 b = 0; b < L.Branches.Num(); ++b)
		{
			if (L.Branches[b].Kind != EBranchKind::Lane) { OnPolyline(L.Branches[b].Samples, b); }
		}

		// ── Hitos lejanos: uno por módulo (55 %), a 30-250 m del camino, visibles desde él ──
		TArray<FPathSample> All = L.Main;
		for (const FBranch& B : L.Branches) { All.Append(B.Samples); }
		PathDetail::FSampleGrid Grid;
		Grid.Build(All, L.WorldSize);
		for (const FModule& M : L.Modules)
		{
			KindsFor(M.Biome, Arches, Plaza, Far);
			if (Far.Num() == 0 || !Rng.Chance(0.55)) { continue; }
			const EFormation Kind = Far[Rng.RangeInt(0, Far.Num() - 1)];
			double R = 0.0, H = 0.0;
			LandmarkSize(Kind, Rng, R, H);
			for (int32 Try = 0; Try < 40; ++Try)
			{
				const int32 X = Rng.RangeInt(0, L.RasterW - 1);
				const int32 Y = Rng.RangeInt(0, L.RasterH - 1);
				if (L.ModuleOfCell[L.CellIndex(X, Y)] != M.Id) { continue; }
				FVector2D C = L.CellCenter(X, Y);
				const double Coast = L.CoastY(C.X);
				if (Kind == EFormation::SeaStack)
				{
					// Mar adentro, frente a la costa del módulo.
					if (Coast - C.Y > 30000.0) { break; }
					C.Y = Coast + Rng.Range(2500.0, 12000.0);
				}
				else if (Kind == EFormation::Lighthouse)
				{
					// En tierra, asomado al mar.
					if (Coast - C.Y > 30000.0) { break; }
					C.Y = Coast - Rng.Range(1500.0, 4000.0);
				}
				else if (C.X < 8000.0 || C.X > L.WorldSize - 8000.0 || C.Y < 8000.0 || C.Y > Coast - 6000.0)
				{
					continue;
				}
				double D = 0.0;
				const int32 Near = Grid.Nearest(C, 40000.0, D);
				const double Edge = Near == INDEX_NONE ? 40000.0 : D - All[Near].Width * 0.5;
				const double MinEdge = Kind == EFormation::StiltHut ? 900.0 + R : 3000.0 + R;
				if (Edge < MinEdge || Edge > 25000.0 + R || !ClearOfFeatures(L, C, R + 1500.0)) { continue; }
				FFeature F;
				F.Type = EFeature::Formation;
				F.Biome = M.Biome;
				F.Location = FVector(C, 0.0);
				// Mirando hacia el camino más cercano.
				const FVector2D ToPath = Near == INDEX_NONE ? FVector2D(1.0, 0.0) : (All[Near].P - C).GetSafeNormal();
				F.Dir = ToPath.IsNearlyZero() ? FVector2D(1.0, 0.0) : ToPath;
				F.Radius = R;
				F.Height = H;
				F.Aux = static_cast<int32>(Kind);
				F.Aux2 = Rng.RangeInt(0, 1 << 20);
				L.Features.Add(F);
				break;
			}
		}
	}
}
