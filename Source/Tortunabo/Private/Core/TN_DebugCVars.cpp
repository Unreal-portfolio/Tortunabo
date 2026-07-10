#include "Core/TN_DebugCVars.h"
#include "HAL/IConsoleManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "World/TN_JellyfishActor.h"

namespace TNDebug
{
	int32 EnemyDebug = 0;

	// Actores con tick auto-apagado (medusa) no pintarían su draw si el CVar se
	// activa mientras duermen: re-encender su tick al cambiar el valor. El propio
	// Tick se auto-apaga de nuevo cuando el CVar vuelve a 0 y no hay squish.
	static void OnEnemyDebugChanged(IConsoleVariable*)
	{
		if (EnemyDebug == 0 || !GEngine) { return; }
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (!World || !World->IsGameWorld()) { continue; }
			for (TActorIterator<ATN_JellyfishActor> It(World); It; ++It)
			{
				It->SetActorTickEnabled(true);
			}
		}
	}

	static FAutoConsoleVariableRef CVarEnemyDebug(
		TEXT("TN.Enemy.Debug"),
		EnemyDebug,
		TEXT("1 = draw debug de enemigos (radios del servidor, estado FSM, vista cliente en naranja)."),
		FConsoleVariableDelegate::CreateStatic(&OnEnemyDebugChanged),
		ECVF_Cheat);
}
