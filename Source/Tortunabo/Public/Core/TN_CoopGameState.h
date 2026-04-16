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

	/** Appends a Quick Chat entry on the server and notifies all clients via Multicast. */
	void AddQuickChatEntry(int32 SenderPlayerId, uint8 MessageID, float ServerTimeSeconds);

	/** Envía una entrada individual a todos los clientes (más eficiente que replicar el array completo). */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastNewChatEntry(FTN_QuickChatEntry Entry);

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
	 * Historial local de los últimos MaxQuickChatEntries mensajes.
	 * Mantenido localmente en todas las máquinas via MulticastNewChatEntry.
	 * Ya no se replica como array completo — ahorro de ~130 bytes por mensaje.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "QuickChat")
	TArray<FTN_QuickChatEntry> QuickChatHistory;

	/** Maximum number of entries kept in QuickChatHistory. */
	static constexpr int32 MaxQuickChatEntries = 10;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION()
	void OnRep_MatchFlowState();

	/** Saves the local player's RaceScore to the save game when Results state is entered.
	 *  Idempotente: usa bLocalScorePersisted para evitar doble-escritura si se llama
	 *  varias veces en el mismo ciclo de Results (ej. BroadcastFlowStateChange + OnRep). */
	void PersistLocalPlayerScoreIfResults();

	/** True después de la primera llamada exitosa a PersistLocalPlayerScoreIfResults en Results.
	 *  Se resetea en BeginPlay para que una nueva run pueda persistir su propio score. */
	bool bLocalScorePersisted = false;

	int32 NextQuickChatSequence = 0;
	int32 LastProcessedQuickChatSequence = 0;
};
