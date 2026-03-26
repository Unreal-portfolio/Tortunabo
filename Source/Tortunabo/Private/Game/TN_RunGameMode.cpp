#include "Game/TN_RunGameMode.h"
#include "Core/TN_CoopGameState.h"
#include "Core/TN_CoopPlayerState.h"
#include "Player/MP_GamePlayerController.h"
#include "Player/TortugaCharacter.h"
#include "Multiplayer/MP_GameInstance.h"
#include "World/TN_RescuePickup.h"
#include "GameFramework/SpectatorPawn.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "TimerManager.h"

ATN_RunGameMode::ATN_RunGameMode()
{
	GameStateClass = ATN_CoopGameState::StaticClass();
	PlayerStateClass = ATN_CoopPlayerState::StaticClass();
	PlayerControllerClass = AMP_GamePlayerController::StaticClass();
	DefaultPawnClass = ATortugaCharacter::StaticClass();
	bUseSeamlessTravel = true;
}

void ATN_RunGameMode::BeginPlay()
{
	Super::BeginPlay();
	EnsureFallbackPlayerStart();

	// ── Safety check: detectar si el mapa cargó con la clase C++ base en vez del BP ──
	// Si GetClass() es exactamente ATN_RunGameMode (no un BP hijo), significa que
	// WorldSettings no tiene GameMode Override → se está usando la clase C++ base,
	// que spawna TortugaCharacter sin mesh ni input en vez de BP_TortugaCharacter.
	if (GetClass() == ATN_RunGameMode::StaticClass())
	{
		UE_LOG(LogTemp, Error, TEXT("[RunGameMode] ════════════════════════════════════════════════════════"));
		UE_LOG(LogTemp, Error, TEXT("[RunGameMode] ¡USANDO CLASE C++ BASE! No hay BP GameMode."));
		UE_LOG(LogTemp, Error, TEXT("[RunGameMode] Esto causa: sin tortuga visible, sin input, sin HUD."));
		UE_LOG(LogTemp, Error, TEXT("[RunGameMode] FIX: En LVL_Run → WorldSettings → GameMode Override"));
		UE_LOG(LogTemp, Error, TEXT("[RunGameMode]       → seleccionar BP_RunGameMode."));
		UE_LOG(LogTemp, Error, TEXT("[RunGameMode] ════════════════════════════════════════════════════════"));
	}

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		EnsurePlayerSpawned(It->Get());
	}

	NextFinishRank = 1;
	bMatchStarted = false;

	// ── Leer cuántos jugadores había en el lobby ──────────────────────────
	if (const UMP_GameInstance* GI = Cast<UMP_GameInstance>(GetGameInstance()))
	{
		ExpectedPlayersFromLobby = FMath::Max(1, GI->PendingTravelPlayerCount);
	}
	else
	{
		ExpectedPlayersFromLobby = 1;
	}

	// ── Fase de staging: esperar a que reconecten todos los clientes ──────
	SetFlowState(ETNMatchFlowState::WaitingForPlayers);

	if (ATN_CoopGameState* TNGS = GetGameState<ATN_CoopGameState>())
	{
		TNGS->FinishedPlayers = 0;
		TNGS->ServerMatchElapsedTime = 0.f;
		TNGS->CountdownValue = 0;
		TNGS->ExpectedPlayers = ExpectedPlayersFromLobby;
	}

	for (APlayerState* BasePS : GameState->PlayerArray)
	{
		if (ATN_CoopPlayerState* TNPS = Cast<ATN_CoopPlayerState>(BasePS))
		{
			TNPS->bIsAlive = true;
			TNPS->bHasFinishedRun = false;
			TNPS->bIsDBNO = false;
			TNPS->DBNOBleedoutTimeRemaining = -1.f;
			TNPS->FinishRank = 0;
			TNPS->bIsEliminated = false;
			TNPS->FinishTimeSeconds = -1.f;
			TNPS->DeathZoneTimeRemaining = -1.f;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[RunGameMode] BeginPlay: Waiting for %d players (staging)"), ExpectedPlayersFromLobby);

	// Timeout de seguridad: si no llegan todos, arranca igual
	GetWorldTimerManager().SetTimer(WaitingTimeoutTimerHandle, this,
		&ATN_RunGameMode::OnWaitingTimeout, WaitingForPlayersTimeoutSeconds, false);

	// Intentar arrancar de inmediato si el host ya cuenta como 1/1
	TryStartMatch();
}

void ATN_RunGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	UE_LOG(LogTemp, Log, TEXT("[RunGameMode] PostLogin: %s  (Pawn=%s)"),
		*GetNameSafe(NewPlayer),
		NewPlayer ? *GetNameSafe(NewPlayer->GetPawn()) : TEXT("NULL"));

	EnsurePlayerSpawned(NewPlayer);

	// Inicializar el PlayerState del jugador que acaba de conectarse.
	// En non-seamless travel, los PlayerStates se recrean; esto garantiza que
	// todos los clientes (no solo los que estaban en BeginPlay) arranquen limpiamente.
	if (ATN_CoopPlayerState* TNPS = NewPlayer ? NewPlayer->GetPlayerState<ATN_CoopPlayerState>() : nullptr)
	{
		TNPS->bIsAlive = true;
		TNPS->bHasFinishedRun = false;
		TNPS->bIsDBNO = false;
		TNPS->DBNOBleedoutTimeRemaining = -1.f;
		TNPS->FinishRank = 0;
		TNPS->bIsEliminated = false;
		TNPS->FinishTimeSeconds = -1.f;
		TNPS->DeathZoneTimeRemaining = -1.f;
	}

	// Actualizar el conteo de jugadores conectados en el GameState
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
		TNGS->ExpectedPlayers = ExpectedPlayersFromLobby;

		UE_LOG(LogTemp, Log, TEXT("[RunGameMode] PostLogin: ConnectedPlayers=%d  ExpectedFromLobby=%d  MatchStarted=%s"),
			TotalPlayers, ExpectedPlayersFromLobby, bMatchStarted ? TEXT("YES") : TEXT("NO"));
	}

	// Si aún estamos en staging, comprobar si ya tenemos a todos
	TryStartMatch();
}

