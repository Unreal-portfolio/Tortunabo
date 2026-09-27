#include "Game/TN_BeachRaceGameMode.h"
#include "Game/TN_BeachRaceGameState.h"
#include "Core/TN_Log.h"
#include "Core/TN_CoopGameState.h"
#include "Core/TN_CoopPlayerState.h"
#include "Multiplayer/MP_GameInstance.h"
#include "Player/MP_GamePlayerController.h"
#include "Player/TortugaCharacter.h"
#include "Player/TN_CarryComponent.h"
#include "Player/TN_ShellComponent.h"
#include "World/Beach/TN_BeachRaceGenerator.h"
#include "World/Beach/TN_BeachStun.h"
#include "World/Beach/TN_BeachStunComponent.h"
#include "World/Beach/TN_BeachTypes.h"
#include "World/TN_DeathZoneVolume.h"
#include "World/TN_StormVolume.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/DateTime.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace TNBeachRaceGameModeDetail
{
	/**
	 * CountdownValue durante la pantalla del campeón (no hay cuenta: se espera al anfitrión). Por encima de 1 para que
	 * la música de fin de partida no se funda (el director funde en el último segundo) ni se cierre el huevo de la
	 * pantalla de carga (se cierra con CountdownValue == 1 en Results). Al elegir se pone a 1 y ahí sí se funden.
	 */
	constexpr int32 ChampionHoldCountdown = 99;

	/** Velocidad media de la carrera (cm/s) para estimar los minutos de la ronda (Docs/Modo_Carrera.md: ~4 m/s). */
	constexpr double AverageRaceSpeed = 400.0;

	/** Cada cuánto se mira la meta, el vacío y los sitios seguros durante la carrera. */
	constexpr float WatchInterval = 0.1f;

	/** Sitios seguros que se recuerdan por tortuga (con SafeSpotSampleSeconds = 0,5: los últimos 6 s). */
	constexpr int32 MaxSafeSpots = 12;

	ATN_BeachRaceGameMode* FindGameMode(const UWorld* World)
	{
		return World ? World->GetAuthGameMode<ATN_BeachRaceGameMode>() : nullptr;
	}

	int32 IntArg(const TArray<FString>& Args, int32 Index, int32 Default)
	{
		return Args.IsValidIndex(Index) ? FCString::Atoi(*Args[Index]) : Default;
	}

	float FloatArg(const TArray<FString>& Args, int32 Index, float Default)
	{
		return Args.IsValidIndex(Index) ? FCString::Atof(*Args[Index]) : Default;
	}

	/** Consola: corre Action con el GameMode de la playa del anfitrión o avisa de que aquí no hay carrera. */
	void WithGameMode(const UWorld* World, const TCHAR* Command, TFunctionRef<void(ATN_BeachRaceGameMode&)> Action)
	{
		if (ATN_BeachRaceGameMode* GM = FindGameMode(World))
		{
			Action(*GM);
			return;
		}
		UE_LOG(LogTortunabo, Warning, TEXT("[Carrera] %s: solo en el anfitrión y en la playa (LVL_BeachRace)."), Command);
	}

	FAutoConsoleCommandWithWorldAndArgs CmdWinRound(TEXT("TN.Race.WinRound"),
		TEXT("Carrera en la playa: el jugador N (0 = el primero, normalmente el anfitrión) gana la ronda en curso como si tocara el agua."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			WithGameMode(World, TEXT("TN.Race.WinRound"), [&Args](ATN_BeachRaceGameMode& GM) { GM.DebugWinRound(IntArg(Args, 0, 0)); });
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdChampion(TEXT("TN.Race.Champion"),
		TEXT("Carrera en la playa: el jugador N llega a las conchas del campeón y se pasa directamente a su pantalla."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			WithGameMode(World, TEXT("TN.Race.Champion"), [&Args](ATN_BeachRaceGameMode& GM) { GM.DebugChampion(IntArg(Args, 0, 0)); });
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdStun(TEXT("TN.Race.Stun"),
		TEXT("Carrera en la playa: aturde al jugador. TN.Race.Stun [segundos = 3] [jugador = 0]."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			WithGameMode(World, TEXT("TN.Race.Stun"), [&Args](ATN_BeachRaceGameMode& GM) { GM.DebugStun(IntArg(Args, 1, 0), FloatArg(Args, 0, 3.f)); });
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdKill(TEXT("TN.Race.Kill"),
		TEXT("Carrera en la playa: pasa al jugador N por la ruta de muerte (aquí aturde; en una zona de muerte, vuelve a un sitio seguro)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			WithGameMode(World, TEXT("TN.Race.Kill"), [&Args](ATN_BeachRaceGameMode& GM) { GM.DebugKill(IntArg(Args, 0, 0)); });
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdVoid(TEXT("TN.Race.Void"),
		TEXT("Carrera en la playa: tira al jugador N al vacío (vuelve a su último sitio seguro, aturdido)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			WithGameMode(World, TEXT("TN.Race.Void"), [&Args](ATN_BeachRaceGameMode& GM) { GM.DebugVoid(IntArg(Args, 0, 0)); });
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdPlayAgain(TEXT("TN.Race.PlayAgain"),
		TEXT("Carrera en la playa, pantalla del campeón: Volver a jugar."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ATN_BeachRaceGameMode::RequestChampionChoice(World, ETNBeachChampionChoice::PlayAgain);
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdChangeMode(TEXT("TN.Race.ChangeMode"),
		TEXT("Carrera en la playa, pantalla del campeón: Cambiar de modo (vuelve al lobby con el cooperativo elegido)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ATN_BeachRaceGameMode::RequestChampionChoice(World, ETNBeachChampionChoice::ChangeMode);
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdMenu(TEXT("TN.Race.Menu"),
		TEXT("Carrera en la playa, pantalla del campeón: Salir al menú principal."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ATN_BeachRaceGameMode::RequestChampionChoice(World, ETNBeachChampionChoice::Quit);
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdMode(TEXT("TN.Mode"),
		TEXT("Modo de la próxima partida que salga del lobby (en el anfitrión): TN.Mode Coop|Race. Sin argumento, dice el actual."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UMP_GameInstance* GI = World ? Cast<UMP_GameInstance>(World->GetGameInstance()) : nullptr;
			if (!GI)
			{
				return;
			}
			if (Args.Num() > 0)
			{
				if (Args[0].Equals(TEXT("Coop"), ESearchCase::IgnoreCase) || Args[0].Equals(TEXT("Cooperativo"), ESearchCase::IgnoreCase))
				{
					GI->SelectedProcMode = ETNProcGameMode::Coop;
				}
				else if (Args[0].Equals(TEXT("Race"), ESearchCase::IgnoreCase) || Args[0].Equals(TEXT("Carrera"), ESearchCase::IgnoreCase))
				{
					GI->SelectedProcMode = ETNProcGameMode::Race;
				}
			}
			UE_LOG(LogTortunabo, Log, TEXT("[Modo] Próxima partida: %s"), *UEnum::GetValueAsString(GI->SelectedProcMode));
		}));
}

ATN_BeachRaceGameMode::ATN_BeachRaceGameMode()
{
	GameStateClass = ATN_BeachRaceGameState::StaticClass();
	GeneratorClass = ATN_BeachRaceGenerator::StaticClass();

	// Los mismos Blueprints que el mapa procedural: la clase C++ sirve tal cual como GameMode de LVL_BeachRace.
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
	// Sin RescuePickupClass: en la playa nadie muere, así que no hay rescates.
}

// ─────────────────────────────────────────────────────────────────────────────
// Arranque
// ─────────────────────────────────────────────────────────────────────────────

void ATN_BeachRaceGameMode::BeginPlay()
{
	ResolveUrlOptions();
	EnsureGenerator();

	// La primera ronda se reparte antes de que la base cree a nadie: así la salida ya tiene sus sitios cuando los
	// jugadores llegan del lobby. La cuenta atrás de esta ronda es el huevo de la pantalla de carga.
	CurrentRound = 1;
	bShowPreRaceCountdown = false;
	PrepareRound(false);

	Super::BeginPlay();

	UE_LOG(LogTortunabo, Log, TEXT("[Carrera] Playa · gana quien llegue a %d conchas · generador %s"), WinsToWinMatch, *GetNameSafe(Generator));
	SyncGameState();
}

void ATN_BeachRaceGameMode::ResolveUrlOptions()
{
	// Para probar sin lobby: open LVL_BeachRace?BeachSeed=42?BeachWins=1
	const FString SeedOption = UGameplayStatics::ParseOption(OptionsString, TEXT("BeachSeed"));
	UrlSeed = SeedOption.IsEmpty() ? 0 : FCString::Atoi(*SeedOption);
	const FString WinsOption = UGameplayStatics::ParseOption(OptionsString, TEXT("BeachWins"));
	if (!WinsOption.IsEmpty())
	{
		WinsToWinMatch = FMath::Max(1, FCString::Atoi(*WinsOption));
	}
}

void ATN_BeachRaceGameMode::EnsureGenerator()
{
	if (!Generator)
	{
		for (TActorIterator<ATN_BeachRaceGenerator> It(GetWorld()); It; ++It)
		{
			Generator = *It;
			break;
		}
	}
	if (!Generator)
	{
		UClass* Class = GeneratorClass ? GeneratorClass.Get() : ATN_BeachRaceGenerator::StaticClass();
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Generator = GetWorld()->SpawnActor<ATN_BeachRaceGenerator>(Class, FTransform::Identity, Params);
		UE_LOG(LogTortunabo, Warning, TEXT("[Carrera] El nivel no tenía generador de la playa: creado %s."), *GetNameSafe(Generator));
	}
}

void ATN_BeachRaceGameMode::OnWaitingTimeout()
{
	// La base avisa cuando han llegado todos del lobby (o venció la espera). La carrera arranca cuando además la ronda
	// está repartida y las tortugas colocadas en la salida.
	if (bMatchStarted)
	{
		return;
	}
	bPlayersArrived = true;
	GetWorldTimerManager().ClearTimer(WaitingTimeoutTimerHandle);
	PollRoundReady();
}

void ATN_BeachRaceGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	if (!NewPlayer)
	{
		return;
	}
	if (bRoundActive)
	{
		// Llega con la ronda en marcha (reconexión): sale desde la salida, como todas.
		if (ATortugaCharacter* Turtle = Cast<ATortugaCharacter>(NewPlayer->GetPawn()))
		{
			const float HalfHeight = Turtle->GetCapsuleComponent() ? Turtle->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : GetDefaultHalfHeight();
			const FTransform Start = PutOnFloor(GetStartTransformFor(GetStartSlot(NewPlayer)), HalfHeight);
			TeleportTurtle(Turtle, Start);
			NewPlayer->ClientSetRotation(Start.Rotator(), true);
		}
	}
	else
	{
		FreezePlayers();
	}
}

void ATN_BeachRaceGameMode::Logout(AController* Exiting)
{
	if (APlayerController* PC = Cast<APlayerController>(Exiting))
	{
		FrozenControllers.Remove(PC);
		SafeSpots.Remove(PC);
		ReleaseCarry(Cast<ATortugaCharacter>(PC->GetPawn()));
	}
	// La base limpia lo suyo y, con la partida en marcha, mira si la ronda ha acabado. Si se va quien iba primero, la
	// ronda sigue con los demás; el podio y el campeón conservan su PlayerState mientras exista (luego, null).
	Super::Logout(Exiting);
}

// ─────────────────────────────────────────────────────────────────────────────
// Ronda: preparación, cuenta atrás y carrera
// ─────────────────────────────────────────────────────────────────────────────

void ATN_BeachRaceGameMode::PrepareRound(bool bCleanup)
{
	CancelRoundTimers();
	bRoundActive = false;
	if (bCleanup)
	{
		CleanupRoundActors();
	}

	const int32 BaseSeed = UrlSeed != 0 ? UrlSeed : FixedSeed;
	if (BaseSeed != 0)
	{
		RoundSeed = BaseSeed + (CurrentRound - 1);
	}
	else
	{
		const uint64 Ticks = static_cast<uint64>(FDateTime::UtcNow().GetTicks());
		const uint32 Mixed = static_cast<uint32>(Ticks) ^ static_cast<uint32>(Ticks >> 32)
			^ (static_cast<uint32>(FMath::Rand()) << 8)
			^ (static_cast<uint32>(CurrentRound) * 0x9E3779B9u);
		RoundSeed = static_cast<int32>(Mixed & 0x7FFFFFFFu);
	}

	// El terreno es fijo: el generador solo vuelve a repartir decorado, trampas y enemigos.
	if (Generator)
	{
		Generator->GenerateRound(RoundSeed);
	}
	else
	{
		UE_LOG(LogTortunabo, Error, TEXT("[Carrera] Sin generador: la ronda %d se corre sin reparto ni meta."), CurrentRound);
	}

	bPreparingRound = true;
	PrepStartTime = GetWorld()->GetTimeSeconds();
	SetRacePhase(ETNBeachRacePhase::Waiting);
	GetWorldTimerManager().SetTimer(PrepPollHandle, this, &ATN_BeachRaceGameMode::PollRoundReady, 0.25f, true);
	UE_LOG(LogTortunabo, Log, TEXT("[Carrera] Preparando la ronda %d (semilla %d)."), CurrentRound, RoundSeed);
	SyncGameState();
}

void ATN_BeachRaceGameMode::PollRoundReady()
{
	if (!bPreparingRound)
	{
		GetWorldTimerManager().ClearTimer(PrepPollHandle);
		return;
	}

	FreezePlayers();
	if (!bPlayersArrived)
	{
		return;
	}
	const float Waited = GetWorld()->GetTimeSeconds() - PrepStartTime;
	if (Waited < MinPreRoundSeconds)
	{
		return;
	}
	const bool bReady = !Generator || Generator->IsRoundReady();
	if (!bReady && Waited < RoundReadyTimeoutSeconds)
	{
		return;
	}
	if (!bReady)
	{
		UE_LOG(LogTortunabo, Warning, TEXT("[Carrera] El generador no tiene lista la ronda %d tras %.0f s: se corre igual."), CurrentRound, Waited);
	}

	bPreparingRound = false;
	GetWorldTimerManager().ClearTimer(PrepPollHandle);

	if (CurrentRound == 1)
	{
		// Las conchas son de esta partida (el PlayerState viaja desde el lobby o de una carrera anterior).
		for (APlayerState* BasePS : GameState->PlayerArray)
		{
			if (ATN_CoopPlayerState* PS = Cast<ATN_CoopPlayerState>(BasePS))
			{
				PS->RoundWins = 0;
				PS->TeamIndex = -1;
			}
		}
		LastRoundWonByPlayer.Reset();
	}

	PlacePlayersAtStart();

	if (bShowPreRaceCountdown && PreRaceCountdownSeconds > 0.f)
	{
		BeginPhaseClock(PreRaceCountdownSeconds);
		GetWorldTimerManager().SetTimer(PhaseEndHandle, this, &ATN_BeachRaceGameMode::BeginRace, PreRaceCountdownSeconds, false);
	}
	else
	{
		BeginRace();
	}
}

void ATN_BeachRaceGameMode::BeginRace()
{
	StopPhaseClock();
	GetWorldTimerManager().ClearTimer(PhaseEndHandle);
	UnfreezePlayers();

	if (!bMatchStarted)
	{
		// Primera ronda: el arranque de la base (InProgress y cronómetro). Rompe el huevo con «¡ADELANTE!».
		Super::OnWaitingTimeout();
	}
	else
	{
		// Rondas siguientes: sin huevo, el rótulo «¡ADELANTE!» sale solo al volver a InProgress.
		MatchStartServerTime = GetWorld()->GetTimeSeconds();
		SetFlowState(ETNMatchFlowState::InProgress);
	}

	bRoundActive = true;
	SafeSpots.Reset();
	NextSafeSampleTime = 0.f;
	LowestGroundZ = CourseOrigin.Z;
	SetRacePhase(ETNBeachRacePhase::Racing);
	StartStorm();

	GetWorldTimerManager().SetTimer(WatchHandle, this, &ATN_BeachRaceGameMode::WatchRacers, TNBeachRaceGameModeDetail::WatchInterval, true);
	GetWorldTimerManager().ClearTimer(RoundTimeLimitHandle);
	if (RoundTimeLimitSeconds > 0.f)
	{
		GetWorldTimerManager().SetTimer(RoundTimeLimitHandle, this, &ATN_BeachRaceGameMode::OnRoundTimeLimit, RoundTimeLimitSeconds, false);
	}

	UE_LOG(LogTortunabo, Log, TEXT("[Carrera] ═══ Ronda %d en marcha · semilla %d ═══"), CurrentRound, RoundSeed);
	SyncGameState();
}

void ATN_BeachRaceGameMode::WatchRacers()
{
	if (!bRoundActive)
	{
		GetWorldTimerManager().ClearTimer(WatchHandle);
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	const bool bSample = Now >= NextSafeSampleTime;
	if (bSample)
	{
		NextSafeSampleTime = Now + SafeSpotSampleSeconds;
	}

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		const ATN_CoopPlayerState* PS = PC ? PC->GetPlayerState<ATN_CoopPlayerState>() : nullptr;
		if (!PS || PS->bHasFinishedRun || PS->IsOnlyASpectator())
		{
			continue;
		}
		APawn* Pawn = PC->GetPawn();
		if (!Pawn)
		{
			// Sin peón en plena carrera: el motor lo ha destruido por debajo del KillZ del nivel.
			RescueTurtle(PC, TEXT("fuera del mundo"));
			continue;
		}
		const FVector Location = Pawn->GetActorLocation();
		if (Generator && Generator->IsFinishWater(Location))
		{
			MarkPlayerFinished(PC);
			if (!bRoundActive)
			{
				return;
			}
			continue;
		}
		if (Location.Z < GetVoidZ())
		{
			RescueTurtle(PC, TEXT("vacío"));
			continue;
		}
		if (bSample)
		{
			SampleSafeSpot(PC, Cast<ACharacter>(Pawn), Now);
		}
	}
}

void ATN_BeachRaceGameMode::OnRoundTimeLimit()
{
	if (!bRoundActive)
	{
		return;
	}
	// Gana la concha quien esté más cerca del mar.
	APlayerController* Best = nullptr;
	float BestProgress = -TNumericLimits<float>::Max();
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		if (!Pawn)
		{
			continue;
		}
		const float Progress = GetCourseProgress(Pawn);
		if (Progress > BestProgress)
		{
			BestProgress = Progress;
			Best = PC;
		}
	}
	const ATN_CoopPlayerState* BestState = Best ? Best->GetPlayerState<ATN_CoopPlayerState>() : nullptr;
	EndRound(Best, BestState
		? FString::Printf(TEXT("¡Tiempo! Gana %s, la más cerca del mar"), *BestState->GetPlayerName())
		: FString(TEXT("¡Tiempo! Nadie gana la ronda")));
}

// ─────────────────────────────────────────────────────────────────────────────
// Meta y «muerte»
// ─────────────────────────────────────────────────────────────────────────────

void ATN_BeachRaceGameMode::MarkPlayerFinished(APlayerController* PlayerController)
{
	if (!HasAuthority() || !PlayerController || !bRoundActive)
	{
		return;
	}
	ATN_CoopPlayerState* PS = PlayerController->GetPlayerState<ATN_CoopPlayerState>();
	if (!PS || PS->bHasFinishedRun || !PS->bIsAlive)
	{
		return;
	}

	// Al agua: fuera del caparazón y del aturdimiento antes de que la base la oculte y la pase a espectadora.
	if (ATortugaCharacter* Turtle = Cast<ATortugaCharacter>(PlayerController->GetPawn()))
	{
		ReleaseCarry(Turtle);
		if (UTN_BeachStunComponent* Stun = UTN_BeachStunComponent::FindOn(Turtle))
		{
			Stun->EndStun(true);
		}
		if (UTN_ShellComponent* Shell = Turtle->GetShellComponent())
		{
			Shell->SetExitLocked(false);
			Shell->ForceExitShell();
		}
	}

	// La base asigna puesto y puntos (la música de victoria sale de ahí) y pasa a espectadora; la ronda la cierra esto.
	bSuppressRoundCheck = true;
	Super::MarkPlayerFinished(PlayerController);
	bSuppressRoundCheck = false;
	if (PS->bHasFinishedRun)
	{
		EndRound(PlayerController, FString::Printf(TEXT("¡%s gana la ronda!"), *PS->GetPlayerName()));
	}
}

void ATN_BeachRaceGameMode::MarkPlayerDead(APlayerController* PlayerController)
{
	// Todas las rutas de muerte del juego acaban aquí (zonas de muerte, tormenta, caídas largas y enemigos, vía
	// ATortugaCharacter::RequestKill): en la playa nadie muere, así que nunca se llama a la base.
	if (!HasAuthority() || !PlayerController)
	{
		return;
	}
	ATN_CoopPlayerState* PS = PlayerController->GetPlayerState<ATN_CoopPlayerState>();
	if (!PS || PS->bHasFinishedRun)
	{
		return;
	}
	PS->DeathZoneTimeRemaining = -1.f;
	if (!bRoundActive)
	{
		// Preparando, en el recuento o con el campeón: nada (las tortugas están quietas o en la salida).
		return;
	}

	ATortugaCharacter* Turtle = Cast<ATortugaCharacter>(PlayerController->GetPawn());
	if (!Turtle)
	{
		RescueTurtle(PlayerController, TEXT("sin tortuga"));
		return;
	}
	const FVector Location = Turtle->GetActorLocation();
	if (Generator && Generator->IsFinishWater(Location))
	{
		// Una caída larga que acaba en el agua de meta es llegar, no caerse.
		MarkPlayerFinished(PlayerController);
		return;
	}
	if (Location.Z < GetVoidZ() || IsInsideHazard(Turtle))
	{
		// Dentro de una zona de muerte o de la tormenta seguiría «muriendo»: vuelve a un sitio seguro.
		RescueTurtle(PlayerController, TEXT("zona de muerte"));
		return;
	}
	TNBeach::StunTurtle(Turtle, DeathStunSeconds);
}

void ATN_BeachRaceGameMode::UpdateRoundProgressAndMaybeFinish()
{
	if (bSuppressRoundCheck || !bRoundActive || !GameState)
	{
		return;
	}
	// Gana la ronda la primera en el agua (llega aquí tras una desconexión o si otra pieza llamó a la base).
	int32 Total = 0;
	int32 Finished = 0;
	int32 BestRank = TNumericLimits<int32>::Max();
	APlayerController* Winner = nullptr;
	FString WinnerName;
	for (APlayerState* BasePS : GameState->PlayerArray)
	{
		const ATN_CoopPlayerState* PS = Cast<ATN_CoopPlayerState>(BasePS);
		if (!PS)
		{
			continue;
		}
		++Total;
		if (PS->bHasFinishedRun && !PS->bIsEliminated)
		{
			++Finished;
			if (PS->FinishRank > 0 && PS->FinishRank < BestRank)
			{
				BestRank = PS->FinishRank;
				Winner = PS->GetPlayerController();
				WinnerName = PS->GetPlayerName();
			}
		}
	}
	if (ATN_CoopGameState* CoopState = GetGameState<ATN_CoopGameState>())
	{
		CoopState->FinishedPlayers = Finished;
		CoopState->ExpectedPlayers = Total;
	}
	if (Winner)
	{
		EndRound(Winner, FString::Printf(TEXT("¡%s gana la ronda!"), *WinnerName));
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Fin de ronda, recuento y campeón
// ─────────────────────────────────────────────────────────────────────────────

void ATN_BeachRaceGameMode::EndRound(APlayerController* Winner, const FString& ResultText)
{
	if (!bRoundActive)
	{
		return;
	}
	bRoundActive = false;
	GetWorldTimerManager().ClearTimer(WatchHandle);
	GetWorldTimerManager().ClearTimer(RoundTimeLimitHandle);
	StopStorm(false);

	ATN_CoopPlayerState* WinnerState = Winner ? Winner->GetPlayerState<ATN_CoopPlayerState>() : nullptr;
	if (WinnerState)
	{
		++WinnerState->RoundWins;
		LastRoundWonByPlayer.Add(WinnerState->GetPlayerId(), CurrentRound);
	}
	if (ATN_BeachRaceGameState* BeachState = GetBeachGameState())
	{
		BeachState->RoundWinner = WinnerState;
		BeachState->RoundResultText = ResultText;
	}

	// Recuento: todos quietos donde estén; la concha nueva la pinta la interfaz a partir de RoundWinner y RoundWins.
	FreezePlayers();
	SetFlowState(ETNMatchFlowState::Countdown);
	SetRacePhase(ETNBeachRacePhase::RoundResults);
	BeginPhaseClock(RoundResultsSeconds);
	GetWorldTimerManager().SetTimer(PhaseEndHandle, this, &ATN_BeachRaceGameMode::AfterRoundResults, RoundResultsSeconds, false);

	UE_LOG(LogTortunabo, Log, TEXT("[Carrera] Ronda %d terminada: %s"), CurrentRound, *ResultText);
	SyncGameState();
}

void ATN_BeachRaceGameMode::AfterRoundResults()
{
	StopPhaseClock();
	ATN_CoopPlayerState* Best = nullptr;
	for (APlayerState* BasePS : GameState->PlayerArray)
	{
		ATN_CoopPlayerState* PS = Cast<ATN_CoopPlayerState>(BasePS);
		if (!PS || PS->RoundWins < WinsToWinMatch)
		{
			continue;
		}
		const int32 LastWon = LastRoundWonByPlayer.FindRef(PS->GetPlayerId());
		const int32 BestLastWon = Best ? LastRoundWonByPlayer.FindRef(Best->GetPlayerId()) : -1;
		if (!Best || PS->RoundWins > Best->RoundWins || (PS->RoundWins == Best->RoundWins && LastWon > BestLastWon))
		{
			Best = PS;
		}
	}
	if (Best)
	{
		EnterChampion(Best);
	}
	else
	{
		StartNextRound();
	}
}

void ATN_BeachRaceGameMode::StartNextRound()
{
	++CurrentRound;
	bShowPreRaceCountdown = true;
	ResetRoundPlayerStates();
	SetFlowState(ETNMatchFlowState::WaitingForPlayers);
	PrepareRound(true);
}

void ATN_BeachRaceGameMode::EnterChampion(ATN_CoopPlayerState* ChampionState)
{
	CancelRoundTimers();
	bRoundActive = false;
	bMatchOver = true;
	StopStorm(false);
	FreezePlayers();

	// Podio: el campeón y, detrás, por conchas; a igualdad, quien ganó una ronda más tarde; después, por orden de llegada
	// a la partida.
	TArray<ATN_CoopPlayerState*> Standings;
	for (APlayerState* BasePS : GameState->PlayerArray)
	{
		if (ATN_CoopPlayerState* PS = Cast<ATN_CoopPlayerState>(BasePS))
		{
			Standings.Add(PS);
		}
	}
	Standings.Sort([this, ChampionState](const ATN_CoopPlayerState& A, const ATN_CoopPlayerState& B)
	{
		if ((&A == ChampionState) != (&B == ChampionState))
		{
			return &A == ChampionState;
		}
		if (A.RoundWins != B.RoundWins)
		{
			return A.RoundWins > B.RoundWins;
		}
		const int32 LastA = LastRoundWonByPlayer.FindRef(A.GetPlayerId());
		const int32 LastB = LastRoundWonByPlayer.FindRef(B.GetPlayerId());
		if (LastA != LastB)
		{
			return LastA > LastB;
		}
		return A.GetPlayerId() < B.GetPlayerId();
	});

	if (ATN_BeachRaceGameState* BeachState = GetBeachGameState())
	{
		BeachState->Champion = ChampionState;
		BeachState->Podium.Reset();
		for (int32 Index = 0; Index < Standings.Num() && Index < 3; ++Index)
		{
			BeachState->Podium.Add(Standings[Index]);
		}
		// La tabla de siempre (HUD de resultados y música de fin de partida): puesto por conchas.
		BeachState->RaceResults.Reset();
		for (int32 Index = 0; Index < Standings.Num(); ++Index)
		{
			BeachState->Server_UpsertRaceResult(Standings[Index]->GetPlayerId(), Standings[Index]->GetPlayerName(),
				Index + 1, 0.f, Standings[Index]->RoundWins, false);
		}
		SetFlowState(ETNMatchFlowState::Results);
		BeachState->CountdownValue = TNBeachRaceGameModeDetail::ChampionHoldCountdown;
		BeachState->PhaseSecondsLeft = 0.f;
	}
	SetRacePhase(ETNBeachRacePhase::Champion);

	UE_LOG(LogTortunabo, Log, TEXT("[Carrera] ═══ ¡%s es campeona con %d conchas tras %d rondas! Esperando al anfitrión. ═══"),
		ChampionState ? *ChampionState->GetPlayerName() : TEXT("nadie"), ChampionState ? ChampionState->RoundWins : 0, CurrentRound);
	SyncGameState();
}

// ─────────────────────────────────────────────────────────────────────────────
// Pantalla del campeón
// ─────────────────────────────────────────────────────────────────────────────

bool ATN_BeachRaceGameMode::RequestChampionChoice(const UObject* WorldContextObject, ETNBeachChampionChoice Choice)
{
	UWorld* World = WorldContextObject && GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	if (!World)
	{
		return false;
	}
	if (ATN_BeachRaceGameMode* GM = TNBeachRaceGameModeDetail::FindGameMode(World))
	{
		// Anfitrión: decide por todos, y solo con la partida acabada.
		if (!GM->bMatchOver || GM->bLeaving)
		{
			UE_LOG(LogTortunabo, Warning, TEXT("[Carrera] %s: solo en la pantalla del campeón."), *UEnum::GetValueAsString(Choice));
			return false;
		}
		switch (Choice)
		{
		case ETNBeachChampionChoice::PlayAgain:  GM->PlayAgain(); break;
		case ETNBeachChampionChoice::ChangeMode: GM->ChangeModeAndReturnToLobby(); break;
		case ETNBeachChampionChoice::Quit:       GM->QuitToMainMenu(); break;
		default: return false;
		}
		return true;
	}
	// Cliente: lo demás lo decide el anfitrión; salir, cada uno por su cuenta.
	if (Choice == ETNBeachChampionChoice::Quit)
	{
		if (UMP_GameInstance* GI = Cast<UMP_GameInstance>(World->GetGameInstance()))
		{
			GI->HandleReturnToMenu();
			return true;
		}
	}
	return false;
}

bool ATN_BeachRaceGameMode::CanLocalPlayerChoose(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject && GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	const ATN_BeachRaceGameMode* GM = TNBeachRaceGameModeDetail::FindGameMode(World);
	return GM && GM->bMatchOver && !GM->bLeaving;
}

void ATN_BeachRaceGameMode::PlayAgain()
{
	if (!HasAuthority() || bLeaving)
	{
		return;
	}
	UE_LOG(LogTortunabo, Log, TEXT("[Carrera] Volver a jugar: otra partida en la playa."));
	CancelRoundTimers();
	bMatchOver = false;
	ResetMatchScores();
	CurrentRound = 1;
	bShowPreRaceCountdown = true;
	ResetRoundPlayerStates();
	SetFlowState(ETNMatchFlowState::WaitingForPlayers);
	PrepareRound(true);
}

void ATN_BeachRaceGameMode::ChangeModeAndReturnToLobby()
{
	if (!HasAuthority() || bLeaving)
	{
		return;
	}
	// Con dos modos, «el otro» de la carrera es el cooperativo; en el lobby se sale a él al ponerse listos.
	if (UMP_GameInstance* GI = Cast<UMP_GameInstance>(GetGameInstance()))
	{
		GI->SelectedProcMode = ETNProcGameMode::Coop;
	}
	UE_LOG(LogTortunabo, Log, TEXT("[Carrera] Cambiar de modo: vuelta al lobby con el cooperativo elegido."));
	LeaveAfterDelay([this]()
	{
		FinishRoundAndReturnToLobby();
	});
}

void ATN_BeachRaceGameMode::QuitToMainMenu()
{
	if (!HasAuthority() || bLeaving)
	{
		return;
	}
	UE_LOG(LogTortunabo, Log, TEXT("[Carrera] Salir: el anfitrión vuelve al menú y la partida se cierra."));
	LeaveAfterDelay([this]()
	{
		// Peones fuera antes de cortar la voz (como antes de cualquier viaje) y, al fotograma siguiente, al menú.
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* PC = It->Get();
			if (APawn* Pawn = PC ? PC->GetPawn() : nullptr)
			{
				Pawn->Destroy();
			}
		}
		GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			if (UMP_GameInstance* GI = Cast<UMP_GameInstance>(GetGameInstance()))
			{
				GI->HandleReturnToMenu();
			}
		}));
	});
}

void ATN_BeachRaceGameMode::LeaveAfterDelay(TFunction<void()> Action)
{
	bLeaving = true;
	CancelRoundTimers();
	// Con CountdownValue = 1 en Results se cierra el huevo en todas las pantallas y la música se funde, como al final
	// de la cuenta de los resultados de siempre.
	if (ATN_CoopGameState* CoopState = GetGameState<ATN_CoopGameState>())
	{
		CoopState->CountdownValue = 1;
	}
	GetWorldTimerManager().SetTimer(LeaveHandle, FTimerDelegate::CreateWeakLambda(this, [DeferredAction = MoveTemp(Action)]()
	{
		DeferredAction();
	}), ChampionLeaveDelaySeconds, false);
}

// ─────────────────────────────────────────────────────────────────────────────
// Estado de la partida y de las rondas
// ─────────────────────────────────────────────────────────────────────────────

void ATN_BeachRaceGameMode::ResetMatchScores()
{
	LastRoundWonByPlayer.Reset();
	for (APlayerState* BasePS : GameState->PlayerArray)
	{
		if (ATN_CoopPlayerState* PS = Cast<ATN_CoopPlayerState>(BasePS))
		{
			PS->RoundWins = 0;
			PS->TeamIndex = -1;
		}
	}
	if (ATN_BeachRaceGameState* BeachState = GetBeachGameState())
	{
		BeachState->RoundWinner = nullptr;
		BeachState->Champion = nullptr;
		BeachState->Podium.Reset();
		BeachState->RoundResultText.Reset();
		BeachState->NotifyRacePhaseChanged();
	}
}

void ATN_BeachRaceGameMode::ResetRoundPlayerStates()
{
	for (APlayerState* BasePS : GameState->PlayerArray)
	{
		if (ATN_CoopPlayerState* PS = Cast<ATN_CoopPlayerState>(BasePS))
		{
			PS->ResetForNewRace();
		}
	}
	NextFinishRank = 1;
	if (ATN_CoopGameState* CoopState = GetGameState<ATN_CoopGameState>())
	{
		CoopState->RaceResults.Reset();
		CoopState->FinishedPlayers = 0;
		CoopState->CountdownValue = 0;
		CoopState->OnRaceResultsUpdated.Broadcast();
	}
}

void ATN_BeachRaceGameMode::CancelRoundTimers()
{
	FTimerManager& Timers = GetWorldTimerManager();
	Timers.ClearTimer(PrepPollHandle);
	Timers.ClearTimer(PhaseClockHandle);
	Timers.ClearTimer(PhaseEndHandle);
	Timers.ClearTimer(WatchHandle);
	Timers.ClearTimer(RoundTimeLimitHandle);
	bPreparingRound = false;
	PhaseEndTime = 0.f;
}

void ATN_BeachRaceGameMode::CleanupRoundActors()
{
	StopStorm(true);
	SafeSpots.Reset();

	// Fuera todas las tortugas: la ronda nueva las crea limpias (sin caparazón, aturdimiento ni carga) en la salida.
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (APawn* Pawn = PC ? PC->GetPawn() : nullptr)
		{
			PC->UnPossess();
			Pawn->Destroy();
		}
	}
	for (TActorIterator<ATortugaCharacter> It(GetWorld()); It; ++It)
	{
		if (IsValid(*It) && !It->IsActorBeingDestroyed())
		{
			It->Destroy();
		}
	}
}

ETNBeachRacePhase ATN_BeachRaceGameMode::GetRacePhase() const
{
	const ATN_BeachRaceGameState* BeachState = GetBeachGameState();
	return BeachState ? BeachState->RacePhase : ETNBeachRacePhase::Waiting;
}

ATN_BeachRaceGenerator* ATN_BeachRaceGameMode::GetGenerator() const
{
	return Generator;
}

ATN_BeachRaceGameState* ATN_BeachRaceGameMode::GetBeachGameState() const
{
	return GetGameState<ATN_BeachRaceGameState>();
}

void ATN_BeachRaceGameMode::SetRacePhase(ETNBeachRacePhase NewPhase) const
{
	if (ATN_BeachRaceGameState* BeachState = GetBeachGameState())
	{
		BeachState->RacePhase = NewPhase;
		BeachState->NotifyRacePhaseChanged();
	}
}

void ATN_BeachRaceGameMode::BeginPhaseClock(float Seconds)
{
	PhaseEndTime = GetWorld()->GetTimeSeconds() + FMath::Max(0.f, Seconds);
	TickPhaseClock();
	GetWorldTimerManager().SetTimer(PhaseClockHandle, this, &ATN_BeachRaceGameMode::TickPhaseClock, 0.25f, true);
}

void ATN_BeachRaceGameMode::StopPhaseClock()
{
	GetWorldTimerManager().ClearTimer(PhaseClockHandle);
	PhaseEndTime = 0.f;
	if (ATN_BeachRaceGameState* BeachState = GetBeachGameState())
	{
		BeachState->PhaseSecondsLeft = 0.f;
		BeachState->CountdownValue = 0;
	}
}

void ATN_BeachRaceGameMode::TickPhaseClock()
{
	const float Left = FMath::Max(0.f, PhaseEndTime - GetWorld()->GetTimeSeconds());
	if (ATN_BeachRaceGameState* BeachState = GetBeachGameState())
	{
		// PhaseSecondsLeft para la interfaz de la carrera; CountdownValue para la cuenta de siempre (música y HUD).
		BeachState->PhaseSecondsLeft = Left;
		BeachState->CountdownValue = FMath::CeilToInt(Left);
	}
	if (Left <= 0.f)
	{
		GetWorldTimerManager().ClearTimer(PhaseClockHandle);
	}
}

void ATN_BeachRaceGameMode::SyncGameState() const
{
	ATN_BeachRaceGameState* BeachState = GetBeachGameState();
	if (!BeachState)
	{
		return;
	}
	BeachState->ProcMode = ETNProcGameMode::Race;
	BeachState->ProcDifficulty = ETNProcDifficulty::Normal;
	BeachState->CurrentRound = CurrentRound;
	BeachState->RoundTarget = WinsToWinMatch;
	BeachState->bRoundInProgress = bRoundActive;
	BeachState->MapSeed = RoundSeed;
	BeachState->EstimatedMinutes = static_cast<float>(TNBeach::CourseLength / TNBeachRaceGameModeDetail::AverageRaceSpeed / 60.0);
	BeachState->NotifyRoundInfoChanged();
}

// ─────────────────────────────────────────────────────────────────────────────
// Tortugas: salida, congelado, sitios seguros
// ─────────────────────────────────────────────────────────────────────────────

AActor* ATN_BeachRaceGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	// Cada jugador aparece en su sitio de la salida (PlayerStart creado en ejecución la primera vez que se usa).
	if (Generator && Generator->IsRoundReady())
	{
		const int32 Slot = GetStartSlot(Player);
		if (!StartPoints.IsValidIndex(Slot) || !IsValid(StartPoints[Slot]))
		{
			const FTransform Start = PutOnFloor(GetStartTransformFor(Slot), GetDefaultHalfHeight());
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			APlayerStart* Point = GetWorld()->SpawnActor<APlayerStart>(APlayerStart::StaticClass(), Start, Params);
			if (Point)
			{
				Point->PlayerStartTag = FName(TEXT("TNBeachStart"));
				if (StartPoints.Num() <= Slot)
				{
					StartPoints.SetNum(Slot + 1);
				}
				StartPoints[Slot] = Point;
			}
		}
		if (StartPoints.IsValidIndex(Slot) && IsValid(StartPoints[Slot]))
		{
			return StartPoints[Slot];
		}
	}
	return Super::ChoosePlayerStart_Implementation(Player);
}

int32 ATN_BeachRaceGameMode::GetStartSlot(const AController* Controller) const
{
	if (!Controller || !GameState)
	{
		return 0;
	}
	const int32 Count = FMath::Max(1, GameState->PlayerArray.Num());
	const int32 Index = FMath::Max(0, GameState->PlayerArray.IndexOfByKey(Controller->PlayerState));
	// La salida es escalonada: los sitios rotan cada ronda para que nadie salga siempre delante.
	return (Index + FMath::Max(0, CurrentRound - 1)) % Count;
}

FTransform ATN_BeachRaceGameMode::GetStartTransformFor(int32 Slot) const
{
	if (Generator)
	{
		return Generator->GetStartTransform(Slot);
	}
	// Sin generador: un PlayerStart del nivel, con los sitios en fila a su derecha.
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		if (It->PlayerStartTag != FName(TEXT("TNBeachStart")))
		{
			const FTransform Base = It->GetActorTransform();
			return FTransform(Base.GetRotation(), Base.GetLocation() + Base.GetRotation().GetRightVector() * (200.0 * Slot));
		}
	}
	return FTransform::Identity;
}

FTransform ATN_BeachRaceGameMode::PutOnFloor(const FTransform& Transform, float HalfHeight) const
{
	FVector Location = Transform.GetLocation();
	const FRotator Facing(0.f, Transform.Rotator().Yaw, 0.f);
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	const FCollisionQueryParams Query(SCENE_QUERY_STAT(TNBeachPutOnFloor), false);
	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByObjectType(Hit, Location + FVector(0.0, 0.0, 300.0), Location - FVector(0.0, 0.0, 1500.0), Objects, Query)
		&& Hit.ImpactNormal.Z > 0.5)
	{
		Location.Z = Hit.ImpactPoint.Z + HalfHeight + 2.0;
	}
	return FTransform(Facing, Location);
}

float ATN_BeachRaceGameMode::GetDefaultHalfHeight() const
{
	const ACharacter* Cdo = DefaultPawnClass ? Cast<ACharacter>(DefaultPawnClass->GetDefaultObject()) : nullptr;
	const UCapsuleComponent* Capsule = Cdo ? Cdo->GetCapsuleComponent() : nullptr;
	return Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 90.f;
}

void ATN_BeachRaceGameMode::PlacePlayersAtStart()
{
	const FTransform Reference = GetStartTransformFor(0);
	CourseOrigin = Reference.GetLocation();
	CourseForward = FRotator(0.f, Reference.Rotator().Yaw, 0.f).Vector();

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC)
		{
			continue;
		}
		APawn* Pawn = PC->GetPawn();
		const bool bSpectating = PC->GetStateName() == NAME_Spectating || (PC->PlayerState && PC->PlayerState->IsOnlyASpectator());
		if (!Pawn || bSpectating)
		{
			RespawnControllerFresh(PC);
			Pawn = PC->GetPawn();
		}
		if (!Pawn)
		{
			continue;
		}
		const ACharacter* Character = Cast<ACharacter>(Pawn);
		const float HalfHeight = Character && Character->GetCapsuleComponent()
			? Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()
			: GetDefaultHalfHeight();
		const FTransform Start = PutOnFloor(GetStartTransformFor(GetStartSlot(PC)), HalfHeight);
		if (ATortugaCharacter* Turtle = Cast<ATortugaCharacter>(Pawn))
		{
			TeleportTurtle(Turtle, Start);
		}
		else
		{
			Pawn->SetActorLocationAndRotation(Start.GetLocation(), Start.GetRotation(), false, nullptr, ETeleportType::TeleportPhysics);
		}
		Pawn->SetActorHiddenInGame(false);
		Pawn->SetActorEnableCollision(true);
		PC->ClientSetRotation(Start.Rotator(), true);
	}
	// Quietos en la salida hasta que se dé la salida.
	FreezePlayers();
}

void ATN_BeachRaceGameMode::RespawnControllerFresh(APlayerController* PlayerController)
{
	if (APawn* OldPawn = PlayerController->GetPawn())
	{
		PlayerController->UnPossess();
		OldPawn->Destroy();
	}
	if (APlayerState* PS = PlayerController->PlayerState)
	{
		PS->SetIsSpectator(false);
		PS->SetIsOnlyASpectator(false);
	}
	PlayerController->ChangeState(NAME_Playing);
	PlayerController->ClientGotoState(NAME_Playing);

	RestartPlayer(PlayerController);

	if (APawn* NewPawn = PlayerController->GetPawn())
	{
		NewPawn->EnableInput(PlayerController);
		PlayerController->SetViewTarget(NewPawn);
	}
}

void ATN_BeachRaceGameMode::FreezePlayers()
{
	// Servidor (el peón) y cliente (el input). Si el peón cambia, se vuelve a avisar al cliente porque ClientRestart
	// limpia los flags de input. Las tortugas en su bola de caparazón siguen con su física.
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC)
		{
			continue;
		}
		APawn* Pawn = PC->GetPawn();
		const TWeakObjectPtr<APawn>* Frozen = FrozenControllers.Find(PC);
		if (!Frozen || Frozen->Get() != Pawn)
		{
			PC->ClientIgnoreMoveInput(true);
			FrozenControllers.Add(PC, Pawn);
		}
		if (ACharacter* Character = Cast<ACharacter>(Pawn))
		{
			UCharacterMovementComponent* Move = Character->GetCharacterMovement();
			if (Move && Move->MovementMode != MOVE_None)
			{
				Move->StopMovementImmediately();
				Move->DisableMovement();
			}
		}
	}
}

void ATN_BeachRaceGameMode::UnfreezePlayers()
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC)
		{
			continue;
		}
		if (ATortugaCharacter* Turtle = Cast<ATortugaCharacter>(PC->GetPawn()))
		{
			const UTN_ShellComponent* Shell = Turtle->GetShellComponent();
			UCharacterMovementComponent* Move = Turtle->GetCharacterMovement();
			if (Move && Move->MovementMode == MOVE_None && !(Shell && Shell->HasLocalBody()))
			{
				Move->SetMovementMode(MOVE_Falling);
			}
		}
		if (AMP_GamePlayerController* TNPC = Cast<AMP_GamePlayerController>(PC))
		{
			TNPC->ClientRestorePlayerInput();
			if (TNPC->IsLocalController())
			{
				TNPC->ForceRestoreInput();
			}
		}
		else
		{
			PC->ResetIgnoreInputFlags();
			PC->ClientIgnoreMoveInput(false);
		}
	}
	FrozenControllers.Reset();
}

