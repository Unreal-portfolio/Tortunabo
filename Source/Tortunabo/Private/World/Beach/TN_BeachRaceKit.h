#pragma once

#include "CoreMinimal.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "UObject/UObjectGlobals.h"
#include "World/Beach/TN_BeachLayout.h"
#include "../ProcMap/TN_ProcMapMeshKit.h"
#include "../../Lobby/TN_CastleKit.h"

/**
 * Utilidades de la playa del modo carrera compartidas por los archivos de ATN_BeachRaceGenerator (construcción del
 * terreno y escenografía): materiales del mapa procedural, subida de secciones, colores y copias escaladas de buffers.
 */
namespace TNBeachRaceKit
{
	/** Versión de la construcción: al cambiarla, la playa ya construida en el editor se rehace. */
	constexpr uint32 BuildVersion = 2u;

	/** Casillas por lado de cada tesela del terreno (las rondas rehacen solo las teselas con hoyos). */
	constexpr int32 TerrainTileQuads = 40;

	/** Etiqueta de los componentes que crea el generador (se quitan aunque se pierda su lista). */
	inline FName GeneratedTag()
	{
		static const FName Tag(TEXT("TNBeachGen"));
		return Tag;
	}

	inline UMaterialInterface* LoadMaterial(const TCHAR* Path, UMaterialInterface* Fallback)
	{
		UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, Path);
		return Mat ? Mat : Fallback;
	}

	/** Terreno del mapa procedural: con el dato de primitiva 0 a 1, relieve por normales y, con alfa 1, grano y guijarros. */
	inline UMaterialInterface* TerrainMaterial()
	{
		return LoadMaterial(TEXT("/Game/ProcMap/Materials/M_ProcTerrain.M_ProcTerrain"), TNCastleKit::VertexColorMaterial());
	}

	inline UMaterialInterface* FoliageMaterial()
	{
		return LoadMaterial(TEXT("/Game/ProcMap/Materials/M_ProcFoliage.M_ProcFoliage"), TNCastleKit::VertexColorMaterial());
	}

	/** Mar animado del mapa procedural (MI_ProcSeaAnim) o, si falta, el mar plano. */
	inline UMaterialInterface* SeaMaterial()
	{
		return LoadMaterial(TEXT("/Game/ProcMap/Materials/MI_ProcSeaAnim.MI_ProcSeaAnim"),
			LoadMaterial(TEXT("/Game/ProcMap/Materials/MI_ProcSea.MI_ProcSea"), TNCastleKit::VertexColorMaterial()));
	}

	/** Color sRGB 0xRRGGBB en lineal (alfa 0) para las mallas procedurales. */
	inline FLinearColor Hex(uint32 Value)
	{
		return TNCastleKit::Col(Value);
	}

	/** Ruido estable 0..1 por índices. */
	inline double Hash01(int32 A, int32 B, uint32 Seed)
	{
		return 0.5 + 0.5 * TNProcMesh::TNProcHashNoise(A, B, Seed);
	}

	/** Sube unos buffers como sección Section del componente, con o sin colisión. */
	inline void Upload(UProceduralMeshComponent* Comp, int32 Section, const TNProcMesh::FTNProcMeshBuffers& B, UMaterialInterface* Mat, bool bCollision)
	{
		if (!Comp || B.IsEmpty()) { return; }
		const TArray<FProcMeshTangent> NoTangents;
		Comp->CreateMeshSection_LinearColor(Section, B.Verts, B.Tris, B.Normals, B.UVs, B.Colors, NoTangents, bCollision);
		if (Mat) { Comp->SetMaterial(Section, Mat); }
	}

	/** Añade Src a Dst escalado Scale veces alrededor de Origin (las normales no cambian con una escala uniforme). */
	inline void AppendScaled(TNProcMesh::FTNProcMeshBuffers& Dst, const TNProcMesh::FTNProcMeshBuffers& Src, const FVector& Origin, double Scale)
	{
		const int32 Base = Dst.Verts.Num();
		for (int32 i = 0; i < Src.Verts.Num(); ++i)
		{
			Dst.Verts.Add(Origin + Src.Verts[i] * Scale);
			Dst.Normals.Add(Src.Normals[i]);
			Dst.UVs.Add(Src.UVs[i] * Scale);
			Dst.Colors.Add(Src.Colors[i]);
		}
		for (const int32 Index : Src.Tris) { Dst.Tris.Add(Base + Index); }
	}
}
