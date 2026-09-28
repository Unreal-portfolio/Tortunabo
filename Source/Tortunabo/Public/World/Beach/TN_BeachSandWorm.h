#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TN_BeachSandWorm.generated.h"

class ACharacter;

/**
 * Gusano de arena gigante del modo carrera (Docs/Modo_Carrera.md, «Gusano de arena»): cuando se acaba la cuenta atrás
 * de 10 s tras la primera tortuga en el agua, a cada tortuga que no ha llegado le sale de debajo de la arena un gusano
 * enorme que se la come de un bocado y se vuelve a hundir. Es un remate cómico: nadie muere (la tortuga se queda dentro,
 * oculta, hasta la ronda siguiente, que vuelve a crear a todas en la salida).
 *
 * Contrato: ATN_BeachRaceGameMode llama a EatTurtle en el servidor al llegar la cuenta a 0 y espera EatSeconds antes
 * del recuento. Implementación: Private/World/Beach/TN_BeachSandWorm.cpp.
 */
UCLASS()
class TORTUNABO_API ATN_BeachSandWorm : public AActor
{
	GENERATED_BODY()

public:
	/** Duración total de la escena (aviso en la arena, salida, bocado y vuelta a la arena), en segundos. */
	static constexpr float EatSeconds = 3.2f;

	/**
	 * Servidor: crea (replicado) un gusano que sale bajo Turtle y se la come; la tortuga queda quieta, sin control y
	 * oculta desde el bocado. Devuelve el gusano, o nullptr si no se puede (sin autoridad, tortuga nula o ya comida).
	 */
	static ATN_BeachSandWorm* EatTurtle(ACharacter* Turtle);

	/** true si Turtle ya está en la boca de un gusano (en cualquier máquina). */
	static bool IsBeingEaten(const ACharacter* Turtle);
};