void ATN_BeachRaceGameMode::ReleaseCarry(ATortugaCharacter* Turtle) const
{
	UTN_CarryComponent* Carry = Turtle ? Turtle->GetCarryComponent() : nullptr;
	if (!Carry)
	{
		return;
	}
	if (Carry->IsCarrying())
	{
		Carry->ForceRelease(false);
	}
	if (ATortugaCharacter* Carrier = Carry->GetCarrier())
	{
		if (UTN_CarryComponent* CarrierCarry = Carrier->GetCarryComponent())
		{
			CarrierCarry->ForceRelease(false);
		}
	}
}

void ATN_BeachRaceGameMode::TeleportTurtle(ATortugaCharacter* Turtle, const FTransform& Transform) const
{
	if (!Turtle)
	{
		return;
	}
	ReleaseCarry(Turtle);
	if (Turtle->IsKnockedDown())
	{
		Turtle->RecoverFromKnockdown();
	}
	// Con caja física la tortuga sigue a la caja: primero fuera del caparazón (si estaba aturdida, StunTurtle la vuelve
	// a meter después en el sitio nuevo).
	if (UTN_ShellComponent* Shell = Turtle->GetShellComponent())
	{
		Shell->SetExitLocked(false);
		Shell->ForceExitShell();
	}
	UCharacterMovementComponent* Move = Turtle->GetCharacterMovement();
	if (Move)
	{
		Move->StopMovementImmediately();
	}
	Turtle->SetActorLocationAndRotation(Transform.GetLocation(), Transform.GetRotation(), false, nullptr, ETeleportType::TeleportPhysics);
	if (Move)
	{
		Move->SetMovementMode(MOVE_Falling);
	}
}

