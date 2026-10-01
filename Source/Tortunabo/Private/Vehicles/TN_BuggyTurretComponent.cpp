#include "Vehicles/TN_BuggyTurretComponent.h"
#include "Vehicles/TN_Buggy.h"
#include "Vehicles/TN_BuggyGunnerPawn.h"
#include "Vehicles/TN_BuggyMath.h"
#include "Vehicles/TN_RallyProjectile.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

UTN_BuggyTurretComponent::UTN_BuggyTurretComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
	ProjectileClass = ATN_RallyProjectile::StaticClass();
}

void UTN_BuggyTurretComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UTN_BuggyTurretComponent, AimYaw);
	DOREPLIFETIME(UTN_BuggyTurretComponent, AimPitch);
	DOREPLIFETIME(UTN_BuggyTurretComponent, Heat01);
	DOREPLIFETIME(UTN_BuggyTurretComponent, bOverheated);
	DOREPLIFETIME(UTN_BuggyTurretComponent, SpecialAmmo);
	DOREPLIFETIME(UTN_BuggyTurretComponent, SpecialCharges);
}

ATN_Buggy* UTN_BuggyTurretComponent::GetBuggy() const
{
	return Cast<ATN_Buggy>(GetOwner());
}

FRotator UTN_BuggyTurretComponent::GetDisplayAim() const
{
	const ATN_Buggy* Buggy = GetBuggy();
	const ATN_BuggyGunnerPawn* Gunner = Buggy ? Buggy->GetGunnerPawn() : nullptr;
	if (Gunner && Gunner->IsLocallyControlled())
	{
		return Gunner->GetLocalAim();
	}
	return FRotator(AimPitch, AimYaw, 0.f);
}

FVector UTN_BuggyTurretComponent::GetAimWorldDirection() const
{
	const AActor* Owner = GetOwner();
	return TNRallyTurret::AimWorldDirection(Owner ? Owner->GetActorRotation() : FRotator::ZeroRotator, FRotator(AimPitch, AimYaw, 0.f));
}

void UTN_BuggyTurretComponent::SetAimRelative(const FRotator& RelativeAim)
{
	const FRotator Clamped = TNRallyTurret::ClampAim(RelativeAim);
	AimYaw = static_cast<float>(Clamped.Yaw);
	AimPitch = static_cast<float>(Clamped.Pitch);
}

void UTN_BuggyTurretComponent::GiveSpecial(ETNRallyAmmo Ammo, int32 Charges)
{
	Special = TNRallyTurret::Give(Ammo, Charges);
	SyncReplicatedState();
}

void UTN_BuggyTurretComponent::SyncReplicatedState()
{
	Heat01 = HeatState.Heat;
	bOverheated = TNRallyTurret::IsOverheated(HeatState);
	SpecialAmmo = Special.Ammo;
	SpecialCharges = Special.Charges;
	if (AActor* Owner = GetOwner())
	{
		Owner->ForceNetUpdate();
	}
}

bool UTN_BuggyTurretComponent::TryFire(bool bSpecial, const FVector& WorldDir)
{
	ATN_Buggy* Buggy = GetBuggy();
	UWorld* World = GetWorld();
	if (!Buggy || !World || !Buggy->HasAuthority() || WorldDir.IsNearlyZero() || WorldDir.ContainsNaN())
	{
		return false;
	}
	// La carrera bloquea la torreta fuera de Racing y Finishing (ATN_RallyGameMode::ApplyWeaponLocks).
	if (Buggy->AreWeaponsLocked())
	{
		UE_LOG(LogTNBuggy, Verbose, TEXT("%s: torreta bloqueada por la carrera, no dispara"), *Buggy->GetName());
		return false;
	}
	const double Now = World->GetTimeSeconds();
	const ETNRallyAmmo Ammo = bSpecial ? Special.Ammo : ETNRallyAmmo::Coco;
	const TNRallyTurret::FAmmoSpec Spec = TNRallyTurret::SpecFor(Ammo);
	if (bSpecial)
	{
		if (!TNRallyTurret::CanFireSpecial(Special) || !TNRallyTurret::CadenceOk(Now, LastSpecialShot, Spec.FireInterval))
		{
			return false;
		}
	}
	else if (!TNRallyTurret::CanFireCoco(HeatState) || !TNRallyTurret::CadenceOk(Now, LastCocoShot, Spec.FireInterval))
	{
		return false;
	}

	// El apuntado se limita como el de la artillera: la dirección final siempre cumple -10..+45 de cabeceo.
	const FRotator Relative = TNRallyTurret::RelativeAimFromWorld(Buggy->GetActorRotation(), WorldDir);
	SetAimRelative(Relative);
	const FVector Dir = TNRallyTurret::AimWorldDirection(Buggy->GetActorRotation(), Relative);
	const FVector Muzzle = GetComponentLocation() + Dir * MuzzleDistanceCm;

	UClass* Class = ProjectileClass ? ProjectileClass.Get() : ATN_RallyProjectile::StaticClass();
	ATN_RallyProjectile* Projectile = World->SpawnActorDeferred<ATN_RallyProjectile>(Class, FTransform(Dir.Rotation(), Muzzle),
		Buggy, Buggy, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Projectile)
	{
		UE_LOG(LogTNBuggy, Error, TEXT("%s: no se ha podido crear el proyectil de %s"), *Buggy->GetName(), *UEnum::GetValueAsString(Ammo));
		return false;
	}
	// El proyectil hereda la velocidad del buggy (si no, al disparar hacia delante el buggy lo alcanzaría), salvo la
	// burbuja: es lenta a propósito para que la pueda coger cualquiera, también el propio buggy.
	const FVector Inherited = Ammo == ETNRallyAmmo::Burbuja ? FVector::ZeroVector : Buggy->GetVelocity();
	Projectile->Init(Ammo, Dir * Spec.SpeedCms + Inherited, Buggy);
	Projectile->FinishSpawning(FTransform(Dir.Rotation(), Muzzle));

	if (bSpecial)
	{
		Special = TNRallyTurret::AfterSpecialShot(Special);
		LastSpecialShot = Now;
	}
	else
	{
		HeatState = TNRallyTurret::AfterCocoShot(HeatState);
		LastCocoShot = Now;
	}
	Buggy->ApplyVelocityImpulse(TNRallyTurret::RecoilVelocity(Dir, Spec.RecoilCms));
	SyncReplicatedState();
	UE_LOG(LogTNBuggy, Log, TEXT("%s: disparo %s dir=(%.2f, %.2f, %.2f) retroceso=%.0f calor=%.2f cargas=%d"), *Buggy->GetName(),
		*UEnum::GetValueAsString(Ammo), Dir.X, Dir.Y, Dir.Z, Spec.RecoilCms, HeatState.Heat, Special.Charges);
	return true;
}

void UTN_BuggyTurretComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const AActor* Owner = GetOwner();
	if (Owner && Owner->HasAuthority())
	{
		HeatState = TNRallyTurret::Cool(HeatState, DeltaTime);
		// Replica a saltos de 0,05 o al cambiar el sobrecalentamiento: el HUD no necesita más.
		if (bOverheated != TNRallyTurret::IsOverheated(HeatState) || FMath::Abs(HeatState.Heat - Heat01) >= 0.05f
			|| (HeatState.Heat == 0.f && Heat01 != 0.f))
		{
			Heat01 = HeatState.Heat;
			bOverheated = TNRallyTurret::IsOverheated(HeatState);
		}
	}
	// La torreta gira con el apuntado que se ve en esta máquina.
	SetRelativeRotation(GetDisplayAim());
}
