#include "Lobby/TN_HQGameMode.h"
#include "Core/TN_CoopGameState.h"
#include "Core/TN_CoopPlayerState.h"
#include "Player/TortugaCharacter.h"
#include "Player/MP_GamePlayerController.h"
#include "Multiplayer/MP_GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

ATN_HQGameMode::ATN_HQGameMode()
{
	GameStateClass = ATN_CoopGameState::StaticClass();
	PlayerStateClass = ATN_CoopPlayerState::StaticClass();
	PlayerControllerClass = AMP_GamePlayerController::StaticClass();
	DefaultPawnClass = ATortugaCharacter::StaticClass();
	bUseSeamlessTravel = true;
}

void ATN_HQGameMode::BeginPlay()
{
	Super::BeginPlay();
	EnsureFallbackPlayerStart();

	// ── Tutorial first-time check (fallback para join directo sin seamless) ──
	// Para seamless travel, el flag se evalúa en HandleSeamlessTravelPlayer
	// ANTES de que Super spawne el pawn.  Este bloque cubre el caso en que el
	// jugador accede directamente al mapa sin pasar por LVL_Menu.
	CheckAndSetTutorialFlag();

	// ── Safety check: detectar si el mapa cargó con la clase C++ base en vez del BP ──
	if (GetClass() == ATN_HQGameMode::StaticClass())
	{
		UE_LOG(LogTemp, Error, TEXT("[HQGameMode] ════════════════════════════════════════════════════════"));
		UE_LOG(LogTemp, Error, TEXT("[HQGameMode] ¡USANDO CLASE C++ BASE! No hay BP GameMode."));
		UE_LOG(LogTemp, Error, TEXT("[HQGameMode] FIX: En LVL_HQ → WorldSettings → GameMode Override"));
		UE_LOG(LogTemp, Error, TEXT("[HQGameMode]       → seleccionar BP_HQGameMode."));
		UE_LOG(LogTemp, Error, TEXT("[HQGameMode] ════════════════════════════════════════════════════════"));
	}

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		EnsurePlayerSpawned(It->Get());
	}

	RefreshLobbyState();
}

AActor* ATN_HQGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	// ── Tutorial check — aquí es el único sitio donde SIEMPRE se ejecuta ─────
	// Llamarlo aquí garantiza que el flag está evaluado justo antes de decidir
	// el spawn, independientemente del camino (seamless travel, PostLogin, etc.)
	// CheckAndSetTutorialFlag es idempotente: llamadas repetidas no hacen nada.
	CheckAndSetTutorialFlag();

	// ── Primera vez: dirigir al spawn del tutorial ───────────────────────────
	if (bShouldUseTutorialStart)
	{
		for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
		{
			if ((*It)->PlayerStartTag == TutorialStartTag)
			{
				UE_LOG(LogTemp, Log, TEXT("[HQGameMode] ChoosePlayerStart → tutorial start encontrado."));
				return *It;
			}
		}
		UE_LOG(LogTemp, Warning,
			TEXT("[HQGameMode] bShouldUseTutorialStart=true pero no hay PlayerStart con tag '%s' — spawn normal."),
			*TutorialStartTag.ToString());
	}

	// ── Selección normal: excluir spawns reservados para el tutorial ─────────
	TArray<AActor*> PlayerStarts;
	UGameplayStatics::GetAllActorsOfClass(this, APlayerStart::StaticClass(), PlayerStarts);

	// Quitar del pool cualquier PlayerStart con el tag de tutorial
	PlayerStarts.RemoveAll([this](const AActor* A)
	{
		const APlayerStart* PS = Cast<APlayerStart>(A);
		return PS && PS->PlayerStartTag == TutorialStartTag;
	});

	if (PlayerStarts.Num() == 0)
	{
		if (APlayerStart* FallbackStart = EnsureFallbackPlayerStart())
		{
			return FallbackStart;
		}
		return Super::ChoosePlayerStart_Implementation(Player);
	}

	// Barajar para evitar siempre el mismo orden
	for (int32 i = PlayerStarts.Num() - 1; i > 0; --i)
	{
		const int32 j = FMath::RandRange(0, i);
		PlayerStarts.Swap(i, j);
	}

	// Primera pasada: buscar un PlayerStart sin ningún pawn cerca (< 200 cm)
	for (AActor* Start : PlayerStarts)
	{
		bool bOccupied = false;
		for (TActorIterator<APawn> PawnIt(GetWorld()); PawnIt; ++PawnIt)
		{
			const APawn* P = *PawnIt;
			if (P && P->Controller != Player &&
			    FVector::DistSquared(Start->GetActorLocation(), P->GetActorLocation()) < 200.f * 200.f)
			{
				bOccupied = true;
				break;
			}
		}
		if (!bOccupied)
		{
			return Start;
		}
	}

	// Fallback: todos ocupados
	return PlayerStarts[0];
}


