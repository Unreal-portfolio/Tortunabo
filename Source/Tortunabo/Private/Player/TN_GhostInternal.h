#pragma once

#include "CoreMinimal.h"

class APawn;
class APlayerController;
class ATN_GhostEgg;
class UWorld;

/**
 * Enganches internos del fantasma espectador (Docs/Fantasma_Espectador.md): los llaman AMP_GamePlayerController y los
 * actores del fantasma; no son parte del contrato (Public/Player/TN_Ghost.h). Todo es de servidor.
 */
namespace TNGhostInternal
{
	/** El jugador pasa a espectador (AMP_GamePlayerController::EnterSpectateMode): crea su fantasma y apunta su tortuga. */
	void OnEnterSpectate(APlayerController* PC);

	/** El jugador vuelve a tener tortuga (OnPossess): su fantasma se desvanece. */
	void OnPossess(APlayerController* PC, APawn* Pawn);

	/** Llega la hora de eclosionar el huevo de ReviveIntoEgg: crea la tortuga dentro, la lanza de un saltito y avisa. */
	void CompleteRevive(ATN_GhostEgg* Egg);

	/** Órdenes de prueba (TN.Ghost.*), en el servidor. */
	enum class EDebugCommand : uint8
	{
		Revive,
		Become,
	};

	/**
	 * Ejecuta una orden de prueba sobre el jugador PlayerIndex (orden de los PlayerController del servidor: 0 = anfitrión).
	 * Con PlayerIndex < 0: quien la pide (Requester, si llega de la consola de un cliente) o, si no, el primer fantasma
	 * (Revive) o el anfitrión (Become).
	 */
	void RunDebugCommand(UWorld* World, EDebugCommand Command, int32 PlayerIndex, APlayerController* Requester = nullptr);
}
