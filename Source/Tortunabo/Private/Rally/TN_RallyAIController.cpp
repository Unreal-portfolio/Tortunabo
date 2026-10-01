#include "Rally/TN_RallyAIController.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Rally/TN_RallyGameState.h"
#include "Rally/TN_RallyLogic.h"
#include "Rally/TN_RallyTrack.h"
#include "Rally/TN_RallyVehicle.h"

namespace
{
	/** Distancia extra hasta la tangente con la que se mide la curva que viene (cm) y segundos de velocidad que se suman. */
	constexpr double RallyAICornerProbeCm = 3000.0;
	constexpr double RallyAICornerProbeSeconds = 1.0;
	/** Con el acelerador a fondo y a menos de esta velocidad durante RallyAIStuckSeconds, marcha atrás RallyAIReverseSeconds. */
	constexpr float RallyAIStuckKmh = 3.f;
	constexpr float RallyAIStuckSeconds = 2.f;
	constexpr double RallyAIReverseSeconds = 1.5;
	/** Un salto mayor es una reaparición: el arco se vuelve a buscar en toda la pista. */
	constexpr double RallyAIJumpCm = 3000.0;
	/** Coseno del cono de disparo hacia delante. */
	constexpr double RallyAIFireConeCos = 0.6;
}

ATN_RallyAIController::ATN_RallyAIController()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 1.f / 30.f;
}

void ATN_RallyAIController::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	// PlayerState propio (como AAIController con bWantsPlayerState): nombre en la tabla de puestos y en el HUD.
	if (IsValid(this) && GetNetMode() != NM_Client)
	{
		InitPlayerState();
		if (PlayerState)
		{
			PlayerState->SetIsABot(true);
		}
	}
}

ATN_RallyTrack* ATN_RallyAIController::ResolveTrack()
{
	if (!CachedTrack.IsValid())
	{
		const ATN_RallyGameState* RallyState = GetWorld() ? GetWorld()->GetGameState<ATN_RallyGameState>() : nullptr;
		CachedTrack = RallyState ? RallyState->GetTrack() : nullptr;
	}
	return CachedTrack.Get();
}

void ATN_RallyAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ITN_RallyVehicle* RallyVehicle = Cast<ITN_RallyVehicle>(GetPawn());
	const ATN_RallyGameState* RallyState = GetWorld() ? GetWorld()->GetGameState<ATN_RallyGameState>() : nullptr;
	ATN_RallyTrack* Track = ResolveTrack();
	if (!HasAuthority() || !RallyVehicle || !RallyState || !Track || !Track->IsBuilt())
	{
		return;
	}
	if (RallyState->Phase != ETNRallyPhase::Racing && RallyState->Phase != ETNRallyPhase::Finishing)
	{
		RallyVehicle->SetAIDriveInput(0.f, 1.f, 0.f, true);
		return;
	}
	Drive(DeltaSeconds, *Track);
}

void ATN_RallyAIController::Drive(float DeltaSeconds, ATN_RallyTrack& Track)
{
	APawn* Vehicle = GetPawn();
	ITN_RallyVehicle* RallyVehicle = Cast<ITN_RallyVehicle>(Vehicle);
	const FVector Location = Vehicle->GetActorLocation();
	const FVector Forward = Vehicle->GetActorForwardVector();
	if (!bHasArc || FVector::Dist(Location, LastLocation) > RallyAIJumpCm)
	{
		Arc = Track.FindArcGlobal(Location);
		bHasArc = true;
	}
	else
	{
		Arc = Track.FindArcNear(Location, Arc);
	}
	LastLocation = Location;

	if (RallyVehicle->IsFlipped())
	{
		// Volcado: el buggy se endereza solo a los 4 s.
		RallyVehicle->SetAIDriveInput(0.f, 0.f, 0.f, false);
		return;
	}

	const double SpeedCms = FMath::Abs(RallyVehicle->GetForwardSpeedCms());
	const float SpeedKmh = static_cast<float>(TNRally::CmsToKmh(SpeedCms));
	const FVector Target = Track.GetLocationAtArc(Arc + LookAheadBaseCm + SpeedCms * LookAheadSeconds);
	const float Steer = TNRally::SteerToward(Forward, Target - Location, SteerSaturationDeg);
	const float TargetKmh = TNRally::CornerSpeedKmh(Track.GetDirectionAtArc(Arc),
		Track.GetDirectionAtArc(Arc + RallyAICornerProbeCm + SpeedCms * RallyAICornerProbeSeconds), MaxSpeedKmh, MinCornerSpeedKmh);

	const double Time = GetWorld()->GetTimeSeconds();
	if (Time < ReverseUntil)
	{
		// Marcha atrás con la dirección invertida (en Chaos, frenar parado da marcha atrás).
		RallyVehicle->SetAIDriveInput(0.f, 1.f, -Steer, false);
		return;
	}
	const float Throttle = SpeedKmh < TargetKmh ? 1.f : 0.25f;
	const float Brake = SpeedKmh > TargetKmh + 12.f ? 0.8f : 0.f;
	SlowSeconds = (Throttle > 0.5f && SpeedKmh < RallyAIStuckKmh) ? SlowSeconds + DeltaSeconds : 0.f;
	if (SlowSeconds > RallyAIStuckSeconds)
	{
		SlowSeconds = 0.f;
		ReverseUntil = Time + RallyAIReverseSeconds;
	}
	RallyVehicle->SetAIDriveInput(Throttle, Brake, Steer, false);
	TryFire(Location, Forward);
}

void ATN_RallyAIController::TryFire(const FVector& Location, const FVector& Forward)
{
	const double Time = GetWorld()->GetTimeSeconds();
	const ATN_RallyGameState* RallyState = GetWorld()->GetGameState<ATN_RallyGameState>();
	ITN_RallyVehicle* RallyVehicle = Cast<ITN_RallyVehicle>(GetPawn());
	if (Time < NextFireTime || !RallyState || !RallyVehicle)
	{
		return;
	}
	const FTNRallyStanding* Mine = RallyState->FindStandingForVehicle(GetPawn());
	if (!Mine || Mine->bFinished || Mine->Place <= 1 || !RallyState->Standings.IsValidIndex(Mine->Place - 2))
	{
		return;
	}
	const APawn* Ahead = RallyState->Standings[Mine->Place - 2].Vehicle;
	if (!Ahead)
	{
		return;
	}
	const FVector ToTarget = Ahead->GetActorLocation() - Location;
	const FVector Direction = ToTarget.GetSafeNormal();
	if (ToTarget.SizeSquared() > FMath::Square(FireRangeCm) || (Direction | Forward) < RallyAIFireConeCos)
	{
		return;
	}
	const bool bSpecial = RallyVehicle->GetSpecialAmmo() != ETNRallyAmmo::None && FMath::FRand() < SpecialFireChance;
	RallyVehicle->AIFire(Direction, bSpecial);
	NextFireTime = Time + FireIntervalSeconds;
}
