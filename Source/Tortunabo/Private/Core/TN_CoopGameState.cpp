#include "Core/TN_CoopGameState.h"
#include "Core/TN_Log.h"
#include "Core/TN_CoopPlayerState.h"
#include "Core/TN_MatchFlowTypes.h"
#include "Multiplayer/MP_GameInstance.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"

ATN_CoopGameState::ATN_CoopGameState()
{
}

void ATN_CoopGameState::OnRep_MatchFlowState()
{
	// Fires on remote clients when the replicated value arrives
	PersistLocalPlayerScoreIfResults();
	OnMatchFlowStateChanged.Broadcast(MatchFlowState);
}

void ATN_CoopGameState::AddQuickChatEntry(int32 SenderPlayerId, uint8 MessageID, float ServerTimeSeconds)
{
	FTN_QuickChatEntry Entry;
	Entry.Sequence = ++NextQuickChatSequence;
	Entry.SenderPlayerId = SenderPlayerId;
	Entry.MessageID = MessageID;
	Entry.ServerTime = ServerTimeSeconds;

	// Multicast envía la entrada individualmente — más eficiente que replicar todo el array.
	// MulticastNewChatEntry_Implementation actualiza el historial local y dispara el delegate.
	MulticastNewChatEntry(Entry);
}

void ATN_CoopGameState::MulticastNewChatEntry_Implementation(FTN_QuickChatEntry Entry)
{
	QuickChatHistory.Add(Entry);
	if (QuickChatHistory.Num() > MaxQuickChatEntries)
	{
		QuickChatHistory.RemoveAt(0);
	}

	LastProcessedQuickChatSequence = Entry.Sequence;
	OnQuickChatReceived.Broadcast(Entry);
}

FText ATN_CoopGameState::ResolveQuickChatSenderName(int32 SenderPlayerId) const
{
	for (APlayerState* PS : PlayerArray)
	{
		if (!PS)
		{
			continue;
		}

		const int32 LocalPlayerId = PS->GetPlayerId();
		if (LocalPlayerId == SenderPlayerId)
		{
			return FText::FromString(PS->GetPlayerName());
		}
	}

	return FText::FromString(TEXT("?"));
}

void ATN_CoopGameState::BroadcastFlowStateChange()
{
	// Must be called by game modes on the server after setting MatchFlowState.
	// OnRep does NOT fire on the authoritative machine, so we broadcast manually.
	// Reset idempotency guard when state changes so the new state can persist later.
	if (MatchFlowState != ETNMatchFlowState::Results)
	{
		bLocalScorePersisted = false;
	}
	PersistLocalPlayerScoreIfResults();
	OnMatchFlowStateChanged.Broadcast(MatchFlowState);
}

void ATN_CoopGameState::PersistLocalPlayerScoreIfResults()
{
	if (MatchFlowState != ETNMatchFlowState::Results)
	{
		return;
	}

	// Idempotency guard — prevents double-counting if BroadcastFlowStateChange
	// and OnRep_MatchFlowState both fire on the same machine in the same Results cycle.
	if (bLocalScorePersisted)
	{
		return;
	}
	bLocalScorePersisted = true;

	UMP_GameInstance* GI = Cast<UMP_GameInstance>(GetGameInstance());
	if (!GI)
	{
		return;
	}

	// Find the local player's PlayerState and persist their race score.
	const UWorld* World = GetWorld();
	const APlayerController* LocalPC = World ? World->GetFirstPlayerController() : nullptr;
	for (APlayerState* PS : PlayerArray)
	{
		if (PS && LocalPC && PS == LocalPC->PlayerState)
		{
			if (const ATN_CoopPlayerState* TNPS = Cast<ATN_CoopPlayerState>(PS))
			{
				if (TNPS->RaceScore > 0)
				{
					GI->AddRaceScore(TNPS->RaceScore);
					UE_LOG(LogTortunabo, Log, TEXT("[CoopGameState] Persisted RaceScore=%d for local player '%s'"),
						TNPS->RaceScore, *PS->GetPlayerName());
				}
			}
			break;
		}
	}
}

void ATN_CoopGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ATN_CoopGameState, MatchFlowState);
	DOREPLIFETIME(ATN_CoopGameState, ReadyPlayers);
	DOREPLIFETIME(ATN_CoopGameState, ConnectedPlayers);
	DOREPLIFETIME(ATN_CoopGameState, PlayersInStartZone);
	DOREPLIFETIME(ATN_CoopGameState, ExpectedPlayers);
	DOREPLIFETIME(ATN_CoopGameState, CountdownValue);
	// ServerMatchElapsedTime cambia cada frame → SkipOwner reduce tráfico
	// (el listen-server ya lo calcula localmente)
	DOREPLIFETIME_CONDITION(ATN_CoopGameState, ServerMatchElapsedTime, COND_SkipOwner);
	DOREPLIFETIME(ATN_CoopGameState, FinishedPlayers);
	DOREPLIFETIME(ATN_CoopGameState, RaceResults);
}

void ATN_CoopGameState::OnRep_RaceResults()
{
	OnRaceResultsUpdated.Broadcast();
}

void ATN_CoopGameState::Server_UpsertRaceResult(int32 InPlayerId, const FString& InPlayerName,
	int32 InFinishRank, float InFinishTime, int32 InRaceScore, bool bInEliminated)
{
	if (!HasAuthority()) { return; }

	FTN_RaceResultEntry NewEntry;
	NewEntry.PlayerId          = InPlayerId;
	NewEntry.PlayerName        = InPlayerName;
	NewEntry.FinishRank        = InFinishRank;
	NewEntry.FinishTimeSeconds = InFinishTime;
	NewEntry.RaceScore         = InRaceScore;
	NewEntry.bIsEliminated     = bInEliminated;

	const int32 ExistingIdx = RaceResults.IndexOfByKey(NewEntry);
	if (ExistingIdx != INDEX_NONE)
	{
		RaceResults[ExistingIdx] = NewEntry;
	}
	else
	{
		RaceResults.Add(NewEntry);
	}

	// Sort: ranks 1,2,3,4 primero, eliminados al final (FinishRank=0).
	RaceResults.Sort([](const FTN_RaceResultEntry& A, const FTN_RaceResultEntry& B)
	{
		const int32 RankA = A.bIsEliminated ? INT32_MAX : (A.FinishRank > 0 ? A.FinishRank : INT32_MAX - 1);
		const int32 RankB = B.bIsEliminated ? INT32_MAX : (B.FinishRank > 0 ? B.FinishRank : INT32_MAX - 1);
		return RankA < RankB;
	});

	// Listen-server: dispara delegate manualmente (OnRep no llega al host).
	OnRaceResultsUpdated.Broadcast();
}


