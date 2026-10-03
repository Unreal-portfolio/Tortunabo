#include "Game/TN_SurvivalGameMode.h"
#include "Core/TN_Log.h"
#include "Core/TN_CoopGameState.h"
#include "Core/TN_CoopPlayerState.h"
#include "Core/TN_GameModeSpawnUtils.h"
#include "World/TN_ChunkManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "UObject/ConstructorHelpers.h"
#include "EngineUtils.h"
#include "TimerManager.h"

namespace
{
	/** Distancia a la meta cuando no se puede medir (sin pawn o sin manager): la peor posible. */
	constexpr float UnknownRemaining = 1.e9f;
}

ATN_SurvivalGameMode::ATN_SurvivalGameMode()
{
	bAllowRevive = false;

	// Los mismos Blueprints que BP_RunGameMode (como ATN_ProcMapGameMode): la clase C++ sirve tal cual como
	// ?game=Survival sin un BP propio.
	static ConstructorHelpers::FClassFinder<APawn> TurtleBP(TEXT("/Game/Blueprints/Characters/BP_TortugaCharacter"));
	if (TurtleBP.Succeeded())
	{
		DefaultPawnClass = TurtleBP.Class;
	}
	static ConstructorHelpers::FClassFinder<APlayerController> ControllerBP(TEXT("/Game/Blueprints/Gameplay/Controllers/BP_GamePlayerController"));
	if (ControllerBP.Succeeded())
	{
		PlayerControllerClass = ControllerBP.Class;
	}
}

void ATN_SurvivalGameMode::StartPlay()
{
	// StartPlay va antes del BeginPlay de los actores del nivel: así el manager genera el nivel 1 y no el buffer del Clásico.
	if (ATN_ChunkManager* Manager = FindChunkManager())
	{
		Manager->SetLevelMode(true);
	}
	else
	{
		UE_LOG(LogTortunabo, Error, TEXT("[Survival] No hay ATN_ChunkManager en el mapa: Supervivencia necesita LVL_Run."));
	}

	Super::StartPlay();
}

