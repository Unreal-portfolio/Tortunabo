#pragma once

#include "CoreMinimal.h"
#include "World/ProcMap/TN_ProcMapEnums.h"
#include "TN_ProcMapMeshKit.h"

/**
 * Colores del suelo del camino del mapa procedural (los usa ATN_ProcMapGenerator::BuildTerrain y el recorrido del
 * tutorial, ATN_TutorialCourse, para que su pasillo se vea igual que un camino del cooperativo).
 */
namespace TNProcMesh
{
	/** Luminancia de un color lineal. */
	inline float TNLuminance(const FLinearColor& C) { return 0.2126f * C.R + 0.7152f * C.G + 0.0722f * C.B; }

	/**
	 * Color del suelo del camino con contraste claro frente a sus paredes (media del suelo y la roca del
	 * bioma), sea cual sea el color de los assets: en los biomas claros (arena, roca gris, pueblos) se
	 * oscurece hasta 0,36 veces la luminancia de las paredes, como tierra pisada; en los oscuros (selva,
	 * volcán, manglar) se aclara hasta ~2,2 veces. Conserva el tono del camino del bioma, algo menos
	 * saturado para que no chille.
	 */
	inline FLinearColor TNContrastPath(const FLinearColor& Path, const FLinearColor& Ground, const FLinearColor& Rock)
	{
		const float Walls = 0.5f * (TNLuminance(Ground) + TNLuminance(Rock));
		const float Own = FMath::Max(0.01f, TNLuminance(Path));
		const float Target = Walls > 0.28f ? FMath::Min(Own, Walls * 0.36f) : FMath::Max(Own, Walls * 2.2f + 0.06f);
		FLinearColor Out = Path * (Target / Own);
		Out = TNProcLerpColor(Out, FLinearColor(Target, Target, Target), 0.25f);
		const float Peak = FMath::Max3(Out.R, Out.G, Out.B);
		if (Peak > 0.92f) { Out = Out * (0.92f / Peak); }
		Out.A = 1.f;
		return Out;
	}

	/**
	 * Color de sendero de cada bioma, de tono y luminosidad claramente distintos de sus paredes: tierra
	 * anaranjada clara en la selva, barro claro en el manglar, ceniza rojiza en el volcán, arena mojada en
	 * la playa, arcilla roja en el desierto, grava ocre oscura en la roca, adoquín pizarra en los pueblos y
	 * tablas oscuras en el agua. Se mezcla un poco con el camino del asset (ya con contraste) para que el
	 * asset siga contando.
	 */
	inline FLinearColor TNTrailColor(ETNProcBiome Biome, const FLinearColor& AssetPath)
	{
		FLinearColor Trail;
		switch (Biome)
		{
			case ETNProcBiome::Jungle:   Trail = FLinearColor(0.78f, 0.5f, 0.26f); break;
			case ETNProcBiome::Mangrove: Trail = FLinearColor(0.6f, 0.48f, 0.3f); break;
			case ETNProcBiome::Volcanic: Trail = FLinearColor(0.46f, 0.19f, 0.09f); break;
			case ETNProcBiome::Beach:    Trail = FLinearColor(0.24f, 0.17f, 0.1f); break;
			case ETNProcBiome::Desert:   Trail = FLinearColor(0.32f, 0.1f, 0.05f); break;
			case ETNProcBiome::Rocky:    Trail = FLinearColor(0.21f, 0.13f, 0.06f); break;
			case ETNProcBiome::Water:    Trail = FLinearColor(0.18f, 0.14f, 0.09f); break;
			default:                     Trail = FLinearColor(0.1f, 0.11f, 0.15f); break;
		}
		FLinearColor Out = TNProcLerpColor(Trail, AssetPath, 0.2f);
		Out.A = 1.f;
		return Out;
	}
}
