#pragma once

#include "TN_MatchFlowTypes.generated.h"

/** One entry in the Quick Chat history (replicated on CoopGameState). */
USTRUCT(BlueprintType)
struct FTN_QuickChatEntry
{
	GENERATED_BODY()

	/** Monotonic server sequence for dedup / JIP processing. */
	UPROPERTY(BlueprintReadOnly)
	int32 Sequence = 0;

	/** Compact sender identifier (PlayerState::PlayerId clamped to uint16). */
	UPROPERTY(BlueprintReadOnly)
	int32 SenderPlayerId = 0;

	/** Message catalog ID resolved locally via DataAsset. */
	UPROPERTY(BlueprintReadOnly)
	uint8 MessageID = 0;

	/** Server world time when the message was sent (for ordering). */
	UPROPERTY(BlueprintReadOnly)
	float ServerTime = 0.f;
};

UENUM(BlueprintType)
enum class ETNMatchFlowState : uint8
{
	WaitingForPlayers UMETA(DisplayName = "Waiting For Players"),
	Countdown UMETA(DisplayName = "Countdown"),
	Cinematic UMETA(DisplayName = "Cinematic"),
	InProgress UMETA(DisplayName = "In Progress"),
	Results UMETA(DisplayName = "Results")
};

/**
 * Entrada del scoreboard global.
 * Replicado en TN_CoopGameState — todos los clientes ven la lista completa
 * tras Results, ordenada por FinishRank (eliminados al final).
 */
USTRUCT(BlueprintType)
struct FTN_RaceResultEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	int32 PlayerId = 0;

	UPROPERTY(BlueprintReadOnly)
	FString PlayerName;

	UPROPERTY(BlueprintReadOnly)
	int32 FinishRank = 0;

	UPROPERTY(BlueprintReadOnly)
	float FinishTimeSeconds = 0.f;

	UPROPERTY(BlueprintReadOnly)
	int32 RaceScore = 0;

	UPROPERTY(BlueprintReadOnly)
	bool bIsEliminated = false;

	bool operator==(const FTN_RaceResultEntry& Other) const
	{
		return PlayerId == Other.PlayerId;
	}
};

