#include "World/TN_TerrainModuleTile.h"
#include "World/TN_TerrainModuleAsset.h"
#include "World/TN_TerrainModuleDecisions.h"
#include "World/TN_TerrainModuleWallDecisions.h"
#include "Core/TN_Log.h"
#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "ProceduralMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/** Primera sección de TerrainMesh con arcos; la 0 es el terreno. */
	constexpr int32 ArchSectionOffset = 1;

	/** Color por instancia de basura: 3 floats de PerInstanceCustomData. */
	constexpr int32 WallJunkCustomDataFloats = 3;

	const TCHAR* const WallJunkShapeNames[TNGridJunk::NumShapes] = { TEXT("Cube"), TEXT("Sphere"), TEXT("Cylinder"), TEXT("Cone") };
	const TCHAR* const WallSideNames[TNGridLogic::NumSides] = { TEXT("North"), TEXT("East"), TEXT("South"), TEXT("West") };
}

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

	BridgeInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("BridgeInstances"));
	BridgeInstances->SetupAttachment(RootComponent);
	BridgeInstances->SetMobility(EComponentMobility::Static);
	BridgeInstances->SetCollisionProfileName(TEXT("BlockAll"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		BridgeInstances->SetStaticMesh(CubeMesh.Object);
	}

	// Muros de basura: un ISM por forma (subobjetos con nombre estable) y una caja por lado.
	for (int32 Shape = 0; Shape < TNGridJunk::NumShapes; ++Shape)
	{
		const FString MeshPath = FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), WallJunkShapeNames[Shape], WallJunkShapeNames[Shape]);
		ConstructorHelpers::FObjectFinder<UStaticMesh> ShapeMesh(*MeshPath);
		UInstancedStaticMeshComponent* Instances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(
			*FString::Printf(TEXT("WallJunk_%s"), WallJunkShapeNames[Shape]));
		Instances->SetupAttachment(RootComponent);
		Instances->SetMobility(EComponentMobility::Static);
		Instances->NumCustomDataFloats = WallJunkCustomDataFloats;
		Instances->SetCollisionProfileName(TEXT("BlockAll"));
		if (ShapeMesh.Succeeded()) { Instances->SetStaticMesh(ShapeMesh.Object); }
		WallJunk.Add(Instances);
	}
	for (int32 Side = 0; Side < TNGridLogic::NumSides; ++Side)
	{
		UBoxComponent* Blocker = CreateDefaultSubobject<UBoxComponent>(*FString::Printf(TEXT("WallBlocker_%s"), WallSideNames[Side]));
		Blocker->SetupAttachment(RootComponent);
		Blocker->SetMobility(EComponentMobility::Static);
		Blocker->SetCollisionProfileName(TEXT("BlockAll"));
		Blocker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Blocker->SetHiddenInGame(true);
		WallBlockers.Add(Blocker);
	}
}

void ATN_TerrainModuleTile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ATN_TerrainModuleTile, BlockedExits, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ATN_TerrainModuleTile, WallSeed, COND_InitialOnly);
}

void ATN_TerrainModuleTile::InitializeModule(uint8 InBlockedExits, int32 InWallSeed)
{
	BlockedExits = InBlockedExits;
	WallSeed = InWallSeed;
}