void ATN_RunGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);

	// Actualizar conteo tras desconexión
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
		// ExpectedPlayers se mantiene desde el lobby para que la UI muestre "X / ExpectedFromLobby"
	}

	UE_LOG(LogTemp, Log, TEXT("[RunGameMode] Logout: %s"), *GetNameSafe(Exiting));

	// Comprobar si todos los jugadores restantes han terminado
	if (bMatchStarted)
	{
		UpdateRoundProgressAndMaybeFinish();
	}
	else
	{
		// Si aún estamos en staging y alguien se desconecta,
		// reducir la expectativa para no esperarle
		ExpectedPlayersFromLobby = FMath::Max(1, ExpectedPlayersFromLobby - 1);
		TryStartMatch();
	}
}

// ── Staging: esperar a todos los jugadores antes de empezar ────────────────────

void ATN_RunGameMode::TryStartMatch()
{
	if (bMatchStarted)
	{
		return;
	}

	int32 ConnectedNow = 0;
	for (APlayerState* BasePS : GameState->PlayerArray)
	{
		if (Cast<ATN_CoopPlayerState>(BasePS))
		{
			++ConnectedNow;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[RunGameMode] TryStartMatch: Connected=%d  Expected=%d"),
		ConnectedNow, ExpectedPlayersFromLobby);

	if (ConnectedNow >= ExpectedPlayersFromLobby)
	{
		OnWaitingTimeout(); // Reutiliza la misma función de arranque
	}
}

void ATN_RunGameMode::OnWaitingTimeout()
{
	if (bMatchStarted)
	{
		return;
	}

	bMatchStarted = true;
	GetWorldTimerManager().ClearTimer(WaitingTimeoutTimerHandle);

	MatchStartServerTime = GetWorld()->GetTimeSeconds();
	SetFlowState(ETNMatchFlowState::InProgress);

	// Actualizar conteo final
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
		TNGS->ExpectedPlayers = TotalPlayers; // Ahora sí es "de verdad"
	}

	UE_LOG(LogTemp, Log, TEXT("[RunGameMode] ═══ MATCH STARTED! ═══  (waited for players or timeout)"));
}

