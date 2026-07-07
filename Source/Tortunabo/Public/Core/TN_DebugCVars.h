#pragma once

#include "CoreMinimal.h"

/**
 * CVars de debug del juego para acelerar playtests (PIE 4P / Standalone).
 * Los draws son no-op en Shipping (DrawDebugHelpers) y el CVar es ECVF_Cheat.
 */
namespace TNDebug
{
	/**
	 * 1 = dibujar debug de enemigos: radios reales del servidor (círculo de la
	 * gaviota, detección/ataque del cangrejo, BounceZone de la medusa), estado
	 * FSM sobre cada enemigo, y la vista del cliente en color naranja para
	 * comparar server vs cliente en PIE multi-ventana.
	 * Consola: TN.Enemy.Debug 1
	 */
	extern int32 EnemyDebug;
}
