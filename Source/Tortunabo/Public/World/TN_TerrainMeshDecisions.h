#pragma once

#include "CoreMinimal.h"
#include "Misc/Compression.h"
#include "World/TN_GridTerrainDecisions.h"

/**
 * Trozo del mapa volumétrico como función PURA: lectura del binario que escriben
 * Scripts/terrain_vol/export.py (formato TNTM1) y Scripts/terrain_volumes/Variants (TNTM2) y
 * paso a la malla del tile.
 *
 * TNTM1 (little endian):
 *   char[4] 'TNTM', uint32 versión (1), uint32 vértices N, triángulos M, instancias K,
 *   float32 N*3 posiciones (uu, locales al centro del trozo), float32 N*3 normales,
 *   uint8 N*4 color RGBA (color LINEAL * 255), uint32 M*3 índices (cara visible de Unreal),
 *   float32 K*11 instancias de algas: forma, x, y, z, yaw, sx, sy, sz, r, g, b.
 *
 * TNTM2 (little endian, cuantizado y comprimido):
 *   cabecera: char[4] 'TNTM', uint32 versión (2), uint32 N, M, K, float32 origin[3] (uu),
 *     float32 step[3] (uu por unidad cuantizada; pos = origin + q * step), uint32 raw_size
 *     (bytes del payload descomprimido), uint32 zlib_size (bytes del payload comprimido).
 *   payload (zlib con cabecera RFC1950, como zlib.compress de Python;
 *     FCompression::UncompressMemory(NAME_Zlib, ...) en este fichero):
 *     uint16 N*3 posiciones cuantizadas, int16 N*2 normales en codificación octaédrica snorm
 *     (valor / 32767), uint8 N*4 color RGBA (mismo significado que en TNTM1), int32 M*3 índices
 *     en delta (idx[i] - idx[i-1] sobre el array plano, idx[-1] = 0), float32 K*11 instancias
 *     de algas (mismo formato que TNTM1).
 *   El orden de vértices por triángulo es el mismo que en TNTM1.
 */
namespace TNTerrainMesh
{
	constexpr uint32 FormatVersion = 1;
	constexpr uint32 FormatVersion2 = 2;
	constexpr int32 HeaderBytesV1 = 20;
	constexpr int32 HeaderBytesV2 = 52;
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

		/** Decodificación octaédrica snorm -> normal unitaria (Meyer et al.), inversa de la
		 * cuantización que hace Scripts/terrain_vol/export.py al escribir TNTM2. */
		inline FVector3f DecodeOctahedralNormal(int16 EncodedX, int16 EncodedY)
		{
			FVector3f N(EncodedX / 32767.f, EncodedY / 32767.f, 0.f);
			N.Z = 1.f - FMath::Abs(N.X) - FMath::Abs(N.Y);
			if (N.Z < 0.f)
			{
				const float OldX = N.X;
				N.X = (1.f - FMath::Abs(N.Y)) * (OldX >= 0.f ? 1.f : -1.f);
				N.Y = (1.f - FMath::Abs(OldX)) * (N.Y >= 0.f ? 1.f : -1.f);
			}
			return N.GetSafeNormal();
		}

		/** Algas: mismo layout en TNTM1 y TNTM2. false (y Error) si una forma no se reconoce. */
		inline bool ReadFoliage(const TArray<float>& InstanceData, int64 NumInstances, FChunk& Chunk, FString& Error)
		{
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
			return true;
		}

