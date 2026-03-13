#include "Lobby/TN_HQGameMode.h"
#include "Core/TN_CoopGameState.h"
#include "Core/TN_CoopPlayerState.h"
#include "Player/TortugaCharacter.h"
#include "Player/MP_GamePlayerController.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "TimerManager.h"

ATN_HQGameMode::ATN_HQGameMode()
{
	GameStateClass = ATN_CoopGameState::StaticClass();
	PlayerStateClass = ATN_CoopPlayerState::StaticClass();
	PlayerControllerClass = AMP_GamePlayerController::StaticClass();
	DefaultPawnClass = ATortugaCharacter::StaticClass();
	bUseSeamlessTravel = false;
}

void ATN_HQGameMode::BeginPlay()
{
	Super::BeginPlay();
	RefreshLobbyState();
}

void ATN_HQGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	if (ATN_CoopPlayerState* TNPS = NewPlayer ? NewPlayer->GetPlayerState<ATN_CoopPlayerState>() : nullptr)
	{
		TNPS->bIsInReadyZone = false;
		TNPS->bHasFinishedRun = false;
		TNPS->FinishRank = 0;
		TNPS->FinishTimeSeconds = -1.f;
	}

	RefreshLobbyState();
}

void ATN_HQGameMode::Logout(AController* Exiting)
{
	if (ATN_CoopPlayerState* TNPS = Exiting ? Exiting->GetPlayerState<ATN_CoopPlayerState>() : nullptr)
	{
		TNPS->bIsInReadyZone = false;
	}

	Super::Logout(Exiting);
	RefreshLobbyState();
}

void ATN_HQGameMode::SetPlayerReadyState(APlayerController* PlayerController, bool bReady)
{
	if (!HasAuthority() || !PlayerController)
	{
		return;
	}

	if (ATN_CoopPlayerState* TNPS = PlayerController->GetPlayerState<ATN_CoopPlayerState>())
	{
		TNPS->bIsInReadyZone = bReady;
	}

	RefreshLobbyState();
}

void ATN_HQGameMode::RefreshLobbyState()
{
	ATN_CoopGameState* TNGS = GetGameState<ATN_CoopGameState>();
	if (!TNGS)
	{
		return;
	}

	int32 ConnectedPlayers = 0;
	int32 ReadyPlayers = 0;
	for (APlayerState* BasePS : GameState->PlayerArray)
	{
		if (ATN_CoopPlayerState* TNPS = Cast<ATN_CoopPlayerState>(BasePS))
		{
			++ConnectedPlayers;
			if (TNPS->bIsInReadyZone)
			{
				++ReadyPlayers;
			}
		}
	}

	const int32 ExpectedPlayers = LobbyExpectedPlayers;
	TNGS->ExpectedPlayers = ExpectedPlayers;
	TNGS->ReadyPlayers = ReadyPlayers;

	if (!bCountdownRunning)
	{
		SetFlowState(ETNMatchFlowState::WaitingForPlayers);
	}

	if (ConnectedPlayers >= ExpectedPlayers && ReadyPlayers >= ExpectedPlayers)
	{
		if (!bCountdownRunning)
		{
			StartCountdown();
		}
	}
	else if (bCountdownRunning)
	{
		ResetCountdown();
	}
}

void ATN_HQGameMode::StartCountdown()
{
	bCountdownRunning = true;
	CurrentCountdownValue = CountdownStartValue;

	SetFlowState(ETNMatchFlowState::Countdown);
	if (ATN_CoopGameState* TNGS = GetGameState<ATN_CoopGameState>())
	{
		TNGS->CountdownValue = CurrentCountdownValue;
	}

	GetWorldTimerManager().SetTimer(CountdownTimerHandle, this, &ATN_HQGameMode::TickCountdown, 1.0f, true);
}

void ATN_HQGameMode::TickCountdown()
{
	ATN_CoopGameState* TNGS = GetGameState<ATN_CoopGameState>();
	if (!TNGS)
	{
		ResetCountdown();
		return;
	}

	int32 ReadyPlayers = 0;
	for (APlayerState* BasePS : GameState->PlayerArray)
	{
		if (const ATN_CoopPlayerState* TNPS = Cast<ATN_CoopPlayerState>(BasePS))
		{
			if (TNPS->bIsInReadyZone)
			{
				++ReadyPlayers;
			}
		}
	}

	if (ReadyPlayers != TNGS->ExpectedPlayers)
	{
		ResetCountdown();
		return;
	}

	--CurrentCountdownValue;
	TNGS->CountdownValue = CurrentCountdownValue;

	if (CurrentCountdownValue <= 0)
	{
		GetWorldTimerManager().ClearTimer(CountdownTimerHandle);
		bCountdownRunning = false;
		SetFlowState(ETNMatchFlowState::Cinematic);
		GetWorldTimerManager().SetTimer(TravelTimerHandle, this, &ATN_HQGameMode::BeginMatchTravel, CinematicDelaySeconds, false);
	}
}

void ATN_HQGameMode::ResetCountdown()
{
	GetWorldTimerManager().ClearTimer(CountdownTimerHandle);
	bCountdownRunning = false;
	CurrentCountdownValue = 0;

	if (ATN_CoopGameState* TNGS = GetGameState<ATN_CoopGameState>())
	{
		TNGS->CountdownValue = 0;
	}

	SetFlowState(ETNMatchFlowState::WaitingForPlayers);
}

void ATN_HQGameMode::BeginMatchTravel()
{
	if (UWorld* World = GetWorld())
	{
		World->ServerTravel(MatchMapPath + TEXT("?listen?game=/Script/Tortunabo.TN_RunGameMode"));
	}
}

void ATN_HQGameMode::SetFlowState(ETNMatchFlowState NewState) const
{
	if (ATN_CoopGameState* TNGS = GetGameState<ATN_CoopGameState>())
	{
		TNGS->MatchFlowState = NewState;
	}
}



