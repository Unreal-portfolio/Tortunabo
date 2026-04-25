#include "Game/TN_RunGameMode.h"
#include "Core/TN_CoopGameState.h"
#include "Core/TN_CoopPlayerState.h"
#include "Player/MP_GamePlayerController.h"
#include "Player/TortugaCharacter.h"
#include "Multiplayer/MP_GameInstance.h"
#include "World/TN_RescuePickup.h"
#include "Player/TN_InventoryComponent.h"
#include "World/TN_DeathZoneVolume.h"
#include "World/TN_ChunkManager.h"
#include "World/TN_CollectionZone.h"
#include "GameFramework/SpectatorPawn.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerController.h"
#include "Components/SkeletalMeshComponent.h"
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

	// Suscribirse a las CollectionZones del nivel para que el GameMode reaccione
	// cuando completen su goal (sumar bonus, log, futuras señales de progresión).
	for (TActorIterator<ATN_CollectionZone> It(GetWorld()); It; ++It)
	{
		if (ATN_CollectionZone* Zone = *It)
		{
			Zone->OnZoneGoalReached.AddUObject(this, &ATN_RunGameMode::HandleCollectionZoneGoal);
		}
	}

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
			TNPS->RaceScore = 0;
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
		TNPS->RaceScore = 0;
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

	// ── Iniciar cronómetro de carrera (actualiza ServerMatchElapsedTime cada 0.5s) ──
	GetWorldTimerManager().SetTimer(RaceClockTimerHandle, this,
		&ATN_RunGameMode::TickRaceClock, 0.5f, true);

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
	if (!TNPS)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MarkPlayerFinished] '%s' — sin PlayerState, ignorado."), *GetNameSafe(PlayerController));
		return;
	}
	if (TNPS->bHasFinishedRun)
	{
		UE_LOG(LogTemp, Log, TEXT("[MarkPlayerFinished] '%s' — ya terminó (bHasFinishedRun=true), ignorado."), *GetNameSafe(PlayerController));
		return;
	}
	if (!TNPS->bIsAlive)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MarkPlayerFinished] '%s' — bIsAlive=false, ignorado (¿murió antes de llegar?)."), *GetNameSafe(PlayerController));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[MarkPlayerFinished] ══ '%s' cruzó la meta ══ Rank=%d"), *GetNameSafe(PlayerController), NextFinishRank);
	TNPS->bHasFinishedRun = true;
	TNPS->FinishTimeSeconds = GetWorld()->GetTimeSeconds() - MatchStartServerTime;
	TNPS->FinishRank = NextFinishRank++;

	// ── Asignar puntos según posición de llegada (#26) ──────────────────────
	// 1º=400, 2º=300, 3º=200, 4º=100. Más de 4 jugadores → 50 pts por seguridad.
	// Nota: RaceScore se SUMA al actual (que puede traer score de ScorePickups
	// recogidos durante la run), no se sobrescribe.
	static const int32 RankScoreTable[] = { 400, 300, 200, 100 };
	const int32 RankIndex = TNPS->FinishRank - 1;
	const int32 RankScore = (RankIndex >= 0 && RankIndex < 4) ? RankScoreTable[RankIndex] : 50;
	TNPS->RaceScore += RankScore;
	UE_LOG(LogTemp, Log, TEXT("[FINISH] RaceScore+=%d → total=%d para '%s' (Rank=%d)"),
		RankScore, TNPS->RaceScore, *GetNameSafe(PlayerController), TNPS->FinishRank);

	// Scoreboard global
	if (ATN_CoopGameState* GS = GetGameState<ATN_CoopGameState>())
	{
		GS->Server_UpsertRaceResult(TNPS->GetPlayerId(), TNPS->GetPlayerName(),
			TNPS->FinishRank, TNPS->FinishTimeSeconds, TNPS->RaceScore, false);
	}

	// Limpiar DBNO si llegaron a meta mientras estaban noqueados
	if (TNPS->bIsDBNO)
	{
		TNPS->bIsDBNO = false;
		TNPS->DBNOBleedoutTimeRemaining = -1.f;
		DBNOPlayers.Remove(PlayerController);
		if (DBNOPlayers.Num() == 0)
		{
			GetWorldTimerManager().ClearTimer(DBNOBleedoutTimerHandle);
		}
	}

	// ── Detener el pawn y ocultarlo para que no se vea en la meta ──
	if (APawn* Pawn = PlayerController->GetPawn())
	{
		if (ACharacter* Ch = Cast<ACharacter>(Pawn))
		{
			if (UCharacterMovementComponent* CMC = Ch->GetCharacterMovement())
			{
				CMC->StopMovementImmediately();
				CMC->DisableMovement();
			}
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
	// Un jugador que ya cruzó la meta no puede morir retroactivamente.
	// Evita que un countdown de death zone rezagado sobreescriba el rank ganado.
	if (TNPS->bHasFinishedRun)
	{
		return;
	}

	// ── Tótem auto-revive: si el jugador lleva un tótem en el inventario,
	//    se consume automáticamente y cancela la muerte. ─────────────────────────
	if (APawn* DyingPawn = PlayerController->GetPawn())
	{
		if (ATortugaCharacter* DyingChar = Cast<ATortugaCharacter>(DyingPawn))
		{
			if (UTN_InventoryComponent* Inv = DyingChar->GetInventoryComponent())
			{
				FTN_InventoryItem ConsumedTotem;
				if (Inv->TryConsumeItemByUseType(ETN_ItemUseType::Totem, ConsumedTotem))
				{
					// Tótem consumido → cancelar muerte + feedback visual
					UE_LOG(LogTemp, Log, TEXT("[Totem] Auto-revive activado para %s — totem consumido."),
						*GetNameSafe(PlayerController));
					DyingChar->Multicast_OnTotemAutoRevive();
					return;
				}
			}
		}
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

	UE_LOG(LogTemp, Log, TEXT("[DEATH] '%s' eliminated · time=%.2fs · score=%d"),
		*GetNameSafe(PlayerController), TNPS->FinishTimeSeconds, TNPS->RaceScore);

	// Scoreboard global — eliminado al final del array
	if (ATN_CoopGameState* GS = GetGameState<ATN_CoopGameState>())
	{
		GS->Server_UpsertRaceResult(TNPS->GetPlayerId(), TNPS->GetPlayerName(),
			0, TNPS->FinishTimeSeconds, TNPS->RaceScore, true);
	}

	// Clean up from DBNO tracking if present
	DBNOPlayers.Remove(PlayerController);
	ReviveImmunePlayers.Remove(PlayerController);

	// Cancelar el timer de inmunidad post-revive si sigue activo.
	// Sin esto, un timer antiguo puede eliminar la inmunidad de una segunda revivida.
	if (FTimerHandle* T = ImmunityTimers.Find(PlayerController))
	{
		GetWorldTimerManager().ClearTimer(*T);
		ImmunityTimers.Remove(PlayerController);
	}

	FVector DeathLocation = FVector::ZeroVector;

	if (APawn* Pawn = PlayerController->GetPawn())
	{
		DeathLocation = Pawn->GetActorLocation();

		if (ATortugaCharacter* Character = Cast<ATortugaCharacter>(Pawn))
		{
			Character->RecoverFromKnockdown();
		}

		if (ACharacter* Ch = Cast<ACharacter>(Pawn))
		{
			if (UCharacterMovementComponent* CMC = Ch->GetCharacterMovement())
			{
				CMC->StopMovementImmediately();
			}
		}
		Pawn->DisableInput(PlayerController);

		// Pawn muerto SIEMPRE relevante para todos los clientes. Sin esto, cuando el
		// PC muerto entra espectador y la cámara se aleja (ej. viendo a un compañero
		// vivo que avanza), el netcull quita el pawn muerto del cliente → ragdoll
		// "desaparece tras unos segundos" aunque el servidor lo tenga vivo. Restaurado
		// a default en RevivePlayer.
		Pawn->bAlwaysRelevant = true;
		Pawn->SetNetDormancy(DORM_Awake);

		// Q1-13: activar ragdoll de muerte — pawn queda visible (es el visual del rescate)
		if (ATortugaCharacter* Character = Cast<ATortugaCharacter>(Pawn))
		{
			Character->SetDeadVisual(true);
			// Fallback: si no hay PhysicsAsset, ragdoll imposible → ocultar pawn.
			// NO usar IsSimulatingPhysics() aquí: Chaos puede devolver false
			// inmediatamente tras SetAllBodiesSimulatePhysics(true) por asincronía
			// → el pawn se ocultaba por error aunque el ragdoll se activara bien
			// en el tick siguiente. Check estático: existe PhysicsAsset → ragdoll OK.
			USkeletalMeshComponent* SM = Character->GetMesh();
			const bool bHasRagdollSetup = SM && SM->GetPhysicsAsset();
			if (!bHasRagdollSetup)
			{
				UE_LOG(LogTemp, Warning, TEXT("[Death] Sin PhysicsAsset en BP — ocultando pawn %s (fallback sin ragdoll)"), *GetNameSafe(Pawn));
				Pawn->SetActorHiddenInGame(true);
				Pawn->SetActorEnableCollision(false);
			}
		}
		else
		{
			Pawn->SetActorHiddenInGame(true);
			Pawn->SetActorEnableCollision(false);
		}
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
			// Q1-14: el pawn ragdolleado es el visual — ocultar la malla del pickup
			Pickup->HideInteractableMesh();
			RescuePickups.Add(TNPS->GetPlayerId(), Pickup);
			UE_LOG(LogTemp, Log, TEXT("[Death] Spawned RescuePickup for %s (PlayerId=%d) at (%.0f,%.0f,%.0f)"),
				*GetNameSafe(PlayerController), TNPS->GetPlayerId(),
				DeathLocation.X, DeathLocation.Y, DeathLocation.Z);

			// DEATH-01: timer para swap ragdoll→pickup tras DeathRagdollDurationSeconds.
			// CreateUObject permite que ClearAllTimersForObject en EndPlay cancele si el
			// game mode se destruye. El PlayerId viaja como payload del delegate.
			FTimerDelegate Del = FTimerDelegate::CreateUObject(
				this, &ATN_RunGameMode::FinalizeDeathVisual, TNPS->GetPlayerId());
			FTimerHandle& H = DeathFinalizeTimers.FindOrAdd(TNPS->GetPlayerId());
			GetWorldTimerManager().SetTimer(H, Del, DeathRagdollDurationSeconds, false);
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

		// FIX ragdoll despawn intermitente: cambiar el Owner del pawn muerto al
		// GameMode. Sin este cambio, si el PC hace Logout (timeout, quit,
		// PIE-shutdown), UE engine puede destruir cascada los actors Owner=PC
		// → ragdoll se evapora junto al PC. Owner=GameMode desliga el pawn del
		// ciclo de vida del PC. El pawn solo se destruirá explícitamente desde
		// RevivePlayer (cuando se reataca el mesh) o FinishRoundAndReturnToLobby.
		DeadPawn->SetOwner(this);
		// Garantía adicional: lifespan infinito (por si algo lo asignó por otro lado).
		DeadPawn->SetLifeSpan(0.f);
		UE_LOG(LogTemp, Log, TEXT("[Death] Detached pawn from PC ownership · pawn=%s now owned by GameMode"),
			*GetNameSafe(DeadPawn));
	}

	MovePlayerToSpectator(PlayerController);
	UpdateRoundProgressAndMaybeFinish();
}

void ATN_RunGameMode::FinalizeDeathVisual(int32 PlayerId)
{
	// DEATH-01 · callback del timer lanzado en MarkPlayerDead.
	// Diseño (2026-04-24 rev 2): el ragdoll del pawn es el visual PERMANENTE del
	// pickup. Tras DeathRagdollDurationSeconds (cuando el ragdoll ya ha caído y
	// se ha estabilizado), sincronizamos la posición del trigger del pickup con
	// la pos real del cuerpo. El mesh del pickup NUNCA se muestra — el body del
	// ragdoll ES el visual. El trigger captura la interacción para revive.
	DeathFinalizeTimers.Remove(PlayerId);

	// Si el jugador ya revivió, RevivePlayer habrá destruido el pickup → salimos.
	TWeakObjectPtr<ATN_RescuePickup>* PickupPtr = RescuePickups.Find(PlayerId);
	ATN_RescuePickup* Pickup = PickupPtr ? PickupPtr->Get() : nullptr;
	if (!Pickup) { return; }

	TWeakObjectPtr<APawn>* PawnPtr = DeadPlayerPawns.Find(PlayerId);
	APawn* DeadPawn = PawnPtr ? PawnPtr->Get() : nullptr;

	if (DeadPawn)
	{
		// Leer pos y yaw del root bone del ragdoll para colocar el trigger del pickup
		// donde realmente ha caído el cuerpo (puede haber rodado por pendientes, etc.).
		if (const ATortugaCharacter* Character = Cast<ATortugaCharacter>(DeadPawn))
		{
			if (USkeletalMeshComponent* SKM = Character->GetMesh())
			{
				if (SKM->IsSimulatingPhysics())
				{
					const FName RootBone = SKM->GetBoneName(0);
					const FVector RagdollLoc = SKM->GetBoneLocation(RootBone, EBoneSpaces::WorldSpace);
					const FRotator BodyYaw(0.f, SKM->GetComponentRotation().Yaw, 0.f);
					Pickup->SetActorLocationAndRotation(RagdollLoc, BodyYaw);
				}
				else
				{
					// Fallback sin PhysicsAsset: en lugar de mostrar el mesh del pickup
					// (que daba un visual feo "icono flotante" tras revive si el timer
					// de cleanup race-condicionaba), mostrar el pawn donde quedó como
					// visual estático. RevivePlayer hará SetActorHiddenInGame(false)
					// igualmente y restablecerá; aquí solo exponemos algo visible.
					if (APawn* DeadPawnFallback = Cast<APawn>(DeadPawn))
					{
						DeadPawnFallback->SetActorHiddenInGame(false);
					}
					// Sincronizar pos del pickup trigger con el pawn (interactable a su lado)
					Pickup->SetActorLocation(DeadPawn->GetActorLocation());
				}
			}
		}
		// NOTA: NO ocultamos el pawn. El ragdoll es el visual del pickup.
		// RevivePlayer reataca el mesh al capsule cuando el jugador revive.
	}

	UE_LOG(LogTemp, Log, TEXT("[Death] FinalizeDeathVisual PlayerId=%d → trigger sincronizado con ragdoll"), PlayerId);
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

	const bool bWasDBNO        = TNPS->bIsDBNO;
	const bool bWasDead        = !TNPS->bIsAlive && TNPS->bIsEliminated;
	const ATortugaCharacter* TurtleCheck = Cast<ATortugaCharacter>(PlayerController->GetPawn());
	const bool bWasKnockedDown = TurtleCheck ? TurtleCheck->IsKnockedDown() : false;

	if (!bWasDBNO && !bWasDead && !bWasKnockedDown)
	{
		return; // Nothing to revive
	}

	// ── Knockdown-only revival: pawn sigue poseído, solo cancelar el knockdown ──
	// bIsAlive=true, bIsDBNO=false → no hay que re-poseer ni restaurar estado.
	if (bWasKnockedDown && !bWasDBNO && !bWasDead)
	{
		if (ATortugaCharacter* Turtle = Cast<ATortugaCharacter>(PlayerController->GetPawn()))
		{
			Turtle->RecoverFromKnockdown();
		}
		return;
	}

	// ── Restaurar estado ──────────────────────────────────────────────────
	TNPS->bIsDBNO = false;
	TNPS->DBNOBleedoutTimeRemaining = -1.f;
	TNPS->bIsAlive = true;
	TNPS->bHasFinishedRun = false;
	TNPS->bIsEliminated = false;
	TNPS->FinishRank = 0;
	TNPS->FinishTimeSeconds = -1.f;
	TNPS->DeathZoneTimeRemaining = -1.f;
	TNPS->RaceScore = 0;

	// Remove from DBNO tracking
	DBNOPlayers.Remove(PlayerController);
	if (DBNOPlayers.Num() == 0)
	{
		GetWorldTimerManager().ClearTimer(DBNOBleedoutTimerHandle);
	}

	// DEATH-01: cancelar el timer de swap ragdoll→pickup si aún no disparó.
	// Sin esto: si revives antes de DeathRagdollDurationSeconds, el timer
	// ejecutaría FinalizeDeathVisual tras la revivida → ocultaría el pawn vivo.
	if (FTimerHandle* DeathTimer = DeathFinalizeTimers.Find(TNPS->GetPlayerId()))
	{
		GetWorldTimerManager().ClearTimer(*DeathTimer);
		DeathFinalizeTimers.Remove(TNPS->GetPlayerId());
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
	// ── Conceder inmunidad ANTES de re-habilitar colisión ────────────────────
	// SetActorEnableCollision(true) dispara OnBoxBeginOverlap sincrónicamente.
	// Si el pawn revivido cae dentro de una death zone, OnBoxBeginOverlap ve
	// IsPlayerReviveImmune=false si la inmunidad se añade después → B6 ineficaz.
	// Al añadirla aquí, OnBoxBeginOverlap recibe PC ya inmune → descarta el overlap.
	ReviveImmunePlayers.Add(PlayerController);
	// Cancelar timer anterior si existe (ej: jugador revivido dos veces rápido).
	// Sin esto, el timer antiguo expiraría y eliminaría la inmunidad activa de la segunda revivida.
	FTimerHandle& ImmunityTimer = ImmunityTimers.FindOrAdd(PlayerController);
	GetWorldTimerManager().ClearTimer(ImmunityTimer);
	TWeakObjectPtr<APlayerController> WeakImmunityPC = PlayerController;
	TWeakObjectPtr<ATN_RunGameMode> WeakGM(this);
	GetWorldTimerManager().SetTimer(ImmunityTimer, [WeakGM, WeakImmunityPC]()
	{
		ATN_RunGameMode* GM = WeakGM.Get();
		if (!GM) { return; }
		GM->ReviveImmunePlayers.Remove(WeakImmunityPC);
		GM->ImmunityTimers.Remove(WeakImmunityPC);

		// ── Forzar re-evaluación de death zones ──────────────────────────────
		// SetActorEnableCollision(true) no dispara OnBoxBeginOverlap de forma
		// fiable cuando el pawn ya estaba geométricamente dentro del volumen.
		// Tras expirar la inmunidad, comprobamos manualmente todas las death zones.
		if (APlayerController* ImmunityPC = WeakImmunityPC.Get())
		{
			for (TActorIterator<ATN_DeathZoneVolume> It(GM->GetWorld()); It; ++It)
			{
				(*It)->ForceCheckPlayer(ImmunityPC);
			}
		}
	}, ReviveImmunitySeconds, false);

	if (Pawn)
	{
		// ORDEN CRÍTICO (2026-04-24):
		// 1. SetDeadVisual(false) PRIMERO → apaga ragdoll, re-attachea mesh al capsule,
		//    restaura bReplicateMovement=true. Sin este paso previo, SetActorLocation
		//    sobre un pawn con mesh fully-simulated dispara el warning
		//    "Attempting to move a fully simulated skeletal mesh".
		// 2. SetActorLocation DESPUÉS → teleporta a zona segura con capsule+mesh ya
		//    re-sincronizados.
		if (ATortugaCharacter* Character = Cast<ATortugaCharacter>(Pawn))
		{
			Character->RecoverFromKnockdown();
			Character->SetDeadVisual(false); // Restaurar extremidades + apagar ragdoll
		}

		// Restaurar CMC antes del teleport
		if (ACharacter* Ch = Cast<ACharacter>(Pawn))
		{
			if (UCharacterMovementComponent* CMC = Ch->GetCharacterMovement())
			{
				CMC->SetMovementMode(MOVE_Walking);
			}
		}

		// ── Teleportar a zona de chunks activa antes de revivir ───────────────
		// El pawn estuvo hidden en la posición de muerte. CleanupChunks puede haber
		// destruido el chunk que lo rodeaba mientras el líder avanzaba. Pero si NO
		// fue destruido, revivir en la posición de muerte es lo más natural y
		// preserva la inmersión (player aparece donde se rescató).
		//
		// Estrategia: priorizar la pos actual del pawn (donde está el ragdoll). Si
		// está demasiado lejos del próximo chunk a spawnear (> 4000 cm) asumimos
		// que el chunk que lo contenía fue destruido y caemos al SafeLocation
		// (próximo chunk) como fallback.
		{
			ATN_ChunkManager* ChunkManager = nullptr;
			for (TActorIterator<ATN_ChunkManager> It(GetWorld()); It; ++It)
			{
				ChunkManager = *It;
				break;
			}

			if (ChunkManager)
			{
				const FVector RagdollLoc   = Pawn->GetActorLocation();
				const FVector NextChunkLoc = ChunkManager->GetSafeReviveLocation();
				const float DistToNext     = FVector::Dist(RagdollLoc, NextChunkLoc);

				FVector ReviveLoc;
				if (DistToNext < 4000.f)
				{
					// Cerca de zona activa → revivir donde cayó el ragdoll +100 Z
					ReviveLoc = RagdollLoc + FVector(0.f, 0.f, 100.f);
					UE_LOG(LogTemp, Log, TEXT("[Revive] In-place at ragdoll location (%.0f,%.0f,%.0f) · DistToNext=%.0f"),
						ReviveLoc.X, ReviveLoc.Y, ReviveLoc.Z, DistToNext);
				}
				else
				{
					// Lejos del avance → chunk antiguo posiblemente destruido, fallback adelante
					ReviveLoc = NextChunkLoc;
					UE_LOG(LogTemp, Warning, TEXT("[Revive] Ragdoll lejos del avance (%.0f cm) → teleport adelante (%.0f,%.0f,%.0f)"),
						DistToNext, ReviveLoc.X, ReviveLoc.Y, ReviveLoc.Z);
				}

				Pawn->SetActorLocation(ReviveLoc, false, nullptr, ETeleportType::TeleportPhysics);
			}
		}

		// Restaurar visibilidad (el pawn fue ocultado en MarkPlayerDead)
		Pawn->SetActorHiddenInGame(false);
		Pawn->SetActorEnableCollision(true);
		// Restaurar relevancy default (tras MarkPlayerDead forzado bAlwaysRelevant=true)
		Pawn->bAlwaysRelevant = false;

		// ── Sacar del modo espectador: re-poseer el pawn ──────────────────
		// 1) Resetear flag de espectador en el PlayerState
		if (PlayerController->PlayerState)
		{
			PlayerController->PlayerState->SetIsOnlyASpectator(false);
		}
		// 2) Forzar UnPossess si el PC aún posee al pawn.
		//    En el listen-server, ChangeState(Spectating) puede NO llamar UnPossess
		//    internamente, dejando al PC poseyendo al pawn muerto. Si Possess(Pawn)
		//    recibe el mismo pawn que ya posee, es un no-op → ChangeState(Playing)
		//    nunca se llama y el host se queda atrapado en estado Spectating.
		if (PlayerController->GetPawn() == Pawn)
		{
			PlayerController->UnPossess();
		}
		// 3) Re-poseer el pawn (ahora garantizado que no es no-op)
		PlayerController->Possess(Pawn);
		// 4) Safety: forzar ChangeState(Playing) por si Possess no lo hizo
		PlayerController->ChangeState(NAME_Playing);

		// 5) EnableInput DESPUÉS de Possess — APawn::EnableInput verifica
		//    que el PC == Pawn->Controller. Si se llama antes de Possess,
		//    Controller es nullptr (por UnPossess) y EnableInput falla
		//    silenciosamente, dejando bInputEnabled = false para siempre.
		Pawn->EnableInput(PlayerController);

		// 6) ClientRestart: dice al cliente que ahora controla este pawn
		PlayerController->ClientRestart(Pawn);
		// 7) Apuntar cámara al pawn (limpia el ViewTarget del espectador)
		PlayerController->SetViewTarget(Pawn);
		// 8) ClientRestorePlayerInput: limpia IgnoreMoveInput/IgnoreLookInput
		if (AMP_GamePlayerController* TNPC = Cast<AMP_GamePlayerController>(PlayerController))
		{
			TNPC->ClientRestorePlayerInput();
			// Client RPCs no se ejecutan en el listen-server — llamada directa para el host.
			if (TNPC->IsLocalController())
			{
				TNPC->ForceRestoreInput();
			}

			// 9) Timer repetitivo de seguridad: re-aplica input cada 0.1s durante 1s.
			//    Cubre TODOS los edge cases donde la cadena
			//    Possess → AcknowledgedPawn → ClientRestart → PawnClientRestart
			//    no ha terminado, o donde UE re-incrementa IgnoreMoveInput internamente.
			TWeakObjectPtr<ATortugaCharacter> WeakChar(Cast<ATortugaCharacter>(Pawn));
			TWeakObjectPtr<AMP_GamePlayerController> WeakPC(TNPC);
			TWeakObjectPtr<APawn> WeakPawn(Pawn);
			TWeakObjectPtr<ATN_RunGameMode> WeakGM2(this);
			TSharedPtr<FTimerHandle> RetryHandle = MakeShared<FTimerHandle>();
			TSharedPtr<int32> RetryCount = MakeShared<int32>(10); // 10 × 0.1s = 1s
			GetWorldTimerManager().SetTimer(*RetryHandle,
				[WeakChar, WeakPC, WeakPawn, RetryCount, RetryHandle, WeakGM2]()
			{
				ATN_RunGameMode* GM2 = WeakGM2.Get();
				if (!GM2 || *RetryCount <= 0)
				{
					if (GM2) { GM2->GetWorldTimerManager().ClearTimer(*RetryHandle); }
					return;
				}
				--(*RetryCount);

				if (WeakPawn.IsValid())
				{
					WeakPawn->EnableInput(nullptr); // nullptr bypasses Controller check
				}
				if (WeakChar.IsValid())
				{
					WeakChar->ReapplyInputMapping();
				}
				if (WeakPC.IsValid())
				{
					WeakPC->ForceRestoreInput();
				}
				if (WeakPawn.IsValid() && WeakPawn->GetController())
				{
					if (ACharacter* Ch = Cast<ACharacter>(WeakPawn.Get()))
					{
						if (UCharacterMovementComponent* CMC = Ch->GetCharacterMovement())
						{
							if (CMC->MovementMode == MOVE_None)
							{
								CMC->SetMovementMode(MOVE_Walking);
							}
						}
					}
				}
			}, 0.1f, true);
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[Revive] No pawn found for %s (PlayerId=%d) — cannot restore visual/control!"),
			*GetNameSafe(PlayerController), TNPS->GetPlayerId());
	}

	// Limpiar la entrada de DeadPlayerPawns
	DeadPlayerPawns.Remove(TNPS->GetPlayerId());

	UE_LOG(LogTemp, Log, TEXT("[Revive] %s revived! (%.1fs immunity) wasDBNO=%s wasDead=%s wasKnocked=%s"),
		*GetNameSafe(PlayerController), ReviveImmunitySeconds,
		bWasDBNO ? TEXT("YES") : TEXT("NO"),
		bWasDead ? TEXT("YES") : TEXT("NO"),
		bWasKnockedDown ? TEXT("YES") : TEXT("NO"));
}

APawn* ATN_RunGameMode::GetDeadPlayerPawn(int32 PlayerId) const
{
	if (const TWeakObjectPtr<APawn>* PawnPtr = DeadPlayerPawns.Find(PlayerId))
	{
		return PawnPtr->Get();
	}
	return nullptr;
}

bool ATN_RunGameMode::IsPlayerReviveImmune(APlayerController* PC) const
{
	return PC && ReviveImmunePlayers.Contains(PC);
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

void ATN_RunGameMode::TickRaceClock()
{
	if (ATN_CoopGameState* TNGS = GetGameState<ATN_CoopGameState>())
	{
		TNGS->ServerMatchElapsedTime = GetWorld()->GetTimeSeconds() - MatchStartServerTime;
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
	GetWorldTimerManager().ClearTimer(RaceClockTimerHandle);

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
			ATortugaCharacter* Char = *It;
			if (IsValid(Char) && !Char->IsActorBeingDestroyed())
			{
				Char->Destroy();
			}
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
			const FName SavedSkin   = TNPS->EquippedSkinId;

			TNPS->bIsAlive = true;
			TNPS->bHasFinishedRun = false;
			TNPS->bIsDBNO = false;
			TNPS->DBNOBleedoutTimeRemaining = -1.f;
			TNPS->FinishRank = 0;
			TNPS->bIsEliminated = false;
			TNPS->FinishTimeSeconds = -1.f;
			TNPS->DeathZoneTimeRemaining = -1.f;
			TNPS->RaceScore = 0;

			// Asegurar que tiene pawn
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

void ATN_RunGameMode::HandleCollectionZoneGoal(ATN_CollectionZone* Zone)
{
	if (!HasAuthority() || !Zone) { return; }

	const int32 Bonus = Zone->GoalReachedBonusScore;
	if (Bonus <= 0) { return; }

	// Bonus a TODOS los jugadores activos (no eliminados, no terminados aún).
	// Cooperativo: completar la zona beneficia al equipo entero.
	int32 Beneficiaries = 0;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC) { continue; }
		ATN_CoopPlayerState* TNPS = PC->GetPlayerState<ATN_CoopPlayerState>();
		if (!TNPS || TNPS->bIsEliminated || TNPS->bHasFinishedRun) { continue; }
		TNPS->RaceScore += Bonus;
		++Beneficiaries;
	}

	UE_LOG(LogTemp, Log, TEXT("[COLLECTION] GameMode reaccionó a zone '%s' goal · +%d a %d players activos"),
		*Zone->GetName(), Bonus, Beneficiaries);
}