void ATN_HQGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
	EnsurePlayerSpawned(NewPlayer);
}

void ATN_HQGameMode::PostLogin(APlayerController* NewPlayer)
{
	// Tutorial check aquí cubre el caso de un join fresco (sin seamless travel).
	// En seamless travel PostLogin NO se llama → lo cubre HandleSeamlessTravelPlayer.
	CheckAndSetTutorialFlag();

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

void ATN_HQGameMode::CheckAndSetTutorialFlag()
{
	// Idempotente: si ya detectamos primera vez en esta sesión, no repetir.
	if (bShouldUseTutorialStart)
	{
		return;
	}

	if (UMP_GameInstance* GI = Cast<UMP_GameInstance>(GetGameInstance()))
	{
		if (!GI->HasCompletedTutorial())
		{
			bShouldUseTutorialStart = true;
			GI->SetTutorialCompleted();
			UE_LOG(LogTemp, Log, TEXT("[HQGameMode] Primera partida detectada — spawn en zona tutorial (tag: '%s')."), *TutorialStartTag.ToString());
		}
	}
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
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("[HQGameMode] BeginMatchTravel: GetWorld() es null, travel cancelado."));
		return;
	}

	// ── Guardar cuántos jugadores hay en el lobby ANTES de viajar ──────
	// GameInstance sobrevive al seamless travel; TN_RunGameMode
	// lo leerá en BeginPlay para saber cuántos jugadores esperar.
	if (UMP_GameInstance* GI = Cast<UMP_GameInstance>(GetGameInstance()))
	{
		int32 ConnectedCount = 0;
		for (APlayerState* BasePS : GameState->PlayerArray)
		{
			if (Cast<ATN_CoopPlayerState>(BasePS))
			{
				++ConnectedCount;
			}
		}
		GI->PendingTravelPlayerCount = ConnectedCount;
		UE_LOG(LogTemp, Log, TEXT("[HQGameMode] Saved PendingTravelPlayerCount = %d"), ConnectedCount);
	}

	// ── Destroy all pawns BEFORE travel for WASAPI cleanup ──────────────
	// EndPlay(Destroyed) fires on ProximityVoiceComponents while WASAPI
	// is still fully alive → safe audio cleanup, no ACCESS_VIOLATION.
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC) { continue; }
		if (APawn* Pawn = PC->GetPawn())
		{
			Pawn->Destroy();
		}
	}

	// ── Seamless ServerTravel — NetDriver persists, connections stay alive ─
	// NO ?listen (seamless travel reuses the existing NetDriver).
	// NO destroying NetDriver (that kills client connections).
	// NO ClientNotifyServerTravel (clients travel with the server automatically).
	const FString TravelURL = MatchMapPath;
	UE_LOG(LogTemp, Log, TEXT("[HQGameMode] Seamless ServerTravel to: %s"), *TravelURL);
	World->ServerTravel(TravelURL);
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

// ── Seamless Travel Handlers ──────────────────────────────────────────────────

