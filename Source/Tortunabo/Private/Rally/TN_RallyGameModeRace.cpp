// ATN_RallyGameMode: progreso de la carrera en el servidor (salida anticipada, puertas, cajas, contramano, reaparición y
// puestos). Las reglas son las de TNRally (TN_RallyLogic.h).
#include "Rally/TN_RallyGameMode.h"

#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"
#include "Rally/TN_RallyAmmoBox.h"
#include "Rally/TN_RallyPlayerState.h"
#include "Rally/TN_RallyTrack.h"
#include "Rally/TN_RallyVehicle.h"
#include "World/TN_DeathZoneVolume.h"

namespace TNRallyGameModeStats
{
	/** Definido en TN_RallyGameMode.cpp. */
	extern int32 RacesRun;
}

namespace
{
	/** Un salto mayor entre dos fotogramas es un teletransporte: no suma al recorrido ni cruza puertas. */
	constexpr double RallyTeleportJumpCm = 5000.0;
	/** Antes de la salida el progreso se mide desde 100 m detrás de la puerta 0 (parrilla). */
	constexpr double RallyPreStartRefCm = 10000.0;
	/** Margen bajo la cota del agua para reaparecer. */
	constexpr double RallyWaterMarginCm = 50.0;
}

void ATN_RallyGameMode::ConsumeRespawnRequests(bool bRacing)
{
	const double Time = Now();
	for (FTeamRuntime& Team : Teams)
	{
		ITN_RallyVehicle* RallyVehicle = Cast<ITN_RallyVehicle>(Team.Vehicle.Get());
		if (!RallyVehicle)
		{
			continue;
		}
		// Bajo el KillZ: vuelve a la pista en cualquier fase (al último arco o, antes de la salida, a su hueco).
		if (RallyVehicle->ConsumeFellOutOfWorld())
		{
			RallyVehicle->ConsumeRespawnRequest();
			RespawnTeam(Team, ERespawnReason::Hazard);
			continue;
		}
		// Se consume siempre: una petición de antes del verde no se guarda para la carrera.
		if (!RallyVehicle->ConsumeRespawnRequest())
		{
			continue;
		}
		if (bRacing && !Team.bFinished && !Team.bRetired && Time >= Team.ImmuneUntil)
		{
			RespawnTeam(Team, ERespawnReason::Request);
		}
	}
}

bool ATN_RallyGameMode::RollAmmoFor(AActor* Vehicle, ETNRallyAmmo& OutAmmo, int32& OutCharges) const
{
	const FTeamRuntime* Team = FindTeamByVehicle(Vehicle);
	const int32 Active = FMath::Max(1, Teams.FilterByPredicate([](const FTeamRuntime& Entry) { return !Entry.bRetired; }).Num());
	const int32 Place = Team && Team->Place > 0 ? Team->Place : 1;
	OutAmmo = TNRally::PickAmmo(TNRally::AmmoWeightsForPlace(Place, Active), FMath::FRand());
	OutCharges = TNRally::ChargesFor(OutAmmo);
	return OutCharges > 0;
}

void ATN_RallyGameMode::CheckEarlyStarts()
{
	const ATN_RallyGameState* RallyState = GetRallyGameState();
	for (FTeamRuntime& Team : Teams)
	{
		ITN_RallyVehicle* RallyVehicle = Cast<ITN_RallyVehicle>(Team.Vehicle.Get());
		if (Team.bEarlyPenalized || !RallyVehicle)
		{
			continue;
		}
		const FVector Moved = Team.Vehicle->GetActorLocation() - Team.GridTransform.GetLocation();
		if ((Moved | Team.GridTransform.GetRotation().GetForwardVector()) <= TNRally::EarlyStartDisplacementCm)
		{
			continue;
		}
		// Salida anticipada: de vuelta al hueco y motor cortado hasta 1 s después del verde.
		Team.bEarlyPenalized = true;
		Team.EngineUnlockAt = RallyState->StartServerTime + EarlyStartPenaltySeconds;
		RallyVehicle->RallyTeleport(Team.GridTransform, 0.f, 0.f);
		RallyVehicle->SetEngineLocked(true);
		Team.PrevLocation = Team.GridTransform.GetLocation();
		UE_LOG(LogTNRally, Log, TEXT("[RallyGameMode] Salida anticipada del equipo %d: motor cortado hasta %.1f s después del verde."),
			Team.TeamIndex, EarlyStartPenaltySeconds);
	}
}

