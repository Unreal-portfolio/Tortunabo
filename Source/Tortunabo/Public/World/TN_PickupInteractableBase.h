#pragma once

#include "CoreMinimal.h"
#include "Core/TN_InventoryTypes.h"
#include "World/TN_InteractableBase.h"
#include "TN_PickupInteractableBase.generated.h"

class UTN_InventoryComponent;

UCLASS()
class TORTUNABO_API ATN_PickupInteractableBase : public ATN_InteractableBase
{
	GENERATED_BODY()

public:
	ATN_PickupInteractableBase();

	virtual bool CanInteract(APawn* Interactor) const override;
	virtual void Interact(APawn* Interactor) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category = "Pickup")
	void InitializeFromInventoryItem(const FTN_InventoryItem& NewPickupItem);

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup")
	FTN_InventoryItem PickupItem;

	UPROPERTY(ReplicatedUsing = OnRep_Taken, BlueprintReadOnly, Category = "Pickup")
	bool bTaken = false;

	UFUNCTION(BlueprintImplementableEvent, Category = "Pickup")
	void OnPickedUp(APawn* Interactor);

private:
	UFUNCTION()
	void OnRep_Taken();

	void ApplyTakenState();
};