ATN_ChunkManager* ATN_SurvivalGameMode::FindChunkManager() const
{
	for (TActorIterator<ATN_ChunkManager> It(GetWorld()); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

void ATN_SurvivalGameMode::OnWaitingTimeout()
{
	const bool bWasStarted = bMatchStarted;
	Super::OnWaitingTimeout();

	if (!bWasStarted && bMatchStarted)
	{
		StartingPlayers = FMath::Max(1, TN_CountConnectedCoopPlayers(GameState));
		UE_LOG(LogTortunabo, Log, TEXT("[Survival] Empieza con %d jugador(es)%s."),
			StartingPlayers, StartingPlayers == 1 ? TEXT(" (solitario: hasta que muera)") : TEXT(""));
	}
}

void ATN_SurvivalGameMode::Logout(AController* Exiting)
{
	// Irse en la espera no cuenta: quien vuelve antes de empezar juega la partida entera.
	if (const APlayerState* ExitingPS = Exiting && bMatchStarted ? Exiting->PlayerState : nullptr)
	{
		LeftPlayerIds.Add(ExitingPS->GetPlayerId());
		FinishedPawns.Remove(ExitingPS->GetPlayerId());
	}

	// La base vuelve a evaluar el nivel (UpdateRoundProgressAndMaybeFinish) sin el que se va.
	Super::Logout(Exiting);
}

void ATN_SurvivalGameMode::MarkPlayerFinished(APlayerController* PlayerController)
{
	if (bMatchOver)
	{
		return;
	}

	ATN_CoopPlayerState* TNPS = PlayerController ? PlayerController->GetPlayerState<ATN_CoopPlayerState>() : nullptr;
	APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	if (TNPS && Pawn && TNPS->bIsAlive && !TNPS->bHasFinishedRun)
	{
		// Antes de Super: al llegar pasa a espectador y deja de poseer el pawn, que se reutiliza en el siguiente nivel.
		FinishedPawns.Add(TNPS->GetPlayerId(), Pawn);
	}

	Super::MarkPlayerFinished(PlayerController);
}

void ATN_SurvivalGameMode::MarkPlayerDead(APlayerController* PlayerController)
{
	if (bMatchOver)
	{
		return;
	}

	ATN_CoopPlayerState* TNPS = PlayerController ? PlayerController->GetPlayerState<ATN_CoopPlayerState>() : nullptr;
	if (HasAuthority() && TNPS && TNPS->bIsAlive && !TNPS->bHasFinishedRun)
	{
		// Antes de Super, que ya evalúa el nivel con este muerto y deja de poseer el pawn.
		FDeathRecord Record;
		Record.Level = CurrentLevel;
		Record.Time = GetWorld()->GetTimeSeconds();
		const APawn* Pawn = PlayerController->GetPawn();
		const ATN_ChunkManager* Manager = FindChunkManager();
		Record.Remaining = Pawn && Manager ? Manager->GetRemainingDistance(Pawn->GetActorLocation()) : UnknownRemaining;
		DeathRecords.Add(TNPS->GetPlayerId(), Record);

		UE_LOG(LogTortunabo, Log, TEXT("[Survival] '%s' cae en el nivel %d a %.0f uu de la meta."),
			*GetNameSafe(PlayerController), Record.Level, Record.Remaining);
	}

	Super::MarkPlayerDead(PlayerController);

	// El tótem le salvó: sigue viva y el apunte no vale.
	if (TNPS && TNPS->bIsAlive)
	{
		DeathRecords.Remove(TNPS->GetPlayerId());
	}
}

TArray<FTNSurvivalPlayer> ATN_SurvivalGameMode::GatherPlayers() const
{
	TArray<FTNSurvivalPlayer> Players;
	if (!GameState)
	{
		return Players;
	}

	for (APlayerState* BasePS : GameState->PlayerArray)
	{
		const ATN_CoopPlayerState* TNPS = Cast<ATN_CoopPlayerState>(BasePS);
		// Fuera quien se fue y quien entró con la partida en marcha (SitOutPlayerIds): ni sigue ni gana.
		if (!TNPS || LeftPlayerIds.Contains(TNPS->GetPlayerId()) || SitOutPlayerIds.Contains(TNPS->GetPlayerId()))
		{
			continue;
		}

		FTNSurvivalPlayer& P = Players.AddDefaulted_GetRef();
		P.Id = TNPS->GetPlayerId();
		P.bAlive = TNPS->bIsAlive;
		P.bFinishedLevel = TNPS->bIsAlive && TNPS->bHasFinishedRun;
		if (!P.bAlive)
		{
			const FDeathRecord* Record = DeathRecords.Find(P.Id);
			P.LevelDied = Record ? Record->Level : CurrentLevel;
			P.DeathTime = Record ? Record->Time : 0.f;
			P.DeathRemaining = Record ? Record->Remaining : UnknownRemaining;
		}
	}
	return Players;
}

void ATN_SurvivalGameMode::UpdateRoundProgressAndMaybeFinish()
{
	if (bMatchOver || !bMatchStarted || GetWorldTimerManager().IsTimerActive(LevelTransitionTimerHandle))
	{
		return;
	}

	const TArray<FTNSurvivalPlayer> Players = GatherPlayers();

	if (ATN_CoopGameState* TNGS = GetGameState<ATN_CoopGameState>())
	{
		int32 Resolved = 0;
		for (const FTNSurvivalPlayer& P : Players)
		{
			Resolved += (!P.bAlive || P.bFinishedLevel) ? 1 : 0;
		}
		TNGS->FinishedPlayers = Resolved;
		TNGS->ExpectedPlayers = Players.Num();
		TNGS->ServerMatchElapsedTime = GetWorld()->GetTimeSeconds() - MatchStartServerTime;
	}

	const FTNSurvivalDecision Decision = TNSurvivalLogic::DecideLevelOutcome(Players, StartingPlayers);
	switch (Decision.Outcome)
	{
	case ETNSurvivalOutcome::Advance:
		UE_LOG(LogTortunabo, Log, TEXT("[Survival] Nivel %d superado. El siguiente sale en %.1f s."), CurrentLevel, LevelTransitionSeconds);
		GetWorldTimerManager().SetTimer(LevelTransitionTimerHandle, this, &ATN_SurvivalGameMode::AdvanceLevel, LevelTransitionSeconds, false);
		break;
	case ETNSurvivalOutcome::Winner:
	case ETNSurvivalOutcome::SoloOver:
		FinishSurvival(Decision.WinnerId);
		break;
	default:
		break;
	}
}

void ATN_SurvivalGameMode::AdvanceLevel()
{
	if (bMatchOver)
	{
		return;
	}

	ATN_ChunkManager* Manager = FindChunkManager();
	if (!Manager)
	{
		UE_LOG(LogTortunabo, Error, TEXT("[Survival] Sin ATN_ChunkManager no hay siguiente nivel: fin de partida."));
		FinishSurvival(INDEX_NONE);
		return;
	}

	++CurrentLevel;
	NextFinishRank = 1;

	// Los cuerpos del nivel anterior se quedarían cayendo al desaparecer sus chunks.
	for (const TPair<int32, TWeakObjectPtr<APawn>>& Pair : DeadPlayerPawns)
	{
		if (Pair.Value.IsValid())
		{
			Pair.Value->Destroy();
		}
	}
	DeadPlayerPawns.Reset();

	Manager->BuildLevel(CurrentLevel);

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		ATN_CoopPlayerState* TNPS = PC ? PC->GetPlayerState<ATN_CoopPlayerState>() : nullptr;
		if (!TNPS || !TNPS->bIsAlive || LeftPlayerIds.Contains(TNPS->GetPlayerId()))
		{
			continue;
		}

		TNPS->bHasFinishedRun = false;
		TNPS->FinishRank = 0;
		TNPS->DeathZoneTimeRemaining = -1.f;
		TNPS->ForceNetUpdate();

		const AActor* Start = FindPlayerStart(PC);
		const FVector StartLocation = Start ? Start->GetActorLocation() : Manager->GetActorLocation() + FVector(0.f, 0.f, 100.f);
		const FRotator StartRotation(0.f, Start ? Start->GetActorRotation().Yaw : Manager->GetActorRotation().Yaw, 0.f);

		APawn* Pawn = FinishedPawns.FindRef(TNPS->GetPlayerId()).Get();
		if (Pawn)
		{
			// Mismo camino que una reanimación: visible, con colisión, poseído y con el input restaurado.
			RestorePossessionAfterRevive(PC, Pawn, StartLocation, true);
			Pawn->SetActorRotation(StartRotation);
			PC->ClientSetRotation(StartRotation);
		}
		else
		{
			if (PC->PlayerState)
			{
				PC->PlayerState->SetIsOnlyASpectator(false);
			}
			RestartPlayer(PC);
		}
	}
	FinishedPawns.Reset();

	UE_LOG(LogTortunabo, Log, TEXT("[Survival] ═══ Nivel %d ═══"), CurrentLevel);
	UpdateRoundProgressAndMaybeFinish();
}

void ATN_SurvivalGameMode::FinishSurvival(int32 WinnerId)
{
	if (bMatchOver)
	{
		return;
	}
	bMatchOver = true;
	GetWorldTimerManager().ClearTimer(LevelTransitionTimerHandle);

	const TArray<FTNSurvivalPlayer> Players = GatherPlayers();
	const TArray<int32> Ranked = TNSurvivalLogic::RankPlayers(Players, WinnerId);
	const float Now = GetWorld()->GetTimeSeconds() - MatchStartServerTime;

	ATN_CoopGameState* TNGS = GetGameState<ATN_CoopGameState>();
	for (int32 Index = 0; Index < Ranked.Num(); ++Index)
	{
		for (APlayerState* BasePS : GameState->PlayerArray)
		{
			ATN_CoopPlayerState* TNPS = Cast<ATN_CoopPlayerState>(BasePS);
			if (!TNPS || TNPS->GetPlayerId() != Ranked[Index])
			{
				continue;
			}

			TNPS->FinishRank = Index + 1;
			TNPS->ForceNetUpdate();
			const float Time = TNPS->bIsAlive ? Now : TNPS->FinishTimeSeconds;
			if (TNGS)
			{
				// Todos con puesto (sin «eliminado»): el marcador se ordena por FinishRank.
				TNGS->Server_UpsertRaceResult(TNPS->GetPlayerId(), TNPS->GetPlayerName(), Index + 1, Time, TNPS->RaceScore, false);
			}
			break;
		}
	}

	UE_LOG(LogTortunabo, Log, TEXT("[Survival] ═══ Fin de partida en el nivel %d · %s PlayerId=%d ═══"),
		CurrentLevel, StartingPlayers <= 1 ? TEXT("solitario,") : TEXT("gana"), WinnerId);

	StartResults();
}
