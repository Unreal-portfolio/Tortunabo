#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "TN_CoopPlayerState.generated.h"

UCLASS()
class TORTUNABO_API ATN_CoopPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	ATN_CoopPlayerState();

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Coop")
	bool bIsInReadyZone = false;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Coop")
	bool bHasFinishedRun = false;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Coop")
	bool bIsAlive = true;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Coop")
	float DeathZoneTimeRemaining = -1.f;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Cosmetics")
	FName EquippedHelmetId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Coop")
	float FinishTimeSeconds = -1.f;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Coop")
	int32 FinishRank = 0;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};

