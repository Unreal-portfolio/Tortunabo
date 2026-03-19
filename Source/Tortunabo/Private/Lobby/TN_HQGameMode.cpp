#include "Lobby/TN_HQGameMode.h"
#include "Core/TN_CoopGameState.h"
#include "Core/TN_CoopPlayerState.h"
#include "Player/TortugaCharacter.h"
#include "Player/MP_GamePlayerController.h"
#include "Voice/ProximityVoiceComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
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
	EnsureFallbackPlayerStart();

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		EnsurePlayerSpawned(It->Get());
	}

	RefreshLobbyState();
}

AActor* ATN_HQGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	TArray<AActor*> PlayerStarts;
	UGameplayStatics::GetAllActorsOfClass(this, APlayerStart::StaticClass(), PlayerStarts);
	if (PlayerStarts.Num() > 0)
	{
		const int32 Index = FMath::RandRange(0, PlayerStarts.Num() - 1);
		return PlayerStarts[Index];
	}

	if (APlayerStart* FallbackStart = EnsureFallbackPlayerStart())
	{
		return FallbackStart;
	}

	return Super::ChoosePlayerStart_Implementation(Player);
}


void ATN_HQGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
	EnsurePlayerSpawned(NewPlayer);
}

void ATN_HQGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	EnsurePlayerSpawned(NewPlayer);

	if (ATN_CoopPlayerState* TNPS = NewPlayer ? NewPlayer->GetPlayerState<ATN_CoopPlayerState>() : nullptr)
	{
		TNPS->bIsInReadyZone = false;
		TNPS->bIsAlive = true;
		TNPS->bHasFinishedRun = false;
		TNPS->DeathZoneTimeRemaining = -1.f;
		TNPS->FinishRank = 0;
		TNPS->FinishTimeSeconds = -1.f;
	}

	RefreshLobbyState();
}

void ATN_HQGameMode::EnsurePlayerSpawned(APlayerController* PlayerController)
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
		UE_LOG(LogTemp, Warning, TEXT("[Lobby] Could not find or create a PlayerStart for %s"), *GetNameSafe(PlayerController));
		return;
	}

	APawn* SpawnedPawn = SpawnDefaultPawnFor(PlayerController, PlayerStart);
	if (!SpawnedPawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Lobby] Failed to spawn default pawn for %s at %s"), *GetNameSafe(PlayerController), *GetNameSafe(PlayerStart));
		return;
	}

	PlayerController->Possess(SpawnedPawn);
	SetPlayerDefaults(SpawnedPawn);
	UE_LOG(LogTemp, Log, TEXT("[Lobby] Spawned and possessed pawn %s for %s"), *GetNameSafe(SpawnedPawn), *GetNameSafe(PlayerController));
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
	TNGS->ConnectedPlayers = ConnectedPlayers;
	TNGS->PlayersInStartZone = ReadyPlayers;
	TNGS->ReadyPlayers = ReadyPlayers;

	if (!bCountdownRunning)
	{
		SetFlowState(ETNMatchFlowState::WaitingForPlayers);
	}

	// Countdown starts when ALL connected players are inside the ready zone.
	// This allows solo testing (1/1) and adapts to any party size (2/2, 3/3, etc.).
	if (ConnectedPlayers > 0 && ReadyPlayers >= ConnectedPlayers)
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

	UE_LOG(LogTemp, Log, TEXT("[HQGameMode] Countdown iniciado: %d segundos."), CurrentCountdownValue);

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

	int32 ConnectedNow = 0;
	int32 ReadyNow = 0;
	for (APlayerState* BasePS : GameState->PlayerArray)
	{
		if (const ATN_CoopPlayerState* TNPS = Cast<ATN_CoopPlayerState>(BasePS))
		{
			++ConnectedNow;
			if (TNPS->bIsInReadyZone)
			{
				++ReadyNow;
			}
		}
	}

	// Cancel countdown if any connected player left the zone
	if (ConnectedNow <= 0 || ReadyNow < ConnectedNow)
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

		// Stop all audio capture streams while WASAPI is still alive.
		// This prevents ACCESS_VIOLATION crashes during level teardown.
		UProximityVoiceComponent::ShutdownAllCapture(World);

		const FString TravelURL = MatchMapPath + TEXT("?listen?game=/Script/Tortunabo.TN_RunGameMode");
		UE_LOG(LogTemp, Log, TEXT("[HQGameMode] Iniciando ServerTravel hacia: %s"), *TravelURL);
		World->ServerTravel(TravelURL);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[HQGameMode] BeginMatchTravel: GetWorld() es null, travel cancelado."));
	}
}

APlayerStart* ATN_HQGameMode::EnsureFallbackPlayerStart()
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
	SpawnParams.Name = TEXT("LobbyFallbackPlayerStart");

	APlayerStart* Spawned = GetWorld()->SpawnActor<APlayerStart>(APlayerStart::StaticClass(), FVector(0.f, 0.f, 150.f), FRotator::ZeroRotator, SpawnParams);
	if (Spawned)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Lobby] No PlayerStart found in lobby map. Spawned fallback PlayerStart at world origin."));
	}

	return Spawned;
}

void ATN_HQGameMode::SetFlowState(ETNMatchFlowState NewState) const
{
	if (ATN_CoopGameState* TNGS = GetGameState<ATN_CoopGameState>())
	{
		TNGS->MatchFlowState = NewState;
		TNGS->BroadcastFlowStateChange(); // Notify server-side (listen server host); clients get OnRep
	}
}



