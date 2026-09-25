// ─────────────────────────────────────────────────────────────────────────────
// ATN_ProcMapGenerator — vegetación, actores del mapa, peligros y PCG.
// ─────────────────────────────────────────────────────────────────────────────

#include "World/ProcMap/TN_ProcMapGenerator.h"
#include "World/ProcMap/TN_ProcMapFeatures.h"
#include "World/ProcMap/TN_ProcMapTerrain.h"
#include "World/ProcMap/TN_ProcTraversalActors.h"
#include "World/ProcMap/TN_ProcWaterActors.h"
#include "World/ProcMap/TN_ProcPuzzleActors.h"
#include "World/ProcMap/TN_ProcEggNest.h"
#include "World/ProcMap/TN_ProcMapActorUtils.h"
#include "TN_ProcMapKeepOut.h"
#include "Core/TN_Log.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PCGComponent.h"
#include "PCGGraph.h"

// ─────────────────────────────────────────────────────────────────────────────
// Vegetación y props
// ─────────────────────────────────────────────────────────────────────────────

void ATN_ProcMapGenerator::BuildScatter()
{
	using namespace TNProcMap;
	const double World = Layout.WorldSize;

	// Exclusiones: estructuras, huevos, géiseres, huecos, puzles y tramos no tallados del camino.
	FTNProcKeepOut Keep;
	Keep.AddLayout(Layout);
	// La vegetación, las rocas y los objetos sueltos los pone BuildFlora (mallas propias): con ella no
	// queda ninguna capa de formas básicas del motor (greybox).
	const bool bFlora = !Settings || Settings->bProceduralFlora;

	// Caja envolvente de cada bioma (en el raster de módulos) para no recorrer todo el mapa por capa.
	FVector2D BMin[NumBiomes], BMax[NumBiomes];
	for (int32 b = 0; b < NumBiomes; ++b) { BMin[b] = FVector2D(1e18, 1e18); BMax[b] = FVector2D(-1e18, -1e18); }
	for (int32 y = 0; y < Layout.RasterH; ++y)
	{
		for (int32 x = 0; x < Layout.RasterW; ++x)
		{
			const int32 Mod = Layout.ModuleOfCell[Layout.CellIndex(x, y)];
			if (Mod < 0) { continue; }
			const int32 b = BiomeIndex(Layout.Modules[Mod].Biome);
			const FVector2D C = Layout.CellCenter(x, y);
			BMin[b] = FVector2D(FMath::Min(BMin[b].X, C.X), FMath::Min(BMin[b].Y, C.Y));
			BMax[b] = FVector2D(FMath::Max(BMax[b].X, C.X), FMath::Max(BMax[b].Y, C.Y));
		}
	}

	const FVector2D LatMin = LatticeOrigin;
	const FVector2D LatMax = LatticeOrigin + FVector2D((LatticeNX - 1) * LatticeSpacing, (LatticeNY - 1) * LatticeSpacing);
	int32 TotalInstances = 0;

	for (int32 b = 0; b < NumBiomes; ++b)
	{
		if (BMin[b].X > BMax[b].X) { continue; }
		const ETNProcBiome Biome = BiomeFromIndex(b);
		TArray<FTNProcScatterLayer> ScatterLayers;
		if (const UTN_ProcBiomeDataAsset* Asset = Settings ? Settings->FindBiome(Biome) : nullptr)
		{
			ScatterLayers = Asset->Scatter;
		}
		else
		{
			TN_DefaultBiomeScatter(Biome, ScatterLayers);
		}

		for (int32 LayerIdx = 0; LayerIdx < ScatterLayers.Num(); ++LayerIdx)
		{
			const FTNProcScatterLayer& Layer = ScatterLayers[LayerIdx];
			if (!Layer.Mesh || Layer.DensityPer100m2 <= 0.f) { continue; }
			if (bFlora && Layer.Mesh->GetPathName().StartsWith(TEXT("/Engine/BasicShapes/")))
			{
				continue;
			}

			const bool bWalls = Layer.Zone == ETNProcScatterZone::Walls;
			const double Blend = 6000.0;
			FVector2D Min = bWalls ? LatMin : FVector2D(FMath::Max(0.0, BMin[b].X - Blend), FMath::Max(0.0, BMin[b].Y - Blend));
			FVector2D Max = bWalls ? LatMax : FVector2D(FMath::Min(World, BMax[b].X + Blend), FMath::Min(World, BMax[b].Y + Blend));
			if (bWalls)
			{
				// En los muros solo el bioma dominante de esa zona del borde.
				Min = FVector2D(FMath::Max(LatMin.X, BMin[b].X - 40000.0), FMath::Max(LatMin.Y, BMin[b].Y - 40000.0));
				Max = FVector2D(FMath::Min(LatMax.X, BMax[b].X + 40000.0), FMath::Min(LatMax.Y, BMax[b].Y + 40000.0));
			}

			const double Spacing = FMath::Sqrt(1.0e6 / static_cast<double>(Layer.DensityPer100m2));
			const int32 CX = FMath::Max(1, FMath::CeilToInt((Max.X - Min.X) / Spacing));
			const int32 CY = FMath::Max(1, FMath::CeilToInt((Max.Y - Min.Y) / Spacing));
			FRng Rng(static_cast<uint64>(Layout.Params.Seed) * 1000003ull + static_cast<uint64>(b) * 7919ull + static_cast<uint64>(LayerIdx) * 104729ull);
			const double MaxSlopeCos = FMath::Cos(FMath::DegreesToRadians(static_cast<double>(Layer.MaxSlopeDeg)));

			TArray<FTransform> Transforms;
			for (int32 cy = 0; cy < CY; ++cy)
			{
				for (int32 cx = 0; cx < CX; ++cx)
				{
					const FVector2D P = Min + FVector2D((cx + Rng.Unit()) * Spacing, (cy + Rng.Unit()) * Spacing);
					const double Pick = Rng.Unit();
					const double Yaw = Rng.Range(0.0, 360.0);
					const double ScaleT = Rng.Unit();

					// Mezcla natural en las transiciones: el bioma se sortea con sus pesos.
					double W[NumBiomes];
					Layout.BiomeWeightsAt(P, W);
					double Acc = 0.0;
					int32 Chosen = NumBiomes - 1;
					for (int32 k = 0; k < NumBiomes; ++k)
					{
						Acc += W[k];
						if (Pick < Acc) { Chosen = k; break; }
					}
					if (Chosen != b) { continue; }

					const double H = TerrainHeightMap(P);
					const FVector N = TerrainNormalMap(P);
					const double Edge = PathDistanceMap(P);
					const bool bInside = P.X >= 0.0 && P.Y >= 0.0 && P.X <= World && P.Y <= Layout.CoastY(P.X);
					const double EdgeDist = FMath::Min(FMath::Min(P.X, World - P.X), P.Y);
					const bool bWallZone = !bInside || EdgeDist < Layout.WallInset(P.X) + 1000.0;

					bool bOk = false;
					switch (Layer.Zone)
					{
						case ETNProcScatterZone::OffPath:
							bOk = bInside && Edge >= Layer.MinPathDistance && H > 40.0 && N.Z >= MaxSlopeCos;
							break;
						case ETNProcScatterZone::PathEdge:
							bOk = bInside && Edge >= Layer.MinPathDistance && Edge <= Layer.MinPathDistance + 500.0 && H > 15.0 && N.Z >= MaxSlopeCos;
							break;
						case ETNProcScatterZone::Walls:
							bOk = bWallZone && H > 300.0 && N.Z >= MaxSlopeCos;
							break;
						case ETNProcScatterZone::Shallows:
							bOk = bInside && H < 10.0 && H > -160.0 && Edge >= Layer.MinPathDistance;
							break;
					}
					if (!bOk || Keep.Blocked(P)) { continue; }

					const double S = FMath::Lerp(static_cast<double>(Layer.ScaleRange.X), static_cast<double>(Layer.ScaleRange.Y), ScaleT);
					FQuat Rot = FQuat(FRotator(0.0, Yaw, 0.0));
					if (Layer.bAlignToNormal)
					{
						Rot = FQuat::FindBetweenNormals(FVector::UpVector, N) * Rot;
					}
					const FVector Loc = FVector(P.X, P.Y, H + Layer.ZOffset);
					Transforms.Add(FTransform(Rot, Loc, Layer.ScaleAxes * S));
				}
			}
			if (Transforms.Num() == 0) { continue; }

			UHierarchicalInstancedStaticMeshComponent* HISM = NewObject<UHierarchicalInstancedStaticMeshComponent>(this, NAME_None, RF_Transient);
			HISM->SetupAttachment(RootComponent);
			HISM->SetStaticMesh(Layer.Mesh);
			HISM->SetCollisionProfileName(Layer.bCollision ? TEXT("BlockAll") : TEXT("NoCollision"));
			HISM->SetCanEverAffectNavigation(Layer.bCollision);
			if (Layer.CullDistance > 0.f)
			{
				HISM->SetCullDistances(FMath::RoundToInt(Layer.CullDistance * 0.8f), FMath::RoundToInt(Layer.CullDistance));
			}
			HISM->RegisterComponent();
			if (Layer.Material)
			{
				HISM->SetMaterial(0, Layer.Material);
			}
			if (Layer.bApplyTint)
			{
				// Con material propio solo se tiñe si lo admite; sin él, material greybox tintable.
				TNProcActors::Tint(HISM, Layer.Tint, Layer.Material == nullptr);
			}
			// Transformadas en espacio del mapa = espacio local del generador.
			HISM->AddInstances(Transforms, false, false);
			ScatterComponents.Add(HISM);
			TotalInstances += Transforms.Num();
		}
	}
	UE_LOG(LogTortunabo, Log, TEXT("[ProcMap] Vegetación y props: %d instancias en %d capas."), TotalInstances, ScatterComponents.Num());
}