void ATN_RunGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
	EnsurePlayerSpawned(NewPlayer);

	UE_LOG(LogTemp, Log, TEXT("[RunGameMode] HandleStartingNewPlayer: %s  (Pawn=%s)"),
		*GetNameSafe(NewPlayer),
		NewPlayer ? *GetNameSafe(NewPlayer->GetPawn()) : TEXT("NULL"));
}

AActor* ATN_RunGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	TArray<AActor*> PlayerStarts;
	UGameplayStatics::GetAllActorsOfClass(this, APlayerStart::StaticClass(), PlayerStarts);
	if (PlayerStarts.Num() == 0)
	{
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

	// Fallback: todos ocupados → devolver el primero (barajado)
	UE_LOG(LogTemp, Warning, TEXT("[RunGameMode] ChoosePlayerStart: todos los PlayerStarts ocupados — usando fallback"));
	return PlayerStarts[0];
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

	// ── Detener el pawn y ocultarlo para que no se vea en la meta ──
	if (APawn* Pawn = PlayerController->GetPawn())
	{
		if (UCharacterMovementComponent* CMC = Cast<ACharacter>(Pawn) ? Cast<ACharacter>(Pawn)->GetCharacterMovement() : nullptr)
		{
			CMC->StopMovementImmediately();
			CMC->DisableMovement();
		}
		Pawn->DisableInput(PlayerController);

		// Ocultar al jugador que terminó (su personaje "desaparece")
		Pawn->SetActorHiddenInGame(true);
		Pawn->SetActorEnableCollision(false);
	}

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
	TNPS->bIsDBNO = false;
	TNPS->DBNOBleedoutTimeRemaining = -1.f;
	// Los eliminados NO consumen un puesto de carrera (NextFinishRank sólo avanza al cruzar la meta).
	// FinishRank = 0 para eliminados; la UI usa bIsEliminated para distinguirlos.
	// Guardamos el tiempo real de muerte para que la UI pueda ordenar eliminados por tiempo.
	TNPS->FinishRank = 0;
	TNPS->bIsEliminated = true;
	TNPS->FinishTimeSeconds = GetWorld()->GetTimeSeconds() - MatchStartServerTime;
	TNPS->DeathZoneTimeRemaining = -1.f;

	// Clean up from DBNO tracking if present
	DBNOPlayers.Remove(PlayerController);
	ReviveImmunePlayers.Remove(PlayerController);

	FVector DeathLocation = FVector::ZeroVector;

	if (APawn* Pawn = PlayerController->GetPawn())
	{
		DeathLocation = Pawn->GetActorLocation();

		// ── Limpiar knockdown visual si lo tenía ──
		if (ATortugaCharacter* Character = Cast<ATortugaCharacter>(Pawn))
		{
			Character->RecoverFromKnockdown();
		}

		// ── Ocultar el pawn completamente (no dejar cadáver) ──
		if (UCharacterMovementComponent* CMC = Cast<ACharacter>(Pawn) ? Cast<ACharacter>(Pawn)->GetCharacterMovement() : nullptr)
		{
			CMC->StopMovementImmediately();
			CMC->DisableMovement();
		}
		Pawn->DisableInput(PlayerController);
		Pawn->SetActorHiddenInGame(true);
		Pawn->SetActorEnableCollision(false);
	}

	// ── Spawnear pickup de rescate en la posición de muerte ──
	if (RescuePickupClass)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		ATN_RescuePickup* Pickup = GetWorld()->SpawnActor<ATN_RescuePickup>(
			RescuePickupClass, DeathLocation + FVector(0.f, 0.f, 30.f), FRotator::ZeroRotator, SpawnParams);

		if (Pickup)
		{
			Pickup->SetDeadPlayerId(TNPS->GetPlayerId());
			RescuePickups.Add(TNPS->GetPlayerId(), Pickup);
			UE_LOG(LogTemp, Log, TEXT("[Death] Spawned RescuePickup for %s (PlayerId=%d) at (%.0f,%.0f,%.0f)"),
				*GetNameSafe(PlayerController), TNPS->GetPlayerId(),
				DeathLocation.X, DeathLocation.Y, DeathLocation.Z);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Death] RescuePickupClass is not set! Assign it in BP_RunGameMode → Class Defaults."));
	}

	// ── Guardar referencia al pawn ANTES de entrar en espectador ──────────
	// EnterSpectateMode → ChangeState(Spectating) → UnPossess → GetPawn() devuelve nullptr.
	// Sin esta referencia, RevivePlayer no podría encontrar el pawn para re-poseerlo.
	if (APawn* DeadPawn = PlayerController->GetPawn())
	{
		DeadPlayerPawns.Add(TNPS->GetPlayerId(), DeadPawn);
		UE_LOG(LogTemp, Log, TEXT("[Death] Saved pawn ref for PlayerId=%d → %s"), TNPS->GetPlayerId(), *GetNameSafe(DeadPawn));
	}

	MovePlayerToSpectator(PlayerController);
	UpdateRoundProgressAndMaybeFinish();
}

