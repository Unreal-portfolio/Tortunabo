// ATN_Buggy: control de estabilidad sin freno de mano (#288) y turbo (#294). La barra la gasta y la recarga el servidor; el
// par y el empuje se aplican en cada máquina que simula el chasis (como el antivuelco), y la llama y el sonido, en cada
// máquina con pantalla a partir del estado replicado.

#include "Vehicles/TN_Buggy.h"
#include "Vehicles/TN_BuggyData.h"
#include "Vehicles/TN_RallyTurretLogic.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Components/AudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"

void ATN_Buggy::UpdateAirborne()
{
	const UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement();
	if (!Move || !Move->HasValidPhysicsState())
	{
		bAirborne = false;
		return;
	}
	bool bAnyContact = false;
	for (int32 Index = 0; Index < Move->Wheels.Num() && !bAnyContact; ++Index)
	{
		bAnyContact = Move->GetWheelState(Index).bInContact;
	}
	bAirborne = !bAnyContact;
}

void ATN_Buggy::ApplyStability()
{
	USkeletalMeshComponent* Chassis = GetMesh();
	if (!Chassis->IsSimulatingPhysics())
	{
		return;
	}
	const UTN_BuggyData* BuggyData = GetData();
	TNBuggy::FStabilityTuning Tuning;
	Tuning.StartSlipDeg = BuggyData->StabilityStartSlipDeg;
	Tuning.Stiffness = BuggyData->StabilityStiffness;
	Tuning.Damping = BuggyData->StabilityDamping;
	Tuning.MaxAccel = BuggyData->StabilityMaxAccel;
	if (Tuning.Stiffness <= 0.f)
	{
		return;
	}
	const FVector Up = GetActorUpVector();
	const FVector Velocity = GetVelocity();
	const float Slip = TNBuggy::SlipAngleDeg(GetActorForwardVector(), Velocity);
	const float YawRate = static_cast<float>(Chassis->GetPhysicsAngularVelocityInRadians() | Up);
	const float Flat = static_cast<float>(FVector(Velocity.X, Velocity.Y, 0.f).Size());
	const float Accel = TNBuggy::StabilityYawAccel(Slip, YawRate, Flat, bHandbrakeHeld, bAirborne, Tuning);
	if (Accel != 0.f)
	{
		// Como aceleración (bAccelChange): igual para cualquier inercia del chasis.
		Chassis->AddTorqueInRadians(Up * Accel, NAME_None, true);
	}
}

bool ATN_Buggy::IsBoosting() const
{
	// Con el motor cortado o el freno de carrera puesto no hay turbo en ninguna máquina, aunque bBoostActive aún no haya
	// llegado (en los clientes llega con retraso).
	if (IsEngineLocked() || bRaceBrakeHeld)
	{
		return false;
	}
	// La conductora local (cliente) no recibe bBoostActive: lo predice con la misma regla que el servidor.
	if (IsLocallyControlled() && !HasAuthority())
	{
		return bBoostHeld && BoostCharge01 > 0.f;
	}
	return bBoostActive;
}

void ATN_Buggy::UpdateBoost(float DeltaSeconds)
{
	const UTN_BuggyData* Tuning = GetData();
	TNBuggy::FBoostTuning Boost;
	Boost.DrainPerSecond = Tuning->BoostDrainPerSecond;
	Boost.DriftRechargePerSecond = Tuning->BoostDriftRechargePerSecond;
	Boost.AirRechargePerSecond = Tuning->BoostAirRechargePerSecond;
	Boost.MinDriftSlipDeg = Tuning->BoostMinDriftSlipDeg;

	const FVector Velocity = GetVelocity();
	TNBuggy::FBoostInput In;
	In.bWantBoost = bBoostHeld && DriverController != nullptr;
	// El freno de carrera cuenta como motor cortado: ni empuja ni gasta la barra.
	In.bEngineLocked = IsEngineLocked() || bRaceBrakeHeld;
	In.bHandbrake = bHandbrakeHeld;
	In.bAirborne = bAirborne;
	In.SlipDeg = TNBuggy::SlipAngleDeg(GetActorForwardVector(), Velocity);
	In.SpeedCms = static_cast<float>(FVector(Velocity.X, Velocity.Y, 0.f).Size());

	const TNBuggy::FBoostStep Step = TNBuggy::AdvanceBoost(BoostCharge01, In, DeltaSeconds, Boost);
	BoostCharge01 = Step.Charge01;
	if (Step.bActive != bBoostActive)
	{
		bBoostActive = Step.bActive;
		ForceNetUpdate();
	}
}

