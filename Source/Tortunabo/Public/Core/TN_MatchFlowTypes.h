#pragma once

#include "TN_MatchFlowTypes.generated.h"

UENUM(BlueprintType)
enum class ETNMatchFlowState : uint8
{
	WaitingForPlayers UMETA(DisplayName = "Waiting For Players"),
	Countdown UMETA(DisplayName = "Countdown"),
	Cinematic UMETA(DisplayName = "Cinematic"),
	InProgress UMETA(DisplayName = "In Progress"),
	Results UMETA(DisplayName = "Results")
};

