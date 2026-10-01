// ATN_RallyGameMode: opciones, emparejado en buggies y fases. El progreso de carrera (puertas, puestos, reaparición) está
// en TN_RallyGameModeRace.cpp.
#include "Rally/TN_RallyGameMode.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Rally/TN_RallyAIController.h"
#include "Rally/TN_RallyPlayerController.h"
#include "Rally/TN_RallyPlayerState.h"
#include "Rally/TN_RallyTrack.h"
#include "Rally/TN_RallyVehicle.h"

namespace TNRallyGameModeStats
{
	/** Carreras terminadas en este proceso (sobrevive al ?Restart, que crea otro GameMode): para ?Races=N. */
	int32 RacesRun = 0;
}

ATN_RallyGameMode::ATN_RallyGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	GameStateClass = ATN_RallyGameState::StaticClass();
	PlayerStateClass = ATN_RallyPlayerState::StaticClass();
	PlayerControllerClass = ATN_RallyPlayerController::StaticClass();
	// Nadie nace como tortuga: cada jugador se sienta en un buggy (o mira la carrera).
	DefaultPawnClass = nullptr;
	AIControllerClass = ATN_RallyAIController::StaticClass();
	// ATN_Buggy vive en Vehicles/ (otra rama): se resuelve por nombre al juntar las dos.
	VehicleClass = TSoftClassPtr<APawn>(FSoftClassPath(TEXT("/Script/Tortunabo.TN_Buggy")));
}

void ATN_RallyGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	const FString VariantOption = UGameplayStatics::ParseOption(Options, TEXT("Variant"));
	Variant = VariantOption.IsEmpty() ? DefaultVariant : FName(*VariantOption);
	Seats = FMath::Clamp(UGameplayStatics::GetIntOption(Options, TEXT("Seats"), 2), 1, 2);
	Bots = FMath::Clamp(UGameplayStatics::GetIntOption(Options, TEXT("Bots"), 0), 0, TNRally::MaxGridSlots);
	bLapsFromUrl = UGameplayStatics::HasOption(Options, TEXT("Laps"));
	Laps = FMath::Clamp(UGameplayStatics::GetIntOption(Options, TEXT("Laps"), DefaultLaps), 1, 9);
	bAutoStart = UGameplayStatics::HasOption(Options, TEXT("AutoStart"));
	RaceTimeoutSeconds = FMath::Max(0, UGameplayStatics::GetIntOption(Options, TEXT("RaceTimeout"), 0));
	RaceLimit = FMath::Max(0, UGameplayStatics::GetIntOption(Options, TEXT("Races"), 0));
	UE_LOG(LogTNRally, Log, TEXT("[RallyGameMode] %s: variante %s, %d plaza(s) por buggy, %d bots, %d vueltas%s."),
		*MapName, *Variant.ToString(), Seats, Bots, Laps, bAutoStart ? TEXT(", salida sin jugadoras") : TEXT(""));
}

void ATN_RallyGameMode::StartPlay()
{
	// La pista se construye antes del BeginPlay de los actores: el terreno de la variante tiene que estar cargado para que
	// las puertas, los bordes y la parrilla encuentren el suelo.
	if (ATN_RallyGameState* RallyState = GetRallyGameState())
	{
		RallyState->Variant = Variant;
		Track = RallyState->PrepareTrack(Variant);
		bTrackReady = Track && Track->IsBuilt();
		if (bTrackReady && !bLapsFromUrl && Track->GetManifestLaps() > 0)
		{
			Laps = FMath::Clamp(Track->GetManifestLaps(), 1, 9);
		}
		RallyState->bCircuit = bTrackReady && Track->IsCircuit();
		RallyState->NumGates = bTrackReady ? Track->GetGateCount() : 0;
		RallyState->Laps = RallyState->bCircuit ? Laps : 1;
		RallyState->Phase = ETNRallyPhase::Warmup;
	}
	if (!bTrackReady)
	{
		UE_LOG(LogTNRally, Error, TEXT("[RallyGameMode] Sin pista para la variante '%s': la carrera no empieza."), *Variant.ToString());
	}

	Super::StartPlay();

	TArray<TWeakObjectPtr<APlayerController>> Waiting = MoveTemp(PendingPlayers);
	PendingPlayers.Reset();
	for (const TWeakObjectPtr<APlayerController>& Player : Waiting)
	{
		if (Player.IsValid())
		{
			bTrackReady ? AssignPlayer(Player.Get()) : Spectate(Player.Get());
		}
	}
	if (bTrackReady)
	{
		SpawnBots();
		RebuildStandings();
		ATN_RallyGameState* RallyState = GetRallyGameState();
		if (bAutoStart && RallyState && RallyState->PhaseEndServerTime <= 0.f && Teams.Num() > 0)
		{
			RallyState->PhaseEndServerTime = static_cast<float>(Now() + WarmupSeconds);
		}
	}
}

