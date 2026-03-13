#include "Core/TN_CoopGameState.h"
#include "Net/UnrealNetwork.h"

ATN_CoopGameState::ATN_CoopGameState()
{
}

void ATN_CoopGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ATN_CoopGameState, MatchFlowState);
	DOREPLIFETIME(ATN_CoopGameState, ReadyPlayers);
	DOREPLIFETIME(ATN_CoopGameState, ExpectedPlayers);
	DOREPLIFETIME(ATN_CoopGameState, CountdownValue);
	DOREPLIFETIME(ATN_CoopGameState, ServerMatchElapsedTime);
	DOREPLIFETIME(ATN_CoopGameState, FinishedPlayers);
}

