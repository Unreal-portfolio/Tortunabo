#pragma once

#include "CoreMinimal.h"

/**
 * Lógica pura del vadeo: a partir de la profundidad de agua decide el
 * multiplicador de velocidad a aplicar. Sin UWorld, sin componentes — igual
 * que TN_ShellDecisions.h, para que los tests cubran el código real que usa
 * UTN_WadingComponent en vez de una copia paralela de las reglas.
 */
namespace TNWadingLogic
{
	/**
	 * @brief Multiplicador de velocidad de vadeo según la profundidad actual.
	 * Por debajo (o igual) de MinDepth no hay efecto (1.0 = velocidad normal).
	 * Entre MinDepth y FullDepth interpola linealmente hasta WadeSpeedMultiplier;
	 * por encima de FullDepth, satura en WadeSpeedMultiplier.
	 */
	inline float ComputeSpeedMultiplier(float Depth, float MinDepth, float FullDepth, float WadeSpeedMultiplier)
	{
		if (Depth <= MinDepth)
		{
			return 1.f;
		}

		const float Range = FMath::Max(FullDepth - MinDepth, KINDA_SMALL_NUMBER);
		const float Alpha = FMath::Clamp((Depth - MinDepth) / Range, 0.f, 1.f);
		return FMath::Lerp(1.f, WadeSpeedMultiplier, Alpha);
	}
}