void ATN_RallyGameMode::TickProgress()
{
	const TNRally::FLapRules Rules = MakeLapRules();
	const double Time = Now();
	for (FTeamRuntime& Team : Teams)
	{
		APawn* Vehicle = Team.Vehicle.Get();
		ITN_RallyVehicle* RallyVehicle = Cast<ITN_RallyVehicle>(Vehicle);
		if (!RallyVehicle || Team.bRetired)
		{
			continue;
		}
		if (Team.EngineUnlockAt > 0.0 && Time >= Team.EngineUnlockAt)
		{
			Team.EngineUnlockAt = 0.0;
			RallyVehicle->SetEngineLocked(false);
		}
		const FVector Current = Vehicle->GetActorLocation();
		const FVector Previous = Team.PrevLocation;
		Team.PrevLocation = Current;
		if (Team.bFinished || FVector::DistSquared(Previous, Current) > FMath::Square(RallyTeleportJumpCm))
		{
			continue;
		}
		Team.OdometerCm += FVector::Dist(Previous, Current);

		const int32 NextGate = Rules.NextGateIndex(Team.GatesPassed);
		double Alpha = 0.0;
		bool bForward = false;
		if (TNRally::SegmentCrossesGate(Previous, Current, Track->GetGateCrossingTransform(NextGate), Track->GetGateHalfExtent(), Alpha, bForward))
		{
			HandleGateCrossing(Team, NextGate, bForward, Alpha, GetWorld()->GetDeltaSeconds());
		}
		CheckAmmoBoxes(Team, Previous, Current);
	}
}

void ATN_RallyGameMode::HandleGateCrossing(FTeamRuntime& Team, int32 GateIndex, bool bForward, double Alpha, float DeltaSeconds)
{
	const TNRally::FLapRules Rules = MakeLapRules();
	const double SplineBetween = Team.LastGate == INDEX_NONE ? 0.0 : Track->GetArcBetweenGates(Team.LastGate, GateIndex);
	const TNRally::EGateCheck Check = TNRally::CheckGate(GateIndex, Rules.NextGateIndex(Team.GatesPassed), bForward, Team.OdometerCm,
		SplineBetween, Team.GatesPassed > 0);
	if (Check != TNRally::EGateCheck::Valid)
	{
		UE_LOG(LogTNRally, Log, TEXT("[RallyGameMode] Equipo %d: puerta %d no cuenta (%s; recorrido %.0f m de %.0f m)."),
			Team.TeamIndex, GateIndex, Check == TNRally::EGateCheck::Shortcut ? TEXT("atajo") : TEXT("sentido contrario"),
			Team.OdometerCm / 100.0, SplineBetween / 100.0);
		return;
	}
	++Team.GatesPassed;
	Team.LastGate = GateIndex;
	Team.OdometerCm = 0.0;
	if (Rules.IsFinished(Team.GatesPassed))
	{
		ATN_RallyGameState* RallyState = GetRallyGameState();
		// Hora del cruce interpolada dentro del fotograma.
		const double CrossTime = Now() - (1.0 - Alpha) * DeltaSeconds;
		Team.bFinished = true;
		Team.FinishSeconds = FMath::Max(0.0, CrossTime - RallyState->StartServerTime);
		Team.bWrongWay = false;
		UE_LOG(LogTNRally, Log, TEXT("[RallyGameMode] Equipo %d en meta: %.2f s."), Team.TeamIndex, Team.FinishSeconds);
		if (RallyState->Phase == ETNRallyPhase::Racing)
		{
			StartFinishing();
		}
	}
	RebuildStandings();
}

