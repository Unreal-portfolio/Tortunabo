#include "Menu/MP_MenuGameMode.h"
#include "Menu/MP_MenuPlayerController.h"

AMP_MenuGameMode::AMP_MenuGameMode()
{
	DefaultPawnClass = nullptr;
	PlayerControllerClass = AMP_MenuPlayerController::StaticClass();
}

