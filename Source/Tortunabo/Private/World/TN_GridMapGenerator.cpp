#include "World/TN_GridMapGenerator.h"
#include "World/TN_GridPathDecisions.h"
#include "Core/TN_Log.h"
#include "Components/SceneComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Math/RandomStream.h"

const FName ATN_GridMapGenerator::GeneratedTileTag(TEXT("TNGridTile"));

ATN_GridMapGenerator::ATN_GridMapGenerator()
{
	PrimaryActorTick.bCanEverTick = false;

	// Demo local: sin replicación. Misma semilla → mismo mapa en cada máquina.
	bReplicates = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
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

	if (!StraightTileClass || !TurnTileClass)
	{
		UE_LOG(LogTortunabo, Error, TEXT("[GridMap] Faltan StraightTileClass / TurnTileClass en '%s'."), *GetName());
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

	for (const TNGridLogic::FTNGridCell& Cell : TNGridLogic::ClassifyPath(Path))
	{
		const TSubclassOf<AActor> TileClass =
			(Cell.Type == TNGridLogic::ETNGridTileType::Turn) ? TurnTileClass : StraightTileClass;
		SpawnTile(TileClass, Cell.Coord, Cell.YawSteps);
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

	if (bDebugDrawPath)
	{
		DrawPathDebug(Path);
	}

	UE_LOG(LogTortunabo, Log, TEXT("[GridMap] Mapa %dx%d generado: camino de %d celdas, semilla %d."),
		GridSize, GridSize, Path.Num(), LastUsedSeed);
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

AActor* ATN_GridMapGenerator::SpawnTile(TSubclassOf<AActor> TileClass, FIntPoint Cell, int32 YawSteps)
{
	UWorld* World = GetWorld();
	if (!World || !TileClass)
	{
		return nullptr;
	}

	const FQuat LocalYaw(FRotator(0.f, YawSteps * 90.f, 0.f));
	const FTransform TileTransform(GetActorQuat() * LocalYaw, GetCellWorldLocation(Cell));

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	// Transient: los tiles generados en editor no se guardan dentro del .umap.
	Params.ObjectFlags |= RF_Transient;

	AActor* Tile = World->SpawnActor<AActor>(TileClass, TileTransform, Params);
	if (!Tile)
	{
		UE_LOG(LogTortunabo, Error, TEXT("[GridMap] SpawnActor falló para '%s' en celda (%d, %d)."),
			*TileClass->GetName(), Cell.X, Cell.Y);
		return nullptr;
	}

	Tile->Tags.AddUnique(GeneratedTileTag);
#if WITH_EDITOR
	Tile->SetFolderPath(TEXT("GridMap_Generated"));
#endif
	return Tile;
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
