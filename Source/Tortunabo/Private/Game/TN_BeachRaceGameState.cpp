#include "Game/TN_BeachRaceGameState.h"
#include "Core/TN_CoopPlayerState.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"

void ATN_BeachRaceGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATN_BeachRaceGameState, RacePhase);
	DOREPLIFETIME(ATN_BeachRaceGameState, RoundWinner);
	DOREPLIFETIME(ATN_BeachRaceGameState, RoundHalfShells);
	DOREPLIFETIME(ATN_BeachRaceGameState, Champion);
	DOREPLIFETIME(ATN_BeachRaceGameState, Podium);
	DOREPLIFETIME(ATN_BeachRaceGameState, PhaseSecondsLeft);
	DOREPLIFETIME(ATN_BeachRaceGameState, FinishCountdown);
	DOREPLIFETIME(ATN_BeachRaceGameState, FinishCountdownEndTime);
	DOREPLIFETIME(ATN_BeachRaceGameState, FinishCountdownSeconds);
	DOREPLIFETIME(ATN_BeachRaceGameState, bSprintFinal);
	DOREPLIFETIME(ATN_BeachRaceGameState, SprintFinalists);
}

float ATN_BeachRaceGameState::GetFinishCountdownLeft() const
{
	if (FinishCountdown != ETNBeachFinishCountdown::Counting)
	{
		return 0.f;
	}
	const double Left = static_cast<double>(FinishCountdownEndTime) - GetServerWorldTimeSeconds();
	return static_cast<float>(FMath::Clamp(Left, 0.0, static_cast<double>(FMath::Max(1.f, FinishCountdownSeconds))));
}

int32 ATN_BeachRaceGameState::GetShellHalves(const APlayerState* PlayerState)
{
	const ATN_CoopPlayerState* PS = Cast<ATN_CoopPlayerState>(PlayerState);
	return PS ? PS->RaceShellHalves : 0;
}