void ATN_Buggy::ApplyBoostPush()
{
	USkeletalMeshComponent* Chassis = GetMesh();
	if (!IsBoosting() || bAirborne || !Chassis->IsSimulatingPhysics())
	{
		return;
	}
	const UTN_BuggyData* Tuning = GetData();
	const float BoostTop = TNRallyTurret::BuggyTopSpeedCms * Tuning->BoostTopSpeedMultiplier;
	const float Accel = TNBuggy::BoostPushAccel(GetForwardSpeedCms(), BoostTop, Tuning->BoostPushAccel, Tuning->BoostPushFadeBandCms);
	if (Accel > 0.f)
	{
		// En el centro de masas y como aceleración: lleva la punta por encima del corte de régimen del motor.
		Chassis->AddForce(GetActorForwardVector() * Accel, NAME_None, true);
	}
}

void ATN_Buggy::RefreshBoostEffects()
{
	const bool bWanted = HasActorBegunPlay() && !IsActorBeingDestroyed() && IsBoosting() && GetNetMode() != NM_DedicatedServer;
	if (bWanted == bBoostEffectsOn)
	{
		return;
	}
	bBoostEffectsOn = bWanted;
	if (!bWanted)
	{
		if (BoostEffectComponent)
		{
			BoostEffectComponent->Deactivate();
			BoostEffectComponent = nullptr;
		}
		if (BoostSoundComponent)
		{
			BoostSoundComponent->FadeOut(0.25f, 0.f);
			BoostSoundComponent = nullptr;
		}
		return;
	}
	// Sujetos a la carrocería: la llama sigue al escape (el del modelo de la tienda). Se destruyen solos al desactivarse.
	const FVector Exhaust = GetExhaustLocal();
	if (BoostEffect)
	{
		BoostEffectComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(BoostEffect, Body, NAME_None, Exhaust,
			FRotator(0.f, 180.f, 0.f), EAttachLocation::KeepRelativeOffset, true);
	}
	if (BoostStartSound)
	{
		UGameplayStatics::SpawnSoundAttached(BoostStartSound, Body, NAME_None, Exhaust, EAttachLocation::KeepRelativeOffset, true);
	}
	if (BoostSound)
	{
		BoostSoundComponent = UGameplayStatics::SpawnSoundAttached(BoostSound, Body, NAME_None, Exhaust,
			EAttachLocation::KeepRelativeOffset, true);
	}
}

void ATN_Buggy::SetBoostHeld(bool bHeld)
{
	bBoostHeld = bHeld;
	if (!HasAuthority())
	{
		ServerSetBoostHeld(bHeld);
	}
}

void ATN_Buggy::ServerSetBoostHeld_Implementation(bool bHeld)
{
	// Solo se guarda el botón: UpdateBoost decide con la carga y el motor si el turbo empuja.
	bBoostHeld = bHeld;
}

void ATN_Buggy::SetRaceBrakeHeld(bool bHeld)
{
	if (!HasAuthority() || bRaceBrakeHeld == bHeld)
	{
		return;
	}
	bRaceBrakeHeld = bHeld;
	if (bHeld)
	{
		bBoostActive = false;
		ApplyRaceBrake();
	}
	else
	{
		ReleaseRaceBrake();
	}
	ApplyEngineTorque();
	ForceNetUpdate();
}

void ATN_Buggy::OnRep_RaceBrake()
{
	if (!bRaceBrakeHeld)
	{
		ReleaseRaceBrake();
	}
}

void ATN_Buggy::ApplyRaceBrake()
{
	UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement();
	if (!Move)
	{
		return;
	}
	// Cada fotograma: la entrada de la conductora (o del piloto IA) puede haber vuelto a pisar el acelerador.
	Move->SetThrottleInput(0.f);
	Move->SetBrakeInput(1.f);
}

void ATN_Buggy::ReleaseRaceBrake()
{
	// El freno de la carrera se quita; si la conductora tiene pisado el suyo, su entrada lo vuelve a poner el siguiente
	// fotograma (Triggered).
	if (UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement(); Move && (HasAuthority() || IsLocallyControlled()))
	{
		Move->SetBrakeInput(0.f);
	}
}
