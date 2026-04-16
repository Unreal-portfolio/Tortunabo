#include "Core/TN_CoopGameState.h"
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
	PersistLocalPlayerScoreIfResults();
	OnMatchFlowStateChanged.Broadcast(MatchFlowState);
}

void ATN_CoopGameState::PersistLocalPlayerScoreIfResults()
{
	if (MatchFlowState != ETNMatchFlowState::Results)
	{
		return;
	}

	UMP_GameInstance* GI = Cast<UMP_GameInstance>(GetGameInstance());
	if (!GI)
	{
		return;
	}

	// Find the local player's PlayerState and persist their race score.
	// IsLocalPlayerState() correctly identifies the owning player on each machine.
	for (APlayerState* PS : PlayerArray)
	{
		if (PS && PS->IsLocalPlayerState())
		{
			if (const ATN_CoopPlayerState* TNPS = Cast<ATN_CoopPlayerState>(PS))
			{
				if (TNPS->RaceScore > 0)
				{
					GI->AddRaceScore(TNPS->RaceScore);
					UE_LOG(LogTemp, Log, TEXT("[CoopGameState] Persisted RaceScore=%d for local player '%s'"),
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
}


