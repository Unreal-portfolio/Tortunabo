#pragma once

#include "CoreMinimal.h"
#include "TN_InventoryTypes.generated.h"

class UStaticMesh;

USTRUCT(BlueprintType)
struct FTN_InventoryItem
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	FName ItemId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	TObjectPtr<UStaticMesh> EquippedMesh = nullptr;

	bool IsValid() const
	{
		return ItemId != NAME_None;
	}
};