// ── DBNO (Down But Not Out) ────────────────────────────────────────────────────

void ATN_RunGameMode::EnterDBNO(APlayerController* PlayerController)
{
	if (!HasAuthority() || !PlayerController)
	{
		return;
	}

	ATN_CoopPlayerState* TNPS = PlayerController->GetPlayerState<ATN_CoopPlayerState>();
	if (!TNPS || !TNPS->bIsAlive || TNPS->bIsDBNO)
	{
		return;  // Already dead, already DBNO, or invalid
	}

	// Check revive immunity — recently revived players can't be downed again immediately
	if (ReviveImmunePlayers.Contains(PlayerController))
	{
		UE_LOG(LogTemp, Log, TEXT("[DBNO] %s has revive immunity — ignoring DBNO trigger"), *GetNameSafe(PlayerController));
		return;
	}

	TNPS->bIsDBNO = true;
	TNPS->DBNOBleedoutTimeRemaining = DBNOBleedoutSeconds;

	// Apply infinite knockdown (Duration=0 means permanent — we'll clear it manually on revive/death)
	if (ATortugaCharacter* Character = Cast<ATortugaCharacter>(PlayerController->GetPawn()))
	{
		Character->ApplyKnockdown(DBNOBleedoutSeconds + 5.f);  // Generous duration, death/revive clears it
	}

	// Register in DBNO tracking map
	DBNOPlayers.Add(PlayerController, DBNOBleedoutSeconds);

	// Start the shared bleedout timer if not already running
	if (!GetWorldTimerManager().IsTimerActive(DBNOBleedoutTimerHandle))
	{
		GetWorldTimerManager().SetTimer(DBNOBleedoutTimerHandle, this,
			&ATN_RunGameMode::TickDBNOBleedout, 0.1f, true);
	}

	UE_LOG(LogTemp, Log, TEXT("[DBNO] %s entered DBNO state (%.1fs bleedout)"), *GetNameSafe(PlayerController), DBNOBleedoutSeconds);

	// Check if ALL alive players are now in DBNO (no one to revive)
	CheckAllAliveDBNO();
}

