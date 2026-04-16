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
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ScorePickup")
	TObjectPtr<UStaticMeshComponent> PickupMesh;

	/** Radio de recogida (cm). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ScorePickup")
	TObjectPtr<USphereComponent> CollectSphere;

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
