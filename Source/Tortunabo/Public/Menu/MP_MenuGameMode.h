#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MP_MenuGameMode.generated.h"

/**
 * @brief GameMode mínimo del mapa de menú principal (LVL_Menu).
 *
 * No tiene reglas de juego — sólo sirve para que el menú principal exista
 * con un PlayerController y un widget de UI. La lógica real vive en
 * AMP_MenuPlayerController.
 */
UCLASS()
class TORTUNABO_API AMP_MenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AMP_MenuGameMode();
};
