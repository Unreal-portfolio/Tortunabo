#pragma once

#include "CoreMinimal.h"

class ACharacter;
class UObject;

/**
 * Aturdimiento del modo carrera (en la playa no se muere: lo que mataría, aturde). Lo usan trampas, enemigos y las
 * reglas (ATN_BeachRaceGameMode). Implementación: Private/World/Beach/TN_BeachStun.cpp (agente de reglas).
 */
namespace TNBeach
{
	/**
	 * Servidor: mete a la tortuga en su caparazón como bola (UTN_ShellComponent: cuerpo físico, salida bloqueada) con
	 * la velocidad Launch (cero = cae donde está), la deja temblando y mareada (pájaros y estrellas) durante Seconds y
	 * después la suelta para que pueda salir. Si ya estaba aturdida, alarga el aturdimiento hasta el mayor de los dos
	 * finales. No hace nada en clientes, con tortugas muertas ni con Seconds <= 0.
	 */
	TORTUNABO_API void StunTurtle(ACharacter* Turtle, float Seconds, const FVector& Launch = FVector::ZeroVector);

	/**
	 * Servidor: derribo con ragdoll y mareo, como el de la piel de plátano (ATortugaCharacter::ApplyKnockdown), con el
	 * empujón Impulse, durante Seconds. Para golpes secos (erizo, cagada de gaviota, rueda de quad…): no todo es la bola.
	 * No hace nada en clientes, con tortugas muertas ni con Seconds <= 0.
	 */
	TORTUNABO_API void KnockDownTurtle(ACharacter* Turtle, float Seconds, const FVector& Impulse = FVector::ZeroVector);

	/** true si la tortuga está aturdida por StunTurtle (en cualquier máquina, con estado replicado). */
	TORTUNABO_API bool IsTurtleStunned(const ACharacter* Turtle);

	/**
	 * true si en este mundo no se muere (modo carrera en la playa): las zonas de muerte, caídas, tormenta y enemigos
	 * aturden en vez de matar. Lo decide el GameMode del servidor y se replica en el GameState.
	 */
	TORTUNABO_API bool IsNoDeathWorld(const UObject* WorldContext);
}