void ATN_RunGameMode::RevivePlayer(APlayerController* PlayerController)
{
	if (!HasAuthority() || !PlayerController)
	{
		return;
	}

	ATN_CoopPlayerState* TNPS = PlayerController->GetPlayerState<ATN_CoopPlayerState>();
	if (!TNPS)
	{
		return;
	}

	const bool bWasDBNO = TNPS->bIsDBNO;
	const bool bWasDead = !TNPS->bIsAlive && TNPS->bIsEliminated;

	if (!bWasDBNO && !bWasDead)
	{
		return; // Nothing to revive
	}

	// ── Restaurar estado ──────────────────────────────────────────────────
	TNPS->bIsDBNO = false;
	TNPS->DBNOBleedoutTimeRemaining = -1.f;
	TNPS->bIsAlive = true;
	TNPS->bHasFinishedRun = false;
	TNPS->bIsEliminated = false;
	TNPS->FinishRank = 0;
	TNPS->FinishTimeSeconds = -1.f;

	// Remove from DBNO tracking
	DBNOPlayers.Remove(PlayerController);
	if (DBNOPlayers.Num() == 0)
	{
		GetWorldTimerManager().ClearTimer(DBNOBleedoutTimerHandle);
	}

	// ── Destruir el pickup de rescate si existe ───────────────────────────
	if (TWeakObjectPtr<ATN_RescuePickup>* PickupPtr = RescuePickups.Find(TNPS->GetPlayerId()))
	{
		if (PickupPtr->IsValid())
		{
			(*PickupPtr)->Destroy();
		}
		RescuePickups.Remove(TNPS->GetPlayerId());
	}

	// ── Restaurar pawn visual y movimiento ────────────────────────────────
	// GetPawn() puede ser null si el jugador está en modo espectador (UnPossess).
	// En ese caso, buscamos el pawn guardado en DeadPlayerPawns.
	APawn* Pawn = PlayerController->GetPawn();
	if (!Pawn)
	{
		if (TWeakObjectPtr<APawn>* PawnPtr = DeadPlayerPawns.Find(TNPS->GetPlayerId()))
		{
			Pawn = PawnPtr->Get();
		}
	}
	if (Pawn)
	{
		// Restaurar visibilidad (el pawn fue ocultado en MarkPlayerDead)
		Pawn->SetActorHiddenInGame(false);
		Pawn->SetActorEnableCollision(true);

		if (ATortugaCharacter* Character = Cast<ATortugaCharacter>(Pawn))
		{
			Character->RecoverFromKnockdown();
			Character->SetDeadVisual(false); // Restaurar extremidades
		}

		Pawn->EnableInput(PlayerController);
		if (UCharacterMovementComponent* CMC = Cast<ACharacter>(Pawn) ? Cast<ACharacter>(Pawn)->GetCharacterMovement() : nullptr)
		{
			CMC->SetMovementMode(MOVE_Walking);
		}

		// ── Sacar del modo espectador: re-poseer el pawn ──────────────────
		// 1) Resetear flag de espectador en el PlayerState
		if (PlayerController->PlayerState)
		{
			PlayerController->PlayerState->SetIsOnlyASpectator(false);
		}
		// 2) Re-poseer el pawn
		PlayerController->Possess(Pawn);
		// 3) ClientRestart: dice al cliente que ahora controla este pawn
		//    y restaura el viewtarget (la cámara vuelve a su pawn).
		//    Sin esto el jugador seguiría viendo la cámara del espectador.
		PlayerController->ClientRestart(Pawn);
		// 4) ClientRestorePlayerInput: limpia IgnoreMoveInput/IgnoreLookInput
		//    que quedaron incrementados por ChangeState(Spectating) en el cliente.
		//    Sin esto el jugador se mueve con la cámara pero no puede moverse.
		if (AMP_GamePlayerController* TNPC = Cast<AMP_GamePlayerController>(PlayerController))
		{
			TNPC->ClientRestorePlayerInput();
			// Client RPCs no se ejecutan en el listen-server — llamada directa para el host.
			if (TNPC->IsLocalController())
			{
				TNPC->ForceRestoreInput();

				// Belt-and-suspenders: en el siguiente tick re-aplicamos el mapping context
				// de Enhanced Input + limpiamos los flags de ignore. Esto cubre el edge case
				// donde la cadena Possess → AcknowledgedPawn → ClientRestart → PawnClientRestart
				// no ha completado del todo antes de que ForceRestoreInput() termine.
				TWeakObjectPtr<ATortugaCharacter> WeakChar(Cast<ATortugaCharacter>(Pawn));
				TWeakObjectPtr<AMP_GamePlayerController> WeakPC(TNPC);
				GetWorldTimerManager().SetTimerForNextTick([WeakChar, WeakPC]()
				{
					if (WeakChar.IsValid())
					{
						WeakChar->ReapplyInputMapping();
					}
					if (WeakPC.IsValid())
					{
						WeakPC->ForceRestoreInput();
					}
				});
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[Revive] No pawn found for %s (PlayerId=%d) — cannot restore visual/control!"),
			*GetNameSafe(PlayerController), TNPS->GetPlayerId());
	}

	// Limpiar la entrada de DeadPlayerPawns
	DeadPlayerPawns.Remove(TNPS->GetPlayerId());

	// Grant brief immunity
	ReviveImmunePlayers.Add(PlayerController);
	FTimerHandle ImmunityTimer;
	TWeakObjectPtr<APlayerController> WeakPC = PlayerController;
	GetWorldTimerManager().SetTimer(ImmunityTimer, [this, WeakPC]()
	{
		ReviveImmunePlayers.Remove(WeakPC);
	}, ReviveImmunitySeconds, false);

	UE_LOG(LogTemp, Log, TEXT("[Revive] %s revived! (%.1fs immunity) wasDBNO=%s wasDead=%s"),
		*GetNameSafe(PlayerController), ReviveImmunitySeconds,
		bWasDBNO ? TEXT("YES") : TEXT("NO"),
		bWasDead ? TEXT("YES") : TEXT("NO"));
}

APawn* ATN_RunGameMode::GetDeadPlayerPawn(int32 PlayerId) const
{
	if (const TWeakObjectPtr<APawn>* PawnPtr = DeadPlayerPawns.Find(PlayerId))
	{
		return PawnPtr->Get();
	}
	return nullptr;
}

void ATN_RunGameMode::TickDBNOBleedout()
{
	if (!HasAuthority())
	{
		return;
	}

	TArray<TWeakObjectPtr<APlayerController>> Keys;
	DBNOPlayers.GetKeys(Keys);

	for (const TWeakObjectPtr<APlayerController>& WeakPC : Keys)
	{
		APlayerController* PC = WeakPC.Get();
		if (!PC)
		{
			DBNOPlayers.Remove(WeakPC);
			continue;
		}

		float* Remaining = DBNOPlayers.Find(WeakPC);
		if (!Remaining) { continue; }

		*Remaining = FMath::Max(0.f, *Remaining - 0.1f);

		// Sync to PlayerState for HUD
		if (ATN_CoopPlayerState* TNPS = PC->GetPlayerState<ATN_CoopPlayerState>())
		{
			TNPS->DBNOBleedoutTimeRemaining = *Remaining;
		}

		if (*Remaining <= KINDA_SMALL_NUMBER)
		{
			UE_LOG(LogTemp, Log, TEXT("[DBNO] %s bleedout expired → dying for real"), *GetNameSafe(PC));
			DBNOPlayers.Remove(WeakPC);
			MarkPlayerDead(PC);
		}
	}

	if (DBNOPlayers.Num() == 0)
	{
		GetWorldTimerManager().ClearTimer(DBNOBleedoutTimerHandle);
	}
	else
	{
		// Comprobar si TODOS los vivos están ahora en DBNO (nadie puede revivir).
		// Esto cubre el caso de que el segundo jugador caiga mientras el primero
		// ya estaba en DBNO via el bleedout tick (EnterDBNO no se llama de nuevo).
		CheckAllAliveDBNO();
	}
}

void ATN_RunGameMode::CheckAllAliveDBNO()
{
	if (!HasAuthority() || !GameState)
	{
		return;
	}

	int32 AliveCount = 0;
	int32 DBNOCount = 0;

	for (APlayerState* BasePS : GameState->PlayerArray)
	{
		if (const ATN_CoopPlayerState* CastPS = Cast<ATN_CoopPlayerState>(BasePS))
		{
			if (CastPS->bIsAlive)
			{
				++AliveCount;
				if (CastPS->bIsDBNO)
				{
					++DBNOCount;
				}
			}
		}
	}

	// If ALL alive players are in DBNO, no one can revive → kill everyone
	if (AliveCount > 0 && AliveCount == DBNOCount)
	{
		UE_LOG(LogTemp, Log, TEXT("[DBNO] All %d alive players are in DBNO — killing everyone"), AliveCount);

		TArray<TWeakObjectPtr<APlayerController>> Keys;
		DBNOPlayers.GetKeys(Keys);
		for (const TWeakObjectPtr<APlayerController>& WeakPC : Keys)
		{
			if (APlayerController* PC = WeakPC.Get())
			{
				MarkPlayerDead(PC);
			}
		}
		DBNOPlayers.Empty();
		GetWorldTimerManager().ClearTimer(DBNOBleedoutTimerHandle);
	}
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
		// ── Destroy all pawns for WASAPI cleanup ─────────────────────────────
		// ProximityVoiceComponent::EndPlay(Destroyed) fires while audio is alive.
		// With seamless travel the connection stays alive — no socket race condition.
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* PC = It->Get();
			if (!PC) { continue; }
			if (APawn* Pawn = PC->GetPawn())
			{
				Pawn->Destroy();
			}
		}

		// ── Destruir spectator pawns de jugadores eliminados ──────────────────
		// Los SpectatorPawns NO están controlados por PlayerController.GetPawn()
		// en el loop anterior → quedan como "cuerpos fantasma" en el lobby.
		for (TActorIterator<ASpectatorPawn> It(World); It; ++It)
		{
			It->Destroy();
		}

		// ── Destruir RescuePickups residuales ─────────────────────────────────
		for (TActorIterator<ATN_RescuePickup> It(World); It; ++It)
		{
			It->Destroy();
		}

		// ── Destruir TortugaCharacters sin controller (por si el loop anterior los dejó)
		// Evita que lleguen al lobby como meshes fantasma.
		for (TActorIterator<ATortugaCharacter> It(World); It; ++It)
		{
			It->Destroy();
		}

		// ── Seamless ServerTravel — connection persists, no NetDriver destroy ─
		const FString TravelURL = LobbyMapPath;
		UE_LOG(LogTemp, Log, TEXT("[RunGameMode] Seamless ServerTravel to: %s"), *TravelURL);
		World->ServerTravel(TravelURL);
	}
}

