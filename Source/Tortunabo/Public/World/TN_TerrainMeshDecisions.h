#pragma once

#include "CoreMinimal.h"
#include "World/TN_GridTerrainDecisions.h"

/**
 * Trozo del mapa volumétrico como función PURA: lectura del binario que escribe
 * Scripts/terrain_vol/export.py (formato TNTM1) y paso a la malla del tile.
 *
 * Formato (little endian):
 *   char[4] 'TNTM', uint32 versión (1), uint32 vértices N, triángulos M, instancias K,
 *   float32 N*3 posiciones (uu, locales al centro del trozo), float32 N*3 normales,
 *   uint8 N*4 color RGBA (color LINEAL * 255), uint32 M*3 índices (cara visible de Unreal),
 *   float32 K*11 instancias de algas: forma, x, y, z, yaw, sx, sy, sz, r, g, b.
 */
namespace TNTerrainMesh
{
	constexpr uint32 FormatVersion = 1;
	constexpr int32 HeaderBytes = 20;
	constexpr int32 InstanceFloats = 11;
	constexpr int32 NumFoliageShapes = 3;

	struct FFoliage
	{
		uint8 Shape = 0;
		FTransform Transform;
		FLinearColor Color = FLinearColor::Green;
	};

	struct FChunk
	{
		TArray<FVector3f> Vertices;
		TArray<FVector3f> Normals;
		TArray<FColor> Colors;
		TArray<int32> Triangles;
		TArray<FFoliage> Foliage;
	};

	namespace Detail
	{
		template <typename T>
		void ReadArray(TArrayView<const uint8> Bytes, int64& Offset, int32 Count, TArray<T>& Out)
		{
			Out.SetNumUninitialized(Count);
			if (Count > 0) { FMemory::Memcpy(Out.GetData(), Bytes.GetData() + Offset, sizeof(T) * Count); }
			Offset += static_cast<int64>(sizeof(T)) * Count;
		}
	}

	/** Lee un trozo. false (y Error) si la cabecera, los tamaños o los índices no cuadran. */
	inline bool ParseChunk(TArrayView<const uint8> Bytes, FChunk& Out, FString& Error)
	{
		if (Bytes.Num() < HeaderBytes || FMemory::Memcmp(Bytes.GetData(), "TNTM", 4) != 0)
		{
			Error = TEXT("no es un trozo TNTM");
			return false;
		}
		uint32 Header[4];
		FMemory::Memcpy(Header, Bytes.GetData() + 4, sizeof(Header));
		const uint32 Version = Header[0];
		const int64 NumVertices = Header[1], NumTriangles = Header[2], NumInstances = Header[3];
		if (Version != FormatVersion)
		{
			Error = FString::Printf(TEXT("versión %u, se esperaba %u"), Version, FormatVersion);
			return false;
		}
		const int64 Expected = HeaderBytes + NumVertices * (12 + 12 + 4) + NumTriangles * 12 + NumInstances * InstanceFloats * 4;
		if (Expected != Bytes.Num() || NumVertices > MAX_int32 / 3 || NumTriangles > MAX_int32 / 3)
		{
			Error = FString::Printf(TEXT("tamaño %d, se esperaban %lld bytes"), Bytes.Num(), Expected);
			return false;
		}

		FChunk Chunk;
		int64 Offset = HeaderBytes;
		Detail::ReadArray(Bytes, Offset, static_cast<int32>(NumVertices), Chunk.Vertices);
		Detail::ReadArray(Bytes, Offset, static_cast<int32>(NumVertices), Chunk.Normals);
		Detail::ReadArray(Bytes, Offset, static_cast<int32>(NumVertices), Chunk.Colors);   // RGBA en orden de bytes
		TArray<uint32> Indices;
		Detail::ReadArray(Bytes, Offset, static_cast<int32>(NumTriangles * 3), Indices);
		TArray<float> InstanceData;
		Detail::ReadArray(Bytes, Offset, static_cast<int32>(NumInstances * InstanceFloats), InstanceData);

		Chunk.Triangles.Reserve(Indices.Num());
		for (const uint32 Index : Indices)
		{
			if (Index >= NumVertices)
			{
				Error = FString::Printf(TEXT("índice %u fuera de %lld vértices"), Index, NumVertices);
				return false;
			}
			Chunk.Triangles.Add(static_cast<int32>(Index));
		}
		// FColor guarda los bytes como B, G, R, A: el binario viene en R, G, B, A.
		for (FColor& Color : Chunk.Colors) { Swap(Color.R, Color.B); }

		Chunk.Foliage.Reserve(static_cast<int32>(NumInstances));
		for (int64 I = 0; I < NumInstances; ++I)
		{
			const float* F = InstanceData.GetData() + I * InstanceFloats;
			const int32 Shape = FMath::RoundToInt32(F[0]);
			if (Shape < 0 || Shape >= NumFoliageShapes)
			{
				Error = FString::Printf(TEXT("forma de alga %d desconocida"), Shape);
				return false;
			}
			FFoliage& Plant = Chunk.Foliage.AddDefaulted_GetRef();
			Plant.Shape = static_cast<uint8>(Shape);
			Plant.Transform = FTransform(FRotator(0.0, F[4], 0.0), FVector(F[1], F[2], F[3]), FVector(F[5], F[6], F[7]));
			Plant.Color = FLinearColor(F[8], F[9], F[10]);
		}
		Out = MoveTemp(Chunk);
		return true;
	}

	/** Malla del tile: los colores del binario son LINEALES (byte / 255), no sRGB. */
	inline TNGridTerrain::FTileMesh ToTileMesh(const TArray<FVector3f>& Vertices, const TArray<FVector3f>& Normals,
		const TArray<FColor>& Colors, const TArray<int32>& Triangles)
	{
		TNGridTerrain::FTileMesh Mesh;
		if (Normals.Num() != Vertices.Num() || Colors.Num() != Vertices.Num() || Triangles.Num() % 3 != 0) { return Mesh; }
		Mesh.Vertices.Reserve(Vertices.Num());
		Mesh.Normals.Reserve(Vertices.Num());
		Mesh.Colors.Reserve(Vertices.Num());
		for (int32 I = 0; I < Vertices.Num(); ++I)
		{
			Mesh.Vertices.Add(FVector(Vertices[I]));
			Mesh.Normals.Add(FVector(Normals[I]));
			const FColor& C = Colors[I];
			Mesh.Colors.Add(FLinearColor(C.R / 255.f, C.G / 255.f, C.B / 255.f, C.A / 255.f));
		}
		Mesh.Triangles = Triangles;
		return Mesh;
	}
}
