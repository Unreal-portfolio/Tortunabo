#include "World/TN_GridTerrainTile.h"
#include "World/TN_GridTerrainDecisions.h"
#include "Core/TN_Log.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "ProceduralMeshComponent.h"

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
	TerrainMesh->CreateMeshSection_LinearColor(0, Mesh.Vertices, Mesh.Triangles, Mesh.Normals, Mesh.UVs,
		Mesh.Colors, TArray<FProcMeshTangent>(), /*bCreateCollision=*/true);

	if (TerrainMaterial)
	{
		TerrainMesh->SetMaterial(0, TerrainMaterial);
	}

	bTerrainBuilt = true;

	UE_LOG(LogTortunabo, Verbose, TEXT("[GridTerrain] Celda (%d, %d) construida en %s: %d vértices, semilla %d."),
		Init.Coord.X, Init.Coord.Y, HasAuthority() ? TEXT("servidor") : TEXT("cliente"), Mesh.Vertices.Num(), Init.Seed);
}
