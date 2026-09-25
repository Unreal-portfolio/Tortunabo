// Trozos del mapa volumétrico (TN_TerrainMeshDecisions.h): lectura de los binarios TNTM1 (que
// escribe Scripts/terrain_vol/export.py) y TNTM2 (Scripts/terrain_volumes/Variants) y paso a la
// malla del tile.
//   UnrealEditor-Cmd <uproject> -ExecCmds="Automation RunTests Tortunabo.TerrainMesh; Quit" -nullrhi -unattended

#include "Misc/AutomationTest.h"
#include "Misc/Compression.h"
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

	int16 EncodeSnorm16(float Value)
	{
		return static_cast<int16>(FMath::Clamp(FMath::RoundToInt(Value * 32767.f), -32767, 32767));
	}

	/** Inversa independiente de TNTerrainMesh::Detail::DecodeOctahedralNormal, para el
	 * viaje de ida y vuelta: cuantiza N como hace Scripts/terrain_vol/export.py. */
	void EncodeOctahedralNormal(const FVector3f& N, int16& OutX, int16& OutY)
	{
		const float Sum = FMath::Abs(N.X) + FMath::Abs(N.Y) + FMath::Abs(N.Z);
		float PX = N.X / Sum, PY = N.Y / Sum;
		if (N.Z < 0.f)
		{
			const float OldX = PX;
			PX = (1.f - FMath::Abs(PY)) * (OldX >= 0.f ? 1.f : -1.f);
			PY = (1.f - FMath::Abs(OldX)) * (PY >= 0.f ? 1.f : -1.f);
		}
		OutX = EncodeSnorm16(PX);
		OutY = EncodeSnorm16(PY);
	}

	/** Trozo TNTM2 (3 vértices, 1 triángulo, 1 alga) construido a mano con la misma
	 * cuantización y compresión zlib que Scripts/terrain_vol/export.py, para probar el viaje
	 * de ida y vuelta contra TNTerrainMesh::ParseChunk. */
	TArray<uint8> MakeChunkBytesV2()
	{
		constexpr int32 NumVertices = 3, NumTriangles = 1, NumInstances = 1;
		const FVector3f Positions[NumVertices] = { { 0.f, 0.f, 0.f }, { 100.f, 0.f, 0.f }, { 0.f, 100.f, 50.f } };
		const FVector3f Normals[NumVertices] = { { 0.f, 0.f, 1.f }, { 0.6f, 0.f, 0.8f }, { 0.f, 0.f, 1.f } };
		const uint8 Colors[NumVertices][4] = { { 255, 128, 0, 255 }, { 10, 20, 30, 255 }, { 0, 0, 0, 255 } };
		const int32 Indices[NumTriangles * 3] = { 0, 2, 1 };
		const float Origin[3] = { 0.f, 0.f, 0.f };
		const float Step[3] = { 1.f, 1.f, 1.f };

		TArray<uint8> Raw;
		for (const FVector3f& P : Positions)
		{
			AppendRaw(Raw, static_cast<uint16>(FMath::RoundToInt((P.X - Origin[0]) / Step[0])));
			AppendRaw(Raw, static_cast<uint16>(FMath::RoundToInt((P.Y - Origin[1]) / Step[1])));
			AppendRaw(Raw, static_cast<uint16>(FMath::RoundToInt((P.Z - Origin[2]) / Step[2])));
		}
		for (const FVector3f& N : Normals)
		{
			int16 EncodedX = 0, EncodedY = 0;
			EncodeOctahedralNormal(N, EncodedX, EncodedY);
			AppendRaw(Raw, EncodedX);
			AppendRaw(Raw, EncodedY);
		}
		for (const uint8 (&Color)[4] : Colors) { Raw.Append(Color, 4); }
		int32 Previous = 0;
		for (const int32 Index : Indices)
		{
			AppendRaw(Raw, Index - Previous);
			Previous = Index;
		}
		const float Plant[TNTerrainMesh::InstanceFloats] = { 1.f, 10.f, 20.f, 30.f, 90.f, 1.f, 1.f, 4.f, 0.2f, 0.3f, 0.1f };
		for (const float F : Plant) { AppendRaw(Raw, F); }

		const int32 CompressedBound = FCompression::CompressMemoryBound(NAME_Zlib, Raw.Num());
		TArray<uint8> Compressed;
		Compressed.SetNumUninitialized(CompressedBound);
		int32 CompressedSize = CompressedBound;
		const bool bCompressed = FCompression::CompressMemory(NAME_Zlib, Compressed.GetData(), CompressedSize, Raw.GetData(), Raw.Num());
		check(bCompressed);
		Compressed.SetNum(CompressedSize);

		TArray<uint8> Bytes;
		Bytes.Append(reinterpret_cast<const uint8*>("TNTM"), 4);
		AppendRaw(Bytes, TNTerrainMesh::FormatVersion2);
		AppendRaw(Bytes, uint32(NumVertices));
		AppendRaw(Bytes, uint32(NumTriangles));
		AppendRaw(Bytes, uint32(NumInstances));
		for (const float F : Origin) { AppendRaw(Bytes, F); }
		for (const float F : Step) { AppendRaw(Bytes, F); }
		AppendRaw(Bytes, uint32(Raw.Num()));
		AppendRaw(Bytes, uint32(Compressed.Num()));
		Bytes.Append(Compressed);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNTerrainMeshLoadV2Test,
	"Tortunabo.TerrainMesh.LoadV2",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNTerrainMeshLoadV2Test::RunTest(const FString& Parameters)
{
	const TArray<uint8> Bytes = MakeChunkBytesV2();
	TNTerrainMesh::FChunk Chunk;
	FString Error;
	TestTrue(TEXT("trozo TNTM2 válido"), TNTerrainMesh::ParseChunk(Bytes, Chunk, Error));
	TestEqual(TEXT("vértices"), Chunk.Vertices.Num(), 3);
	TestEqual(TEXT("índices reconstruidos del delta"), Chunk.Triangles, TArray<int32>({ 0, 2, 1 }));
	TestTrue(TEXT("posición cuantizada"), Chunk.Vertices[2].Equals(FVector3f(0.f, 100.f, 50.f)));
	TestTrue(TEXT("normal en eje"), Chunk.Normals[0].Equals(FVector3f(0.f, 0.f, 1.f), 1e-3f));
	TestTrue(TEXT("normal oblicua decodificada"), Chunk.Normals[1].Equals(FVector3f(0.6f, 0.f, 0.8f), 1e-3f));
	TestEqual(TEXT("color R"), Chunk.Colors[0].R, uint8(255));
	TestEqual(TEXT("color G"), Chunk.Colors[0].G, uint8(128));
	TestEqual(TEXT("color B"), Chunk.Colors[0].B, uint8(0));
	TestEqual(TEXT("algas"), Chunk.Foliage.Num(), 1);
	TestEqual(TEXT("forma"), Chunk.Foliage[0].Shape, uint8(1));
	TestTrue(TEXT("posición del alga"), Chunk.Foliage[0].Transform.GetLocation().Equals(FVector(10.0, 20.0, 30.0)));

	// Rechazo limpio de trozos corruptos, sin dejar Chunk a medias.
	TArray<uint8> TruncatedZlib = Bytes;
	TruncatedZlib.Pop();
	TestFalse(TEXT("payload zlib truncado"), TNTerrainMesh::ParseChunk(TruncatedZlib, Chunk, Error));

	TArray<uint8> BadRawSize = Bytes;
	uint32 RawSize = 0;
	FMemory::Memcpy(&RawSize, BadRawSize.GetData() + 44, sizeof(RawSize));
	RawSize += 1;
	FMemory::Memcpy(BadRawSize.GetData() + 44, &RawSize, sizeof(RawSize));
	TestFalse(TEXT("raw_size manipulado"), TNTerrainMesh::ParseChunk(BadRawSize, Chunk, Error));

	TArray<uint8> BadVersion = Bytes;
	BadVersion[4] = 9;
	TestFalse(TEXT("versión desconocida"), TNTerrainMesh::ParseChunk(BadVersion, Chunk, Error));

	TArray<uint8> BadMagic = Bytes;
	BadMagic[0] = 'X';
	TestFalse(TEXT("cabecera"), TNTerrainMesh::ParseChunk(BadMagic, Chunk, Error));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