		/** false (y Error) si la cabecera, los tamaños o los índices del trozo TNTM1 no cuadran. */
		inline bool ParseChunkV1(TArrayView<const uint8> Bytes, FChunk& Out, FString& Error)
		{
			if (Bytes.Num() < HeaderBytesV1)
			{
				Error = FString::Printf(TEXT("tamaño %d, cabecera TNTM1 de %d bytes"), Bytes.Num(), HeaderBytesV1);
				return false;
			}
			uint32 Header[4];
			FMemory::Memcpy(Header, Bytes.GetData() + 4, sizeof(Header));
			const int64 NumVertices = Header[1], NumTriangles = Header[2], NumInstances = Header[3];
			const int64 Expected = HeaderBytesV1 + NumVertices * (12 + 12 + 4) + NumTriangles * 12 + NumInstances * InstanceFloats * 4;
			if (Expected != Bytes.Num() || NumVertices > MAX_int32 / 3 || NumTriangles > MAX_int32 / 3)
			{
				Error = FString::Printf(TEXT("tamaño %d, se esperaban %lld bytes"), Bytes.Num(), Expected);
				return false;
			}

			FChunk Chunk;
			int64 Offset = HeaderBytesV1;
			ReadArray(Bytes, Offset, static_cast<int32>(NumVertices), Chunk.Vertices);
			ReadArray(Bytes, Offset, static_cast<int32>(NumVertices), Chunk.Normals);
			ReadArray(Bytes, Offset, static_cast<int32>(NumVertices), Chunk.Colors);   // RGBA en orden de bytes
			TArray<uint32> Indices;
			ReadArray(Bytes, Offset, static_cast<int32>(NumTriangles * 3), Indices);
			TArray<float> InstanceData;
			ReadArray(Bytes, Offset, static_cast<int32>(NumInstances * InstanceFloats), InstanceData);

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

			if (!ReadFoliage(InstanceData, NumInstances, Chunk, Error)) { return false; }
			Out = MoveTemp(Chunk);
			return true;
		}

