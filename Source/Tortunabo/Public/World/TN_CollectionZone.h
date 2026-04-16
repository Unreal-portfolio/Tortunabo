#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TN_CollectionZone.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class ATortugaCharacter;

/**
 * Zona de recogida (#28).
 *
 * Los jugadores llevan objetos al depósito en esta zona.
 * Al depositar (overlap mientras lleva un objeto):
 *   - Se elimina el objeto del inventario del jugador.
 *   - Se incrementa el contador DepositedCount.
 *   - Si DepositedCount >= RequiredCount: se dispara OnGoalReached() en todas las máquinas.
 *   - Opcionalmente aplica transforms a actores objetivo (puerta que se abre, etc.).
 *
 * "Depositar objeto" = jugador lleva el ítem correcto (CollectibleTag) y hace overlap.
 *
 * NOTA: Si CollectibleTag == NAME_None → acepta cualquier ítem equipado.
 *
 * Uso:
 *   1. Crear BP_CollectionZone hijo.
 *   2. Asignar TargetActors y sus transforms.
 *   3. Ajustar RequiredCount.
 *   4. Override OnItemDeposited / OnGoalReached en BP para VFX/audio.
 */
UCLASS(Blueprintable)
class TORTUNABO_API ATN_CollectionZone : public AActor
{
	GENERATED_BODY()

public:
	ATN_CollectionZone();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CollectionZone")
	TObjectPtr<UBoxComponent> DepositBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CollectionZone")
	TObjectPtr<UStaticMeshComponent> BaseMesh;

	/** Número de depósitos necesarios para activar el evento. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CollectionZone", meta = (ClampMin = "1"))
	int32 RequiredCount = 3;

	/**
	 * Tag del ítem aceptado (FName del UseType o del DisplayName en el ItemTable).
	 * NAME_None = acepta cualquier ítem equipado.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CollectionZone")
	FName CollectibleTag = NAME_None;

	/** Actores que se moverán cuando se alcance el objetivo. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CollectionZone")
	TArray<TObjectPtr<AActor>> TargetActors;

	/** Offset de posición aplicado a cada TargetActor al activar. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CollectionZone")
	FVector ActivationLocationOffset = FVector::ZeroVector;

	/** Offset de rotación aplicado a cada TargetActor al activar. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CollectionZone")
	FRotator ActivationRotationOffset = FRotator::ZeroRotator;

	/** Llamado en TODAS las máquinas cuando se deposita un ítem. */
	UFUNCTION(BlueprintImplementableEvent, Category = "CollectionZone")
	void OnItemDeposited(APawn* Depositor, int32 NewCount, int32 Needed);

	/** Llamado en TODAS las máquinas cuando se alcanza RequiredCount. */
	UFUNCTION(BlueprintImplementableEvent, Category = "CollectionZone")
	void OnGoalReached();

	/** Depósitos actuales (replicado para HUD). */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_DepositedCount, Category = "CollectionZone")
	int32 DepositedCount = 0;

private:
	bool bGoalReached = false;

	UFUNCTION()
	void OnRep_DepositedCount();

	UFUNCTION()
	void OnBoxBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

	bool CheckPlayerHasItem(ATortugaCharacter* Char) const;
	bool ConsumeItemFromPlayer(ATortugaCharacter* Char);
	void ApplyTargetTransforms();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastOnDeposit(APawn* Depositor, int32 NewCount, int32 Needed);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastOnGoalReached();
};
