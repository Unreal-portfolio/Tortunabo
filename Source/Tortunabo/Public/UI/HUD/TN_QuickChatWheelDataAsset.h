#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Misc/DataValidation.h"
#include "UI/HUD/TN_RadialWheelTypes.h"
#include "TN_QuickChatWheelDataAsset.generated.h"

class UTexture2D;

USTRUCT(BlueprintType)
struct FTN_QuickChatWheelEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "QuickChat")
	uint8 MessageID = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "QuickChat")
	FText Text;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "QuickChat")
	TObjectPtr<UTexture2D> Icon = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "QuickChat", meta = (ClampMin = "0.0"))
	float CooldownOverride = 0.f;
};

UCLASS(BlueprintType)
class TORTUNABO_API UTN_QuickChatWheelDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "QuickChat")
	TArray<FTN_QuickChatWheelEntry> Entries;

	const FTN_QuickChatWheelEntry* FindEntryById(uint8 MessageID) const;

	UFUNCTION(BlueprintPure, Category = "QuickChat")
	TArray<FTN_RadialWheelEntryView> BuildWheelEntries() const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};