void ATN_RallyGameMode::CheckAmmoBoxes(FTeamRuntime& Team, const FVector& From, const FVector& To)
{
	for (ATN_RallyAmmoBox* Box : Track->GetAmmoBoxes())
	{
		if (IsValid(Box) && Box->IsAvailable()
			&& FMath::PointDistToSegment(Box->GetActorLocation(), From, To) <= Box->PickupRadiusCm)
		{
			Box->TryCollect(Team.Vehicle.Get());
		}
	}
}

void ATN_RallyGameMode::EvaluateTeams(double DeltaSeconds)
{
	for (FTeamRuntime& Team : Teams)
	{
		EvaluateTeam(Team, DeltaSeconds);
	}
}

void ATN_RallyGameMode::EvaluateTeam(FTeamRuntime& Team, double DeltaSeconds)
{
	const APawn* Vehicle = Team.Vehicle.Get();
	if (!Vehicle || Team.bRetired || Team.bFinished)
	{
		return;
	}
	const TNRally::FLapRules Rules = MakeLapRules();
	const double Length = Track->GetTrackLengthCm();
	const bool bClosed = Track->IsCircuit();
	const FVector Location = Vehicle->GetActorLocation();
	Team.Arc = Track->FindArcNear(Location, Team.Arc);

	// Arco recorrido desde la última puerta (desempate dentro de la misma puerta).
	const bool bStarted = Team.LastGate != INDEX_NONE;
	const double Reference = bStarted ? Track->GetGateArc(Team.LastGate) : Track->GetGateArc(0) - RallyPreStartRefCm;
	const double SegmentLength = bStarted
		? Track->GetArcBetweenGates(Team.LastGate, Rules.NextGateIndex(Team.GatesPassed)) : RallyPreStartRefCm;
	const double Progress = TNRally::ForwardArc(bClosed ? TNRally::WrapArc(Reference, Length, true) : Reference, Team.Arc, Length, bClosed);
	Team.SegmentProgressCm = Progress > SegmentLength + RallyTeleportJumpCm ? 0.0 : Progress;

	const ITN_RallyVehicle* RallyVehicle = Cast<ITN_RallyVehicle>(Vehicle);
	const bool bFlipped = RallyVehicle && RallyVehicle->IsFlipped();
	if (bFlipped && !Team.bWasFlipped)
	{
		++Team.Flips;
		UE_LOG(LogTNRally, Log, TEXT("[RallyGameMode] Equipo %d volcado en el arco %.0f m (%.0f km/h) en (%.0f, %.0f, %.0f)."),
			Team.TeamIndex, Team.Arc / 100.0, TNRally::CmsToKmh(Vehicle->GetVelocity().Size()), Location.X, Location.Y, Location.Z);
	}
	Team.bWasFlipped = bFlipped;

	if (Now() < Team.ImmuneUntil)
	{
		return;
	}
	const FVector Velocity = Vehicle->GetVelocity();
	const FVector Tangent = Track->GetDirectionAtArc(Team.Arc);
	switch (TNRally::UpdateWrongWay(Team.WrongWay, Velocity.GetSafeNormal() | Tangent, TNRally::CmsToKmh(Velocity.Size()), DeltaSeconds))
	{
	case TNRally::EWrongWayEvent::WarningOn:
		Team.bWrongWay = true;
		break;
	case TNRally::EWrongWayEvent::WarningOff:
		Team.bWrongWay = false;
		break;
	case TNRally::EWrongWayEvent::TurnAround:
		TurnAround(Team);
		return;
	default:
		break;
	}
	if (IsInHazard(Location))
	{
		RespawnTeam(Team, ERespawnReason::Hazard);
		return;
	}
	if (TNRally::UpdateOffTrack(Team.OffTrack, FVector::Dist(Location, Track->GetLocationAtArc(Team.Arc)), DeltaSeconds))
	{
		RespawnTeam(Team, ERespawnReason::OffTrack);
		return;
	}
	if (TNRally::UpdateStuck(Team.Stuck, Location, DeltaSeconds))
	{
		RespawnTeam(Team, ERespawnReason::Stuck);
	}
}

