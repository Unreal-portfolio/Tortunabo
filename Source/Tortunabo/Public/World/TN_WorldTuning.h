#pragma once

#include "CoreMinimal.h"

/**
 * Constantes de tuning compartidas entre actores de World/ que no encajan en
 * ningún actor concreto. Sin UWorld, sin estado — solo valores literales
 * repetidos que antes vivían duplicados como números mágicos por fichero.
 */

namespace TNWorldTuning
{
	/**
	 * Retardo (segundos) para diferir inicialización 1 tick tras BeginPlay cuando
	 * el actor puede ser un Child Actor Component dentro de un chunk: los chunks
	 * terminan de posicionar sus hijos después de BeginPlay, así que leer la
	 * transform final requiere esperar ese primer tick.
	 */
	inline constexpr float ChunkChildActorSettleDelay = 0.05f;
}
