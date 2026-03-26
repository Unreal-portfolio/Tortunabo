#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "TN_CosmeticSaveGame.generated.h"

UCLASS()
class TORTUNABO_API UTN_CosmeticSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, Category = "Cosmetics")
	TArray<FName> UnlockedHelmetIds;

	UPROPERTY(BlueprintReadWrite, Category = "Cosmetics")
	FName EquippedHelmetId = NAME_None;

	/** ID del skin de personaje activo. NAME_None = aspecto por defecto. */
	UPROPERTY(BlueprintReadWrite, Category = "Cosmetics")
	FName EquippedSkinId = NAME_None;
};

