#include "Core/TN_CoopGameState.h"
#include "Net/UnrealNetwork.h"

ATN_CoopGameState::ATN_CoopGameState()
{
}

void ATN_CoopGameState::OnRep_MatchFlowState()
{
	// Fires on remote clients when the replicated value arrives
	OnMatchFlowStateChanged.Broadcast(MatchFlowState);
}

void ATN_CoopGameState::BroadcastFlowStateChange()
{
	// Must be called by game modes on the server after setting MatchFlowState.
	// OnRep does NOT fire on the authoritative machine, so we broadcast manually.
	OnMatchFlowStateChanged.Broadcast(MatchFlowState);
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
