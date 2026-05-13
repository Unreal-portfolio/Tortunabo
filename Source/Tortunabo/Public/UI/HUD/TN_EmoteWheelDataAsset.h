#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Misc/DataValidation.h"
#include "UI/HUD/TN_RadialWheelTypes.h"
#include "TN_EmoteWheelDataAsset.generated.h"

class UTexture2D;
class UAnimMontage;

/** @brief Entrada del catálogo de emotes: id, nombre, icono, montage de animación y cooldown. */
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

/**
 * @brief DataAsset con el catálogo de emotes disponibles para la rueda radial.
 *        El PlayerController lo asigna desde Class Defaults; SendEmoteById resuelve por ID.
 */
UCLASS(BlueprintType)
class TORTUNABO_API UTN_EmoteWheelDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Emote")
	TArray<FTN_EmoteWheelEntry> Entries;

	/** @brief Devuelve el puntero a la entrada con EmoteID dado, o nullptr si no existe. */
	const FTN_EmoteWheelEntry* FindEntryById(uint8 EmoteID) const;

	/** @brief Construye el array de vistas (FTN_RadialWheelEntryView) consumibles por el widget. */
	UFUNCTION(BlueprintPure, Category = "Emote")
	TArray<FTN_RadialWheelEntryView> BuildWheelEntries() const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};

