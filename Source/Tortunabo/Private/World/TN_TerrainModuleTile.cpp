#include "World/TN_TerrainModuleTile.h"
#include "World/TN_TerrainModuleAsset.h"
#include "World/TN_TerrainModuleDecisions.h"
#include "Core/TN_Log.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"

ATN_TerrainModuleTile::ATN_TerrainModuleTile()
{
	PrimaryActorTick.bCanEverTick = false;

	// Mismo perfil de red que ATN_GridTerrainTile: el módulo no cambia tras construirse.
	bReplicates = true;
	SetReplicatingMovement(false);
	bAlwaysRelevant = true;
	NetDormancy = DORM_DormantAll;

	TerrainMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("TerrainMesh"));
	RootComponent = TerrainMesh;

	// Cocinado síncrono: con el asíncrono hay una ventana sin colisión justo al spawnear.
	TerrainMesh->bUseAsyncCooking = false;
	TerrainMesh->SetCollisionProfileName(TEXT("BlockAll"));
	TerrainMesh->SetMobility(EComponentMobility::Static);
}

void ATN_TerrainModuleTile::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Actores colocados a mano en el editor: la malla aparece al asignar el asset.
	BuildModule();
}

void ATN_TerrainModuleTile::BeginPlay()
{
	Super::BeginPlay();
	BuildModule();
}

ETNTerrainModuleTopology ATN_TerrainModuleTile::GetTopology() const
{
	return ModuleAsset ? ModuleAsset->Topology : ETNTerrainModuleTopology::Straight;
}

void ATN_TerrainModuleTile::BuildModule()
{
	if (!TerrainMesh)
	{
		return;
	}

	if (!ModuleAsset || !ModuleAsset->IsValidModule())
	{
		TerrainMesh->ClearAllMeshSections();
		BuiltFromAsset.Reset();
		return;
	}

	if (BuiltFromAsset.Get() == ModuleAsset && TerrainMesh->GetNumSections() > 0)
	{
		return;
	}

	TNTerrainModule::FModuleColors Colors;
	Colors.WaterLevel = WaterLevel;
	const TNTerrainModule::FModuleField Field = TNTerrainModule::MakeField(*ModuleAsset, ModuleSize);
	const TNGridTerrain::FTileMesh Mesh = TNTerrainModule::BuildModuleMesh(Field, Colors);

	// Sin UV: el material del terreno es triplanar y deriva las coordenadas de la posición.
	TerrainMesh->CreateMeshSection_LinearColor(0, Mesh.Vertices, Mesh.Triangles, Mesh.Normals,
		TArray<FVector2D>(), Mesh.Colors, TArray<FProcMeshTangent>(), /*bCreateCollision=*/true);
	if (TerrainMaterial)
	{
		TerrainMesh->SetMaterial(0, TerrainMaterial);
	}
	BuiltFromAsset = ModuleAsset;

	UE_LOG(LogTortunabo, Verbose, TEXT("[TerrainModule] '%s' construido desde '%s': %d vértices."),
		*GetName(), *ModuleAsset->GetName(), Mesh.Vertices.Num());
}
