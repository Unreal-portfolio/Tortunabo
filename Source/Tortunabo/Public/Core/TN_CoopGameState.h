#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "Core/TN_MatchFlowTypes.h"
#include "TN_CoopGameState.generated.h"

// Fired on clients when MatchFlowState replicates, and manually on server via BroadcastFlowStateChange()
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMatchFlowStateChanged, ETNMatchFlowState, NewState);

UCLASS()
class TORTUNABO_API ATN_CoopGameState : public AGameState
{
	GENERATED_BODY()

public:
	ATN_CoopGameState();

	// Called by game modes after changing MatchFlowState on the server so that
	// listen-server local players also receive the notification (OnRep does not
	// fire on the machine that owns the variable).
	void BroadcastFlowStateChange();

	UPROPERTY(BlueprintAssignable, Category = "Coop")
	FOnMatchFlowStateChanged OnMatchFlowStateChanged;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MatchFlowState, Category = "Coop")
	ETNMatchFlowState MatchFlowState = ETNMatchFlowState::WaitingForPlayers;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Coop")
	int32 ReadyPlayers = 0;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Coop")
	int32 ConnectedPlayers = 0;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Coop")
	int32 PlayersInStartZone = 0;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Coop")
	int32 ExpectedPlayers = 4;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Coop")
	int32 CountdownValue = 0;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Coop")
	float ServerMatchElapsedTime = 0.f;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Coop")
	int32 FinishedPlayers = 0;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION()
	void OnRep_MatchFlowState();
};