void ATN_BeachRaceGameMode::RescueTurtle(APlayerController* PlayerController, const TCHAR* Reason)
{
	if (!PlayerController)
	{
		return;
	}
	ATortugaCharacter* Turtle = Cast<ATortugaCharacter>(PlayerController->GetPawn());
	const bool bSpectating = PlayerController->GetStateName() == NAME_Spectating;
	if (!Turtle || bSpectating)
	{
		RespawnControllerFresh(PlayerController);
		Turtle = Cast<ATortugaCharacter>(PlayerController->GetPawn());
	}
	if (!Turtle)
	{
		return;
	}
	const float HalfHeight = Turtle->GetCapsuleComponent() ? Turtle->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : GetDefaultHalfHeight();
	const FTransform Safe = PutOnFloor(FindSafeTransform(PlayerController), HalfHeight);
	TeleportTurtle(Turtle, Safe);
	PlayerController->ClientSetRotation(Safe.Rotator(), true);
	if (ATN_CoopPlayerState* PS = PlayerController->GetPlayerState<ATN_CoopPlayerState>())
	{
		PS->DeathZoneTimeRemaining = -1.f;
	}
	TNBeach::StunTurtle(Turtle, RescueStunSeconds);
	UE_LOG(LogTortunabo, Log, TEXT("[Carrera] %s (%s): vuelve a un sitio seguro, aturdida."), *GetNameSafe(PlayerController), Reason);
}

