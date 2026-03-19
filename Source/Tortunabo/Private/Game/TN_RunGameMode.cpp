#include "Game/TN_RunGameMode.h"
#include "Core/TN_CoopGameState.h"
#include "Core/TN_CoopPlayerState.h"
#include "Player/MP_GamePlayerController.h"
#include "Player/TortugaCharacter.h"
#include "Voice/ProximityVoiceComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "TimerManager.h"

ATN_RunGameMode::ATN_RunGameMode()
{
	GameStateClass = ATN_CoopGameState::StaticClass();
	PlayerStateClass = ATN_CoopPlayerState::StaticClass();
	PlayerControllerClass = AMP_GamePlayerController::StaticClass();
	DefaultPawnClass = ATortugaCharacter::StaticClass();
	bUseSeamlessTravel = false;
}

void ATN_RunGameMode::BeginPlay()
{
	Super::BeginPlay();
	EnsureFallbackPlayerStart();

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		EnsurePlayerSpawned(It->Get());
	}

	MatchStartServerTime = GetWorld()->GetTimeSeconds();
	NextFinishRank = 1;
	SetFlowState(ETNMatchFlowState::InProgress);

	if (ATN_CoopGameState* TNGS = GetGameState<ATN_CoopGameState>())
	{
		TNGS->FinishedPlayers = 0;
		TNGS->ServerMatchElapsedTime = 0.f;
		TNGS->CountdownValue = 0;
	}

	for (APlayerState* BasePS : GameState->PlayerArray)
	{
		if (ATN_CoopPlayerState* TNPS = Cast<ATN_CoopPlayerState>(BasePS))
		{
			TNPS->bIsAlive = true;
			TNPS->bHasFinishedRun = false;
			TNPS->FinishRank = 0;
			TNPS->FinishTimeSeconds = -1.f;
			TNPS->DeathZoneTimeRemaining = -1.f;
		}
	}
}

void ATN_RunGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
	EnsurePlayerSpawned(NewPlayer);
}

AActor* ATN_RunGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	TArray<AActor*> PlayerStarts;
	UGameplayStatics::GetAllActorsOfClass(this, APlayerStart::StaticClass(), PlayerStarts);
	if (PlayerStarts.Num() > 0)
	{
		const int32 Index = FMath::RandRange(0, PlayerStarts.Num() - 1);
		return PlayerStarts[Index];
	}

	return Super::ChoosePlayerStart_Implementation(Player);
}

void ATN_RunGameMode::EnsurePlayerSpawned(APlayerController* PlayerController)
{
	if (!HasAuthority() || !PlayerController || PlayerController->GetPawn())
	{
		return;
	}

	RestartPlayer(PlayerController);
	if (PlayerController->GetPawn())
	{
		return;
	}

	AActor* PlayerStart = FindPlayerStart(PlayerController);
	if (!PlayerStart)
	{
		PlayerStart = EnsureFallbackPlayerStart();
	}

	if (!PlayerStart)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Run] Could not find or create a PlayerStart for %s"), *GetNameSafe(PlayerController));
		return;
	}

	APawn* SpawnedPawn = SpawnDefaultPawnFor(PlayerController, PlayerStart);
	if (!SpawnedPawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Run] Failed to spawn default pawn for %s at %s"), *GetNameSafe(PlayerController), *GetNameSafe(PlayerStart));
		return;
	}

	PlayerController->Possess(SpawnedPawn);
	SetPlayerDefaults(SpawnedPawn);
	UE_LOG(LogTemp, Log, TEXT("[Run] Spawned and possessed pawn %s for %s"), *GetNameSafe(SpawnedPawn), *GetNameSafe(PlayerController));
}

APlayerStart* ATN_RunGameMode::EnsureFallbackPlayerStart()
{
	if (!GetWorld())
	{
		return nullptr;
	}

	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		if (APlayerStart* Existing = *It)
		{
			return Existing;
		}
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.Name = TEXT("RunFallbackPlayerStart");

	APlayerStart* Spawned = GetWorld()->SpawnActor<APlayerStart>(APlayerStart::StaticClass(), FVector(0.f, 0.f, 150.f), FRotator::ZeroRotator, SpawnParams);
	if (Spawned)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Run] No PlayerStart found in run map. Spawned fallback PlayerStart at world origin."));
	}

	return Spawned;
}

void ATN_RunGameMode::MarkPlayerFinished(APlayerController* PlayerController)
{
	if (!HasAuthority() || !PlayerController)
	{
		return;
	}

	ATN_CoopPlayerState* TNPS = PlayerController->GetPlayerState<ATN_CoopPlayerState>();
	if (!TNPS || TNPS->bHasFinishedRun || !TNPS->bIsAlive)
	{
		return;
	}

	TNPS->bHasFinishedRun = true;
	TNPS->FinishTimeSeconds = GetWorld()->GetTimeSeconds() - MatchStartServerTime;
	TNPS->FinishRank = NextFinishRank++;

	MovePlayerToSpectator(PlayerController);
	UpdateRoundProgressAndMaybeFinish();
}

