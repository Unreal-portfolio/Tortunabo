#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TN_ScorePickup.generated.h"

class UStaticMeshComponent;
class USphereComponent;

/**
 * Coleccionable de puntuación (#27).
 *
 * Un objeto disperso por el nivel. Al pisarlo / solaparse con él:
 *   - El servidor suma ScoreValue a TN_CoopPlayerState::RaceScore del recogedor.
 *   - Se destruye (o se oculta si bRespawn=true).
 *   - Llama OnPickedUp() en todas las máquinas para VFX/audio.
 *
 * Uso:
 *   1. Crear BP hijo, asignar StaticMesh y efectos.
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

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ScorePickup")
	TObjectPtr<UStaticMeshComponent> PickupMesh;

	/** Radio de recogida (cm). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ScorePickup")
	TObjectPtr<USphereComponent> CollectSphere;

	/**
	 * Concha de vieira que gira como una moneda de plataformas clásico, sube y baja y brilla (M_ProcGlow), con
	 * destellos alrededor. Se usa mientras PickupMesh no tenga una malla propia (la de ayuda del motor cuenta como
	 * vacía); si arte le pone una, se ve esa y la concha no.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ScorePickup")
	TObjectPtr<UStaticMeshComponent> ShellMesh;

	/** Vueltas por segundo del giro de la concha. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ScorePickup", meta = (ClampMin = "0.0"))
	float SpinTurnsPerSecond = 0.45f;

	/** Puntos que se suman al RaceScore del jugador que lo recoge. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ScorePickup", meta = (ClampMin = "1"))
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

	/** Reloj del giro y del balanceo (cada concha desfasada). */
	float SpinTime = 0.f;

	UFUNCTION()
	void OnRep_bActive();

	UFUNCTION()
	void OnSphereOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPickedUp(APawn* Collector);

	void ApplyActiveState(bool bNowActive);
	void Respawn();

	FTimerHandle RespawnTimerHandle;
};