FTransform ATN_BeachRaceGameMode::FindSafeTransform(APlayerController* PlayerController)
{
	TArray<FTNBeachSafeSpot>* Trail = SafeSpots.Find(PlayerController);
	if (Trail && Trail->Num() > 0)
	{
		// El más nuevo con cierta antigüedad (el de justo antes de caer suele estar en el borde); si no, el más viejo.
		const float Now = GetWorld()->GetTimeSeconds();
		int32 Pick = 0;
		for (int32 Index = Trail->Num() - 1; Index >= 0; --Index)
		{
			if (Now - (*Trail)[Index].Time >= SafeSpotMinAgeSeconds)
			{
				Pick = Index;
				break;
			}
		}
		const FTNBeachSafeSpot Spot = (*Trail)[Pick];
		// Lo de después (más cerca de donde cayó) se olvida.
		Trail->SetNum(Pick + 1);
		return FTransform(Spot.Rotation, Spot.Location);
	}
	return GetStartTransformFor(GetStartSlot(PlayerController));
}

void ATN_BeachRaceGameMode::SampleSafeSpot(APlayerController* PlayerController, const ACharacter* Character, float Now)
{
	const UCharacterMovementComponent* Move = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Move || !Move->IsMovingOnGround() || !Move->CurrentFloor.IsWalkableFloor() || TNBeach::IsTurtleStunned(Character))
	{
		return;
	}
	if (const ATortugaCharacter* Turtle = Cast<ATortugaCharacter>(Character))
	{
		if (Turtle->IsInShell() || Turtle->IsKnockedDown())
		{
			return;
		}
	}
	if (IsInsideHazard(Character))
	{
		return;
	}
	FTNBeachSafeSpot Spot;
	Spot.Location = Character->GetActorLocation();
	Spot.Rotation = FRotator(0.f, Character->GetActorRotation().Yaw, 0.f);
	Spot.Time = Now;
	TArray<FTNBeachSafeSpot>& Trail = SafeSpots.FindOrAdd(PlayerController);
	Trail.Add(Spot);
	if (Trail.Num() > TNBeachRaceGameModeDetail::MaxSafeSpots)
	{
		Trail.RemoveAt(0);
	}
	LowestGroundZ = FMath::Min(LowestGroundZ, Spot.Location.Z);
}