void ATN_RallyGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	// No llama a Super: no hay peón por defecto. El anfitrión llega antes de StartPlay, con la pista aún sin construir.
	if (!HasActorBegunPlay() && !bTrackReady)
	{
		PendingPlayers.AddUnique(NewPlayer);
		return;
	}
	bTrackReady ? AssignPlayer(NewPlayer) : Spectate(NewPlayer);
}

void ATN_RallyGameMode::Logout(AController* Exiting)
{
	PendingPlayers.Remove(Cast<APlayerController>(Exiting));
	if (FTeamRuntime* Team = FindTeamByController(Exiting))
	{
		const int32 TeamIndex = Team->TeamIndex;
		APawn* Vehicle = Team->Vehicle.Get();
		ITN_RallyVehicle* RallyVehicle = Cast<ITN_RallyVehicle>(Vehicle);
		if (RallyVehicle)
		{
			RallyVehicle->UnseatController(Exiting);
		}
		const bool bEmpty = !RallyVehicle
			|| (!RallyVehicle->GetSeatController(ETNRallySeat::Driver) && !RallyVehicle->GetSeatController(ETNRallySeat::Gunner));
		if (bEmpty)
		{
			const ETNRallyPhase Phase = GetRallyGameState() ? GetRallyGameState()->Phase : ETNRallyPhase::Warmup;
			if (Phase == ETNRallyPhase::Warmup || Phase == ETNRallyPhase::Countdown)
			{
				if (Vehicle) { Vehicle->Destroy(); }
				Teams.RemoveAll([TeamIndex](const FTeamRuntime& Entry) { return Entry.TeamIndex == TeamIndex; });
			}
			else
			{
				Team->bRetired = true;
				if (RallyVehicle) { RallyVehicle->SetEngineLocked(true); }
			}
		}
		UE_LOG(LogTNRally, Log, TEXT("[RallyGameMode] %s se va del equipo %d%s."), *GetNameSafe(Exiting), TeamIndex,
			bEmpty ? TEXT(" (buggy vacío)") : TEXT(""));
		RebuildStandings();
	}
	Super::Logout(Exiting);
}

ATN_RallyGameState* ATN_RallyGameMode::GetRallyGameState() const
{
	return GetGameState<ATN_RallyGameState>();
}

double ATN_RallyGameMode::Now() const
{
	const ATN_RallyGameState* RallyState = GetRallyGameState();
	return RallyState ? RallyState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
}

TNRally::FLapRules ATN_RallyGameMode::MakeLapRules() const
{
	TNRally::FLapRules Rules;
	Rules.NumGates = Track ? Track->GetGateCount() : 0;
	Rules.bCircuit = Track && Track->IsCircuit();
	Rules.Laps = Rules.bCircuit ? Laps : 1;
	return Rules;
}

// ---- Jugadores y equipos ----

