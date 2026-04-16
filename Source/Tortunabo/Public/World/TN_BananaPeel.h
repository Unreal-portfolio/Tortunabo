#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TN_BananaPeel.generated.h"

class UStaticMeshComponent;
class UBoxComponent;

/**
 * Cáscara de plátano (#6).
 *
 * Trampa pasiva colocada en el suelo.
 * Al pisarla: aplica knockdown al jugador + LaunchCharacter con su velocidad actual
 * (el jugador se desliza en la dirección que llevaba).
 *
 * Autoridad: el servidor detecta el overlap y aplica el efecto.
 *   La cáscara se oculta tras ser pisada y reaparece tras RespawnSeconds.
 *
 * Uso:
 *   1. Crear BP_BananaPeel hijo, asignar un StaticMesh.
 *   2. Colocar en el nivel o spawnear dinámicamente.
 *   3. Si quieres usarla como ítem recogible (ítem en inventario que se coloca),
 *      el pickup spawn llama a SpawnAtLocation() y deja que la cáscara haga su trabajo.
 */
UCLASS(Blueprintable)
class TORTUNABO_API ATN_BananaPeel : public AActor
{
	GENERATED_BODY()

public:
	ATN_BananaPeel();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BananaPeel")
	TObjectPtr<UStaticMeshComponent> PeelMesh;

	/** Trigger de detección. Editar tamaño en el BP hijo. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BananaPeel")
	TObjectPtr<UBoxComponent> TriggerBox;

	/** Duración del knockdown aplicado al jugador (segundos). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "BananaPeel",
		meta = (ClampMin = "0.5"))
	float KnockdownDuration = 2.0f;

	/** Multiplicador del impulso de deslizamiento basado en la velocidad actual del jugador. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "BananaPeel",
		meta = (ClampMin = "0.0"))
	float SlideImpulseMultiplier = 1.5f;

	/** Fuerza mínima de deslizamiento (cm/s) si el jugador está casi parado. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "BananaPeel",
		meta = (ClampMin = "0.0"))
	float MinSlideForce = 600.f;

	/**
	 * Segundos hasta que la cáscara reaparece tras ser pisada. 0 = no reaparece.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "BananaPeel",
		meta = (ClampMin = "0.0"))
	float RespawnSeconds = 10.f;

	/** Llamado en todas las máquinas cuando la cáscara es pisada. Implementar VFX/audio en BP. */
	UFUNCTION(BlueprintImplementableEvent, Category = "BananaPeel")
	void OnPeelTriggered(ATN_BananaPeel* Peel);

private:
	UPROPERTY(ReplicatedUsing = OnRep_bActive)
	bool bActive = true;

	UFUNCTION()
	void OnRep_bActive();

	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

	void ApplyActiveState();
	void Respawn();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastOnTriggered();

	FTimerHandle RespawnTimerHandle;
};
