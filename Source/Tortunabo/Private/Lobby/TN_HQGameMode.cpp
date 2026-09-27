#include "Lobby/TN_HQGameMode.h"
#include "Core/TN_Log.h"
#include "Core/TN_CoopGameState.h"
#include "Core/TN_CoopPlayerState.h"
#include "Core/TN_GameModeSpawnUtils.h"
#include "Player/TortugaCharacter.h"
#include "Player/MP_GamePlayerController.h"
#include "Multiplayer/MP_GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"
#include "Lobby/TN_ChangingBooth.h"
#include "Lobby/TN_GeneralBriefing.h"
#include "Lobby/TN_LobbyReadyZone.h"
#include "Lobby/TN_SandCastleLobby.h"
#include "Lobby/TN_ShopKeeper.h"
#include "Animation/SkeletalMeshActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkinnedAsset.h"

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
	SpawnLobbyShops();

	// ── Tutorial first-time check (fallback para join directo sin seamless) ──
	// Para seamless travel, el flag se evalúa en HandleSeamlessTravelPlayer
	// ANTES de que Super spawne el pawn.  Este bloque cubre el caso en que el
	// jugador accede directamente al mapa sin pasar por LVL_Menu.
	CheckAndSetTutorialFlag();

	// ── Safety check: detectar si el mapa cargó con la clase C++ base en vez del BP ──
	if (GetClass() == ATN_HQGameMode::StaticClass())
	{
		UE_LOG(LogTortunabo, Error, TEXT("[HQGameMode] ════════════════════════════════════════════════════════"));
		UE_LOG(LogTortunabo, Error, TEXT("[HQGameMode] ¡USANDO CLASE C++ BASE! No hay BP GameMode."));
		UE_LOG(LogTortunabo, Error, TEXT("[HQGameMode] FIX: En LVL_HQ → WorldSettings → GameMode Override"));
		UE_LOG(LogTortunabo, Error, TEXT("[HQGameMode]       → seleccionar BP_HQGameMode."));
		UE_LOG(LogTortunabo, Error, TEXT("[HQGameMode] ════════════════════════════════════════════════════════"));
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
				UE_LOG(LogTortunabo, Log, TEXT("[HQGameMode] ChoosePlayerStart → tutorial start encontrado."));
				return *It;
			}
		}
		UE_LOG(LogTortunabo, Warning,
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

	if (AActor* Start = TN_PickUnoccupiedPlayerStart(GetWorld(), PlayerStarts, Player))
	{
		return Start;
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
	TN_EnsurePlayerSpawned(this, PlayerController, [this]() { return EnsureFallbackPlayerStart(); }, TEXT("Lobby"));
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
			UE_LOG(LogTortunabo, Log, TEXT("[HQGameMode] Primera partida detectada — spawn en zona tutorial (tag: '%s')."), *TutorialStartTag.ToString());
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
	if (ConnectedPlayers >= LobbyMinPlayersForStart && ReadyPlayers >= ConnectedPlayers)
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

	UE_LOG(LogTortunabo, Log, TEXT("[HQGameMode] Countdown iniciado: %d segundos."), CurrentCountdownValue);

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
		UE_LOG(LogTortunabo, Error, TEXT("[HQGameMode] BeginMatchTravel: GetWorld() es null, travel cancelado."));
		return;
	}

	// ── Guardar cuántos jugadores hay en el lobby ANTES de viajar ──────
	// GameInstance sobrevive al seamless travel; TN_RunGameMode
	// lo leerá en BeginPlay para saber cuántos jugadores esperar.
	FString TravelURL = MatchMapPath;
	if (UMP_GameInstance* GI = Cast<UMP_GameInstance>(GetGameInstance()))
	{
		const int32 ConnectedCount = TN_CountConnectedCoopPlayers(GameState);
		GI->PendingTravelPlayerCount = ConnectedCount;
		UE_LOG(LogTortunabo, Log, TEXT("[HQGameMode] Saved PendingTravelPlayerCount = %d"), ConnectedCount);

		// ── Modo elegido en el lobby: Clásico → LVL_Run; el resto → mapa procedural ──
		if (GI->SelectedProcMode == ETNProcGameMode::TwoVsTwo && ConnectedCount != 4)
		{
			// El selector ya lo impide, pero alguien pudo salir durante la cuenta atrás.
			UE_LOG(LogTortunabo, Warning, TEXT("[HQGameMode] 2vs2 exige 4 jugadores (hay %d) → Carrera."), ConnectedCount);
			GI->SelectedProcMode = ETNProcGameMode::Race;
		}
		// ── Cómo se pusieron listos (sala de la puerta doble o huevos): así se sale en el mapa procedural ──
		// Antes de destruir los peones; sin castillo (maqueta vieja), la puerta doble.
		GI->PendingStartStyle = ETNMatchStartStyle::Gate;
		for (TActorIterator<ATN_SandCastleLobby> It(World); It; ++It)
		{
			GI->PendingStartStyle = It->GetStartStyle();
			break;
		}
		if (GI->SelectedProcMode != ETNProcGameMode::Classic)
		{
			// También en la URL: la lee ATN_ProcMapGameMode y sustituye a la del viaje anterior.
			TravelURL = ProcMapPath + (GI->PendingStartStyle == ETNMatchStartStyle::Eggs ? TEXT("?ProcStart=Eggs") : TEXT("?ProcStart=Gate"));
		}
		UE_LOG(LogTortunabo, Log, TEXT("[HQGameMode] Modo %s · dificultad %s · salida %s"),
			*UEnum::GetValueAsString(GI->SelectedProcMode), *UEnum::GetValueAsString(GI->SelectedProcDifficulty),
			GI->PendingStartStyle == ETNMatchStartStyle::Eggs ? TEXT("huevos") : TEXT("puerta doble"));
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
	UE_LOG(LogTortunabo, Log, TEXT("[HQGameMode] Seamless ServerTravel to: %s"), *TravelURL);
	World->ServerTravel(TravelURL);
}

APlayerStart* ATN_HQGameMode::EnsureFallbackPlayerStart()
{
	return TN_EnsureFallbackPlayerStart(GetWorld(), TEXT("LobbyFallbackPlayerStart"), TEXT("Lobby"), TEXT("lobby map"));
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
			UE_LOG(LogTortunabo, Log, TEXT("[HQGameMode] HandleSeamlessTravelPlayer: destruyendo pawn prematuro '%s' de %s"),
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

	UE_LOG(LogTortunabo, Log, TEXT("[HQGameMode] PostSeamlessTravel: resetting all players for lobby."));

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

			// HQ-WARN-01: salvaguarda contra "Attempting to move a fully simulated
			// skeletal mesh". Si el pawn recién spawneado llega con el SkM simulando
			// (edge case: OnRep_IsDead llegó antes de BeginPlay, ragdoll sobrevive
			// un tick, etc.), el MulticastForceApplyHelmet/Skin que sigue puede
			// mover el mesh y disparar el warning. Reset defensivo aquí.
			if (APawn* FreshPawn = PC->GetPawn())
			{
				if (ACharacter* Ch = Cast<ACharacter>(FreshPawn))
				{
					if (USkeletalMeshComponent* SKM = Ch->GetMesh())
					{
						if (SKM->IsSimulatingPhysics())
						{
							SKM->SetSimulatePhysics(false);
							UE_LOG(LogTortunabo, Warning,
								TEXT("[HQGameMode] Reset stale physics sim on pawn %s post-travel"),
								*GetNameSafe(FreshPawn));
						}
					}
				}
			}

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
		TNGS->ConnectedPlayers = TN_CountConnectedCoopPlayers(GameState);
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

void ATN_HQGameMode::SpawnLobbyShops()
{
	UWorld* World = GetWorld();
	if (!World) { return; }
	static const FName ShopTag(TEXT("TN_ShopAnchor"));
	static const FName BoothTag(TEXT("TN_BoothAnchor"));
	static const FName GeneralTag(TEXT("TN_GeneralAnchor"));
	/** Donde está el general de la maqueta de LVL_Lobby (TotugaDemo_Rig2). */
	const FVector BlockoutGeneralSpot(-892.0, 1479.0, 0.0);

	bool bHasShop = false;
	bool bHasBooth = false;
	bool bHasGeneral = false;
	TArray<AActor*> ShopAnchors;
	TArray<AActor*> BoothAnchors;
	TArray<AActor*> GeneralAnchors;
	AActor* BlockoutGeneral = nullptr;
	TArray<AActor*> BlockoutKeepers;
	TArray<AActor*> BlockoutBottles;
	TArray<AActor*> BlockoutDoors;
	AActor* Tent = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor) { continue; }
		bHasShop |= Actor->IsA<ATN_ShopKeeper>();
		bHasBooth |= Actor->IsA<ATN_ChangingBooth>();
		bHasGeneral |= Actor->IsA<ATN_GeneralBriefing>();
		if (Actor->ActorHasTag(ShopTag)) { ShopAnchors.Add(Actor); }
		if (Actor->ActorHasTag(BoothTag)) { BoothAnchors.Add(Actor); }
		if (Actor->ActorHasTag(GeneralTag)) { GeneralAnchors.Add(Actor); }
		const FString ClassName = Actor->GetClass()->GetName();
		if (ClassName.Contains(TEXT("VestidorBotella"))) { BlockoutBottles.Add(Actor); }
		else if (ClassName.Contains(TEXT("ShellDoor"))) { BlockoutDoors.Add(Actor); }
		else if (ClassName.Contains(TEXT("ChangingTent"))) { Tent = Actor; }
		if (const ASkeletalMeshActor* SkelActor = Cast<ASkeletalMeshActor>(Actor))
		{
			const USkinnedAsset* Asset = SkelActor->GetSkeletalMeshComponent() ? SkelActor->GetSkeletalMeshComponent()->GetSkinnedAsset() : nullptr;
			if (Asset && Asset->GetName().Contains(TEXT("TotugaDemo")) && Actor->GetActorScale3D().Z >= 3.2f) { BlockoutKeepers.Add(Actor); }
			// El general de la maqueta: la tortuga suelta más cerca de su sitio (sea cual sea su escala).
			if (Asset && Asset->GetName().Contains(TEXT("TotugaDemo")) && FVector::Dist2D(Actor->GetActorLocation(), BlockoutGeneralSpot) < 500.0
				&& (!BlockoutGeneral || FVector::Dist2D(Actor->GetActorLocation(), BlockoutGeneralSpot) < FVector::Dist2D(BlockoutGeneral->GetActorLocation(), BlockoutGeneralSpot)))
			{
				BlockoutGeneral = Actor;
			}
		}
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// El lobby como castillo de arena (P4): solo sobre la maqueta de LVL_Lobby (vallas, torres o paredes «Extrude»
	// de la zona de salida) y si no se ha apagado con TN.Lobby.Castle 0. Se coloca en el origen, a ras del suelo.
	{
		bool bHasCastle = false;
		bool bLooksLikeMaquette = false;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			const AActor* Actor = *It;
			if (!Actor) { continue; }
			bHasCastle |= Actor->IsA<ATN_SandCastleLobby>();
			const FString ClassName = Actor->GetClass()->GetName();
			bLooksLikeMaquette |= Actor->IsA<ATN_LobbyReadyZone>() || ClassName.Contains(TEXT("BP_Fence")) || ClassName.Contains(TEXT("BP_Tower"));
		}
		if (!bHasCastle && bLooksLikeMaquette && ATN_SandCastleLobby::IsEnabled())
		{
			FCollisionQueryParams Query(SCENE_QUERY_STAT(TN_CastleGround), false);
			FHitResult Hit;
			double GroundZ = 0.0;
			if (World->LineTraceSingleByChannel(Hit, FVector(0.0, 0.0, 2000.0), FVector(0.0, 0.0, -3000.0), ECC_WorldStatic, Query))
			{
				GroundZ = Hit.ImpactPoint.Z;
			}
			World->SpawnActor<ATN_SandCastleLobby>(ATN_SandCastleLobby::StaticClass(), FVector(0.0, 0.0, GroundZ), FRotator::ZeroRotator, Params);
			UE_LOG(LogTortunabo, Log, TEXT("[HQGameMode] Castillo de arena colocado (suelo a %.0f)."), GroundZ);
		}
	}

	// Suelo bajo el ancla: traza hacia abajo sin las piezas de la maqueta; si no hay, el fondo de su caja.
	auto GroundOf = [World, &BlockoutKeepers, &BlockoutBottles, &BlockoutDoors, BlockoutGeneral](const AActor* Actor) -> FVector
	{
		FVector Origin, Extent;
		Actor->GetActorBounds(false, Origin, Extent);
		const FVector Top(Actor->GetActorLocation().X, Actor->GetActorLocation().Y, Origin.Z + Extent.Z + 50.0);
		FCollisionQueryParams Query(SCENE_QUERY_STAT(TN_LobbyShopGround), false);
		Query.AddIgnoredActors(BlockoutKeepers);
		Query.AddIgnoredActors(BlockoutBottles);
		Query.AddIgnoredActors(BlockoutDoors);
		if (BlockoutGeneral) { Query.AddIgnoredActor(BlockoutGeneral); }
		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, Top, Top - FVector(0.0, 0.0, Extent.Z * 2.0 + 2000.0), ECC_WorldStatic, Query))
		{
			return Hit.ImpactPoint;
		}
		return FVector(Top.X, Top.Y, Origin.Z - Extent.Z);
	};

	if (!bHasShop)
	{
		// El tendero de la maqueta mira a su +Y (la malla de la tortuga): la tienda mira a su +X.
		AActor* Anchor = ShopAnchors.Num() > 0 ? ShopAnchors[0] : nullptr;
		float Yaw = Anchor ? Anchor->GetActorRotation().Yaw : 0.f;
		if (!Anchor && BlockoutKeepers.Num() > 0)
		{
			BlockoutKeepers.Sort([Tent](const AActor& A, const AActor& B)
			{
				return !Tent || FVector::DistSquared(A.GetActorLocation(), Tent->GetActorLocation()) < FVector::DistSquared(B.GetActorLocation(), Tent->GetActorLocation());
			});
			Anchor = BlockoutKeepers[0];
			Yaw = Anchor->GetActorRotation().Yaw + 90.f;
		}
		if (Anchor)
		{
			const FVector Where = ShopAnchors.Contains(Anchor) ? Anchor->GetActorLocation() : GroundOf(Anchor);
			World->SpawnActor<ATN_ShopKeeper>(ATN_ShopKeeper::StaticClass(), Where, FRotator(0.f, Yaw, 0.f), Params);
			UE_LOG(LogTortunabo, Log, TEXT("[HQGameMode] Tienda colocada en %s (sobre %s)."), *Where.ToString(), *Anchor->GetName());
		}
	}

	if (!bHasBooth)
	{
		const TArray<AActor*>& Spots = BoothAnchors.Num() > 0 ? BoothAnchors : BlockoutBottles;
		// Sin puerta de maqueta al lado, la puerta mira al centro del lobby (el PlayerStart).
		FVector Center = FVector::ZeroVector;
		for (TActorIterator<APlayerStart> It(World); It; ++It)
		{
			if ((*It)->PlayerStartTag != TutorialStartTag) { Center = (*It)->GetActorLocation(); break; }
		}
		for (const AActor* Spot : Spots)
		{
			const bool bTagged = BoothAnchors.Contains(Spot);
			const FVector Where = bTagged ? Spot->GetActorLocation() : GroundOf(Spot);
			float Yaw = bTagged ? Spot->GetActorRotation().Yaw : FMath::RadiansToDegrees(FMath::Atan2(Center.Y - Where.Y, Center.X - Where.X));
			if (!bTagged)
			{
				for (const AActor* DoorActor : BlockoutDoors)
				{
					const FVector ToDoor = DoorActor->GetActorLocation() - Where;
					if (ToDoor.Size2D() < 450.0) { Yaw = FMath::RadiansToDegrees(FMath::Atan2(ToDoor.Y, ToDoor.X)); }
				}
			}
			World->SpawnActor<ATN_ChangingBooth>(ATN_ChangingBooth::StaticClass(), Where, FRotator(0.f, Yaw, 0.f), Params);
		}
		if (Spots.Num() > 0) { UE_LOG(LogTortunabo, Log, TEXT("[HQGameMode] %d probadores colocados."), Spots.Num()); }
	}

	if (!bHasGeneral)
	{
		// Con ancla, donde diga; si no, sobre el general de la maqueta (que se esconde) mirando al centro del lobby, con
		// la mesa delante.
		AActor* Anchor = GeneralAnchors.Num() > 0 ? GeneralAnchors[0] : BlockoutGeneral;
		if (Anchor)
		{
			const bool bTagged = GeneralAnchors.Contains(Anchor);
			const FVector Where = bTagged ? Anchor->GetActorLocation() : GroundOf(Anchor);
			FVector Center = FVector::ZeroVector;
			for (TActorIterator<APlayerStart> It(World); It; ++It)
			{
				if ((*It)->PlayerStartTag != TutorialStartTag) { Center = (*It)->GetActorLocation(); break; }
			}
			const float Yaw = bTagged ? Anchor->GetActorRotation().Yaw : FMath::RadiansToDegrees(FMath::Atan2(Center.Y - Where.Y, Center.X - Where.X));
			World->SpawnActor<ATN_GeneralBriefing>(ATN_GeneralBriefing::StaticClass(), Where, FRotator(0.f, Yaw, 0.f), Params);
			UE_LOG(LogTortunabo, Log, TEXT("[HQGameMode] General colocado en %s (sobre %s)."), *Where.ToString(), *Anchor->GetName());
		}
	}
}
