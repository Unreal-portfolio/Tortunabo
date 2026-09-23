#include "World/TN_TerrainModuleTile.h"
#include "World/TN_TerrainModuleAsset.h"
#include "World/TN_TerrainBiomeDecisions.h"
#include "World/TN_TerrainCoastDecisions.h"
#include "World/TN_TerrainTunnelDecisions.h"
#include "World/TN_TerrainModuleDecisions.h"
#include "World/TN_TerrainModuleWallDecisions.h"
#include "Core/TN_Log.h"
#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "World/TN_TerrainSeamDecisions.h"
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

	/** Malla del motor por forma de alga, en el orden de TNTerrainBiome::EFoliageShape. */
	const TCHAR* const FoliageShapeMeshes[TNTerrainBiome::NumFoliageShapes] = { TEXT("Cylinder"), TEXT("Cone"), TEXT("Sphere") };
	const TCHAR* const FoliageShapeNames[TNTerrainBiome::NumFoliageShapes] = { TEXT("Stalk"), TEXT("Frond"), TEXT("Bush") };
	constexpr int32 FoliageCustomDataFloats = 3;
	constexpr int32 FoliageCullStart = 6000;
	constexpr int32 FoliageCullEnd = 15000;
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

		UBoxComponent* Outer = CreateDefaultSubobject<UBoxComponent>(*FString::Printf(TEXT("OuterBlocker_%s"), WallSideNames[Side]));
		Outer->SetupAttachment(RootComponent);
		Outer->SetMobility(EComponentMobility::Static);
		Outer->SetCollisionProfileName(TEXT("BlockAll"));
		Outer->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Outer->SetHiddenInGame(true);
		OuterBlockers.Add(Outer);
	}

	// Bosque de algas: un ISM por forma, sin colisión ni sombra dinámica (cientos de instancias).
	for (int32 Shape = 0; Shape < TNTerrainBiome::NumFoliageShapes; ++Shape)
	{
		const FString MeshPath = FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), FoliageShapeMeshes[Shape], FoliageShapeMeshes[Shape]);
		ConstructorHelpers::FObjectFinder<UStaticMesh> ShapeMesh(*MeshPath);
		UInstancedStaticMeshComponent* Instances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(
			*FString::Printf(TEXT("Foliage_%s"), FoliageShapeNames[Shape]));
		Instances->SetupAttachment(RootComponent);
		Instances->SetMobility(EComponentMobility::Static);
		Instances->NumCustomDataFloats = FoliageCustomDataFloats;
		Instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Instances->SetCastShadow(false);
		// Hasta ~3000 instancias por módulo: se apagan con la distancia.
		Instances->InstanceStartCullDistance = FoliageCullStart;
		Instances->InstanceEndCullDistance = FoliageCullEnd;
		if (ShapeMesh.Succeeded()) { Instances->SetStaticMesh(ShapeMesh.Object); }
		Foliage.Add(Instances);
	}
}

void ATN_TerrainModuleTile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ATN_TerrainModuleTile, BlockedExits, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ATN_TerrainModuleTile, WallSeed, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ATN_TerrainModuleTile, bMirrored, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ATN_TerrainModuleTile, OuterSides, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ATN_TerrainModuleTile, SeamNeighbors, COND_InitialOnly);
}

