#pragma once

#include "CoreMinimal.h"
#include "TN_InventoryTypes.generated.h"

class UStaticMesh;
class ATN_PickupInteractableBase;
class ATN_ThrowableItemActor;

UENUM(BlueprintType)
enum class ETN_ItemUseType : uint8
{
	None UMETA(DisplayName = "None"),
	SelfStaminaBoost UMETA(DisplayName = "Self Stamina Boost"),
	Throwable UMETA(DisplayName = "Throwable")
};

USTRUCT(BlueprintType)
struct FTN_InventoryItem
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	FName ItemId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	TObjectPtr<UStaticMesh> EquippedMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Use")
	ETN_ItemUseType UseType = ETN_ItemUseType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Use", meta = (ClampMin = "0.0"))
	float StaminaUnlimitedDurationSeconds = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Use", meta = (ClampMin = "0.0"))
	float ThrowSpeed = 1800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|World")
	TSubclassOf<ATN_PickupInteractableBase> PickupActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|World")
	TSubclassOf<ATN_ThrowableItemActor> ThrowableActorClass;

	bool IsValid() const
	{
		return ItemId != NAME_None;
	}
};