bool ATN_BeachRaceGameMode::IsInsideHazard(const APawn* Pawn) const
{
	if (!Pawn)
	{
		return false;
	}
	TArray<AActor*> Overlapping;
	Pawn->GetOverlappingActors(Overlapping, ATN_DeathZoneVolume::StaticClass());
	if (Overlapping.Num() > 0)
	{
		return true;
	}
	Pawn->GetOverlappingActors(Overlapping, ATN_StormVolume::StaticClass());
	return Overlapping.Num() > 0;
}

double ATN_BeachRaceGameMode::GetVoidZ() const
{
	double VoidZ = FMath::Min(CourseOrigin.Z, LowestGroundZ) - VoidDepth;
	// Siempre por encima del KillZ del nivel, para llegar antes de que el motor destruya a la tortuga.
	if (const AWorldSettings* Settings = GetWorldSettings())
	{
		if (Settings->bEnableWorldBoundsChecks)
		{
			VoidZ = FMath::Max(VoidZ, static_cast<double>(Settings->KillZ) + 300.0);
		}
	}
	return VoidZ;
}

float ATN_BeachRaceGameMode::GetCourseProgress(const APawn* Pawn) const
{
	return Pawn ? static_cast<float>(FVector::DotProduct(Pawn->GetActorLocation() - CourseOrigin, CourseForward)) : 0.f;
}