void ATN_RallyGameMode::AssignPlayer(APlayerController* Player)
{
	ATN_RallyPlayerState* RallyPlayer = Player ? Player->GetPlayerState<ATN_RallyPlayerState>() : nullptr;
	if (!RallyPlayer)
	{
		return;
	}
	const ETNRallyPhase Phase = GetRallyGameState() ? GetRallyGameState()->Phase : ETNRallyPhase::Warmup;

	// Biplaza: la 2.ª tortuga de cada pareja es la artillera del primer buggy de jugadoras con la plaza libre.
	if (Seats == 2)
	{
		for (FTeamRuntime& Team : Teams)
		{
			ITN_RallyVehicle* RallyVehicle = Cast<ITN_RallyVehicle>(Team.Vehicle.Get());
			if (Team.bBot || Team.bRetired || !RallyVehicle || RallyVehicle->HasFreeSeat(ETNRallySeat::Driver)
				|| !RallyVehicle->HasFreeSeat(ETNRallySeat::Gunner))
			{
				continue;
			}
			if (RallyVehicle->SeatController(Player, ETNRallySeat::Gunner))
			{
				RallyPlayer->SetRallySeat(Team.TeamIndex, ETNRallySeat::Gunner);
				UE_LOG(LogTNRally, Log, TEXT("[RallyGameMode] %s, artillera del equipo %d."), *RallyPlayer->GetPlayerName(), Team.TeamIndex);
				OnHumanSeated();
				RebuildStandings();
				return;
			}
		}
	}

	// Con la carrera en marcha solo se entra de artillera (late-join fuera del MVP).
	if (Phase != ETNRallyPhase::Warmup && Phase != ETNRallyPhase::Countdown)
	{
		Spectate(Player);
		return;
	}
	const int32 Index = CreateTeam(false);
	ITN_RallyVehicle* RallyVehicle = Teams.IsValidIndex(Index) ? Cast<ITN_RallyVehicle>(Teams[Index].Vehicle.Get()) : nullptr;
	if (!RallyVehicle || !RallyVehicle->SeatController(Player, ETNRallySeat::Driver))
	{
		if (Teams.IsValidIndex(Index))
		{
			if (APawn* Vehicle = Teams[Index].Vehicle.Get()) { Vehicle->Destroy(); }
			Teams.RemoveAt(Index);
		}
		Spectate(Player);
		return;
	}
	RallyPlayer->SetRallySeat(Teams[Index].TeamIndex, ETNRallySeat::Driver);
	UE_LOG(LogTNRally, Log, TEXT("[RallyGameMode] %s conduce el equipo %d (hueco %d)."), *RallyPlayer->GetPlayerName(),
		Teams[Index].TeamIndex, Teams[Index].GridSlot);
	OnHumanSeated();
	RebuildStandings();
}

int32 ATN_RallyGameMode::FindFreeGridSlot() const
{
	for (int32 Slot = 0; Slot < TNRally::MaxGridSlots; ++Slot)
	{
		if (!Teams.ContainsByPredicate([Slot](const FTeamRuntime& Team) { return Team.GridSlot == Slot; }))
		{
			return Slot;
		}
	}
	return INDEX_NONE;
}

int32 ATN_RallyGameMode::CreateTeam(bool bBot)
{
	const int32 Slot = FindFreeGridSlot();
	if (!Track || Slot == INDEX_NONE)
	{
		return INDEX_NONE;
	}
	UClass* Class = VehicleClass.LoadSynchronous();
	if (!Class)
	{
		if (!bLoggedMissingVehicle)
		{
			UE_LOG(LogTNRally, Error, TEXT("[RallyGameMode] No existe la clase del buggy '%s': nadie se sienta."),
				*VehicleClass.ToString());
			bLoggedMissingVehicle = true;
		}
		return INDEX_NONE;
	}
	const FTransform SlotTransform = Track->GetGridSlotTransform(Slot);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	APawn* Vehicle = GetWorld()->SpawnActor<APawn>(Class, SlotTransform, Params);
	ITN_RallyVehicle* RallyVehicle = Cast<ITN_RallyVehicle>(Vehicle);
	if (!RallyVehicle)
	{
		UE_LOG(LogTNRally, Error, TEXT("[RallyGameMode] '%s' no implementa ITN_RallyVehicle."), *GetNameSafe(Class));
		if (Vehicle) { Vehicle->Destroy(); }
		return INDEX_NONE;
	}
	FTeamRuntime Team;
	Team.TeamIndex = NextTeamIndex++;
	Team.Vehicle = Vehicle;
	Team.bBot = bBot;
	Team.GridSlot = Slot;
	Team.GridTransform = SlotTransform;
	Team.PrevLocation = Vehicle->GetActorLocation();
	Team.Arc = Track->FindArcGlobal(Team.PrevLocation);
	RallyVehicle->SetRallyTeamIndex(Team.TeamIndex);
	RallyVehicle->SetEngineLocked(true);
	return Teams.Add(Team);
}