bool ATN_RallyGameMode::IsInHazard(const FVector& Location) const
{
	if (Track->HasWaterZ() && Location.Z < Track->GetWaterZ() - RallyWaterMarginCm)
	{
		return true;
	}
	// Las cajas de muerte del manifest (kill_boxes_uu) y las que haya en el nivel: dentro = reaparición (el volumen solo
	// sabe matar tortugas a pie a través de ATN_RunGameMode).
	for (TActorIterator<ATN_DeathZoneVolume> It(GetWorld()); It; ++It)
	{
		const UBoxComponent* Box = It->FindComponentByClass<UBoxComponent>();
		if (!Box)
		{
			continue;
		}
		const FVector Local = Box->GetComponentTransform().InverseTransformPosition(Location);
		const FVector Extent = Box->GetUnscaledBoxExtent();
		if (FMath::Abs(Local.X) <= Extent.X && FMath::Abs(Local.Y) <= Extent.Y && FMath::Abs(Local.Z) <= Extent.Z)
		{
			return true;
		}
	}
	return false;
}

void ATN_RallyGameMode::RespawnTeam(FTeamRuntime& Team, ERespawnReason Reason)
{
	ITN_RallyVehicle* RallyVehicle = Cast<ITN_RallyVehicle>(Team.Vehicle.Get());
	if (!RallyVehicle)
	{
		return;
	}
	const bool bStarted = Team.LastGate != INDEX_NONE;
	const FTransform Where = bStarted ? Track->GetRespawnTransform(Team.LastGate, Team.TeamIndex) : Team.GridTransform;
	if (const APawn* Vehicle = Team.Vehicle.Get())
	{
		const FVector From = Vehicle->GetActorLocation();
		const FVector To = Where.GetLocation();
		UE_LOG(LogTNRally, Verbose, TEXT("[RallyGameMode] Equipo %d: estaba en (%.0f, %.0f, %.0f) a %.0f km/h con arriba.Z %.2f; va a (%.0f, %.0f, %.0f)."),
			Team.TeamIndex, From.X, From.Y, From.Z, RallyVehicle->GetForwardSpeedCms() * 0.036, Vehicle->GetActorUpVector().Z, To.X, To.Y, To.Z);
	}
	RallyVehicle->RallyTeleport(Where, RespawnLockSeconds, RespawnGhostSeconds);

	const double Time = Now();
	Team.RespawnEndTime = Time + RespawnLockSeconds;
	Team.ImmuneUntil = Team.RespawnEndTime + 0.5;
	Team.Arc = bStarted ? Track->GetGateArc(Team.LastGate) : Track->FindArcGlobal(Where.GetLocation());
	Team.PrevLocation = Where.GetLocation();
	Team.OdometerCm = 0.0;
	Team.bWrongWay = false;
	Team.WrongWay = TNRally::FWrongWayState();
	Team.Stuck = TNRally::FStuckState();
	Team.OffTrack = TNRally::FOffTrackState();

	++Team.Respawns[static_cast<uint8>(Reason)];
	static const TCHAR* ReasonNames[] = { TEXT("zona de muerte o agua"), TEXT("fuera de pista"), TEXT("atasco"), TEXT("petición") };
	UE_LOG(LogTNRally, Log, TEXT("[RallyGameMode] Equipo %d reaparece en la puerta %d (%s) desde el arco %.0f m."), Team.TeamIndex,
		Team.LastGate, ReasonNames[static_cast<uint8>(Reason)], Team.Arc / 100.0);
	RebuildStandings();
}

