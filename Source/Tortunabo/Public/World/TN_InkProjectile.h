#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TN_InkProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class ATortugaCharacter;

/**
 * Proyectil de tinta de calamar — ítem lanzable de un uso (#13).
 *
 * Al impactar a un jugador: emite MulticastApplyInkEffect → llama a
 * ATortugaCharacter::ApplyInkEffect(Duration) en la máquina local del afectado.
 *
 * Movimiento: el servidor mueve el actor en Tick (SetActorLocation) con LaunchVelocity.
 * SetReplicateMovement(true) propaga la posición a los clientes.
 *
 * Ciclo de vida: LifetimeSeconds (default 4s) → auto-destroy.
 *
 * Uso:
 *   ATN_InkProjectile* Proj = ATN_InkProjectile::Spawn(this, BP_InkProjectile,
 *       Origin, Direction, 1200.f);
 */
UCLASS(Blueprintable)
class TORTUNABO_API ATN_InkProjectile : public AActor
{
	GENERATED_BODY()

public:
	ATN_InkProjectile();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * Helper estático para spawnear y lanzar el proyectil desde el servidor.
	 * Devuelve nullptr si WorldContextObject no tiene autoridad o si Class es nulo.
	 */
	UFUNCTION(BlueprintCallable, Category = "InkProjectile",
		meta = (WorldContext = "WorldContextObject"))
	static ATN_InkProjectile* Spawn(const UObject* WorldContextObject,
		TSubclassOf<ATN_InkProjectile> Class,
		FVector Origin,
		FVector Direction,
		float Speed);

protected:
	// ── Componentes ──────────────────────────────────────────────────────────

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "InkProjectile")
	TObjectPtr<UStaticMeshComponent> InkMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "InkProjectile")
	TObjectPtr<USphereComponent> CollisionSphere;

	// ── Config ────────────────────────────────────────────────────────────────

	/** Segundos que la mancha de tinta oscurece la pantalla del jugador impactado. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "InkProjectile",
		meta = (ClampMin = "0.1"))
	float InkDurationSeconds = 5.0f;

	/** Radio de la esfera de colisión del proyectil (cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "InkProjectile",
		meta = (ClampMin = "5.0"))
	float ProjectileRadius = 20.f;

	/** Velocidad de lanzamiento por defecto (cm/s). Solo usada como referencia en BP. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "InkProjectile",
		meta = (ClampMin = "100.0"))
	float LaunchSpeed = 1200.f;

	/** Segundos hasta la auto-destrucción si no impacta nada. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "InkProjectile",
		meta = (ClampMin = "0.5"))
	float LifetimeSeconds = 4.0f;

	/** Aceleración vertical aplicada cada tick (cm/s²). 980 ≈ gravedad terrestre.
	 *  0 = trayectoria recta. Valores >0 curvan la tinta hacia abajo en parábola. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "InkProjectile",
		meta = (ClampMin = "0.0"))
	float GravityAcceleration = 980.f;

private:
	// ── Movimiento ────────────────────────────────────────────────────────────

	/** Velocidad actual del proyectil (establecida al spawnear). Replicada para JIP y debug. */
	UPROPERTY(Replicated)
	FVector LaunchVelocity = FVector::ZeroVector;

	// ── Colisión ──────────────────────────────────────────────────────────────

	UFUNCTION()
	void OnSphereHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, FVector NormalImpulse,
		const FHitResult& Hit);

	// ── Red ───────────────────────────────────────────────────────────────────

	/**
	 * Envía el efecto de tinta a todos los clientes.
	 * La implementación comprueba IsLocallyControlled() para filtrar a quién
	 * muestra el overlay.
	 */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastApplyInkEffect(ATortugaCharacter* Target, float Duration);

	// ── Lifecycle ─────────────────────────────────────────────────────────────

	/** Evita múltiples hits en el mismo frame. */
	bool bHasHit = false;
};
