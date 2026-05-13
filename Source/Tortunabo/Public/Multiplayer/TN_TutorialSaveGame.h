#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "TN_TutorialSaveGame.generated.h"

/**
 * @brief Persiste el estado de completitud del tutorial del jugador local.
 *
 * Almacenado localmente en cada máquina — el flag del listen-server es el que
 * gobierna el enrutado del spawn inicial en TN_HQGameMode (route a tutorial la primera vez).
 */
UCLASS()
class TORTUNABO_API UTN_TutorialSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	/**
	 * true una vez que el jugador ha sido spawneado en la zona de tutorial al menos una vez.
	 * Se pone a true inmediatamente al primer spawn, de modo que las sesiones siguientes
	 * usen el spawn normal.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Tutorial")
	bool bHasCompletedTutorial = false;
};
