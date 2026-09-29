#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/TN_ScoreShells.h"
#include "TN_ScorePickup.generated.h"

class UStaticMeshComponent;
class USphereComponent;
class UPointLightComponent;

/**
 * Coleccionable de puntuación (#27): la concha de puntos.
 *
 * Un objeto disperso por el nivel. Al pisarlo / solaparse con él:
 *   - El servidor suma ScoreValue a TN_CoopPlayerState::RaceScore del recogedor.
 *   - Avisa con ATN_CoopPlayerState::MulticastScoreShellCollected: cada máquina hace el estallido (destello, chispas y
 *     «¡plin!», ATN_ScoreShellBurst) y la del recogedor, la animación de su contador en el HUD.
 *   - Se destruye (o se oculta si bRespawn=true).
 *   - Llama OnPickedUp() (evento de Blueprint).
 *
 * Tamaños (TNScoreShells, según ScoreValue): pequeña de 1, normal de 25 (la de siempre), grande de 50 y reina de 100;
 * cada uno con su tamaño, color, radio de recogida y adornos (las grandes y las reinas llevan halo, destellos de su
 * color, luz y una columna de luz que se ve de lejos). El mapa procedural pone el valor con SetScoreValue.
 *
 * Uso:
 *   1. Crear BP hijo, asignar StaticMesh y efectos (sin malla propia se ve la vieira de código).
 *   2. Ajustar ScoreValue según el diseño.
 *   3. Colocar en el nivel o en chunks.
 */
UCLASS(Blueprintable)
class TORTUNABO_API ATN_ScorePickup : public AActor
{
	GENERATED_BODY()

public:
	ATN_ScorePickup();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * Servidor: cambia los puntos que da y, con ellos, el tamaño y el aspecto (se replica a los clientes). Mejor antes
	 * de FinishSpawning (SpawnActorDeferred), así la concha nace ya con su aspecto en todas las máquinas.
	 */
	void SetScoreValue(int32 NewValue);

	int32 GetScoreValue() const { return ScoreValue; }

	/** Tamaño que corresponde a ScoreValue. */
	TNScoreShells::ETier GetShellTier() const { return TNScoreShells::TierForValue(ScoreValue); }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ScorePickup")
	TObjectPtr<UStaticMeshComponent> PickupMesh;

	/** Radio de recogida (cm): el de su tamaño (TNScoreShells::CollectRadius). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ScorePickup")
	TObjectPtr<USphereComponent> CollectSphere;

	/**
	 * Concha de vieira que gira como una moneda de plataformas clásico, sube y baja y brilla (M_ProcGlow), con
	 * destellos alrededor. Se usa mientras PickupMesh no tenga una malla propia (la de ayuda del motor cuenta como
	 * vacía); si arte le pone una, se ve esa y la concha no.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ScorePickup")
	TObjectPtr<UStaticMeshComponent> ShellMesh;

	/** Vueltas por segundo del giro de la concha normal (las demás, las de su tamaño). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ScorePickup", meta = (ClampMin = "0.0"))
	float SpinTurnsPerSecond = 0.45f;

	/** Puntos que se suman al RaceScore del jugador que lo recoge (y, con ellos, el tamaño de la concha). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, ReplicatedUsing = OnRep_ScoreValue, Category = "ScorePickup", meta = (ClampMin = "1"))
	int32 ScoreValue = 25;

	/**
	 * Si true, el pickup se oculta tras ser recogido y reaparece después de RespawnSeconds.
	 * Si false, se destruye permanentemente.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ScorePickup")
	bool bRespawn = false;

	/** Segundos hasta que reaparece el pickup (solo si bRespawn=true). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ScorePickup", meta = (ClampMin = "1.0"))
	float RespawnSeconds = 10.f;

	/** Llamado en TODAS las máquinas cuando el pickup es recogido. Override en BP para VFX/audio. */
	UFUNCTION(BlueprintImplementableEvent, Category = "ScorePickup")
	void OnPickedUp(APawn* Collector);

private:
	UPROPERTY(ReplicatedUsing = OnRep_bActive)
	bool bActive = true;

	/** Halo blando, columna de luz y luz de las grandes y las reinas (se crean en ejecución). */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> HaloMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> BeamMesh;

	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> GlowLight;

	/** Reloj del giro y del balanceo (cada concha desfasada). */
	float SpinTime = 0.f;

	/** Cada cuánto se mira si la cámara local está cerca (s) y si lo está: lejos, ni gira ni mueve destellos. */
	float ViewCheckClock = 0.f;
	bool bNearView = true;

	/** Tamaño cuyo aspecto está puesto (255 = ninguno todavía). */
	uint8 AppliedTier = 255;

	UFUNCTION()
	void OnRep_bActive();

	UFUNCTION()
	void OnRep_ScoreValue();

	UFUNCTION()
	void OnSphereOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPickedUp(APawn* Collector);

	/** Pone el aspecto del tamaño actual (malla, escala, radio de recogida, destellos, halo, columna y luz). */
	void ApplyTierLook();

	/** Quita los adornos en ejecución (destellos, halo, columna y luz). */
	void ClearTierExtras();

	void ApplyActiveState(bool bNowActive);
	void Respawn();

	FTimerHandle RespawnTimerHandle;
};