void ATN_HQGameMode::HandleSeamlessTravelPlayer(AController*& C)
{
	// ── Tutorial check — DEBE ir ANTES de Super ───────────────────────────────
	// Super::HandleSeamlessTravelPlayer → RestartPlayer → ChoosePlayerStart.
	// Si ponemos el flag después, el pawn ya está spawneado en el sitio normal.
	CheckAndSetTutorialFlag();

	// Limpiar estado espectador ANTES de Super — jugadores que murieron/terminaron
	// en la carrera estaban en modo espectador. Sin esto, PlayerCanRestart() devuelve
	// false y Super no les spawnea pawn.
	if (APlayerController* PC = Cast<APlayerController>(C))
	{
		// ── Destruir pawn prematuro ────────────────────────────────────────────
		// BeginPlay puede spawnear un pawn antes de que HandleSeamlessTravelPlayer
		// lo haga. Super::HandleSeamlessTravelPlayer → RestartPlayer NO destruye
		// el pawn existente → quedarían DOS pawns (ghost pawn inmóvil en spawn).
		if (APawn* OldPawn = PC->GetPawn())
		{
			UE_LOG(LogTemp, Log, TEXT("[HQGameMode] HandleSeamlessTravelPlayer: destruyendo pawn prematuro '%s' de %s"),
				*GetNameSafe(OldPawn), *GetNameSafe(PC));
			OldPawn->Destroy();
		}

		if (PC->PlayerState)
		{
			PC->PlayerState->SetIsOnlyASpectator(false);
		}
	}

	Super::HandleSeamlessTravelPlayer(C);
}

void ATN_HQGameMode::PostSeamlessTravel()
{
	Super::PostSeamlessTravel();

	UE_LOG(LogTemp, Log, TEXT("[HQGameMode] PostSeamlessTravel: resetting all players for lobby."));

	// Resetear estado de todos los jugadores que viajaron y asegurar que tienen pawn
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC) { continue; }

		if (ATN_CoopPlayerState* TNPS = PC->GetPlayerState<ATN_CoopPlayerState>())
		{
			const FName SavedHelmet = TNPS->EquippedHelmetId;
			const FName SavedSkin   = TNPS->EquippedSkinId;

			TNPS->bIsAlive = true;
			TNPS->bHasFinishedRun = false;
			TNPS->bIsDBNO = false;
			TNPS->DBNOBleedoutTimeRemaining = -1.f;
			TNPS->FinishRank = 0;
			TNPS->bIsEliminated = false;
			TNPS->FinishTimeSeconds = -1.f;
			TNPS->DeathZoneTimeRemaining = -1.f;
			TNPS->bIsInReadyZone = false;

			EnsurePlayerSpawned(PC);

			// Forzar aplicación del helmet y skin en todos los clientes.
			// El Multicast incluye un retry deferred para cubrir la race condition
			// donde el pawn aún no ha replicado en los clientes cuando el RPC llega.
			if (SavedHelmet != NAME_None)
			{
				TNPS->MulticastForceApplyHelmet(SavedHelmet);
			}
			// Skin siempre se fuerza (NAME_None = sin skin, también válido de restaurar)
			TNPS->MulticastForceApplySkin(SavedSkin);
		}
		else
		{
			EnsurePlayerSpawned(PC);
		}
	}

	// Actualizar conteo en el GameState
	if (ATN_CoopGameState* TNGS = GetGameState<ATN_CoopGameState>())
	{
		int32 TotalPlayers = 0;
		for (APlayerState* BasePS : GameState->PlayerArray)
		{
			if (Cast<ATN_CoopPlayerState>(BasePS))
			{
				++TotalPlayers;
			}
		}
		TNGS->ConnectedPlayers = TotalPlayers;
		TNGS->ReadyPlayers = 0;
		TNGS->FinishedPlayers = 0;
		TNGS->ServerMatchElapsedTime = 0.f;
		TNGS->CountdownValue = 0;
	}

	RefreshLobbyState();

	// ── Destruir pawns huérfanos (sin controller) un frame después ───────────
	// HandleSeamlessTravelPlayer + EnsurePlayerSpawned pueden crear/destruir pawns
	// en este mismo tick; diferir un frame garantiza que todos los pawns legítimos
	// ya están poseídos antes del barrido.
	GetWorldTimerManager().SetTimerForNextTick([this]()
	{
		UWorld* World = GetWorld();
		if (!World) { return; }

		for (TActorIterator<APawn> It(World); It; ++It)
		{
			APawn* P = *It;
			if (P && !P->GetController())
			{
				P->Destroy();
			}
		}
	});
}



