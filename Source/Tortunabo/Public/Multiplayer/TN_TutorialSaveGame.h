#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "TN_TutorialSaveGame.generated.h"

/**
 * @brief Estado del tutorial de la primera partida del jugador de esta máquina (Docs/Tutorial.md).
 *
 * Cada máquina tiene el suyo (ranura UMP_GameInstance::GetTutorialSlotName: TutorialState_0 y, en el editor con varias
 * ventanas, una por ventana): el tutorial es por jugador. El cliente lo mira al llegar al lobby (y al unirse a una sala,
 * para la opción ?TNTut=1 de la URL) y se lo pide al servidor si no lo ha hecho. Se apunta como hecho al caer por la
 * cascada del final o al saltarlo. Saved/ResetTutorial.txt o la consola (TN.Tutorial.Reset) lo vuelven a dejar por hacer.
 */
UCLASS()
class TORTUNABO_API UTN_TutorialSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	/** true cuando el jugador de esta máquina ha terminado (o saltado) el tutorial: el siguiente lobby empieza normal. */
	UPROPERTY(BlueprintReadWrite, Category = "Tutorial")
	bool bHasCompletedTutorial = false;

	/** Veces que se ha terminado o saltado (para el registro y las pruebas). */
	UPROPERTY(BlueprintReadWrite, Category = "Tutorial")
	int32 TimesCompleted = 0;
};