void ATN_RallyGameMode::TurnAround(FTeamRuntime& Team)
{
	APawn* Vehicle = Team.Vehicle.Get();
	ITN_RallyVehicle* RallyVehicle = Cast<ITN_RallyVehicle>(Vehicle);
	if (!RallyVehicle)
	{
		return;
	}
	// En el sitio, mirando hacia la tangente, sin espera (solo 1 s de fantasma).
	const FVector Location = Vehicle->GetActorLocation() + FVector(0.0, 0.0, 50.0);
	const FRotator Facing(0.0, Track->GetDirectionAtArc(Team.Arc).Rotation().Yaw, 0.0);
	RallyVehicle->RallyTeleport(FTransform(Facing, Location), 0.f, TurnAroundGhostSeconds);
	Team.ImmuneUntil = Now() + TurnAroundGhostSeconds;
	Team.PrevLocation = Location;
	Team.bWrongWay = false;
	++Team.TurnArounds;
	UE_LOG(LogTNRally, Log, TEXT("[RallyGameMode] Equipo %d girado: 4 s en contramano."), Team.TeamIndex);
}

void ATN_RallyGameMode::LogRaceStats(bool bTimedOut) const
{
	int32 Finished = 0;
	int32 Flips = 0;
	int32 TurnArounds = 0;
	int32 Respawns[4] = { 0, 0, 0, 0 };
	double Winner = 0.0;
	for (const FTeamRuntime& Team : Teams)
	{
		Finished += Team.bFinished ? 1 : 0;
		Flips += Team.Flips;
		TurnArounds += Team.TurnArounds;
		for (int32 Reason = 0; Reason < 4; ++Reason)
		{
			Respawns[Reason] += Team.Respawns[Reason];
		}
		if (Team.bFinished && (Winner <= 0.0 || Team.FinishSeconds < Winner))
		{
			Winner = Team.FinishSeconds;
		}
	}
	UE_LOG(LogTNRally, Log,
		TEXT("[RallyStats] carrera %d variante %s: terminados %d/%d, atascos %d, vuelcos %d, caidas %d, fuera_de_pista %d, peticiones %d, giros %d, ganador %.1f s%s"),
		TNRallyGameModeStats::RacesRun, *Variant.ToString(), Finished, Teams.Num(), Respawns[static_cast<uint8>(ERespawnReason::Stuck)], Flips,
		Respawns[static_cast<uint8>(ERespawnReason::Hazard)], Respawns[static_cast<uint8>(ERespawnReason::OffTrack)],
		Respawns[static_cast<uint8>(ERespawnReason::Request)], TurnArounds, Winner, bTimedOut ? TEXT(" (tope de tiempo)") : TEXT(""));
	for (const FTeamRuntime& Team : Teams)
	{
		if (!Team.bFinished)
		{
			UE_LOG(LogTNRally, Log, TEXT("[RallyStats]   sin llegar: equipo %d con %d puertas, arco %.0f m%s"), Team.TeamIndex,
				Team.GatesPassed, Team.Arc / 100.0, Team.bRetired ? TEXT(", retirado") : TEXT(""));
		}
	}
}

