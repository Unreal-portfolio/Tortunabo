#include "World/TN_TerrainMeshTile.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Core/TN_Log.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "World/TN_TerrainMeshAsset.h"
#include "World/TN_TerrainMeshDecisions.h"

namespace
{
	/** Malla del motor por forma de alga, en el orden de TNTerrainBiome::EFoliageShape. */
	const TCHAR* const MeshTileFoliageMeshes[TNTerrainMesh::NumFoliageShapes] = { TEXT("Cylinder"), TEXT("Cone"), TEXT("Sphere") };
	const TCHAR* const MeshTileFoliageNames[TNTerrainMesh::NumFoliageShapes] = { TEXT("Stalk"), TEXT("Frond"), TEXT("Bush") };
	constexpr int32 MeshTileFoliageCustomData = 3;
	constexpr int32 MeshTileFoliageCullStart = 6000;
	constexpr int32 MeshTileFoliageCullEnd = 15000;
}

ATN_TerrainMeshTile::ATN_TerrainMeshTile()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	TerrainMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("TerrainMesh"));
	RootComponent = TerrainMesh;
	// Cocinado síncrono: al empezar la partida la colisión ya está.
	TerrainMesh->bUseAsyncCooking = false;
	TerrainMesh->SetCollisionProfileName(TEXT("BlockAll"));
	TerrainMesh->SetMobility(EComponentMobility::Static);

	for (int32 Shape = 0; Shape < TNTerrainMesh::NumFoliageShapes; ++Shape)
	{
		const FString MeshPath = FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), MeshTileFoliageMeshes[Shape], MeshTileFoliageMeshes[Shape]);
		ConstructorHelpers::FObjectFinder<UStaticMesh> ShapeMesh(*MeshPath);
		UInstancedStaticMeshComponent* Instances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(
			*FString::Printf(TEXT("Foliage_%s"), MeshTileFoliageNames[Shape]));
		Instances->SetupAttachment(RootComponent);
		Instances->SetMobility(EComponentMobility::Static);
		Instances->NumCustomDataFloats = MeshTileFoliageCustomData;
		Instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Instances->SetCastShadow(false);
		Instances->InstanceStartCullDistance = MeshTileFoliageCullStart;
		Instances->InstanceEndCullDistance = MeshTileFoliageCullEnd;
		if (ShapeMesh.Succeeded()) { Instances->SetStaticMesh(ShapeMesh.Object); }
		Foliage.Add(Instances);
	}
}

void ATN_TerrainMeshTile::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	BuildTile();
}

void ATN_TerrainMeshTile::BeginPlay()
{
	Super::BeginPlay();
	BuildTile();
}

void ATN_TerrainMeshTile::BuildTile()
{
	if (!TerrainMesh) { return; }
	if (!MeshAsset || !MeshAsset->IsValidMesh())
	{
		TerrainMesh->ClearAllMeshSections();
		for (UInstancedStaticMeshComponent* Instances : Foliage)
		{
			if (Instances) { Instances->ClearInstances(); }
		}
		BuiltFromAsset.Reset();
		return;
	}
	if (BuiltFromAsset.Get() == MeshAsset && TerrainMesh->GetNumSections() > 0) { return; }

	const TNGridTerrain::FTileMesh Mesh = TNTerrainMesh::ToTileMesh(MeshAsset->Vertices, MeshAsset->Normals,
		MeshAsset->Colors, MeshAsset->Triangles);
	TerrainMesh->ClearAllMeshSections();
	TerrainMesh->CreateMeshSection_LinearColor(0, Mesh.Vertices, Mesh.Triangles, Mesh.Normals, TArray<FVector2D>(),
		Mesh.Colors, TArray<FProcMeshTangent>(), /*bCreateCollision=*/true);
	if (TerrainMaterial) { TerrainMesh->SetMaterial(0, TerrainMaterial); }
	BuildFoliage();
	BuiltFromAsset = MeshAsset;

	UE_LOG(LogTortunabo, Verbose, TEXT("[TerrainMesh] '%s' construido desde '%s': %d vértices, %d algas."),
		*GetName(), *MeshAsset->GetName(), Mesh.Vertices.Num(), MeshAsset->Foliage.Num());
}

void ATN_TerrainMeshTile::BuildFoliage()
{
	for (UInstancedStaticMeshComponent* Instances : Foliage)
	{
		if (Instances) { Instances->ClearInstances(); }
	}
	for (const FTNTerrainMeshFoliage& Plant : MeshAsset->Foliage)
	{
		if (!Foliage.IsValidIndex(Plant.Shape) || !Foliage[Plant.Shape]) { continue; }
		UInstancedStaticMeshComponent* Instances = Foliage[Plant.Shape];
		const int32 Index = Instances->AddInstance(Plant.Transform);
		const float ColorData[MeshTileFoliageCustomData] = { Plant.Color.R, Plant.Color.G, Plant.Color.B };
		Instances->SetCustomData(Index, MakeArrayView(ColorData));
	}
	if (FoliageMaterial)
	{
		for (UInstancedStaticMeshComponent* Instances : Foliage)
		{
			if (Instances) { Instances->SetMaterial(0, FoliageMaterial); }
		}
	}
}
