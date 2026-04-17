#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TN_BananaPeel.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class USoundBase;
class UNiagaraSystem;

/**
 * Cáscara de plátano (#6).
 *
 * Trampa pasiva colocada en el suelo.
 * Al pisarla: aplica knockdown al jugador + LaunchCharacter con su velocidad actual
 * (el jugador se desliza en la dirección que llevaba).
 *
 * La cáscara se DESTRUYE tras ser pisada — sin respawn.
 *
 * Uso:
 *   1. Crear BP_BananaPeel hijo, asignar un StaticMesh, asignar TriggerSound y TriggerVFX.
 *   2. Colocar en el nivel o spawnear dinámicamente desde el sistema de ítems.
 */
UCLASS(Blueprintable)
class TORTUNABO_API ATN_BananaPeel : public AActor
{
	GENERATED_BODY()

public:
	ATN_BananaPeel();

	virtual void BeginPlay() override;

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

	/** Sonido reproducido al activar la trampa. */
	UPROPERTY(EditDefaultsOnly, Category = "BananaPeel|Audio")
	TObjectPtr<USoundBase> TriggerSound;

	/** VFX spawneado en el punto de la cáscara al activarla. */
	UPROPERTY(EditDefaultsOnly, Category = "BananaPeel|VFX")
	TObjectPtr<UNiagaraSystem> TriggerVFX;

private:
	/** Evita doble activación si el overlap llega dos veces antes de la destrucción. */
	bool bTriggered = false;

	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastOnTriggered(FVector Location);
};