void ATN_TerrainModuleTile::InitializeModule(uint8 InBlockedExits, int32 InWallSeed, bool bInMirrored, uint8 InOuterSides)
{
	bMirrored = bInMirrored;
	OuterSides = InOuterSides;
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
		if (OuterBlockers.IsValidIndex(Side) && OuterBlockers[Side])
		{
			const bool bOuter = (OuterSides & TNTerrainModule::SideBit(Side)) != 0;
			const TNTerrainModuleWall::FWallBlocker Outer = TNTerrainCoast::BuildOuterBlocker(Side, ModuleSize);
			OuterBlockers[Side]->SetRelativeLocationAndRotation(Outer.Center, Outer.Rotation);
			OuterBlockers[Side]->SetBoxExtent(Outer.Extent);
			OuterBlockers[Side]->SetCollisionEnabled(bOuter ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		}
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

void ATN_TerrainModuleTile::BuildRocks(const TNTerrainModule::FModuleColors& Colors, const TNTerrainModule::FModuleField& Field)
{
	int32 Section = ArchSectionOffset;
	if (!TerrainMesh || !ModuleAsset)
	{
		return;
	}

	UMaterialInterface* RockMaterial = BridgeMaterial ? BridgeMaterial.Get() : TerrainMaterial.Get();
	auto AddSection = [&](const TNGridTerrain::FTileMesh& Rock)
	{
		if (Rock.Vertices.Num() == 0) { return; }   // roca degenerada editada a mano
		TerrainMesh->CreateMeshSection_LinearColor(Section, Rock.Vertices, Rock.Triangles, Rock.Normals,
			TArray<FVector2D>(), Rock.Colors, TArray<FProcMeshTangent>(), /*bCreateCollision=*/true);
		if (RockMaterial) { TerrainMesh->SetMaterial(Section, RockMaterial); }
		++Section;
	};

	// Semilla del asset + índice: la roca sale igual en todas las máquinas y en cada build.
	for (int32 Index = 0; Index < ModuleAsset->Bridges.Num(); ++Index)
	{
		const FTNTerrainModuleBridge Bridge = bMirrored ? TNTerrainModule::MirrorBridge(ModuleAsset->Bridges[Index]) : ModuleAsset->Bridges[Index];
		const int32 RockSeed = ModuleAsset->Seed * 31 + Index;
		if (Bridge.Kind == ETNTerrainArchKind::Tunnel)
		{
			// Cueva: bóveda que llega al suelo del pasillo y, por fuera, una colina que copia
			// el terreno de alrededor. Con el material del terreno, no el de roca: es relieve.
			const double Floor = TNTerrainModule::SampleHeight(Field, FVector2D(Bridge.Center));
			const TNGridTerrain::FTileMesh Cave = TNTerrainTunnel::BuildCaveMesh(Bridge, Floor, RockSeed, Colors,
				[&Field](const FVector2D& P) { return TNTerrainModule::SampleHeight(Field, P); });
			if (Cave.Vertices.Num() > 0)
			{
				TerrainMesh->CreateMeshSection_LinearColor(Section, Cave.Vertices, Cave.Triangles, Cave.Normals,
					TArray<FVector2D>(), Cave.Colors, TArray<FProcMeshTangent>(), /*bCreateCollision=*/true);
				if (TerrainMaterial) { TerrainMesh->SetMaterial(Section, TerrainMaterial); }
				++Section;
			}
		}
		else
		{
			AddSection(TNTerrainModule::BuildArchMesh(Bridge, RockSeed, Colors));
		}
	}
	for (int32 Index = 0; Index < ModuleAsset->Monoliths.Num(); ++Index)
	{
		const FTNTerrainModuleMonolith& Monolith = ModuleAsset->Monoliths[Index];
		AddSection(TNTerrainModule::BuildMonolithMesh(bMirrored ? TNTerrainModule::MirrorMonolith(Monolith) : Monolith,
			ModuleAsset->Seed * 53 + Index, Colors));
	}
}

void ATN_TerrainModuleTile::BuildFoliage(const TNTerrainModule::FModuleField& Field)
{
	for (UInstancedStaticMeshComponent* Instances : Foliage)
	{
		if (Instances) { Instances->ClearInstances(); }
	}
	if (!ModuleAsset || !ModuleAsset->HasBiomeMask())
	{
		return;
	}

	for (const TNTerrainBiome::FFoliageInstance& Plant : TNTerrainBiome::BuildFoliage(Field, ModuleAsset->BiomeMask, ModuleAsset->Seed))
	{
		const int32 Shape = static_cast<int32>(Plant.Shape);
		if (!Foliage.IsValidIndex(Shape) || !Foliage[Shape]) { continue; }
		const int32 InstanceIndex = Foliage[Shape]->AddInstance(Plant.Transform);
		const float ColorData[FoliageCustomDataFloats] = { Plant.Color.R, Plant.Color.G, Plant.Color.B };
		Foliage[Shape]->SetCustomData(InstanceIndex, MakeArrayView(ColorData));
	}

	UMaterialInterface* Material = FoliageMaterial ? FoliageMaterial.Get() : JunkMaterial.Get();
	if (Material)
	{
		for (UInstancedStaticMeshComponent* Instances : Foliage)
		{
			if (Instances) { Instances->SetMaterial(0, Material); }
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

TNGridTerrain::FTileMesh ATN_TerrainModuleTile::BuildSeamedMesh(const TNTerrainModule::FModuleField& Field,
	const TNTerrainModule::FModuleColors& Colors, const TNTerrainModule::FModuleColors& BlendColors) const
{
	// Las alturas de cada vecino (con su costa) viven aquí mientras se construye la malla:
	// FSeamCell solo guarda vistas sobre ellas.
	TArray<TArray<uint16>> NeighborHeights;
	NeighborHeights.Reserve(SeamNeighbors.Num());
	TArray<TNTerrainSeam::FSeamCell> Cells;
	Cells.Reserve(SeamNeighbors.Num() + 1);

	TNTerrainSeam::FSeamCell& Self = Cells.AddDefaulted_GetRef();
	Self.Field = Field;
	Self.Colors = Colors;
	Self.BlendColors = BlendColors;
	Self.BiomeMask = ModuleAsset->BiomeMask;
	Self.bMixed = ModuleAsset->IsMixed();

	for (const FTNSeamNeighbor& Neighbor : SeamNeighbors)
	{
		if (!Neighbor.Asset || !Neighbor.Asset->IsValidModule()) { continue; }
		const TArray<uint16>& Heights = NeighborHeights.Add_GetRef(
			TNTerrainCoast::ApplyCoast(*Neighbor.Asset, ModuleSize, Neighbor.bMirrored, Neighbor.OuterSides, Neighbor.Seed));
		TNTerrainSeam::FSeamCell& Cell = Cells.AddDefaulted_GetRef();
		Cell.Field = TNTerrainModule::MakeField(*Neighbor.Asset, ModuleSize, Neighbor.bMirrored);
		Cell.Field.Heights = Heights;
		Cell.Center = FVector2D(Neighbor.LocalX, Neighbor.LocalY) * ModuleSize;
		Cell.RelYawSteps = Neighbor.RelYawSteps;
		Cell.Colors = TNTerrainBiome::ColorsFor(Neighbor.Asset->Biome, WaterLevel);
		Cell.BlendColors = TNTerrainBiome::ColorsFor(Neighbor.Asset->SecondaryBiome, WaterLevel);
		Cell.BiomeMask = Neighbor.Asset->BiomeMask;
		Cell.bMixed = Neighbor.Asset->IsMixed();
	}

	TNTerrainSeam::FSeamSettings Settings;
	Settings.Band = SeamBand;
	return TNTerrainSeam::BuildFusedMesh(Cells, Settings);
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
		for (UInstancedStaticMeshComponent* Instances : Foliage)
		{
			if (Instances) { Instances->ClearInstances(); }
		}
		BuiltFromAsset.Reset();
		return;
	}

	// Los muros son baratos y dependen de la instancia (bocas tapadas): se rehacen siempre.
	BuildWalls();
	if (BridgeInstances) { BridgeInstances->ClearInstances(); }

	if (BuiltFromAsset.Get() == ModuleAsset && bBuiltMirrored == bMirrored && BuiltOuterSides == OuterSides
		&& BuiltSeamNeighbors == SeamNeighbors && TerrainMesh->GetNumSections() > 0)
	{
		return;
	}

	// Paleta del bioma; un módulo mixto funde hacia la del secundario según su máscara.
	const TNTerrainModule::FModuleColors Colors = TNTerrainBiome::ColorsFor(ModuleAsset->Biome, WaterLevel);
	const TNTerrainModule::FModuleColors BlendColors = TNTerrainBiome::ColorsFor(ModuleAsset->SecondaryBiome, WaterLevel);
	// Costa exterior: los lados que dan fuera del mapa se hunden bajo el agua.
	PlacedHeights = TNTerrainCoast::ApplyCoast(*ModuleAsset, ModuleSize, bMirrored, OuterSides, WallSeed);
	TNTerrainModule::FModuleField Field = TNTerrainModule::MakeField(*ModuleAsset, ModuleSize, bMirrored);
	Field.Heights = PlacedHeights;
	const TNGridTerrain::FTileMesh Mesh = SeamNeighbors.Num() > 0
		? BuildSeamedMesh(Field, Colors, BlendColors)
		: TNTerrainModule::BuildModuleMesh(Field, Colors, ModuleAsset->IsMixed() ? &BlendColors : nullptr, ModuleAsset->BiomeMask);

	// Sin UV: el material del terreno es triplanar y deriva las coordenadas de la posición.
	TerrainMesh->ClearAllMeshSections();
	TerrainMesh->CreateMeshSection_LinearColor(0, Mesh.Vertices, Mesh.Triangles, Mesh.Normals,
		TArray<FVector2D>(), Mesh.Colors, TArray<FProcMeshTangent>(), /*bCreateCollision=*/true);
	if (TerrainMaterial)
	{
		TerrainMesh->SetMaterial(0, TerrainMaterial);
	}
	BuildRocks(Colors, Field);
	BuildFoliage(Field);
	BuiltFromAsset = ModuleAsset;
	bBuiltMirrored = bMirrored;
	BuiltOuterSides = OuterSides;
	BuiltSeamNeighbors = SeamNeighbors;

	UE_LOG(LogTortunabo, Verbose, TEXT("[TerrainModule] '%s' construido desde '%s': %d vértices."),
		*GetName(), *ModuleAsset->GetName(), Mesh.Vertices.Num());
}
