#include "World/TN_TerrainMeshAsset.h"
#include "Core/TN_Log.h"
#include "Misc/FileHelper.h"
#include "World/TN_TerrainMeshDecisions.h"

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
