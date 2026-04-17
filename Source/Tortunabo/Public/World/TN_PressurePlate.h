#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/TN_ButtonGroupManager.h"   // FTN_TransformAction
#include "TN_PressurePlate.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class USoundBase;
class ATortugaCharacter;

/**
 * Placa de activación (#30).
 *
 * Cada placa detecta si tiene un jugador vivo encima.
 * Úsalas junto con ATN_PressurePlateGroupManager (o el BacklogItem #30 gestor)
 * para activar eventos cuando TODOS los jugadores vivos estén sobre placas.
 *
 * La lógica de "todos activados → evento" reside en TN_PressurePlateGroupManager.
 *
 * Uso:
 *   1. Crear BP_PressurePlate, asignar mesh, ajustar TriggerBox.
 *   2. Implementar OnPlatePressed / OnPlateReleased en BP para VFX.
 *   3. Referenciar las placas desde TN_PressurePlateGroupManager.
 */
UCLASS(Blueprintable)
class TORTUNABO_API ATN_PressurePlate : public AActor
{
	GENERATED_BODY()

public:
	ATN_PressurePlate();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** True si hay al menos un jugador vivo encima en este momento. */
	UFUNCTION(BlueprintPure, Category = "PressurePlate")
	bool IsOccupied() const { return bOccupied; }

	/** Delegate servidor-only para que el GroupManager escuche cambios. */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnOccupancyChanged, ATN_PressurePlate*, bool);
	FOnOccupancyChanged OnOccupancyChanged;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PressurePlate")
	TObjectPtr<UStaticMeshComponent> PlateMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PressurePlate")
	TObjectPtr<UBoxComponent> TriggerBox;

	/** Duración mínima que todos deben estar en sus placas para activar el evento (s). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PressurePlate",
		meta = (ClampMin = "0.0"))
	float HoldDurationRequired = 0.f;  // 0 = activación inmediata

	/** Sonido al pisar la placa. */
	UPROPERTY(EditDefaultsOnly, Category = "PressurePlate|Audio")
	TObjectPtr<USoundBase> PressSound;

	/** Sonido al liberar la placa. */
	UPROPERTY(EditDefaultsOnly, Category = "PressurePlate|Audio")
	TObjectPtr<USoundBase> ReleaseSound;

private:
	UPROPERTY(ReplicatedUsing = OnRep_bOccupied)
	bool bOccupied = false;

	UFUNCTION()
	void OnRep_bOccupied();

	/** Conjunto de personajes vivos actualmente sobre la placa. */
	TSet<TWeakObjectPtr<ATortugaCharacter>> OccupyingCharacters;

	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	void RefreshOccupancy();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastOnOccupancyChanged(bool bNewOccupied);
};

/**
 * Gestor de grupo de placas (#30).
 *
 * Monitorea un array de ATN_PressurePlate.
 * Condición de activación: PlayersOnPlates == NumAlivePlayers.
 *   (Todos los jugadores vivos deben estar encima de UNA placa — se permiten
 *    varias en la misma placa si hay menos vivos que placas.)
 *
 * Si HoldDurationRequired > 0: todos deben mantener la condición ese tiempo.
 * Al cumplirse: dispara TriggerActions (igual que TN_ButtonGroupManager).
 */
UCLASS(Blueprintable)
class TORTUNABO_API ATN_PressurePlateGroupManager : public AActor
{
	GENERATED_BODY()

public:
	ATN_PressurePlateGroupManager();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlateGroup")
	TArray<TObjectPtr<ATN_PressurePlate>> ManagedPlates;

	/**
	 * Acciones aplicadas cuando la condición se cumple.
	 * Reusa el struct FTN_TransformAction del ButtonGroupManager.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlateGroup")
	TArray<FTN_TransformAction> TriggerActions;

	/** Segundos que los jugadores deben mantener todas las placas activas para disparar. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlateGroup",
		meta = (ClampMin = "0.0"))
	float HoldDurationRequired = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlateGroup")
	bool bOneShot = true;

	/** Sonido al cumplirse la condición de todas las placas. */
	UPROPERTY(EditDefaultsOnly, Category = "PlateGroup|Audio")
	TObjectPtr<USoundBase> ConditionMetSound;

	/** Sonido al perderse la condición. */
	UPROPERTY(EditDefaultsOnly, Category = "PlateGroup|Audio")
	TObjectPtr<USoundBase> ConditionLostSound;

private:
	bool bTriggered    = false;
	bool bConditionMet = false;

	FTimerHandle HoldTimerHandle;

	void OnOccupancyChanged(ATN_PressurePlate* Plate, bool bOccupied);
	void EvaluateAndHandleCondition();
	bool EvaluateCondition() const;
	void OnHoldTimerExpired();
	void ApplyTriggerActions();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastNotifyConditionChange(bool bMet);
};
