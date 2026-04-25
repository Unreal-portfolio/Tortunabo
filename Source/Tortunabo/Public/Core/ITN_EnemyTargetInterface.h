#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ITN_EnemyTargetInterface.generated.h"

UINTERFACE(MinimalAPI, BlueprintType)
class UTN_EnemyTargetInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Contrato común para enemigos vulnerables a items del jugador.
 *
 * Los items (Concha, Tinta, Throwable, Plátano...) hacen Cast a esta interfaz
 * en sus eventos de overlap/hit y aplican stun/blind. Mantener server-only:
 * el enemigo decide cómo replicar el efecto (multicast visual, OnRep, etc).
 *
 * Stun  → pausa IA y movimiento. El enemigo queda inmóvil hasta que expira.
 * Blind → desorienta. Cada enemigo decide qué significa (Gaviota: pierde target,
 *         Cangrejo: pierde detección, etc).
 */
class TORTUNABO_API ITN_EnemyTargetInterface
{
	GENERATED_BODY()

public:
	/** Aturde al enemigo durante Duration segundos. Llamar solo en servidor. */
	virtual void ApplyStun(float Duration) = 0;

	/** Ciega al enemigo durante Duration segundos. Llamar solo en servidor. */
	virtual void ApplyBlind(float Duration) = 0;

	/** ¿Está actualmente aturdido? */
	virtual bool IsStunned() const = 0;

	/** ¿Está actualmente cegado? */
	virtual bool IsBlinded() const = 0;
};
