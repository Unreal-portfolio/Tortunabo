#pragma once

#include "CoreMinimal.h"
#include "World/TN_DirectInteractableBase.h"
#include "TN_UmbrellaInteractable.generated.h"

class ATortugaCharacter;
class USoundBase;
class UParticleSystem;

/**
 * Sombrilla interactuable (#29).
 *
 * Al pulsar E:
 *   - Activa bHasUmbrellaProtection en el TortugaCharacter del interactor.
 *   - La protección dura UmbrellaDurationSeconds.
 *   - Después se cierra sola (bHasUmbrellaProtection = false).
 *   - La sombrilla entra en cooldown (CooldownSeconds de la base, o ReuseDelay).
 *
 * La gaviota dinámica (TN_EnemySeagull) comprueba bHasUmbrellaProtection
 * antes de matar al jugador. Si está activo, cancela el ataque.
 *
 * Uso:
 *   1. Crear BP_UmbrellaInteractable como hijo.
 *   2. Asignar mesh y VFX.
 *   3. Colocar en el nivel o en chunks.
 */
UCLASS(Blueprintable)
class TORTUNABO_API ATN_UmbrellaInteractable : public ATN_DirectInteractableBase
{
	GENERATED_BODY()

public:
	ATN_UmbrellaInteractable();

	virtual void Interact(APawn* Interactor) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	/** Segundos de protección contra gaviota tras abrir la sombrilla. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Umbrella", meta = (ClampMin = "1.0"))
	float UmbrellaDurationSeconds = 8.f;

	/**
	 * Tiempo de recarga de la sombrilla tras ser usada (segundos).
	 * Durante este tiempo no puede interactuarse de nuevo.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Umbrella", meta = (ClampMin = "1.0"))
	float ReuseDelaySecs = 15.f;

	// ── Audio/VFX automáticos ─────────────────────────────────────────────────
	// Arrastra un asset directamente — C++ lo reproduce sin necesitar nodos BP.

	/** Sonido al abrir la sombrilla. Arrastra tu SoundCue/Wave aquí. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Umbrella|Audio")
	TObjectPtr<USoundBase> SoundOpen;

	/** Sonido al cerrarse la sombrilla (protección expiró). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Umbrella|Audio")
	TObjectPtr<USoundBase> SoundClose;

	/** VFX al abrir la sombrilla (Niagara/Cascade). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Umbrella|FX")
	TObjectPtr<UParticleSystem> VFXOpen;

	/** VFX al cerrarse la sombrilla. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Umbrella|FX")
	TObjectPtr<UParticleSystem> VFXClose;

	// ── Eventos BP (opcional si quieres lógica adicional) ─────────────────────

	/** Llamado en TODAS las máquinas cuando la sombrilla se abre.
	 *  Usar para lógica BP extra (animación, HUD, etc.). Audio/VFX ya se reproducen solos. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Umbrella")
	void OnUmbrellaOpened(APawn* User);

	/** Llamado en TODAS las máquinas cuando la sombrilla se cierra.
	 *  Usar para lógica BP extra. Audio/VFX ya se reproducen solos. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Umbrella")
	void OnUmbrellaClosed();

private:
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastOnUmbrellaOpened(APawn* User);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastOnUmbrellaClosed();

	void HandleUmbrellaExpired();

	FTimerHandle UmbrellaActiveTimerHandle;
	TWeakObjectPtr<ATortugaCharacter> ProtectedCharacter;
};