		/** false (y Error) si la cabecera, la descompresión, los tamaños o los índices del
		 * trozo TNTM2 no cuadran. Nunca deja Out a medias: o vale, o no se toca. */
		inline bool ParseChunkV2(TArrayView<const uint8> Bytes, FChunk& Out, FString& Error)
		{
			if (Bytes.Num() < HeaderBytesV2)
			{
				Error = FString::Printf(TEXT("tamaño %d, cabecera TNTM2 de %d bytes"), Bytes.Num(), HeaderBytesV2);
				return false;
			}
			int64 Offset = 8; // magic + versión, ya consumidos por el llamador
			uint32 NumVertices32 = 0, NumTriangles32 = 0, NumInstances32 = 0;
			FMemory::Memcpy(&NumVertices32, Bytes.GetData() + Offset, 4); Offset += 4;
			FMemory::Memcpy(&NumTriangles32, Bytes.GetData() + Offset, 4); Offset += 4;
			FMemory::Memcpy(&NumInstances32, Bytes.GetData() + Offset, 4); Offset += 4;
			float Origin[3] = {}, Step[3] = {};
			FMemory::Memcpy(Origin, Bytes.GetData() + Offset, sizeof(Origin)); Offset += sizeof(Origin);
			FMemory::Memcpy(Step, Bytes.GetData() + Offset, sizeof(Step)); Offset += sizeof(Step);
			uint32 RawSize = 0, ZlibSize = 0;
			FMemory::Memcpy(&RawSize, Bytes.GetData() + Offset, 4); Offset += 4;
			FMemory::Memcpy(&ZlibSize, Bytes.GetData() + Offset, 4); Offset += 4;
			check(Offset == HeaderBytesV2);

			const int64 NumVertices = NumVertices32, NumTriangles = NumTriangles32, NumInstances = NumInstances32;
			if (NumVertices > MAX_int32 / 3 || NumTriangles > MAX_int32 / 3)
			{
				Error = TEXT("demasiados vértices o triángulos para un int32");
				return false;
			}
			if (static_cast<int64>(HeaderBytesV2) + static_cast<int64>(ZlibSize) != Bytes.Num())
			{
				Error = FString::Printf(TEXT("tamaño %d, se esperaban %d + %u bytes de zlib_size"), Bytes.Num(), HeaderBytesV2, ZlibSize);
				return false;
			}
			const int64 ExpectedRaw = NumVertices * (3 * 2 + 2 * 2 + 4) + NumTriangles * 3 * 4 + NumInstances * InstanceFloats * 4;
			if (ExpectedRaw != static_cast<int64>(RawSize))
			{
				Error = FString::Printf(TEXT("raw_size %u, se esperaban %lld bytes"), RawSize, ExpectedRaw);
				return false;
			}

			TArray<uint8> Raw;
			Raw.SetNumUninitialized(RawSize);
			if (RawSize > 0 && !FCompression::UncompressMemory(NAME_Zlib, Raw.GetData(), static_cast<int64>(RawSize),
				Bytes.GetData() + HeaderBytesV2, static_cast<int64>(ZlibSize)))
			{
				Error = TEXT("no se pudo descomprimir el payload zlib de TNTM2");
				return false;
			}

			FChunk Chunk;
			int64 RawOffset = 0;
			TArray<uint16> QuantPos;
			ReadArray(Raw, RawOffset, static_cast<int32>(NumVertices * 3), QuantPos);
			TArray<int16> OctNormals;
			ReadArray(Raw, RawOffset, static_cast<int32>(NumVertices * 2), OctNormals);
			ReadArray(Raw, RawOffset, static_cast<int32>(NumVertices), Chunk.Colors);
			TArray<int32> Deltas;
			ReadArray(Raw, RawOffset, static_cast<int32>(NumTriangles * 3), Deltas);
			TArray<float> InstanceData;
			ReadArray(Raw, RawOffset, static_cast<int32>(NumInstances * InstanceFloats), InstanceData);

			Chunk.Vertices.SetNumUninitialized(static_cast<int32>(NumVertices));
			Chunk.Normals.SetNumUninitialized(static_cast<int32>(NumVertices));
			for (int64 I = 0; I < NumVertices; ++I)
			{
				Chunk.Vertices[I] = FVector3f(
					Origin[0] + QuantPos[I * 3 + 0] * Step[0],
					Origin[1] + QuantPos[I * 3 + 1] * Step[1],
					Origin[2] + QuantPos[I * 3 + 2] * Step[2]);
				Chunk.Normals[I] = DecodeOctahedralNormal(OctNormals[I * 2 + 0], OctNormals[I * 2 + 1]);
			}
			// FColor guarda los bytes como B, G, R, A: el binario viene en R, G, B, A.
			for (FColor& Color : Chunk.Colors) { Swap(Color.R, Color.B); }

			Chunk.Triangles.Reserve(Deltas.Num());
			int32 Running = 0;
			for (const int32 Delta : Deltas)
			{
				Running += Delta;
				if (Running < 0 || Running >= NumVertices)
				{
					Error = FString::Printf(TEXT("índice %d fuera de %lld vértices"), Running, NumVertices);
					return false;
				}
				Chunk.Triangles.Add(Running);
			}

			if (!ReadFoliage(InstanceData, NumInstances, Chunk, Error)) { return false; }
			Out = MoveTemp(Chunk);
			return true;
		}
	}

	/** Lee un trozo TNTM1 o TNTM2. false (y Error) si la cabecera, los tamaños, la
	 * descompresión o los índices no cuadran: nunca deja Out a medias. */
	inline bool ParseChunk(TArrayView<const uint8> Bytes, FChunk& Out, FString& Error)
	{
		if (Bytes.Num() < 8 || FMemory::Memcmp(Bytes.GetData(), "TNTM", 4) != 0)
		{
			Error = TEXT("no es un trozo TNTM");
			return false;
		}
		uint32 Version = 0;
		FMemory::Memcpy(&Version, Bytes.GetData() + 4, sizeof(Version));
		if (Version == FormatVersion) { return Detail::ParseChunkV1(Bytes, Out, Error); }
		if (Version == FormatVersion2) { return Detail::ParseChunkV2(Bytes, Out, Error); }
		Error = FString::Printf(TEXT("versión %u, se esperaba %u o %u"), Version, FormatVersion, FormatVersion2);
		return false;
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
