#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "Core/TN_MatchFlowTypes.h"
#include "TN_CoopGameState.generated.h"

// Fired on clients when MatchFlowState replicates, and manually on server via BroadcastFlowStateChange()
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMatchFlowStateChanged, ETNMatchFlowState, NewState);

/** Fired on all machines whenever a new Quick Chat message arrives. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuickChatReceived, const FTN_QuickChatEntry&, Entry);

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

	/** Appends a Quick Chat entry on the server and notifies all clients via replication. */
	void AddQuickChatEntry(int32 SenderPlayerId, uint8 MessageID, float ServerTimeSeconds);

	UFUNCTION(BlueprintPure, Category = "QuickChat")
	FText ResolveQuickChatSenderName(int32 SenderPlayerId) const;

	// ── Match Flow ────────────────────────────────────────────────────────────
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

	// ── Quick Chat ────────────────────────────────────────────────────────────
	/** Broadcast on all machines when a new Quick Chat message arrives. Bind in BP or C++. */
	UPROPERTY(BlueprintAssignable, Category = "QuickChat")
	FOnQuickChatReceived OnQuickChatReceived;

	/**
	 * Rolling history of the last MaxQuickChatEntries messages.
	 * Replicated to all clients. OnRep fires the OnQuickChatReceived delegate.
	 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_QuickChatHistory, Category = "QuickChat")
	TArray<FTN_QuickChatEntry> QuickChatHistory;

	/** Maximum number of entries kept in QuickChatHistory. */
	static constexpr int32 MaxQuickChatEntries = 10;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION()
	void OnRep_MatchFlowState();

	UFUNCTION()
	void OnRep_QuickChatHistory();

	int32 NextQuickChatSequence = 0;
	int32 LastProcessedQuickChatSequence = 0;
};
