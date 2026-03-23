#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Misc/DataValidation.h"
#include "UI/HUD/TN_RadialWheelTypes.h"
#include "TN_EmoteWheelDataAsset.generated.h"

class UTexture2D;
class UAnimMontage;

USTRUCT(BlueprintType)
struct FTN_EmoteWheelEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Emote")
	uint8 EmoteID = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Emote")
	FText Name;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Emote")
	TObjectPtr<UTexture2D> Icon = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Emote")
	TObjectPtr<UAnimMontage> Montage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Emote", meta = (ClampMin = "0.0"))
	float Cooldown = 0.5f;
};

UCLASS(BlueprintType)
class TORTUNABO_API UTN_EmoteWheelDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Emote")
	TArray<FTN_EmoteWheelEntry> Entries;

	const FTN_EmoteWheelEntry* FindEntryById(uint8 EmoteID) const;

	UFUNCTION(BlueprintPure, Category = "Emote")
	TArray<FTN_RadialWheelEntryView> BuildWheelEntries() const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};

