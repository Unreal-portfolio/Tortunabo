#pragma once

#include "CoreMinimal.h"
#include "World/TN_DirectInteractableBase.h"
#include "TN_TotemInteractable.generated.h"

class ATortugaCharacter;
class APlayerState;
class USoundBase;
class UParticleSystem;

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

	// ── Audio/VFX automáticos ─────────────────────────────────────────────────

	/** Sonido al activar el tótem con éxito (revive a alguien). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Totem|Audio")
	TObjectPtr<USoundBase> SoundActivate;

	/** Sonido cuando no hay nadie a quien revivir. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Totem|Audio")
	TObjectPtr<USoundBase> SoundNoTarget;

	/** VFX al activar con éxito. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Totem|FX")
	TObjectPtr<UParticleSystem> VFXActivate;

	// ── Eventos BP (opcional) ────────────────────────────────────────────────

	/** Llamado en TODAS las máquinas cuando el tótem revive a un jugador.
	 *  Audio/VFX básicos ya se reproducen solos. Usa este evento para lógica extra (HUD, cámara). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Totem")
	void OnTotemActivated(ATortugaCharacter* RevivedCharacter, ATortugaCharacter* Activator);

	/** Llamado en TODAS las máquinas cuando no hay nadie a quien revivir. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Totem")
	void OnTotemNoTarget();

private:
	/**
	 * Viaja el PlayerState del revivido, no su pawn: el pawn recién re-poseído /
	 * teleportado puede no tener aún canal abierto en clientes remotos y el
	 * puntero llegaría null. El PlayerState existe en todos los clientes desde
	 * el join, así que siempre resuelve; el pawn se deriva en destino.
	 */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastOnActivated(APlayerState* RevivedPS, ATortugaCharacter* Activator);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastOnNoTarget();

	/** Resuelve el pawn del revivido (con reintentos cortos si aún no replicó)
	 *  y dispara OnTotemActivated. Tras agotar reintentos, dispara con null. */
	void NotifyActivatedWhenResolved(APlayerState* RevivedPS, ATortugaCharacter* Activator, int32 AttemptsLeft);
};