// ─────────────────────────────────────────────────────────────────────────────
// Tormenta de bañistas (ATN_BeachStorm, del agente de enemigos: por nombre)
// ─────────────────────────────────────────────────────────────────────────────

void ATN_BeachRaceGameMode::StartStorm()
{
	if (!bStormClassResolved)
	{
		bStormClassResolved = true;
		const FString Path = FString::Printf(TEXT("/Script/Tortunabo.%s"), *StormClassName);
		UClass* Found = FindObject<UClass>(nullptr, *Path);
		if (Found && Found->IsChildOf(AActor::StaticClass()))
		{
			StormClass = Found;
		}
		else
		{
			UE_LOG(LogTortunabo, Warning, TEXT("[Carrera] Sin clase %s: se corre sin tormenta."), *Path);
		}
	}
	if (!StormClass)
	{
		return;
	}
	StopStorm(true);

	// Detrás de la salida, mirando hacia el mar: avanza por donde van las tortugas.
	const FTransform SpawnAt(CourseForward.Rotation(), CourseOrigin - CourseForward * StormSpawnBehind);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Storm = GetWorld()->SpawnActor<AActor>(StormClass, SpawnAt, Params);
	if (Storm)
	{
		CallNoParamFunction(Storm, TEXT("StartStorm"));
		UE_LOG(LogTortunabo, Log, TEXT("[Carrera] Tormenta de bañistas en marcha (%s)."), *GetNameSafe(Storm));
	}
}

