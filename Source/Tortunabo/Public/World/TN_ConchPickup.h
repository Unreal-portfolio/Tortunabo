#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TN_ConchPickup.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class USoundBase;
class UNiagaraSystem;
class ATortugaCharacter;

/**
 * Concha marina — trampa pasiva + ítem recogible (#22).
 *
 * MODO ÍTEM (bIsPlacedTrap = false, estado por defecto):
 *   El jugador camina encima → la concha se destruye (consumida como ítem).
 *   La integración con el inventario se realiza desde el sistema de pickup externo.
 *
 * MODO TRAMPA (bIsPlacedTrap = true):
 *   Al pisar la concha → inmoviliza al jugador durante TrapDurationSeconds.
 *   Tras el timer: restaura MOVE_Walking.
 *   Una vez colocada como trampa, no puede volver al modo ítem.
 *
 * PlaceAsTrap(Location) se llama desde el sistema de ítems cuando el jugador
 * equipa la concha y pulsa Interact.
 *
 * Replicación:
 *   bReplicates = true, bAlwaysRelevant = true.
 *   bIsPlacedTrap se replica para que los clientes muestren el visual correcto.
 */
UCLASS(Blueprintable)
class TORTUNABO_API ATN_ConchPickup : public AActor
{
	GENERATED_BODY()

public:
	ATN_ConchPickup();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * Coloca la concha como trampa en la posición indicada.
	 * Debe llamarse en el servidor (lo invoca el sistema de ítems).
	 */
	UFUNCTION(BlueprintCallable, Category = "Conch")
	void PlaceAsTrap(const FVector& WorldLocation);

protected:
	// ── Componentes ──────────────────────────────────────────────────────────

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Conch")
	TObjectPtr<UStaticMeshComponent> ConchMesh;

	/** Zona de detección de jugadores. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Conch")
	TObjectPtr<USphereComponent> OverlapSphere;

	// ── Config ────────────────────────────────────────────────────────────────

	/** Segundos que el jugador queda inmovilizado al pisar la trampa. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Conch",
		meta = (ClampMin = "0.1"))
	float TrapDurationSeconds = 2.5f;

	/** Radio de la esfera de detección (cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Conch",
		meta = (ClampMin = "10.0"))
	float TrapRadius = 60.f;

	/**
	 * Segundos tras liberar a la víctima antes de que la trampa vuelva a poder atrapar.
	 * Evita que la misma víctima se re-atrape inmediatamente al salir del inmovilizado.
	 * Default 1.0s — ajustar en BP si se quiere una trampa de un solo uso (poner a 0 y
	 * destruir la concha al finalizar; actualmente la concha persiste para reutilización).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Conch",
		meta = (ClampMin = "0.0"))
	float ResetCooldownSeconds = 1.0f;

	// ── Estado replicado ──────────────────────────────────────────────────────

	/**
	 * true  → la concha está en el suelo como trampa activa.
	 * false → la concha está en modo ítem recogible.
	 * Se replica para que los clientes puedan actualizar el visual.
	 */
	UPROPERTY(ReplicatedUsing = OnRep_IsPlacedTrap, BlueprintReadOnly, Category = "Conch")
	bool bIsPlacedTrap = false;

	// ── Audio / VFX ───────────────────────────────────────────────────────────

	/** Sonido al colocar la trampa. */
	UPROPERTY(EditDefaultsOnly, Category = "Conch|Audio")
	TObjectPtr<USoundBase> PlaceSound;

	/** Sonido al atrapar a un jugador. */
	UPROPERTY(EditDefaultsOnly, Category = "Conch|Audio")
	TObjectPtr<USoundBase> TrapSound;

	/** VFX al colocar la trampa. */
	UPROPERTY(EditDefaultsOnly, Category = "Conch|VFX")
	TObjectPtr<UNiagaraSystem> PlaceVFX;

	/** VFX al atrapar a un jugador. */
	UPROPERTY(EditDefaultsOnly, Category = "Conch|VFX")
	TObjectPtr<UNiagaraSystem> TrapVFX;

private:
	// ── Overlap ───────────────────────────────────────────────────────────────

	UFUNCTION()
	void OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

	// ── Replicación ───────────────────────────────────────────────────────────

	UFUNCTION()
	void OnRep_IsPlacedTrap();

	// ── Trampa ────────────────────────────────────────────────────────────────

	/** Envía el evento VFX a todos los clientes cuando la trampa se activa. */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastOnTrapped(APawn* Victim);

	/** Restaura el movimiento del jugador tras TrapDurationSeconds. */
	void RestoreMovement(TWeakObjectPtr<ATortugaCharacter> WeakCharacter);

	/** Reproduce sonido y VFX de colocación de trampa en la máquina local. */
	void PlayPlaceEffects();

	/** Re-arma la trampa (bTrapUsed=false) una vez expira el cooldown. */
	void RearmTrap();

	FTimerHandle TrapTimerHandle;
	FTimerHandle RearmTimerHandle;

	/** Evita que la trampa se active dos veces mientras el personaje sigue en overlap. */
	bool bTrapUsed = false;
};