// ── Seamless Travel Handlers ──────────────────────────────────────────────────

void ATN_RunGameMode::HandleSeamlessTravelPlayer(AController*& C)
{
	// Limpiar estado espectador ANTES de Super — jugadores que murieron/terminaron
	// en la carrera estaban en modo espectador. Sin esto, PlayerCanRestart() devuelve
	// false y Super no les spawnea pawn.
	if (APlayerController* PC = Cast<APlayerController>(C))
	{
		// ── Destruir pawn prematuro ────────────────────────────────────────────
		// BeginPlay itera los PCs y llama EnsurePlayerSpawned, lo que puede
		// spawnear un pawn ANTES de que HandleSeamlessTravelPlayer lo haga.
		// Super::HandleSeamlessTravelPlayer → RestartPlayer NO destruye el pawn
		// existente → quedarían DOS pawns (ghost pawn).
		// Solución: destruir el pawn existente aquí para que Super spawne uno limpio.
		if (APawn* OldPawn = PC->GetPawn())
		{
			UE_LOG(LogTemp, Log, TEXT("[RunGameMode] HandleSeamlessTravelPlayer: destruyendo pawn prematuro '%s' de %s"),
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

void ATN_RunGameMode::PostSeamlessTravel()
{
	Super::PostSeamlessTravel();

	UE_LOG(LogTemp, Log, TEXT("[RunGameMode] PostSeamlessTravel: initializing all players for run."));

	// Resetear estado de todos los jugadores que viajaron
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC) { continue; }

		// Reset PlayerState para la carrera
		if (ATN_CoopPlayerState* TNPS = PC->GetPlayerState<ATN_CoopPlayerState>())
		{
			const FName SavedHelmet = TNPS->EquippedHelmetId;

			TNPS->bIsAlive = true;
			TNPS->bHasFinishedRun = false;
			TNPS->bIsDBNO = false;
			TNPS->DBNOBleedoutTimeRemaining = -1.f;
			TNPS->FinishRank = 0;
			TNPS->bIsEliminated = false;
			TNPS->FinishTimeSeconds = -1.f;
			TNPS->DeathZoneTimeRemaining = -1.f;

			// Asegurar que tiene pawn
			EnsurePlayerSpawned(PC);

			// Forzar aplicación del helmet en todos los clientes una vez que el pawn existe.
			// MulticastForceApplyHelmet evita la race condition del dirty-trick (NAME_None → valor)
			// que podía llegar primero y dejar el casco sin aplicar permanentemente.
			if (SavedHelmet != NAME_None)
			{
				TNPS->MulticastForceApplyHelmet(SavedHelmet);
			}
		}
		else
		{
			// Asegurar que tiene pawn aunque no tenga PlayerState
			EnsurePlayerSpawned(PC);
		}
	}

	// Actualizar conteo y comprobar si podemos arrancar
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
		TNGS->ExpectedPlayers = ExpectedPlayersFromLobby;
	}

	TryStartMatch();
}

void ATN_RunGameMode::SetFlowState(ETNMatchFlowState NewState) const
{
	if (ATN_CoopGameState* TNGS = GetGameState<ATN_CoopGameState>())
	{
		TNGS->MatchFlowState = NewState;
		TNGS->BroadcastFlowStateChange();
	}
}


