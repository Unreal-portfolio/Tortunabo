#pragma once

#include "CoreMinimal.h"
#include "World/TN_DirectInteractableBase.h"
#include "TN_UmbrellaInteractable.generated.h"

class ATortugaCharacter;
class USoundBase;
class UParticleSystem;
class UStaticMesh;

/**
 * Sombrilla interactuable (#29).
 *
 * Al pulsar E:
 *   - Activa bHasUmbrellaProtection en el TortugaCharacter del interactor.
 *   - La protección dura UmbrellaDurationSeconds.
 *   - Después se cierra sola (bHasUmbrellaProtection = false).
 *   - La sombrilla entra en cooldown (CooldownSeconds de la base, o ReuseDelay).
 *
 * Mesh visual:
 *   - MeshClosed: mesh cuando está plegada (default: Cylinder).
 *   - MeshOpen:   mesh cuando está desplegada (default: Cone).
 *   - Si se asignan en el BP hijo, se usan los assets personalizados.
 *   - El swap de mesh ocurre en TODAS las máquinas vía NetMulticast.
 *
 * La gaviota dinámica (TN_EnemySeagull) comprueba bHasUmbrellaProtection
 * antes de matar al jugador. Si está activo, cancela el ataque.
 *
 * Uso:
 *   1. Crear BP_UmbrellaInteractable como hijo.
 *   2. (Opcional) Asignar MeshClosed/MeshOpen con tus propios assets.
 *   3. (Opcional) Asignar SoundOpen/SoundClose/VFXOpen/VFXClose.
 *   4. Colocar en el nivel o en chunks.
 */
UCLASS(Blueprintable)
class TORTUNABO_API ATN_UmbrellaInteractable : public ATN_DirectInteractableBase
{
	GENERATED_BODY()

public:
	ATN_UmbrellaInteractable();

	virtual void BeginPlay() override;
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

	// ── Meshes de estado ──────────────────────────────────────────────────────
	// Si se dejan vacíos, C++ usa Cylinder (cerrada) y Cone (abierta) por defecto.

	/** Mesh cuando la sombrilla está plegada/cerrada. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Umbrella|Mesh")
	TObjectPtr<UStaticMesh> MeshClosed;

	/** Mesh cuando la sombrilla está desplegada/abierta. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Umbrella|Mesh")
	TObjectPtr<UStaticMesh> MeshOpen;

	// ── Audio/VFX automáticos ─────────────────────────────────────────────────

	/** Sonido al abrir la sombrilla. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Umbrella|Audio")
	TObjectPtr<USoundBase> SoundOpen;

	/** Sonido al cerrarse la sombrilla (protección expiró). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Umbrella|Audio")
	TObjectPtr<USoundBase> SoundClose;

	/** VFX al abrir la sombrilla. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Umbrella|FX")
	TObjectPtr<UParticleSystem> VFXOpen;

	/** VFX al cerrarse la sombrilla. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Umbrella|FX")
	TObjectPtr<UParticleSystem> VFXClose;

	// ── Eventos BP opcionales ─────────────────────────────────────────────────

	/** Llamado en TODAS las máquinas cuando la sombrilla se abre.
	 *  Mesh, audio y VFX ya se gestionan solos. Usar para lógica BP extra. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Umbrella")
	void OnUmbrellaOpened(APawn* User);

	/** Llamado en TODAS las máquinas cuando la sombrilla se cierra.
	 *  Mesh, audio y VFX ya se gestionan solos. Usar para lógica BP extra. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Umbrella")
	void OnUmbrellaClosed();

private:
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastOnUmbrellaOpened(APawn* User);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastOnUmbrellaClosed();

	void HandleUmbrellaExpired();

	/** Aplica un mesh al componente Mesh heredado. No hace nada si NewMesh es null. */
	void ApplyUmbrellaMesh(UStaticMesh* NewMesh);

	FTimerHandle UmbrellaActiveTimerHandle;
	TWeakObjectPtr<ATortugaCharacter> ProtectedCharacter;
};
