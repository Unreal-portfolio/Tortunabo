#include "Game/TN_BeachRaceGameState.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"

void ATN_BeachRaceGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATN_BeachRaceGameState, RacePhase);
	DOREPLIFETIME(ATN_BeachRaceGameState, RoundWinner);
	DOREPLIFETIME(ATN_BeachRaceGameState, Champion);
	DOREPLIFETIME(ATN_BeachRaceGameState, Podium);
	DOREPLIFETIME(ATN_BeachRaceGameState, PhaseSecondsLeft);
}