void ATN_RallyGameMode::SpawnBots()
{
	UClass* AIClass = AIControllerClass ? AIControllerClass.Get() : ATN_RallyAIController::StaticClass();
	for (int32 Bot = 0; Bot < Bots; ++Bot)
	{
		const int32 Index = CreateTeam(true);
		if (Index == INDEX_NONE)
		{
			break;
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AController* Pilot = GetWorld()->SpawnActor<AController>(AIClass, Teams[Index].GridTransform, Params);
		ITN_RallyVehicle* RallyVehicle = Cast<ITN_RallyVehicle>(Teams[Index].Vehicle.Get());
		if (!Pilot || !RallyVehicle || !RallyVehicle->SeatController(Pilot, ETNRallySeat::Driver))
		{
			UE_LOG(LogTNRally, Error, TEXT("[RallyGameMode] No se puede sentar al piloto IA %d."), Bot + 1);
			if (Pilot) { Pilot->Destroy(); }
			if (APawn* Vehicle = Teams[Index].Vehicle.Get()) { Vehicle->Destroy(); }
			Teams.RemoveAt(Index);
			break;
		}
		if (APlayerState* BotState = Pilot->GetPlayerState<APlayerState>())
		{
			BotState->SetPlayerName(FText::Format(NSLOCTEXT("Rally", "BotName", "Tortuga IA {0}"), FText::AsNumber(Bot + 1)).ToString());
		}
	}
}

ATN_RallyGameMode::FTeamRuntime* ATN_RallyGameMode::FindTeamByController(const AController* Controller)
{
	if (!Controller)
	{
		return nullptr;
	}
	return Teams.FindByPredicate([Controller](const FTeamRuntime& Team)
	{
		const ITN_RallyVehicle* RallyVehicle = Cast<ITN_RallyVehicle>(Team.Vehicle.Get());
		return RallyVehicle && (RallyVehicle->GetSeatController(ETNRallySeat::Driver) == Controller
			|| RallyVehicle->GetSeatController(ETNRallySeat::Gunner) == Controller);
	});
}

ATN_RallyGameMode::FTeamRuntime* ATN_RallyGameMode::FindTeamByVehicle(const AActor* Vehicle)
{
	return Vehicle ? Teams.FindByPredicate([Vehicle](const FTeamRuntime& Team) { return Team.Vehicle.Get() == Vehicle; }) : nullptr;
}

const ATN_RallyGameMode::FTeamRuntime* ATN_RallyGameMode::FindTeamByVehicle(const AActor* Vehicle) const
{
	return Vehicle ? Teams.FindByPredicate([Vehicle](const FTeamRuntime& Team) { return Team.Vehicle.Get() == Vehicle; }) : nullptr;
}

void ATN_RallyGameMode::Spectate(APlayerController* Player)
{
	if (!Player)
	{
		return;
	}
	if (Track && Track->IsBuilt())
	{
		const FTransform Grid = Track->GetGridSlotTransform(0, 1500.0);
		Player->SetInitialLocationAndRotation(Grid.GetLocation(), Grid.Rotator());
	}
	Player->StartSpectatingOnly();
	UE_LOG(LogTNRally, Log, TEXT("[RallyGameMode] %s mira la carrera."), *GetNameSafe(Player));
}

int32 ATN_RallyGameMode::CountSeatedHumans() const
{
	int32 Count = 0;
	for (const FTeamRuntime& Team : Teams)
	{
		const ITN_RallyVehicle* RallyVehicle = Cast<ITN_RallyVehicle>(Team.Vehicle.Get());
		if (!RallyVehicle)
		{
			continue;
		}
		Count += Cast<APlayerController>(RallyVehicle->GetSeatController(ETNRallySeat::Driver)) ? 1 : 0;
		Count += Cast<APlayerController>(RallyVehicle->GetSeatController(ETNRallySeat::Gunner)) ? 1 : 0;
	}
	return Count;
}

void ATN_RallyGameMode::OnHumanSeated()
{
	ATN_RallyGameState* RallyState = GetRallyGameState();
	if (!RallyState || RallyState->Phase != ETNRallyPhase::Warmup)
	{
		return;
	}
	const double Time = Now();
	if (FirstSeatTime < 0.0)
	{
		FirstSeatTime = Time;
	}
	// Cada llegada da unos segundos más para que la siguiente se siente, con tope desde la primera.
	const double Wanted = FMath::Max<double>(RallyState->PhaseEndServerTime, Time + WarmupSeconds);
	RallyState->PhaseEndServerTime = static_cast<float>(FMath::Min(Wanted, FirstSeatTime + WarmupMaxSeconds));
	RallyState->ForceNetUpdate();
}

// ---- Fases ----

void ATN_RallyGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ATN_RallyGameState* RallyState = GetRallyGameState();
	if (!bTrackReady || !RallyState)
	{
		return;
	}
	UpdatePhase();
	const ETNRallyPhase Phase = RallyState->Phase;
	if (Phase == ETNRallyPhase::Countdown)
	{
		CheckEarlyStarts();
	}
	const bool bRacing = Phase == ETNRallyPhase::Racing || Phase == ETNRallyPhase::Finishing;
	ConsumeRespawnRequests(bRacing);
	if (bRacing)
	{
		TickProgress();
	}
	EvaluateAccumulator += DeltaSeconds;
	if (EvaluateAccumulator >= EvaluateInterval)
	{
		if (bRacing)
		{
			EvaluateTeams(EvaluateAccumulator);
		}
		RebuildStandings();
		EvaluateAccumulator = 0.0;
	}
}

