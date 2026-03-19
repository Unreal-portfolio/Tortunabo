#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/TN_InventoryTypes.h"
#include "TN_InventoryComponent.generated.h"

class UStaticMeshComponent;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class TORTUNABO_API UTN_InventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTN_InventoryComponent();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool TryAddItem(const FTN_InventoryItem& NewItem);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool TryAddOrReplaceEquipped(const FTN_InventoryItem& NewItem, bool bReplaceIfFull = true);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool CanReceiveItem(const FTN_InventoryItem& NewItem, bool bAllowReplaceIfFull = true) const;

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void RotateItems();

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool TryConsumeEquippedItem(FTN_InventoryItem& OutConsumedItem);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool TryExtractEquippedItem(FTN_InventoryItem& OutExtractedItem);

	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool HasEquippedItem() const { return bHasEquippedItem; }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool HasStoredItem() const { return bHasStoredItem; }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	FTN_InventoryItem GetEquippedItem() const { return EquippedItem; }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	FTN_InventoryItem GetStoredItem() const { return StoredItem; }

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Visual")
	FName EquippedAttachSocket = NAME_None;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Visual")
	FVector EquippedRelativeLocation = FVector(20.f, 0.f, 40.f);

	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Visual")
	FRotator EquippedRelativeRotation = FRotator::ZeroRotator;

private:
	UFUNCTION(Server, Reliable)
	void ServerRotateItems();

	UPROPERTY(ReplicatedUsing = OnRep_EquippedItem)
	FTN_InventoryItem EquippedItem;

	UPROPERTY(Replicated)
	bool bHasEquippedItem = false;

	UPROPERTY(ReplicatedUsing = OnRep_StoredItem)
	FTN_InventoryItem StoredItem;

	UPROPERTY(Replicated)
	bool bHasStoredItem = false;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> EquippedVisualMesh;

	/** Componente padre del visual equipado. Guardado para compensar escala en RefreshEquippedVisual. */
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> VisualMeshParent;

	UFUNCTION()
	void OnRep_EquippedItem();

	UFUNCTION()
	void OnRep_StoredItem();

	void RefreshEquippedVisual();
	bool AddItemInternal(const FTN_InventoryItem& NewItem);
	bool AddOrReplaceEquippedInternal(const FTN_InventoryItem& NewItem, bool bReplaceIfFull);
	bool ConsumeEquippedInternal(FTN_InventoryItem& OutItem);
	void SwapSlotsInternal();
};

