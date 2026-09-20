#include "World/TN_GridMapGenerator.h"
#include "World/TN_GridPathDecisions.h"
#include "World/TN_GridTerrainTile.h"
#include "Core/TN_Log.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "Math/RandomStream.h"
#include "UObject/ConstructorHelpers.h"

const FName ATN_GridMapGenerator::GeneratedTileTag(TEXT("TNGridTile"));
const FName ATN_GridMapGenerator::StartTileTag(TEXT("TNGridStart"));
const FName ATN_GridMapGenerator::EndTileTag(TEXT("TNGridEnd"));

namespace
{
	/** Lado del plano básico del motor, en uu. */
	constexpr float EnginePlaneSize = 100.f;
}

ATN_GridMapGenerator::ATN_GridMapGenerator()
{
	PrimaryActorTick.bCanEverTick = false;

	// El generador no se replica: los tiles que spawnea el servidor sí.
	bReplicates = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	WaterPlane = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WaterPlane"));
	WaterPlane->SetupAttachment(RootComponent);
	WaterPlane->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WaterPlane->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneMesh.Succeeded())
	{
		WaterPlane->SetStaticMesh(PlaneMesh.Object);
	}
}

void ATN_GridMapGenerator::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	UpdateWaterPlane();
}

void ATN_GridMapGenerator::BeginPlay()
{
	Super::BeginPlay();

	if (bGenerateOnBeginPlay && HasAuthority())
	{
		Generate();
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Generate
// ─────────────────────────────────────────────────────────────────────────────

void ATN_GridMapGenerator::Generate()
{
	Clear();

	if (!TerrainTileClass && (!StraightTileClass || !TurnTileClass))
	{
		UE_LOG(LogTortunabo, Error,
			TEXT("[GridMap] '%s' no tiene TerrainTileClass ni la pareja StraightTileClass / TurnTileClass."), *GetName());
		return;
	}

	LastUsedSeed = bRandomSeed ? FMath::Rand() : Seed;
	FRandomStream Stream(LastUsedSeed);
	auto RandRange = [&Stream](int32 Min, int32 Max) { return Stream.RandRange(Min, Max); };

	const TArray<FIntPoint> Path = TNGridLogic::GeneratePath(GridSize, MinPathLength, MaxPathLength, RandRange);
	if (Path.Num() == 0)
	{
		UE_LOG(LogTortunabo, Error,
			TEXT("[GridMap] Sin camino para GridSize=%d, longitud [%d, %d], semilla %d. Revisa los parámetros."),
			GridSize, MinPathLength, MaxPathLength, LastUsedSeed);
		return;
	}

	if (TerrainTileClass)
	{
		GenerateTerrain(Path);
	}
	else
	{
		GenerateGreybox(Path, RandRange);
	}

	if (bDebugDrawPath)
	{
		DrawPathDebug(Path);
	}

	UE_LOG(LogTortunabo, Log, TEXT("[GridMap] Mapa %dx%d generado (%s): camino de %d celdas, semilla %d."),
		GridSize, GridSize, TerrainTileClass ? TEXT("terreno") : TEXT("greybox"), Path.Num(), LastUsedSeed);
}

void ATN_GridMapGenerator::GenerateGreybox(const TArray<FIntPoint>& Path,
	TFunctionRef<int32(int32 Min, int32 Max)> RandRange)
{
	for (const TNGridLogic::FTNGridCell& Cell : TNGridLogic::ClassifyPath(Path))
	{
		const TSubclassOf<AActor> TileClass =
			(Cell.Type == TNGridLogic::ETNGridTileType::Turn) ? TurnTileClass : StraightTileClass;
		AActor* Tile = SpawnTile(TileClass, Cell.Coord, Cell.YawSteps);
		if (Tile && Cell.bIsStart) { Tile->Tags.AddUnique(StartTileTag); }
		if (Tile && Cell.bIsEnd)   { Tile->Tags.AddUnique(EndTileTag); }
	}

	const TArray<int32> Fillers = TNGridLogic::BuildFillerMap(GridSize, Path, FillerTileClasses.Num(), RandRange);
	for (int32 Row = 0; Row < GridSize; ++Row)
	{
		for (int32 Col = 0; Col < GridSize; ++Col)
		{
			const FIntPoint Cell(Col, Row);
			const int32 FillerIndex = Fillers[TNGridLogic::CellIndex(GridSize, Cell)];
			if (FillerTileClasses.IsValidIndex(FillerIndex) && FillerTileClasses[FillerIndex])
			{
				// Rotación sorteada solo para romper la repetición visual del relleno.
				SpawnTile(FillerTileClasses[FillerIndex], Cell, RandRange(0, TNGridLogic::NumSides - 1));
			}
		}
	}
}

void ATN_GridMapGenerator::GenerateTerrain(const TArray<FIntPoint>& Path)
{
	FTNGridTileInit Init;
	Init.Seed = LastUsedSeed;
	Init.GridSize = GridSize;
	Init.CellSize = CellSize;
	Init.Path = Path;

	const bool bIsGameWorld = GetWorld() && GetWorld()->IsGameWorld();

	for (int32 Row = 0; Row < GridSize; ++Row)
	{
		for (int32 Col = 0; Col < GridSize; ++Col)
		{
			Init.Coord = FIntPoint(Col, Row);

			// El terreno está expresado en ejes del grid: los tiles no se rotan.
			FTransform TileTransform;
			ATN_GridTerrainTile* Tile = Cast<ATN_GridTerrainTile>(BeginSpawnTile(TerrainTileClass, Init.Coord, 0, TileTransform));
			if (!Tile) { continue; }

			Tile->InitializeTile(Init);
			if (Init.Coord == Path[0])     { Tile->Tags.AddUnique(StartTileTag); }
			if (Init.Coord == Path.Last()) { Tile->Tags.AddUnique(EndTileTag); }
			FinishTile(Tile, TileTransform);

			// En mundo de editor los actores spawneados no ejecutan BeginPlay.
			if (!bIsGameWorld) { Tile->BuildTerrain(); }
		}
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Clear
// ─────────────────────────────────────────────────────────────────────────────

void ATN_GridMapGenerator::Clear()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TArray<AActor*> TilesToDestroy;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Tile = *It;
		// Tiles propios, o huérfanos de una instancia anterior de este generador.
		if (Tile->ActorHasTag(GeneratedTileTag) && (Tile->GetOwner() == this || !IsValid(Tile->GetOwner())))
		{
			TilesToDestroy.Add(Tile);
		}
	}

	for (AActor* Tile : TilesToDestroy)
	{
		Tile->Destroy();
	}

	FlushPersistentDebugLines(World);
}

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

FVector ATN_GridMapGenerator::GetCellWorldLocation(FIntPoint Cell) const
{
	const FVector Local(Cell.Y * CellSize, Cell.X * CellSize, 0.f);
	return GetActorTransform().TransformPosition(Local);
}

AActor* ATN_GridMapGenerator::BeginSpawnTile(TSubclassOf<AActor> TileClass, FIntPoint Cell, int32 YawSteps,
	FTransform& OutTransform)
{
	UWorld* World = GetWorld();
	if (!World || !TileClass)
	{
		return nullptr;
	}

	const FQuat LocalYaw(FRotator(0.f, YawSteps * 90.f, 0.f));
	OutTransform = FTransform(GetActorQuat() * LocalYaw, GetCellWorldLocation(Cell));

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.bDeferConstruction = true;
	if (!World->IsGameWorld())
	{
		// Transient solo en editor: los tiles de previsualización no se guardan en el .umap.
		// En juego son actores normales, que es lo que necesita la replicación.
		Params.ObjectFlags |= RF_Transient;
	}

	AActor* Tile = World->SpawnActor<AActor>(TileClass, OutTransform, Params);
	if (!Tile)
	{
		UE_LOG(LogTortunabo, Error, TEXT("[GridMap] SpawnActor falló para '%s' en celda (%d, %d)."),
			*TileClass->GetName(), Cell.X, Cell.Y);
		return nullptr;
	}

	Tile->Tags.AddUnique(GeneratedTileTag);
	return Tile;
}

void ATN_GridMapGenerator::FinishTile(AActor* Tile, const FTransform& Transform)
{
	Tile->FinishSpawning(Transform);
#if WITH_EDITOR
	Tile->SetFolderPath(TEXT("GridMap_Generated"));
#endif
}

AActor* ATN_GridMapGenerator::SpawnTile(TSubclassOf<AActor> TileClass, FIntPoint Cell, int32 YawSteps)
{
	FTransform TileTransform;
	AActor* Tile = BeginSpawnTile(TileClass, Cell, YawSteps, TileTransform);
	if (Tile)
	{
		FinishTile(Tile, TileTransform);
	}
	return Tile;
}

void ATN_GridMapGenerator::UpdateWaterPlane()
{
	if (!WaterPlane)
	{
		return;
	}

	const ATN_GridTerrainTile* TileDefaults = TerrainTileClass ? TerrainTileClass->GetDefaultObject<ATN_GridTerrainTile>() : nullptr;
	WaterPlane->SetVisibility(TileDefaults != nullptr);
	if (!TileDefaults)
	{
		return;
	}

	const float GridExtent = GridSize * CellSize;
	const float GridCenter = (GridSize - 1) * CellSize * 0.5f;
	WaterPlane->SetRelativeLocation(FVector(GridCenter, GridCenter, TileDefaults->GetSettings().WaterLevel));
	WaterPlane->SetRelativeScale3D(FVector(GridExtent / EnginePlaneSize, GridExtent / EnginePlaneSize, 1.f));
	if (WaterMaterial)
	{
		WaterPlane->SetMaterial(0, WaterMaterial);
	}
}

void ATN_GridMapGenerator::DrawPathDebug(const TArray<FIntPoint>& Path) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector Lift(0.f, 0.f, 150.f);
	for (int32 i = 1; i < Path.Num(); ++i)
	{
		DrawDebugLine(World, GetCellWorldLocation(Path[i - 1]) + Lift, GetCellWorldLocation(Path[i]) + Lift,
			FColor::Yellow, true, -1.f, 0, 20.f);
	}
	DrawDebugSphere(World, GetCellWorldLocation(Path[0]) + Lift, 120.f, 12, FColor::Green, true);
	DrawDebugSphere(World, GetCellWorldLocation(Path.Last()) + Lift, 120.f, 12, FColor::Red, true);
}
