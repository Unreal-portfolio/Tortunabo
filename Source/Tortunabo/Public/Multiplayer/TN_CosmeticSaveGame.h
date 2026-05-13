#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "TN_CosmeticSaveGame.generated.h"

/**
 * @brief SaveGame con el perfil cosmético del jugador local.
 *
 * Guarda cascos desbloqueados, equipados (helmet y skin) y el score
 * acumulado entre carreras. Persistido por UMP_GameInstance en un slot
 * con prefijo CosmeticSaveSlotPrefix + sufijo de Steam ID si está disponible.
 */
UCLASS()
class TORTUNABO_API UTN_CosmeticSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, Category = "Cosmetics")
	TArray<FName> UnlockedHelmetIds;

	UPROPERTY(BlueprintReadWrite, Category = "Cosmetics")
	FName EquippedHelmetId = NAME_None;

	/** ID del skin de personaje activo. NAME_None = aspecto por defecto. */
	UPROPERTY(BlueprintReadWrite, Category = "Cosmetics")
	FName EquippedSkinId = NAME_None;

	/**
	 * Puntos de carrera acumulados (#26).
	 * Se suman al terminar cada carrera según posición de llegada.
	 * 1º=400, 2º=300, 3º=200, 4º=100. Eliminados = 0.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Score")
	int32 AccumulatedRaceScore = 0;
};
