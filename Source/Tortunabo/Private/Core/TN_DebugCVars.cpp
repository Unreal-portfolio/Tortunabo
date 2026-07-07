#include "Core/TN_DebugCVars.h"
#include "HAL/IConsoleManager.h"

namespace TNDebug
{
	int32 EnemyDebug = 0;

	static FAutoConsoleVariableRef CVarEnemyDebug(
		TEXT("TN.Enemy.Debug"),
		EnemyDebug,
		TEXT("1 = draw debug de enemigos (radios del servidor, estado FSM, vista cliente en naranja)."),
		ECVF_Cheat);
}
