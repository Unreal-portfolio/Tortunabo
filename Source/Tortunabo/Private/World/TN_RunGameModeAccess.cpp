#include "World/TN_RunGameModeAccess.h"
#include "Game/TN_RunGameMode.h"
#include "Engine/World.h"

ATN_RunGameMode* TN_ResolveRunGameMode(UWorld* World)
{
	return World ? World->GetAuthGameMode<ATN_RunGameMode>() : nullptr;
}
