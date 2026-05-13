#pragma once

#include "CoreMinimal.h"
#include "TN_RadialWheelTypes.generated.h"

class UTexture2D;

/** @brief Datos visuales de una entrada del wheel radial (no contiene lógica de gameplay). */
USTRUCT(BlueprintType)
struct FTN_RadialWheelEntryView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Radial")
	uint8 EntryId = 0;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Radial")
	FText Label;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Radial")
	TObjectPtr<UTexture2D> Icon = nullptr;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Radial")
	bool bEnabled = true;
};

/** @brief Tipos de rueda radial soportados por el PlayerController. */
UENUM(BlueprintType)
enum class ETN_RadialWheelType : uint8
{
	None,
	Emote,
	QuickChat
};

