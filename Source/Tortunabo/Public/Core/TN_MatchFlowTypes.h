#pragma once

#include "TN_MatchFlowTypes.generated.h"

/**
 * @brief Una entrada del historial de Quick Chat (replicada vía CoopGameState).
 *
 * Cada cliente reconstruye el historial localmente recibiendo entries individuales
 * por Multicast (más eficiente que replicar el array completo).
 */
USTRUCT(BlueprintType)
struct FTN_QuickChatEntry
{
	GENERATED_BODY()

	/** Secuencia monótona del servidor (sirve para dedup y procesado JIP). */
	UPROPERTY(BlueprintReadOnly)
	int32 Sequence = 0;

	/** Identificador compacto del emisor (PlayerState::PlayerId clamped a uint16). */
	UPROPERTY(BlueprintReadOnly)
	int32 SenderPlayerId = 0;

	/** ID del catálogo de mensajes — se resuelve localmente vía DataAsset. */
	UPROPERTY(BlueprintReadOnly)
	uint8 MessageID = 0;

	/** Tiempo de mundo del servidor cuando se envió el mensaje (para ordering). */
	UPROPERTY(BlueprintReadOnly)
	float ServerTime = 0.f;
};

/**
 * @brief Estados del flujo de partida. Replicado en TN_CoopGameState.
 *        Lobby HQ comparte WaitingForPlayers/Countdown; Run usa Cinematic/InProgress/Results.
 */
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