void ATN_TerrainModuleTile::BuildWalls()
{
	for (UInstancedStaticMeshComponent* Instances : WallJunk)
	{
		if (Instances) { Instances->ClearInstances(); }
	}

	for (int32 Side = 0; Side < TNGridLogic::NumSides; ++Side)
	{
		const bool bBlocked = (BlockedExits & TNTerrainModule::SideBit(Side)) != 0;
		if (WallBlockers.IsValidIndex(Side) && WallBlockers[Side])
		{
			const TNTerrainModuleWall::FWallBlocker Blocker = TNTerrainModuleWall::BuildWallBlocker(Side, ModuleSize);
			WallBlockers[Side]->SetRelativeLocationAndRotation(Blocker.Center, Blocker.Rotation);
			WallBlockers[Side]->SetBoxExtent(Blocker.Extent);
			WallBlockers[Side]->SetCollisionEnabled(bBlocked ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		}
		if (!bBlocked) { continue; }

		for (const TNTerrainModuleWall::FWallPiece& Piece : TNTerrainModuleWall::BuildWallPieces(Side, ModuleSize, WallSeed))
		{
			const int32 Shape = static_cast<int32>(Piece.Shape);
			if (!WallJunk.IsValidIndex(Shape) || !WallJunk[Shape]) { continue; }
			const int32 InstanceIndex = WallJunk[Shape]->AddInstance(Piece.Transform);
			const float ColorData[WallJunkCustomDataFloats] = { Piece.Color.R, Piece.Color.G, Piece.Color.B };
			WallJunk[Shape]->SetCustomData(InstanceIndex, MakeArrayView(ColorData));
		}
	}

	if (JunkMaterial)
	{
		for (UInstancedStaticMeshComponent* Instances : WallJunk)
		{
			if (Instances) { Instances->SetMaterial(0, JunkMaterial); }
		}
	}
}

void ATN_TerrainModuleTile::BuildArches(const TNTerrainModule::FModuleColors& Colors)
{
	if (!TerrainMesh || !ModuleAsset)
	{
		return;
	}

	UMaterialInterface* ArchMaterial = BridgeMaterial ? BridgeMaterial.Get() : TerrainMaterial.Get();
	for (int32 Index = 0; Index < ModuleAsset->Bridges.Num(); ++Index)
	{
		// Semilla del asset + índice: la roca sale igual en todas las máquinas y en cada build.
		const TNGridTerrain::FTileMesh Arch = TNTerrainModule::BuildArchMesh(
			ModuleAsset->Bridges[Index], ModuleAsset->Seed * 31 + Index, Colors);
		const int32 Section = ArchSectionOffset + Index;
		TerrainMesh->CreateMeshSection_LinearColor(Section, Arch.Vertices, Arch.Triangles, Arch.Normals,
			TArray<FVector2D>(), Arch.Colors, TArray<FProcMeshTangent>(), /*bCreateCollision=*/true);
		if (ArchMaterial)
		{
			TerrainMesh->SetMaterial(Section, ArchMaterial);
		}
	}
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
		if (BridgeInstances) { BridgeInstances->ClearInstances(); }
		BuiltFromAsset.Reset();
		return;
	}

	// Los muros son baratos y dependen de la instancia (bocas tapadas): se rehacen siempre.
	BuildWalls();
	if (BridgeInstances) { BridgeInstances->ClearInstances(); }

	if (BuiltFromAsset.Get() == ModuleAsset && TerrainMesh->GetNumSections() > 0)
	{
		return;
	}

	TNTerrainModule::FModuleColors Colors;
	Colors.WaterLevel = WaterLevel;
	const TNTerrainModule::FModuleField Field = TNTerrainModule::MakeField(*ModuleAsset, ModuleSize);
	const TNGridTerrain::FTileMesh Mesh = TNTerrainModule::BuildModuleMesh(Field, Colors);

	// Sin UV: el material del terreno es triplanar y deriva las coordenadas de la posición.
	TerrainMesh->ClearAllMeshSections();
	TerrainMesh->CreateMeshSection_LinearColor(0, Mesh.Vertices, Mesh.Triangles, Mesh.Normals,
		TArray<FVector2D>(), Mesh.Colors, TArray<FProcMeshTangent>(), /*bCreateCollision=*/true);
	if (TerrainMaterial)
	{
		TerrainMesh->SetMaterial(0, TerrainMaterial);
	}
	BuildArches(Colors);
	BuiltFromAsset = ModuleAsset;

	UE_LOG(LogTortunabo, Verbose, TEXT("[TerrainModule] '%s' construido desde '%s': %d vértices."),
		*GetName(), *ModuleAsset->GetName(), Mesh.Vertices.Num());
}