void ATN_RallyGameMode::UpdatePhase()
{
	ATN_RallyGameState* RallyState = GetRallyGameState();
	const double Time = Now();
	switch (RallyState->Phase)
	{
	case ETNRallyPhase::Warmup:
		if (RallyState->PhaseEndServerTime > 0.f && Time >= RallyState->PhaseEndServerTime && Teams.Num() > 0)
		{
			StartCountdown();
		}
		break;
	case ETNRallyPhase::Countdown:
		if (Time >= RallyState->StartServerTime)
		{
			StartRacing();
		}
		break;
	case ETNRallyPhase::Racing:
		if (!Teams.ContainsByPredicate([](const FTeamRuntime& Team) { return !Team.bRetired; }))
		{
			StartResults();
		}
		else if (RaceTimeoutSeconds > 0.f && Time >= RallyState->StartServerTime + RaceTimeoutSeconds)
		{
			UE_LOG(LogTNRally, Warning, TEXT("[RallyGameMode] Tope de %.0f s sin nadie en meta: resultados."), RaceTimeoutSeconds);
			StartResults();
		}
		break;
	case ETNRallyPhase::Finishing:
		if (Time >= RallyState->PhaseEndServerTime
			|| !Teams.ContainsByPredicate([](const FTeamRuntime& Team) { return !Team.bRetired && !Team.bFinished; }))
		{
			StartResults();
		}
		break;
	case ETNRallyPhase::Results:
		if (Time >= RallyState->PhaseEndServerTime && !bRestartRequested)
		{
			if (RaceLimit > 0 && TNRallyGameModeStats::RacesRun >= RaceLimit)
			{
				bRestartRequested = true;
				UE_LOG(LogTNRally, Log, TEXT("[RallyStats] %d carreras hechas (?Races=%d): fin."), TNRallyGameModeStats::RacesRun, RaceLimit);
				FPlatformMisc::RequestExit(false, TEXT("TN Rally ?Races"));
				break;
			}
			// ?Restart reutiliza la URL actual: mismo mapa y mismas opciones (?Variant, ?Seats, ?Bots, ?Laps).
			bRestartRequested = true;
			UE_LOG(LogTNRally, Log, TEXT("[RallyGameMode] Carrera nueva en el mismo mapa."));
			GetWorld()->ServerTravel(TEXT("?Restart"), false);
		}
		break;
	}
}

