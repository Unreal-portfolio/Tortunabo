#pragma once

#include "CoreMinimal.h"
#include "World/TN_DirectInteractableBase.h"
#include "TN_TotemInteractable.generated.h"

class ATortugaCharacter;

/**
 * Tótem (#5).
 *
 * Interactuable colocado en el nivel (o droppable como ítem).
 * Al activarlo: revive a un jugador muerto aleatorio y lo spawnea
 * al lado del activador.
 *
 * Implementación:
 *   - Busca PlayerControllers con bIsEliminated=true.
 *   - Selecciona uno aleatoriamente.
 *   - Llama ATN_RunGameMode::RevivePlayer (que maneja estado + re-possess).
 *   - Teleporta al revivido cerca del activador.
 *   - El tótem se destruye tras un uso (configurable, puede reaparecer).
 *
 * Uso:
 *   Crear BP_Totem hijo, asignar mesh y PromptText = "Activar Tótem".
 *   Puede colocarse en el nivel como pickup estático.
 */
UCLASS()
class TORTUNABO_API ATN_TotemInteractable : public ATN_DirectInteractableBase
{
	GENERATED_BODY()

public:
	ATN_TotemInteractable();

	virtual void Interact(APawn* Interactor) override;

protected:
	/** Desplazamiento desde el activador donde aparece el jugador revivido (cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Totem",
		meta = (ClampMin = "50.0"))
	float ReviveSpawnOffset = 150.f;

	/**
	 * Si true, el tótem se destruye tras el primer uso exitoso.
	 * Si false, recarga vía CooldownSeconds y puede usarse varias veces.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Totem")
	bool bDestroyAfterUse = true;

	/** Llamado en todas las máquinas cuando el tótem revive a un jugador. BP implementable. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Totem")
	void OnTotemActivated(ATortugaCharacter* RevivedCharacter, ATortugaCharacter* Activator);

	/** Llamado en todas las máquinas cuando no hay nadie a quien revivir. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Totem")
	void OnTotemNoTarget();

private:
	UFUNCTION(NetMulticast, Reliable)
	void MulticastOnActivated(ATortugaCharacter* RevivedCharacter, ATortugaCharacter* Activator);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastOnNoTarget();
};
