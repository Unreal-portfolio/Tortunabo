#pragma once

#include "CoreMinimal.h"
#include "TN_MatchStartTypes.generated.h"

/**
 * Cómo empieza la partida en el mapa procedural: igual que se pusieron listos en el lobby (ATN_SandCastleLobby). La
 * salida del mapa lleva la misma estructura.
 */
UENUM(BlueprintType)
enum class ETNMatchStartStyle : uint8
{
	/** Todos dentro de la sala de la puerta doble: se sale por la segunda puerta al empezar. */
	Gate UMETA(DisplayName = "Puerta doble"),
	/** En los huevos de la pila: cada uno sale rompiendo su huevo. */
	Eggs UMETA(DisplayName = "Huevos"),
};
