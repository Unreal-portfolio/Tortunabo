#pragma once

#include "CoreMinimal.h"
#include "Core/TN_InventoryTypes.h"
#include "World/TN_InteractableBase.h"
#include "TN_PickupInteractableBase.generated.h"

class UTN_InventoryComponent;
class UDataTable;

UCLASS()
class TORTUNABO_API ATN_PickupInteractableBase : public ATN_InteractableBase
{
	GENERATED_BODY()

public:
	ATN_PickupInteractableBase();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual bool CanInteract(APawn* Interactor) const override;
	virtual void Interact(APawn* Interactor) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category = "Pickup")
	void InitializeFromInventoryItem(const FTN_InventoryItem& NewPickupItem);

protected:
	/**
	 * [Data-driven] DataTable con filas de tipo FTN_InventoryItem.
	 * Asigna DT_Items en BP_GenericPickup → Class Defaults.
	 * Si está relleno junto con ItemRowName, el pickup se auto-configura en BeginPlay.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup|DataTable")
	TObjectPtr<UDataTable> ItemDataTable;

	/**
	 * Nombre de la fila en ItemDataTable que define este ítem.
	 * Ejemplo: "Item_StaminaBoost", "Item_ThrowableBall".
	 * Si se deja vacío, usa los datos de PickupItem directamente (modo manual).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup|DataTable")
	FName ItemRowName = NAME_None;

	/**
	 * Datos del ítem. ReplicatedUsing: clientes aplican la mesh correcta cuando
	 * el pickup se spawnea dinámicamente (ej. bola que aterriza).
	 * Para pickups pre-colocados en nivel, el BP default ya lleva el mesh.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, ReplicatedUsing = OnRep_PickupItem, Category = "Pickup")
	FTN_InventoryItem PickupItem;

	UPROPERTY(ReplicatedUsing = OnRep_Taken, BlueprintReadOnly, Category = "Pickup")
	bool bTaken = false;

	UFUNCTION(BlueprintImplementableEvent, Category = "Pickup")
	void OnPickedUp(APawn* Interactor);

private:
	UFUNCTION()
	void OnRep_Taken();

	UFUNCTION()
	void OnRep_PickupItem();

	void ApplyTakenState();
	void HandleDeferredDestroy();
	void HandleRestoreDormancy();

	FTimerHandle DormancyTimerHandle;
};

