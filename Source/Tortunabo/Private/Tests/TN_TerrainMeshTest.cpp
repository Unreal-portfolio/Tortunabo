// Trozos del mapa volumétrico (TN_TerrainMeshDecisions.h): lectura del binario TNTM1 que
// escribe Scripts/terrain_vol/export.py y paso a la malla del tile.
//   UnrealEditor-Cmd <uproject> -ExecCmds="Automation RunTests Tortunabo.TerrainMesh; Quit" -nullrhi -unattended

#include "Misc/AutomationTest.h"
#include "World/TN_TerrainMeshDecisions.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	template <typename T>
	void AppendRaw(TArray<uint8>& Bytes, const T& Value)
	{
		const uint8* Raw = reinterpret_cast<const uint8*>(&Value);
		Bytes.Append(Raw, sizeof(T));
	}

	/** Un triángulo (3 vértices) y una alga, en el mismo formato que export.py. */
	TArray<uint8> MakeChunkBytes(uint32 BadIndex = 0, float Shape = 1.f)
	{
		TArray<uint8> Bytes;
		Bytes.Append(reinterpret_cast<const uint8*>("TNTM"), 4);
		AppendRaw(Bytes, TNTerrainMesh::FormatVersion);
		AppendRaw(Bytes, uint32(3));
		AppendRaw(Bytes, uint32(1));
		AppendRaw(Bytes, uint32(1));
		const float Positions[9] = { 0.f, 0.f, 0.f, 100.f, 0.f, 0.f, 0.f, 100.f, 50.f };
		const float Normals[9] = { 0.f, 0.f, 1.f, 0.f, 0.f, 1.f, 0.f, 0.f, 1.f };
		for (const float F : Positions) { AppendRaw(Bytes, F); }
		for (const float F : Normals) { AppendRaw(Bytes, F); }
		const uint8 Colors[12] = { 255, 128, 0, 255, 10, 20, 30, 255, 0, 0, 0, 255 };
		Bytes.Append(Colors, 12);
		const uint32 Indices[3] = { 0, BadIndex ? BadIndex : 2u, 1 };
		for (const uint32 I : Indices) { AppendRaw(Bytes, I); }
		const float Plant[TNTerrainMesh::InstanceFloats] = { Shape, 10.f, 20.f, 30.f, 90.f, 1.f, 1.f, 4.f, 0.2f, 0.3f, 0.1f };
		for (const float F : Plant) { AppendRaw(Bytes, F); }
		return Bytes;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNTerrainMeshLoadTest,
	"Tortunabo.TerrainMesh.Load",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNTerrainMeshLoadTest::RunTest(const FString& Parameters)
{
	TNTerrainMesh::FChunk Chunk;
	FString Error;
	TestTrue(TEXT("trozo válido"), TNTerrainMesh::ParseChunk(MakeChunkBytes(), Chunk, Error));
	TestEqual(TEXT("vértices"), Chunk.Vertices.Num(), 3);
	TestEqual(TEXT("índices"), Chunk.Triangles, TArray<int32>({ 0, 2, 1 }));
	TestTrue(TEXT("posición"), Chunk.Vertices[2].Equals(FVector3f(0.f, 100.f, 50.f)));
	TestEqual(TEXT("color R"), Chunk.Colors[0].R, uint8(255));
	TestEqual(TEXT("color G"), Chunk.Colors[0].G, uint8(128));
	TestEqual(TEXT("color B"), Chunk.Colors[0].B, uint8(0));
	TestEqual(TEXT("algas"), Chunk.Foliage.Num(), 1);
	TestEqual(TEXT("forma"), Chunk.Foliage[0].Shape, uint8(1));
	TestTrue(TEXT("posición del alga"), Chunk.Foliage[0].Transform.GetLocation().Equals(FVector(10.0, 20.0, 30.0)));
	TestTrue(TEXT("escala del alga"), Chunk.Foliage[0].Transform.GetScale3D().Equals(FVector(1.0, 1.0, 4.0)));

	TArray<uint8> Truncated = MakeChunkBytes();
	Truncated.Pop();
	TestFalse(TEXT("truncado"), TNTerrainMesh::ParseChunk(Truncated, Chunk, Error));
	TArray<uint8> BadMagic = MakeChunkBytes();
	BadMagic[0] = 'X';
	TestFalse(TEXT("cabecera"), TNTerrainMesh::ParseChunk(BadMagic, Chunk, Error));
	TestFalse(TEXT("índice fuera de rango"), TNTerrainMesh::ParseChunk(MakeChunkBytes(7), Chunk, Error));
	TestFalse(TEXT("forma desconocida"), TNTerrainMesh::ParseChunk(MakeChunkBytes(0, 5.f), Chunk, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNTerrainMeshBuildTest,
	"Tortunabo.TerrainMesh.Build",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNTerrainMeshBuildTest::RunTest(const FString& Parameters)
{
	TNTerrainMesh::FChunk Chunk;
	FString Error;
	TNTerrainMesh::ParseChunk(MakeChunkBytes(), Chunk, Error);
	const TNGridTerrain::FTileMesh Mesh = TNTerrainMesh::ToTileMesh(Chunk.Vertices, Chunk.Normals, Chunk.Colors, Chunk.Triangles);
	TestEqual(TEXT("vértices"), Mesh.Vertices.Num(), 3);
	TestEqual(TEXT("normales"), Mesh.Normals.Num(), 3);
	TestEqual(TEXT("triángulos"), Mesh.Triangles.Num(), 3);
	// El color es lineal: byte / 255, sin conversión sRGB.
	TestTrue(TEXT("color lineal"), Mesh.Colors[0].Equals(FLinearColor(1.f, 128.f / 255.f, 0.f, 1.f), 1e-4f));

	// Arrays que no casan: malla vacía, nunca a medias.
	TArray<FVector3f> Short = Chunk.Normals;
	Short.Pop();
	TestEqual(TEXT("normales de menos"), TNTerrainMesh::ToTileMesh(Chunk.Vertices, Short, Chunk.Colors, Chunk.Triangles).Vertices.Num(), 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
