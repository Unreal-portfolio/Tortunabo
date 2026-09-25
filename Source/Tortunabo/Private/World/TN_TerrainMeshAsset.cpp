#include "World/TN_TerrainMeshAsset.h"
#include "Core/TN_Log.h"
#include "Misc/FileHelper.h"
#include "World/TN_TerrainMeshDecisions.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "PhysicsEngine/BodySetup.h"
#include "StaticMeshAttributes.h"
#include "UObject/Package.h"
#endif

bool UTN_TerrainMeshAsset::LoadFromFile(const FString& Path)
{
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *Path))
	{
		UE_LOG(LogTortunabo, Error, TEXT("[TerrainMesh] '%s': no se puede leer '%s'."), *GetName(), *Path);
		return false;
	}

	TNTerrainMesh::FChunk Chunk;
	FString Error;
	if (!TNTerrainMesh::ParseChunk(Bytes, Chunk, Error))
	{
		UE_LOG(LogTortunabo, Error, TEXT("[TerrainMesh] '%s': '%s' no es válido: %s."), *GetName(), *Path, *Error);
		return false;
	}

	Vertices = MoveTemp(Chunk.Vertices);
	Normals = MoveTemp(Chunk.Normals);
	Colors = MoveTemp(Chunk.Colors);
	Triangles = MoveTemp(Chunk.Triangles);
	Foliage.Reset(Chunk.Foliage.Num());
	for (const TNTerrainMesh::FFoliage& Plant : Chunk.Foliage)
	{
		FTNTerrainMeshFoliage& Entry = Foliage.AddDefaulted_GetRef();
		Entry.Shape = Plant.Shape;
		Entry.Transform = Plant.Transform;
		Entry.Color = Plant.Color;
	}
	MarkPackageDirty();
	return true;
}

#if WITH_EDITOR
UStaticMesh* UTN_TerrainMeshAsset::BuildStaticMesh(const FString& PackagePath, const FString& AssetName, UMaterialInterface* Material)
{
	if (!IsValidMesh())
	{
		UE_LOG(LogTortunabo, Error, TEXT("[TerrainMesh] '%s': sin malla valida para el StaticMesh."), *GetName());
		return nullptr;
	}

	UPackage* Package = CreatePackage(*(PackagePath / AssetName));
	UStaticMesh* Mesh = FindObject<UStaticMesh>(Package, *AssetName);
	const bool bCreated = Mesh == nullptr;
	if (bCreated)
	{
		Mesh = NewObject<UStaticMesh>(Package, *AssetName, RF_Public | RF_Standalone);
	}

	FMeshDescription Description;
	FStaticMeshAttributes Attributes(Description);
	Attributes.Register();
	TVertexAttributesRef<FVector3f> PositionAttr = Attributes.GetVertexPositions();
	TVertexInstanceAttributesRef<FVector3f> NormalAttr = Attributes.GetVertexInstanceNormals();
	TVertexInstanceAttributesRef<FVector4f> ColorAttr = Attributes.GetVertexInstanceColors();
	TVertexInstanceAttributesRef<FVector2f> UVAttr = Attributes.GetVertexInstanceUVs();
	UVAttr.SetNumChannels(1);

	const FName SlotName(TEXT("Terrain"));
	const FPolygonGroupID Group = Description.CreatePolygonGroup();
	Attributes.GetPolygonGroupMaterialSlotNames()[Group] = SlotName;

	// Un vertice y una instancia por vertice del trozo: la malla queda soldada (editable).
	const int32 Count = Vertices.Num();
	Description.ReserveNewVertices(Count);
	Description.ReserveNewVertexInstances(Count);
	Description.ReserveNewTriangles(Triangles.Num() / 3);
	TArray<FVertexInstanceID> Instances;
	Instances.Reserve(Count);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FVertexID Vertex = Description.CreateVertex();
		PositionAttr[Vertex] = Vertices[Index];
		const FVertexInstanceID Instance = Description.CreateVertexInstance(Vertex);
		NormalAttr[Instance] = Normals[Index];
		const FColor& C = Colors[Index];
		// Mismos bytes que la malla procedural (que no convierte a sRGB): el build del StaticMesh
		// pasa el color a sRGB, asi que se le da el color cuyo sRGB son esos bytes.
		const FLinearColor Linear = FLinearColor::FromSRGBColor(FColor(C.R, C.G, C.B, 255));
		ColorAttr[Instance] = FVector4f(Linear.R, Linear.G, Linear.B, 1.f);
		// UV plana (1 unidad = 10 m): la usa el lightmap; el material es triplanar.
		UVAttr.Set(Instance, 0, FVector2f(Vertices[Index].X / 1000.f, Vertices[Index].Y / 1000.f));
		Instances.Add(Instance);
	}
	for (int32 T = 0; T + 2 < Triangles.Num(); T += 3)
	{
		Description.CreateTriangle(Group, { Instances[Triangles[T]], Instances[Triangles[T + 1]], Instances[Triangles[T + 2]] });
	}

	Mesh->SetNumSourceModels(1);
	FStaticMeshSourceModel& Source = Mesh->GetSourceModel(0);
	Source.BuildSettings.bRecomputeNormals = false;
	Source.BuildSettings.bRecomputeTangents = true;
	Source.BuildSettings.bGenerateLightmapUVs = false;
	Source.BuildSettings.bRemoveDegenerates = true;
	// Un unico LOD, sin reduccion (no hay generador de LODs automatico que pueda desajustarse
	// con la malla de sombras): la malla completa del trozo es la que se ve y la que sombrea.
	Source.ReductionSettings.PercentTriangles = 1.f;
	Mesh->GetStaticMaterials().Reset();
	Mesh->GetStaticMaterials().Add(FStaticMaterial(Material, SlotName, SlotName));
	Mesh->CreateMeshDescription(0, MoveTemp(Description));
	Mesh->CommitMeshDescription(0);

	// Causa probable de los parches de sombra en tablero y las facetas grandes en las dunas
	// (2026-09-25): sin Nanite, Lumen ilumina y sombrea estos StaticMesh (decenas de miles de
	// triangulos por trozo) con su Distance Field por objeto, cuyo volumen se genera a una
	// resolucion baja para una malla de este tamano; eso produce el aspecto voxelizado/a
	// cuadros en sombras y AO, y acentua las facetas del marching cubes en las dunas. Nanite
	// sustituye esa representacion por su propia jerarquia de detalle (Lumen usa Nanite Mesh
	// Cards), que sigue el triangulo real de la malla: se activa aqui, con reduccion cero del
	// propio Nanite (KeepPercentTriangles = 1) para no perder detalle, y un mesh de reserva
	// para plataformas o vistas sin soporte Nanite (sombras de RT, algunos HLOD).
	Mesh->NaniteSettings.bEnabled = true;
	Mesh->NaniteSettings.KeepPercentTriangles = 1.f;
	Mesh->NaniteSettings.FallbackPercentTriangles = 0.1f;

	Mesh->Build(/*bInSilent=*/true);

	// Colision de la propia malla (tuneles y voladizos incluidos), no una caja.
	Mesh->CreateBodySetup();
	if (UBodySetup* Body = Mesh->GetBodySetup())
	{
		Body->CollisionTraceFlag = CTF_UseComplexAsSimple;
		Body->InvalidatePhysicsData();
		Body->CreatePhysicsMeshes();
	}
	Mesh->MarkPackageDirty();
	if (bCreated)
	{
		FAssetRegistryModule::AssetCreated(Mesh);
	}
	return Mesh;
}
#endif
