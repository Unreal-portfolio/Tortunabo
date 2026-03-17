#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "Core/TN_MatchFlowTypes.h"
#include "TN_CoopGameState.generated.h"

UCLASS()
class TORTUNABO_API ATN_CoopGameState : public AGameState
{
	GENERATED_BODY()

public:
	ATN_CoopGameState();

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Coop")
	ETNMatchFlowState MatchFlowState = ETNMatchFlowState::WaitingForPlayers;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Coop")
	int32 ReadyPlayers = 0;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Coop")
	int32 ExpectedPlayers = 4;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Coop")
	int32 CountdownValue = 0;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Coop")
	float ServerMatchElapsedTime = 0.f;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Coop")
	int32 FinishedPlayers = 0;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};