void ATN_RallyGameMode::StartCountdown()
{
	ATN_RallyGameState* RallyState = GetRallyGameState();
	// Parrilla: jugadoras delante por orden de llegada y la IA detrás.
	Teams.StableSort([](const FTeamRuntime& A, const FTeamRuntime& B) { return !A.bBot && B.bBot; });
	for (int32 Index = 0; Index < Teams.Num(); ++Index)
	{
		FTeamRuntime& Team = Teams[Index];
		Team.GridSlot = Index;
		Team.GridTransform = Track->GetGridSlotTransform(Index);
		if (ITN_RallyVehicle* RallyVehicle = Cast<ITN_RallyVehicle>(Team.Vehicle.Get()))
		{
			RallyVehicle->RallyTeleport(Team.GridTransform, 0.f, 0.f);
		}
		Team.PrevLocation = Team.GridTransform.GetLocation();
		Team.Arc = Track->FindArcGlobal(Team.PrevLocation);
		Team.bEarlyPenalized = false;
	}
	RallyState->Phase = ETNRallyPhase::Countdown;
	RallyState->StartServerTime = static_cast<float>(Now() + CountdownSeconds);
	RallyState->PhaseEndServerTime = RallyState->StartServerTime;
	// Motor libre durante el semáforo: quien sale antes del verde vuelve a su hueco con el motor cortado hasta 1 s
	// después de la salida (CheckEarlyStarts).
	SetAllEnginesLocked(false);
	RebuildStandings();
	RallyState->ForceNetUpdate();
	UE_LOG(LogTNRally, Log, TEXT("[RallyGameMode] Semáforo: %d buggies, salida a los %.1f s."), Teams.Num(), CountdownSeconds);
}

void ATN_RallyGameMode::StartRacing()
{
	ATN_RallyGameState* RallyState = GetRallyGameState();
	RallyState->Phase = ETNRallyPhase::Racing;
	RallyState->PhaseEndServerTime = 0.f;
	for (FTeamRuntime& Team : Teams)
	{
		ITN_RallyVehicle* RallyVehicle = Cast<ITN_RallyVehicle>(Team.Vehicle.Get());
		if (RallyVehicle && !Team.bRetired && !Team.bEarlyPenalized)
		{
			RallyVehicle->SetEngineLocked(false);
		}
		if (const APawn* Vehicle = Team.Vehicle.Get())
		{
			Team.PrevLocation = Vehicle->GetActorLocation();
		}
		Team.OdometerCm = 0.0;
	}
	RallyState->ForceNetUpdate();
	UE_LOG(LogTNRally, Log, TEXT("[RallyGameMode] ¡Salida!"));
}

void ATN_RallyGameMode::StartFinishing()
{
	ATN_RallyGameState* RallyState = GetRallyGameState();
	RallyState->Phase = ETNRallyPhase::Finishing;
	RallyState->PhaseEndServerTime = static_cast<float>(Now() + FinishGraceSeconds);
	RallyState->ForceNetUpdate();
	UE_LOG(LogTNRally, Log, TEXT("[RallyGameMode] Primer buggy en meta: quedan %.0f s."), FinishGraceSeconds);
}

void ATN_RallyGameMode::StartResults()
{
	ATN_RallyGameState* RallyState = GetRallyGameState();
	if (RallyState->Phase != ETNRallyPhase::Results)
	{
		++TNRallyGameModeStats::RacesRun;
		const bool bTimedOut = RallyState->Phase == ETNRallyPhase::Racing && RaceTimeoutSeconds > 0.f
			&& Now() >= RallyState->StartServerTime + RaceTimeoutSeconds;
		LogRaceStats(bTimedOut);
	}
	RallyState->Phase = ETNRallyPhase::Results;
	RallyState->PhaseEndServerTime = static_cast<float>(Now() + ResultsSeconds);
	SetAllEnginesLocked(true);
	RebuildStandings();
	RallyState->ForceNetUpdate();
	UE_LOG(LogTNRally, Log, TEXT("[RallyGameMode] Resultados.\n%s"), *RallyState->DescribeStatus());
}

void ATN_RallyGameMode::SetAllEnginesLocked(bool bLocked)
{
	for (const FTeamRuntime& Team : Teams)
	{
		if (ITN_RallyVehicle* RallyVehicle = Cast<ITN_RallyVehicle>(Team.Vehicle.Get()))
		{
			RallyVehicle->SetEngineLocked(bLocked || Team.bRetired);
		}
	}
}