// ─────────────────────────────────────────────────────────────────────────────
// Actores de recorrido (todas las máquinas)
// ─────────────────────────────────────────────────────────────────────────────

AActor* ATN_ProcMapGenerator::SpawnMapActor(UClass* Class, const FTransform& Transform, bool bTrackAsServer)
{
	UWorld* World = GetWorld();
	if (!World || !Class)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Owner = this;
	AActor* Actor = World->SpawnActor<AActor>(Class, Transform, Params);
	if (Actor)
	{
		SpawnedActors.Add(Actor);
	}
	return Actor;
}

void ATN_ProcMapGenerator::SpawnTraversalActors()
{
	using namespace TNProcMap;
	const TArray<FPathSample>& M = Layout.Main;
	const double Yaw0 = GetActorRotation().Yaw;

	// Puntos de salida alrededor del claro inicial, mirando al camino.
	StartTransforms.Reset();
	if (M.Num() > 0)
	{
		const FVector2D Ahead = M[FMath::Min(25, M.Num() - 1)].P;
		const FVector2D Face = (Ahead - Layout.StartPoint).GetSafeNormal();
		const double FaceYaw = FMath::RadiansToDegrees(AngleOf(Face)) + Yaw0;
		for (int32 i = 0; i < 8; ++i)
		{
			const double A = TwoPi * i / 8.0;
			const FVector2D P = Layout.StartPoint + DirFromAngle(A) * (450.0 + (i % 2) * 350.0);
			const FVector Loc = MapToWorld2D(P, TerrainHeightMap(P) + 110.0);
			StartTransforms.Add(FTransform(FRotator(0.0, FaceYaw, 0.0), Loc));
		}
	}

	UClass* GeyserClass = (Settings && Settings->GeyserClass) ? Settings->GeyserClass.Get() : ATN_ProcGeyser::StaticClass();

	for (const FFeature& F : Layout.Features)
	{
		const FVector2D C(F.Location.X, F.Location.Y);
		const double Yaw = FMath::RadiansToDegrees(AngleOf(F.Dir)) + Yaw0;
		switch (F.Type)
		{
			case EFeature::Geyser:
			{
				const FVector Loc = MapToWorld2D(C, TerrainHeightMap(C));
				if (ATN_ProcGeyser* Geyser = Cast<ATN_ProcGeyser>(SpawnMapActor(GeyserClass, FTransform(Loc), false)))
				{
					Geyser->SetTarget(MapToWorld(F.Target));
				}
				break;
			}
			case EFeature::SlideZone:
			{
				const TArray<FPathSample>& S = F.BranchIndex == INDEX_NONE ? M : Layout.Branches[F.BranchIndex].Samples;
				TArray<FVector> Points;
				for (int32 i = FMath::Clamp(F.PathIndex, 0, S.Num() - 1); i <= FMath::Clamp(F.Aux, 0, S.Num() - 1); ++i)
				{
					Points.Add(MapToWorld2D(S[i].P, S[i].Z));
				}
				if (ATN_ProcSlideZone* Slide = Cast<ATN_ProcSlideZone>(SpawnMapActor(ATN_ProcSlideZone::StaticClass(), GetActorTransform(), false)))
				{
					Slide->InitFromPoints(Points, static_cast<float>(F.Width));
				}
				break;
			}
			case EFeature::Gap:
			{
				// Fondo de la zanja: caer en un hueco es morir y reaparecer en los huevos. La zona
				// cubre el fondo sin asomar por encima de los labios (zanjas poco hondas junto al agua);
				// en un río de lava, justo bajo su superficie (tocarla ya mata).
				const bool bLava = TNProcMap::IsLavaGap(F);
				const FVector Loc = MapToWorld2D(C, bLava ? F.Location.Z - 260.0 : FMath::Min(TNProcMap::GapFloorZ(F) + 150.0, F.Location.Z - 400.0));
				if (ATN_ProcKillVolume* Kill = Cast<ATN_ProcKillVolume>(SpawnMapActor(ATN_ProcKillVolume::StaticClass(),
					FTransform(FRotator(0.0, Yaw, 0.0), Loc), false)))
				{
					Kill->SetExtent(FVector(F.Height * 0.5, F.Width * 0.5 + TNProcMap::GapTrenchSideOf(F), bLava ? 110.0 : 250.0));
				}
				break;
			}
			case EFeature::LavaPool:
			{
				const FVector Loc = MapToWorld2D(C, F.Location.Z - 120.0);
				if (ATN_ProcKillVolume* Kill = Cast<ATN_ProcKillVolume>(SpawnMapActor(ATN_ProcKillVolume::StaticClass(), FTransform(Loc), false)))
				{
					Kill->SetExtent(FVector(F.Radius * 0.9, F.Radius * 0.9, 150.0));
				}
				break;
			}
			default:
				break;
		}
	}

	// Caerse de un puente colosal mata: cajas de muerte del layout (bajo los tableros).
	for (const FKillBox& K : Layout.KillBoxes)
	{
		if (ATN_ProcKillVolume* Kill = Cast<ATN_ProcKillVolume>(SpawnMapActor(ATN_ProcKillVolume::StaticClass(),
			FTransform(FRotator(0.0, FMath::RadiansToDegrees(AngleOf(K.Dir)) + Yaw0, 0.0), MapToWorld(K.Center)), false)))
		{
			Kill->SetExtent(K.Half);
		}
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Actores del servidor (replicados o solo-servidor)
// ─────────────────────────────────────────────────────────────────────────────

void ATN_ProcMapGenerator::SpawnServerActors()
{
	using namespace TNProcMap;
	const double Yaw0 = GetActorRotation().Yaw;

	for (int32 i = 0; i < StartTransforms.Num(); ++i)
	{
		if (APlayerStart* Start = Cast<APlayerStart>(SpawnMapActor(APlayerStart::StaticClass(), StartTransforms[i], true)))
		{
			Start->PlayerStartTag = TEXT("TNProcStart");
		}
	}

	UClass* NestClass = (Settings && Settings->EggNestClass) ? Settings->EggNestClass.Get() : ATN_ProcEggNest::StaticClass();
	UClass* WallClass = (Settings && Settings->ThrowWallClass) ? Settings->ThrowWallClass.Get() : ATN_ProcThrowWall::StaticClass();
	UClass* GateClass = (Settings && Settings->SabotageGateClass) ? Settings->SabotageGateClass.Get() : ATN_ProcSabotageGate::StaticClass();
	UClass* SwitchClass = (Settings && Settings->SwitchClass) ? Settings->SwitchClass.Get() : ATN_ProcSwitch::StaticClass();

	TMap<int32, ATN_ProcSabotageGate*> GateByFeature;
	for (int32 f = 0; f < Layout.Features.Num(); ++f)
	{
		const FFeature& F = Layout.Features[f];
		const FVector2D C(F.Location.X, F.Location.Y);
		const FRotator Rot(0.0, FMath::RadiansToDegrees(AngleOf(F.Dir)) + Yaw0, 0.0);
		switch (F.Type)
		{
			case EFeature::EggNest:
			{
				const FVector Loc = MapToWorld2D(C, TerrainHeightMap(C));
				if (ATN_ProcEggNest* Nest = Cast<ATN_ProcEggNest>(SpawnMapActor(NestClass, FTransform(Rot, Loc), true)))
				{
					const double Progress = Layout.Main.IsValidIndex(F.PathIndex) ? Layout.Main[F.PathIndex].S : 0.0;
					Nest->InitNest(F.Aux, static_cast<float>(Progress));
					EggNests.Add(Nest);
				}
				break;
			}
			case EFeature::Finish:
			{
				// Empieza en la línea de meta (ya en el agua) y cubre toda la boca de la playa hasta el
				// fondo del volumen, donde los brazos se han abierto más.
				const FVector Loc = MapToWorld2D(C + F.Dir.GetSafeNormal() * (F.Length * 0.5), TNProcMap::SeaLevel - 300.0);
				if (ATN_ProcFinishVolume* Finish = Cast<ATN_ProcFinishVolume>(SpawnMapActor(ATN_ProcFinishVolume::StaticClass(), FTransform(Rot, Loc), true)))
				{
					Finish->SetExtent(FVector(F.Length * 0.5, F.Height * 0.5 + 1500.0, 900.0));
				}
				break;
			}
			case EFeature::ThrowWall:
			{
				const FVector Loc = MapToWorld2D(C, TerrainHeightMap(C) - 20.0);
				if (ATN_ProcThrowWall* Wall = Cast<ATN_ProcThrowWall>(SpawnMapActor(WallClass, FTransform(Rot, Loc), true)))
				{
					Wall->Setup(static_cast<float>(F.Width), static_cast<float>(F.Height), 1200.f);
					if (ATN_ProcSwitch* Switch = Cast<ATN_ProcSwitch>(SpawnMapActor(SwitchClass, FTransform(Rot, Wall->GetSwitchLocation() + FVector(0.0, 0.0, 10.0)), true)))
					{
						Switch->SetTarget(Wall, 8.f);
					}
				}
				break;
			}
			case EFeature::SabotageGate:
			{
				const FVector Loc = MapToWorld2D(C, TerrainHeightMap(C));
				if (ATN_ProcSabotageGate* Gate = Cast<ATN_ProcSabotageGate>(SpawnMapActor(GateClass, FTransform(Rot, Loc), true)))
				{
					Gate->Setup(static_cast<float>(F.Width), static_cast<float>(F.Height));
					GateByFeature.Add(f, Gate);
				}
				break;
			}
			default:
				break;
		}
	}

	// Interruptores de sabotaje: cada uno levanta la compuerta del otro carril.
	for (const FFeature& F : Layout.Features)
	{
		if (F.Type != EFeature::SabotageSwitch) { continue; }
		const FVector2D C(F.Location.X, F.Location.Y);
		ATN_ProcSabotageGate* const* Gate = GateByFeature.Find(F.Aux);
		if (!Gate || !*Gate) { continue; }
		const FVector Loc = MapToWorld2D(C, TerrainHeightMap(C) + 10.0);
		if (ATN_ProcSwitch* Switch = Cast<ATN_ProcSwitch>(SpawnMapActor(SwitchClass, FTransform(Loc), true)))
		{
			Switch->SetTarget(*Gate, 6.f);
		}
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Peligros y enemigos por bioma
// ─────────────────────────────────────────────────────────────────────────────

void ATN_ProcMapGenerator::SpawnHazards()
{
	using namespace TNProcMap;
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const bool bServer = World->GetNetMode() != NM_Client;
	const double Yaw0 = GetActorRotation().Yaw;

	struct FRuleSource
	{
		FTNProcHazardEntry Entry;
		ETNProcBiome Biome = ETNProcBiome::Jungle;
		UStaticMesh* BouncerMesh = nullptr;
		FLinearColor BouncerColor = FLinearColor(0.9f, 0.5f, 0.9f);
	};
	TArray<FRuleSource> Sources;
	TArray<FHazardRule> Rules;

	for (int32 b = 0; b < NumBiomes; ++b)
	{
		const ETNProcBiome Biome = BiomeFromIndex(b);
		TArray<FTNProcHazardEntry> Entries;
		UStaticMesh* BouncerMesh = nullptr;
		FLinearColor BouncerColor(0.9f, 0.5f, 0.9f);
		if (const UTN_ProcBiomeDataAsset* Asset = Settings ? Settings->FindBiome(Biome) : nullptr)
		{
			Entries = Asset->Hazards;
			BouncerMesh = Asset->WaterBouncerMesh;
			BouncerColor = Asset->WaterBouncerColor;
		}
		else
		{
			TN_DefaultBiomeHazards(Biome, Entries);
		}
		for (const FTNProcHazardEntry& E : Entries)
		{
			if (!E.ActorClass) { continue; }
			FRuleSource Src;
			Src.Entry = E;
			Src.Biome = Biome;
			Src.BouncerMesh = BouncerMesh;
			Src.BouncerColor = BouncerColor;
			FHazardRule R;
			R.Id = Sources.Add(Src);
			R.PerKm = E.PerKm;
			R.Placement = static_cast<EHazardPlacement>(static_cast<uint8>(E.Placement));
			R.BiomeMask = 1u << b;
			// Umbrales alineados con los perfiles por defecto (fácil 0.2, normal 0.5, difícil 0.9).
			static const double DifficultyGate[3] = { 0.0, 0.35, 0.75 };
			R.MinDifficulty01 = DifficultyGate[FMath::Clamp(static_cast<int32>(E.MinDifficulty), 0, 2)];
			R.Clearance = E.Clearance;
			R.bMainOnly = E.bMainPathOnly;
			Rules.Add(R);
		}
	}

	const TArray<FHazardSpawn> Spawns = PlanHazards(Layout, Rules, ActiveProfile.HazardDensity, 0x4A2Aull);
	int32 Count = 0;
	for (const FHazardSpawn& H : Spawns)
	{
		const FRuleSource& Src = Sources[H.RuleId];
		UClass* Class = Src.Entry.ActorClass;

		// Solo la fauna de movimiento (corrientes, remolinos) vive en todas las máquinas;
		// el resto (enemigos, spawners, pickups) lo crea el servidor y replica si procede.
		const bool bLocalEverywhere = Class->IsChildOf(ATN_ProcWaterCurrent::StaticClass()) || Class->IsChildOf(ATN_ProcWhirlpool::StaticClass());
		if (!bLocalEverywhere && !bServer) { continue; }

		const double Ground = TerrainHeightMap(H.P);
		double Z = Ground + Src.Entry.ZOffset;
		const bool bWaterFauna = Src.Entry.Placement == ETNProcHazardPlacement::InWater;
		if (bWaterFauna)
		{
			// Necesita agua de verdad bajo ella.
			const double Need = Class->IsChildOf(ATN_ProcWhirlpool::StaticClass()) ? -220.0
				: (Class->IsChildOf(ATN_ProcWaterPredator::StaticClass()) ? -170.0 : -70.0);
			if (Ground > Need) { continue; }
			Z = SeaLevel + (Class->IsChildOf(ATN_ProcWaterPredator::StaticClass()) ? -45.0 : 5.0) + Src.Entry.ZOffset;
		}
		else if (Src.Entry.Placement == ETNProcHazardPlacement::AbovePath)
		{
			Z = Ground + 800.0 + Src.Entry.ZOffset;
		}
		else if (Ground < 0.0)
		{
			continue;
		}

		const FVector Loc = MapToWorld2D(H.P, Z);
		AActor* Actor = SpawnMapActor(Class, FTransform(FRotator(0.0, H.Yaw + Yaw0, 0.0), Loc), true);
		if (!Actor) { continue; }
		++Count;

		if (ATN_ProcWaterBouncer* Bouncer = Cast<ATN_ProcWaterBouncer>(Actor))
		{
			Bouncer->SetVariant(Src.BouncerMesh, Src.BouncerColor);
		}
		else if (ATN_ProcWaterCurrent* Current = Cast<ATN_ProcWaterCurrent>(Actor))
		{
			// Arrastra lejos del camino: volver cuesta.
			const TArray<FPathSample>& Samples = H.BranchIndex == INDEX_NONE ? Layout.Main : Layout.Branches[H.BranchIndex].Samples;
			const FVector2D PathP = Samples.IsValidIndex(H.PathIndex) ? Samples[H.PathIndex].P : H.P;
			const FVector2D Away = (H.P - PathP).GetSafeNormal();
			Current->Setup(FVector(900.0, 500.0, 300.0), GetActorTransform().TransformVectorNoScale(FVector(Away.X, Away.Y, 0.0)), 900.f);
		}
	}
	UE_LOG(LogTortunabo, Log, TEXT("[ProcMap] Peligros y enemigos: %d de %d planificados (%s)."), Count, Spawns.Num(), bServer ? TEXT("servidor") : TEXT("cliente"));
}

// ─────────────────────────────────────────────────────────────────────────────
// PCG opcional por bioma
// ─────────────────────────────────────────────────────────────────────────────

void ATN_ProcMapGenerator::RunBiomePCG()
{
	if (!Settings)
	{
		return;
	}
	for (const UTN_ProcBiomeDataAsset* Asset : Settings->Biomes)
	{
		if (!Asset || !Asset->PCGGraph)
		{
			continue;
		}
		UPCGComponent* PCG = NewObject<UPCGComponent>(this, NAME_None, RF_Transient);
		PCG->RegisterComponent();
		PCG->SetGraph(Asset->PCGGraph);
		PCG->GenerateLocal(true);
		PCGComponents.Add(PCG);
		UE_LOG(LogTortunabo, Log, TEXT("[ProcMap] PCG del bioma %s lanzado."), *UEnum::GetValueAsString(Asset->Biome));
	}
}