void ATN_RunGameMode::MarkPlayerDead(APlayerController* PlayerController)
{
	if (!HasAuthority() || !PlayerController)
	{
		return;
	}

	ATN_CoopPlayerState* TNPS = PlayerController->GetPlayerState<ATN_CoopPlayerState>();
	if (!TNPS || !TNPS->bIsAlive)
	{
		return;
	}

	TNPS->bIsAlive = false;
	TNPS->bHasFinishedRun = true;
	TNPS->FinishRank = 0;
	TNPS->FinishTimeSeconds = -1.f;
	TNPS->DeathZoneTimeRemaining = -1.f;

	if (APawn* Pawn = PlayerController->GetPawn())
	{
		Pawn->DisableInput(PlayerController);
		Pawn->Destroy();
	}

	MovePlayerToSpectator(PlayerController);
	UpdateRoundProgressAndMaybeFinish();
}

void ATN_RunGameMode::UpdateRoundProgressAndMaybeFinish()
{
	ATN_CoopGameState* TNGS = GetGameState<ATN_CoopGameState>();
	if (!TNGS)
	{
		return;
	}

	int32 TotalPlayers = 0;
	int32 ResolvedPlayers = 0;
	int32 AlivePlayers = 0;

	for (APlayerState* BasePS : GameState->PlayerArray)
	{
		if (const ATN_CoopPlayerState* CastPS = Cast<ATN_CoopPlayerState>(BasePS))
		{
			++TotalPlayers;
			if (CastPS->bIsAlive)
			{
				++AlivePlayers;
			}
			if (CastPS->bHasFinishedRun)
			{
				++ResolvedPlayers;
			}
		}
	}

	TNGS->FinishedPlayers = ResolvedPlayers;
	TNGS->ExpectedPlayers = TotalPlayers;
	TNGS->ServerMatchElapsedTime = GetWorld()->GetTimeSeconds() - MatchStartServerTime;

	if (TotalPlayers <= 0 || (ResolvedPlayers != TotalPlayers && AlivePlayers > 0))
	{
		return;
	}

	if (GetWorldTimerManager().IsTimerActive(ResultsTimerHandle))
	{
		return;
	}

	SetFlowState(ETNMatchFlowState::Results);
	ResultsCountdownValue = FMath::CeilToInt(ResultsDurationSeconds);
	TNGS->CountdownValue = ResultsCountdownValue;
	GetWorldTimerManager().SetTimer(ResultsCountdownTimerHandle, this, &ATN_RunGameMode::TickResultsCountdown, 1.0f, true);
	GetWorldTimerManager().SetTimer(ResultsTimerHandle, this, &ATN_RunGameMode::FinishRoundAndReturnToLobby, ResultsDurationSeconds, false);
}

void ATN_RunGameMode::MovePlayerToSpectator(APlayerController* PlayerController) const
{
	if (!PlayerController)
	{
		return;
	}

	if (AMP_GamePlayerController* TNPC = Cast<AMP_GamePlayerController>(PlayerController))
	{
		TNPC->EnterSpectateMode();
	}
	else
	{
		PlayerController->ChangeState(NAME_Spectating);
		PlayerController->StartSpectatingOnly();
	}
}

void ATN_RunGameMode::TickResultsCountdown()
{
	ATN_CoopGameState* TNGS = GetGameState<ATN_CoopGameState>();
	if (!TNGS)
	{
		GetWorldTimerManager().ClearTimer(ResultsCountdownTimerHandle);
		return;
	}

	ResultsCountdownValue = FMath::Max(0, ResultsCountdownValue - 1);
	TNGS->CountdownValue = ResultsCountdownValue;

	if (ResultsCountdownValue <= 0)
	{
		GetWorldTimerManager().ClearTimer(ResultsCountdownTimerHandle);
	}
}

void ATN_RunGameMode::FinishRoundAndReturnToLobby()
{
	GetWorldTimerManager().ClearTimer(ResultsCountdownTimerHandle);

	if (ATN_CoopGameState* TNGS = GetGameState<ATN_CoopGameState>())
	{
		TNGS->CountdownValue = 0;
	}

	if (UWorld* World = GetWorld())
	{

		// Stop all audio capture streams while WASAPI is still alive.
		// This prevents ACCESS_VIOLATION crashes during level teardown.
		UProximityVoiceComponent::ShutdownAllCapture(World);

		World->ServerTravel(LobbyMapPath + TEXT("?listen?game=/Script/Tortunabo.TN_HQGameMode"));
	}
}

void ATN_RunGameMode::SetFlowState(ETNMatchFlowState NewState) const
{
	if (ATN_CoopGameState* TNGS = GetGameState<ATN_CoopGameState>())
	{
		TNGS->MatchFlowState = NewState;
		TNGS->BroadcastFlowStateChange();
	}
}