void ATN_BeachRaceGameMode::StopStorm(bool bDestroy)
{
	if (!Storm)
	{
		return;
	}
	// Al acabar la ronda se para (si sabe pararse, se queda a la vista durante el recuento); al preparar otra, fuera.
	if (bDestroy || !CallNoParamFunction(Storm, TEXT("StopStorm")))
	{
		Storm->Destroy();
		Storm = nullptr;
	}
}

bool ATN_BeachRaceGameMode::CallNoParamFunction(UObject* Target, FName FunctionName)
{
	UFunction* Function = Target ? Target->FindFunction(FunctionName) : nullptr;
	if (!Function || Function->NumParms != 0)
	{
		return false;
	}
	Target->ProcessEvent(Function, nullptr);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Pruebas
// ─────────────────────────────────────────────────────────────────────────────

APlayerController* ATN_BeachRaceGameMode::GetControllerByIndex(int32 PlayerIndex) const
{
	if (!GameState || !GameState->PlayerArray.IsValidIndex(PlayerIndex))
	{
		UE_LOG(LogTortunabo, Warning, TEXT("[Carrera] No hay jugador %d (hay %d)."), PlayerIndex, GameState ? GameState->PlayerArray.Num() : 0);
		return nullptr;
	}
	const APlayerState* PS = GameState->PlayerArray[PlayerIndex];
	return PS ? PS->GetPlayerController() : nullptr;
}

void ATN_BeachRaceGameMode::DebugWinRound(int32 PlayerIndex)
{
	if (!bRoundActive)
	{
		UE_LOG(LogTortunabo, Warning, TEXT("[Carrera] TN.Race.WinRound: no hay ronda en marcha (fase %s)."), *UEnum::GetValueAsString(GetRacePhase()));
		return;
	}
	if (APlayerController* PC = GetControllerByIndex(PlayerIndex))
	{
		MarkPlayerFinished(PC);
	}
}

void ATN_BeachRaceGameMode::DebugChampion(int32 PlayerIndex)
{
	if (bLeaving)
	{
		return;
	}
	APlayerController* PC = GetControllerByIndex(PlayerIndex);
	ATN_CoopPlayerState* PS = PC ? PC->GetPlayerState<ATN_CoopPlayerState>() : nullptr;
	if (!PS)
	{
		return;
	}
	PS->RoundWins = FMath::Max(PS->RoundWins, WinsToWinMatch);
	LastRoundWonByPlayer.Add(PS->GetPlayerId(), CurrentRound);
	if (ATN_BeachRaceGameState* BeachState = GetBeachGameState())
	{
		BeachState->RoundWinner = PS;
	}
	EnterChampion(PS);
}

void ATN_BeachRaceGameMode::DebugKill(int32 PlayerIndex)
{
	if (APlayerController* PC = GetControllerByIndex(PlayerIndex))
	{
		MarkPlayerDead(PC);
	}
}

void ATN_BeachRaceGameMode::DebugVoid(int32 PlayerIndex)
{
	APlayerController* PC = GetControllerByIndex(PlayerIndex);
	ATortugaCharacter* Turtle = PC ? Cast<ATortugaCharacter>(PC->GetPawn()) : nullptr;
	if (!Turtle || !bRoundActive)
	{
		UE_LOG(LogTortunabo, Warning, TEXT("[Carrera] TN.Race.Void: hace falta una tortuga en plena carrera."));
		return;
	}
	const FVector Location = Turtle->GetActorLocation();
	TeleportTurtle(Turtle, FTransform(Turtle->GetActorRotation(), FVector(Location.X, Location.Y, GetVoidZ() - 1000.0)));
}

void ATN_BeachRaceGameMode::DebugStun(int32 PlayerIndex, float Seconds)
{
	APlayerController* PC = GetControllerByIndex(PlayerIndex);
	if (ACharacter* Character = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr)
	{
		TNBeach::StunTurtle(Character, Seconds);
	}
}
