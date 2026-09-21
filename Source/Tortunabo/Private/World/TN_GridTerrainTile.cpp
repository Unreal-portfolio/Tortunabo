#include "World/TN_GridTerrainTile.h"
#include "World/TN_GridJunkDecisions.h"
#include "World/TN_GridTerrainDecisions.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "Core/TN_Log.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "ProceduralMeshComponent.h"

namespace
{
	/** Color por instancia: 3 floats de PerInstanceCustomData. */
	constexpr int32 JunkCustomDataFloats = 3;

	const TCHAR* const JunkShapeNames[TNGridJunk::NumShapes] = { TEXT("Cube"), TEXT("Sphere"), TEXT("Cylinder"), TEXT("Cone") };
}

ATN_GridTerrainTile::ATN_GridTerrainTile()
{
	PrimaryActorTick.bCanEverTick = false;

	// Mismo perfil de red que los chunks (ver ATN_ChunkManager::SpawnAlignedChunk).
	// El terreno no cambia tras construirse, así que el canal se duerme después del
	// envío inicial: DORM_DormantAll (DORM_Initial es solo para actores colocados en el mapa).
	bReplicates = true;
	SetReplicatingMovement(false);
	bAlwaysRelevant = true;
	NetDormancy = DORM_DormantAll;

	TerrainMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("TerrainMesh"));
	RootComponent = TerrainMesh;

	// Cocinado SÍNCRONO: con el asíncrono hay una ventana sin colisión justo al spawnear
	// y un jugador que aparezca encima atraviesa el suelo.
	TerrainMesh->bUseAsyncCooking = false;
	TerrainMesh->SetCollisionProfileName(TEXT("BlockAll"));
	TerrainMesh->SetMobility(EComponentMobility::Static);

	for (int32 Shape = 0; Shape < TNGridJunk::NumShapes; ++Shape)
	{
		const FString MeshPath = FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), JunkShapeNames[Shape], JunkShapeNames[Shape]);
		ConstructorHelpers::FObjectFinder<UStaticMesh> ShapeMesh(*MeshPath);

		for (const bool bSolid : { true, false })
		{
			const FName ComponentName(*FString::Printf(TEXT("%sJunk_%s"), bSolid ? TEXT("Solid") : TEXT("Decor"), JunkShapeNames[Shape]));
			UInstancedStaticMeshComponent* Instances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(ComponentName);
			Instances->SetupAttachment(RootComponent);
			Instances->SetMobility(EComponentMobility::Static);
			Instances->NumCustomDataFloats = JunkCustomDataFloats;
			Instances->SetCollisionProfileName(bSolid ? TEXT("BlockAll") : TEXT("NoCollision"));
			if (ShapeMesh.Succeeded()) { Instances->SetStaticMesh(ShapeMesh.Object); }
			(bSolid ? SolidJunk : DecorJunk).Add(Instances);
		}
	}
}

void ATN_GridTerrainTile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ATN_GridTerrainTile, Init, COND_InitialOnly);
}

void ATN_GridTerrainTile::BeginPlay()
{
	Super::BeginPlay();

	// En cliente las propiedades iniciales llegan antes de BeginPlay; OnRep_Init cubre
	// el caso de que no fuera así.
	BuildTerrain();
}

void ATN_GridTerrainTile::InitializeTile(const FTNGridTileInit& InInit)
{
	Init = InInit;
}

void ATN_GridTerrainTile::OnRep_Init()
{
	BuildTerrain();
}

void ATN_GridTerrainTile::BuildTerrain()
{
	if (bTerrainBuilt || Init.Path.Num() == 0)
	{
		return;
	}

	const TNGridTerrain::FTerrainContext Context =
		TNGridTerrain::BuildContext(Init.Seed, Init.GridSize, Init.CellSize, Init.Path, Settings);
	if (!TNGridTerrain::IsContextValid(Context))
	{
		UE_LOG(LogTortunabo, Error,
			TEXT("[GridTerrain] Parámetros inválidos en '%s': CorridorHalfWidthMax + BankWidth debe ser <= CellSize / 2."),
			*GetName());
		return;
	}

	const TNGridTerrain::FTileMesh Mesh = TNGridTerrain::BuildTileMesh(Context, Init.Coord);
	// Sin UV: el material del terreno es triplanar y deriva las coordenadas de la posición.
	TerrainMesh->CreateMeshSection_LinearColor(0, Mesh.Vertices, Mesh.Triangles, Mesh.Normals,
		TArray<FVector2D>(), Mesh.Colors, TArray<FProcMeshTangent>(), /*bCreateCollision=*/true);

	if (TerrainMaterial)
	{
		TerrainMesh->SetMaterial(0, TerrainMaterial);
	}

	BuildJunk(Context);

	bTerrainBuilt = true;

	UE_LOG(LogTortunabo, Verbose, TEXT("[GridTerrain] Celda (%d, %d) construida en %s: %d vértices, semilla %d."),
		Init.Coord.X, Init.Coord.Y, HasAuthority() ? TEXT("servidor") : TEXT("cliente"), Mesh.Vertices.Num(), Init.Seed);
}

void ATN_GridTerrainTile::BuildJunk(const TNGridTerrain::FTerrainContext& Context)
{
	for (const TNGridJunk::FJunkInstance& Junk : TNGridJunk::BuildTileJunk(Context, Init.Seed, Init.Coord))
	{
		const TArray<TObjectPtr<UInstancedStaticMeshComponent>>& Pool = Junk.bSolid ? SolidJunk : DecorJunk;
		const int32 Shape = static_cast<int32>(Junk.Shape);
		if (!Pool.IsValidIndex(Shape) || !Pool[Shape])
		{
			continue;
		}

		UInstancedStaticMeshComponent* Instances = Pool[Shape];
		const int32 InstanceIndex = Instances->AddInstance(Junk.Transform);
		const float ColorData[JunkCustomDataFloats] = { Junk.Color.R, Junk.Color.G, Junk.Color.B };
		Instances->SetCustomData(InstanceIndex, MakeArrayView(ColorData));
	}

	if (JunkMaterial)
	{
		for (int32 Shape = 0; Shape < TNGridJunk::NumShapes; ++Shape)
		{
			if (SolidJunk.IsValidIndex(Shape) && SolidJunk[Shape]) { SolidJunk[Shape]->SetMaterial(0, JunkMaterial); }
			if (DecorJunk.IsValidIndex(Shape) && DecorJunk[Shape]) { DecorJunk[Shape]->SetMaterial(0, JunkMaterial); }
		}
	}
}
