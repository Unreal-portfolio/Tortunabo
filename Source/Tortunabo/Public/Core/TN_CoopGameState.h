#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "Core/TN_MatchFlowTypes.h"
#include "TN_CoopGameState.generated.h"

/** @brief Disparado en clientes cuando MatchFlowState replica, y manualmente en server vía BroadcastFlowStateChange(). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMatchFlowStateChanged, ETNMatchFlowState, NewState);

/** @brief Disparado en todas las máquinas cuando llega un nuevo mensaje de Quick Chat. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuickChatReceived, const FTN_QuickChatEntry&, Entry);

/** @brief Disparado en todas las máquinas cuando el array global RaceResults se actualiza. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRaceResultsUpdated);

/**
 * @brief GameState replicado: fuente de verdad del estado de partida.
 *
 * Centraliza datos que todos los clientes necesitan ver:
 *  - MatchFlowState (Waiting/Countdown/Cinematic/InProgress/Results).
 *  - Conteos de jugadores (ConnectedPlayers, ReadyPlayers, FinishedPlayers, etc.).
 *  - ServerMatchElapsedTime para el cronómetro de carrera.
 *  - RaceResults: scoreboard global completo replicado al pasar a Results.
 *  - QuickChatHistory: últimos N mensajes mantenidos localmente vía multicast.
 *
 * @note OnRep no dispara en la máquina dueña (server) — los GameModes llaman
 *       BroadcastFlowStateChange() manualmente tras cambiar MatchFlowState
 *       para que el listen-server reciba el delegate igual que un cliente.
 */
UCLASS()
class TORTUNABO_API ATN_CoopGameState : public AGameState
{
	GENERATED_BODY()

public:
	ATN_CoopGameState();

	/**
	 * @brief Notifica a los listeners locales que MatchFlowState ha cambiado.
	 * @note Llamado por los GameModes tras cambiar la variable, porque OnRep
	 *       no dispara en la máquina que es dueña de la variable replicada.
	 */
	void BroadcastFlowStateChange();

	/**
	 * @brief Añade una entrada de Quick Chat en el servidor y notifica a todos los clientes vía Multicast.
	 * @param SenderPlayerId Id del PlayerState emisor.
	 * @param MessageID Id del mensaje en el catálogo.
	 * @param ServerTimeSeconds Tiempo de mundo del servidor cuando se envió.
	 */
	void AddQuickChatEntry(int32 SenderPlayerId, uint8 MessageID, float ServerTimeSeconds);

	/**
	 * @brief Envía una entrada individual de Quick Chat a todos los clientes.
	 * @note Más eficiente que replicar el array completo (ahorro ~130 bytes por mensaje).
	 */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastNewChatEntry(FTN_QuickChatEntry Entry);

	/**
	 * @brief Resuelve el nombre del emisor de un Quick Chat dado su PlayerId.
	 * @param SenderPlayerId Id compacto del emisor.
	 * @return FText con el PlayerName o un fallback si no se encuentra.
	 */
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

	// ── Scoreboard global ─────────────────────────────────────────────────────

	/**
	 * Resultados de carrera replicados a todos los clientes.
	 * Se actualiza desde TN_RunGameMode tras MarkPlayerFinished/MarkPlayerDead.
	 * El widget Results lee esto para mostrar el ranking completo de todos los
	 * jugadores ordenado (1º, 2º, 3º, 4º, eliminados al final).
	 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_RaceResults, Category = "Coop|Score")
	TArray<FTN_RaceResultEntry> RaceResults;

	UPROPERTY(BlueprintAssignable, Category = "Coop|Score")
	FOnRaceResultsUpdated OnRaceResultsUpdated;

	/**
	 * Server-only: añade o actualiza la entrada de un jugador.
	 * El array se mantiene ordenado por FinishRank (eliminados al final con rank=0).
	 */
	void Server_UpsertRaceResult(int32 InPlayerId, const FString& InPlayerName,
		int32 InFinishRank, float InFinishTime, int32 InRaceScore, bool bInEliminated);

	// ── Quick Chat ────────────────────────────────────────────────────────────
	/** Disparado en todas las máquinas cuando llega un nuevo mensaje de Quick Chat. Bindear en BP o C++. */
	UPROPERTY(BlueprintAssignable, Category = "QuickChat")
	FOnQuickChatReceived OnQuickChatReceived;

	/**
	 * Historial local de los últimos MaxQuickChatEntries mensajes.
	 * Mantenido localmente en todas las máquinas via MulticastNewChatEntry.
	 * Ya no se replica como array completo — ahorro de ~130 bytes por mensaje.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "QuickChat")
	TArray<FTN_QuickChatEntry> QuickChatHistory;

	/** Máximo de entradas conservadas en QuickChatHistory. */
	static constexpr int32 MaxQuickChatEntries = 10;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	/** @brief OnRep de MatchFlowState: dispara el delegate OnMatchFlowStateChanged. */
	UFUNCTION()
	void OnRep_MatchFlowState();

	/** @brief OnRep de RaceResults: dispara OnRaceResultsUpdated. */
	UFUNCTION()
	void OnRep_RaceResults();

	/**
	 * @brief Persiste el RaceScore del jugador local en el save game al entrar en Results.
	 * @note Idempotente: usa bLocalScorePersisted para evitar doble escritura si se llama
	 *       varias veces en el mismo ciclo de Results (ej. BroadcastFlowStateChange + OnRep).
	 */
	void PersistLocalPlayerScoreIfResults();

	/**
	 * true tras la primera llamada exitosa a PersistLocalPlayerScoreIfResults en Results.
	 * Se resetea en BeginPlay para que una nueva run pueda persistir su propio score.
	 */
	bool bLocalScorePersisted = false;

	int32 NextQuickChatSequence = 0;
	int32 LastProcessedQuickChatSequence = 0;
};