void ATN_RallyGameMode::RefreshSeats()
{
	ATN_RallyGameState* RallyState = GetRallyGameState();
	for (APlayerState* Player : RallyState->PlayerArray)
	{
		ATN_RallyPlayerState* RallyPlayer = Cast<ATN_RallyPlayerState>(Player);
		const AController* OwnerController = RallyPlayer ? Cast<AController>(RallyPlayer->GetOwner()) : nullptr;
		if (!RallyPlayer)
		{
			continue;
		}
		bool bSeated = false;
		for (const FTeamRuntime& Team : Teams)
		{
			const ITN_RallyVehicle* RallyVehicle = Cast<ITN_RallyVehicle>(Team.Vehicle.Get());
			if (!RallyVehicle || !OwnerController)
			{
				continue;
			}
			for (const ETNRallySeat Seat : { ETNRallySeat::Driver, ETNRallySeat::Gunner })
			{
				if (RallyVehicle->GetSeatController(Seat) == OwnerController)
				{
					RallyPlayer->SetRallySeat(Team.TeamIndex, Seat);
					bSeated = true;
				}
			}
		}
		if (!bSeated)
		{
			RallyPlayer->ClearRallySeat();
		}
	}
}

void ATN_RallyGameMode::RebuildStandings()
{
	ATN_RallyGameState* RallyState = GetRallyGameState();
	if (!RallyState || !Track)
	{
		return;
	}
	RefreshSeats();
	const TNRally::FLapRules Rules = MakeLapRules();
	// Un equipo sin buggy (retirado por CleanupTeams) no sale en los puestos.
	TArray<int32> Listed;
	TArray<TNRally::FStandingKey> Keys;
	Keys.Reserve(Teams.Num());
	for (int32 TeamSlot = 0; TeamSlot < Teams.Num(); ++TeamSlot)
	{
		const FTeamRuntime& Team = Teams[TeamSlot];
		if (!Team.Vehicle.IsValid())
		{
			continue;
		}
		Listed.Add(TeamSlot);
		TNRally::FStandingKey Key;
		Key.Id = Team.TeamIndex;
		Key.bFinished = Team.bFinished;
		Key.FinishTime = Team.FinishSeconds;
		Key.Lap = Rules.LapForGates(Team.GatesPassed);
		Key.GatesPassed = Team.GatesPassed;
		Key.SegmentProgressCm = Team.SegmentProgressCm;
		Key.bRetired = Team.bRetired;
		Keys.Add(Key);
	}
	const TArray<int32> Order = TNRally::SortStandings(Keys);
	const double Time = Now();
	TArray<FTNRallyStanding> Standings;
	Standings.Reserve(Order.Num());
	for (int32 Rank = 0; Rank < Order.Num(); ++Rank)
	{
		FTeamRuntime& Team = Teams[Listed[Order[Rank]]];
		Team.Place = Rank + 1;
		const ITN_RallyVehicle* RallyVehicle = Cast<ITN_RallyVehicle>(Team.Vehicle.Get());
		const AController* Driver = RallyVehicle ? RallyVehicle->GetSeatController(ETNRallySeat::Driver) : nullptr;
		const AController* Gunner = RallyVehicle ? RallyVehicle->GetSeatController(ETNRallySeat::Gunner) : nullptr;
		FTNRallyStanding& Entry = Standings.AddDefaulted_GetRef();
		Entry.TeamIndex = Team.TeamIndex;
		Entry.Vehicle = Team.Vehicle.Get();
		Entry.Driver = Driver ? Driver->PlayerState : nullptr;
		Entry.Gunner = Gunner ? Gunner->PlayerState : nullptr;
		Entry.Place = Team.Place;
		Entry.Lap = Rules.LapForGates(Team.GatesPassed);
		Entry.NextGate = Rules.NextGateIndex(Team.GatesPassed);
		Entry.FinishSeconds = static_cast<float>(Team.FinishSeconds);
		Entry.bFinished = Team.bFinished;
		Entry.bRetired = Team.bRetired;
		Entry.bWrongWay = Team.bWrongWay;
		Entry.bBot = Team.bBot;
		Entry.RespawnEndServerTime = Team.RespawnEndTime > Time ? static_cast<float>(Team.RespawnEndTime) : 0.f;
		Entry.Points = TNRally::PointsForPlace(Team.Place, Team.bFinished);
	}
	RallyState->Standings = MoveTemp(Standings);
}
