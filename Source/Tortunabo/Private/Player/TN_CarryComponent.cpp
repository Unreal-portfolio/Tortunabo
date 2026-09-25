#include "Player/TN_CarryComponent.h"
#include "Player/TortugaCharacter.h"
#include "Player/TN_ShellComponent.h"
#include "Player/TN_StaminaComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "EngineUtils.h"
#include "TimerManager.h"

UTN_CarryComponent::UTN_CarryComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UTN_CarryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UTN_CarryComponent, CarriedTurtle);
	DOREPLIFETIME(UTN_CarryComponent, CarriedBy);
	DOREPLIFETIME_CONDITION(UTN_CarryComponent, bCarriedStruggling, COND_OwnerOnly);
}

ATortugaCharacter* UTN_CarryComponent::GetTurtle() const
{
	return Cast<ATortugaCharacter>(GetOwner());
}

bool UTN_CarryComponent::CanBeGrabbed(const ATortugaCharacter* Target) const
{
	const ATortugaCharacter* Self = GetTurtle();
	if (!Target || Target == Self || Target->IsDead())
	{
		return false;
	}
	if (!Target->IsInShell() && !Target->IsKnockedDown())
	{
		return false;
	}
	const UTN_CarryComponent* Other = Target->GetCarryComponent();
	return Other && !Other->IsBeingCarried() && !Other->IsCarrying();
}

// ─────────────────────────────────────────────────────────────────────────────
// Input
// ─────────────────────────────────────────────────────────────────────────────

bool UTN_CarryComponent::TryGrabNearest()
{
	ATortugaCharacter* Self = GetTurtle();
	if (!Self || Self->IsDead() || Self->IsKnockedDown() || Self->IsInShell() || IsCarrying() || IsBeingCarried() || !GetWorld())
	{
		return false;
	}

	ATortugaCharacter* Best = nullptr;
	float BestDist = GrabRange;
	const FVector Forward = Self->GetActorForwardVector();
	for (TActorIterator<ATortugaCharacter> It(GetWorld()); It; ++It)
	{
		ATortugaCharacter* Candidate = *It;
		if (!CanBeGrabbed(Candidate)) { continue; }
		const FVector To = Candidate->GetActorLocation() - Self->GetActorLocation();
		const float Dist = To.Size();
		const bool bInFront = FVector::DotProduct(Forward, To.GetSafeNormal2D()) > 0.2f || Dist < 120.f;
		if (Dist < BestDist && bInFront)
		{
			BestDist = Dist;
			Best = Candidate;
		}
	}
	if (!Best)
	{
		return false;
	}
	ServerGrab(Best);
	return true;
}

void UTN_CarryComponent::RequestThrow()
{
	if (!IsCarrying()) { return; }
	const ATortugaCharacter* Self = GetTurtle();
	const FRotator Aim = Self && Self->GetController() ? Self->GetController()->GetControlRotation() : FRotator::ZeroRotator;
	ServerThrow(Aim);
}

void UTN_CarryComponent::RequestDrop()
{
	if (IsCarrying())
	{
		ServerDrop();
	}
}

void UTN_CarryComponent::SetStruggleInput(bool bInStruggling)
{
	const bool bWant = bInStruggling && IsBeingCarried();
	if (bWant != bLocalStruggleSent)
	{
		bLocalStruggleSent = bWant;
		ServerSetStruggling(bWant);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Servidor
// ─────────────────────────────────────────────────────────────────────────────

void UTN_CarryComponent::ServerGrab_Implementation(ATortugaCharacter* Target)
{
	ATortugaCharacter* Self = GetTurtle();
	if (!Self || Self->IsDead() || Self->IsKnockedDown() || Self->IsInShell() || IsCarrying() || IsBeingCarried())
	{
		return;
	}
	if (!CanBeGrabbed(Target) || FVector::Dist(Self->GetActorLocation(), Target->GetActorLocation()) > GrabRange + 80.f)
	{
		return;
	}

	// Aturdida: pasa a ser una bola de caparazón mientras la llevan.
	if (Target->IsKnockedDown())
	{
		Target->RecoverFromKnockdown();
	}
	if (UTN_ShellComponent* Shell = Target->GetShellComponent())
	{
		Shell->ForceEnterShell();
		Shell->SetExitLocked(true);
	}

	UTN_CarryComponent* Other = Target->GetCarryComponent();
	CarriedTurtle = Target;
	bCarriedStruggling = false;
	Other->CarriedBy = Self;
	Other->bStruggling = false;
	Other->StruggleTime = 0.f;
	Other->bAwaitingBounce = false;
	ApplyCarrierLocalState(true);
	Other->ApplyCarriedLocalState(Self);

	if (GrabSound)
	{
		Self->MulticastPlaySfx(GrabSound);
	}
}

void UTN_CarryComponent::ServerThrow_Implementation(FRotator AimRotation)
{
	ATortugaCharacter* Self = GetTurtle();
	ATortugaCharacter* Carried = CarriedTurtle;
	if (!Self || !Carried)
	{
		return;
	}

	// Mirar al frente lanza a ~40°; levantar la cámara lanza más alto.
	const float AimPitch = FRotator::NormalizeAxis(AimRotation.Pitch);
	const float Pitch = FMath::Clamp(AimPitch + 40.f, MinThrowPitch, MaxThrowPitch);
	const FVector Dir = FRotator(Pitch, AimRotation.Yaw, 0.f).Vector();
	const float Speed = ThrowSpeed * (bCarriedStruggling ? StruggleThrowMultiplier : 1.f);
	const FVector Flat = FRotator(0.f, AimRotation.Yaw, 0.f).Vector();
	const FVector Start = Self->GetActorLocation() + Flat * 70.f + FVector(0.f, 0.f, CarryHeight + 20.f);

	Release(Carried, Start, Dir * Speed, true);
	if (ThrowSound)
	{
		Self->MulticastPlaySfx(ThrowSound);
	}
}

void UTN_CarryComponent::ServerDrop_Implementation()
{
	ATortugaCharacter* Self = GetTurtle();
	ATortugaCharacter* Carried = CarriedTurtle;
	if (!Self || !Carried)
	{
		return;
	}
	const FVector Start = Self->GetActorLocation() + Self->GetActorForwardVector() * 130.f + FVector(0.f, 0.f, 30.f);
	Release(Carried, Start, FVector::ZeroVector, false);
}

void UTN_CarryComponent::ServerSetStruggling_Implementation(bool bInStruggling)
{
	bStruggling = bInStruggling && IsBeingCarried();
	if (!bStruggling)
	{
		StruggleTime = 0.f;
	}
	if (ATortugaCharacter* Carrier = CarriedBy)
	{
		if (UTN_CarryComponent* CarrierComp = Carrier->GetCarryComponent())
		{
			CarrierComp->bCarriedStruggling = bStruggling;
		}
	}
}

void UTN_CarryComponent::ForceRelease(bool bEscapeHop)
{
	ATortugaCharacter* Self = GetTurtle();
	ATortugaCharacter* Carried = CarriedTurtle;
	if (!Self || !Carried || !Self->HasAuthority())
	{
		return;
	}
	const FVector Side = Self->GetActorRightVector() * (FMath::RandBool() ? 1.f : -1.f);
	const FVector Start = Self->GetActorLocation() + Side * 90.f + FVector(0.f, 0.f, CarryHeight);
	const FVector Hop = bEscapeHop ? Side * 350.f + FVector(0.f, 0.f, 450.f) : FVector::ZeroVector;
	Release(Carried, Start, Hop, false);
}

void UTN_CarryComponent::Release(ATortugaCharacter* Carried, const FVector& Location, const FVector& Velocity, bool bThrown)
{
	UTN_CarryComponent* Other = Carried ? Carried->GetCarryComponent() : nullptr;
	CarriedTurtle = nullptr;
	bCarriedStruggling = false;
	ApplyCarrierLocalState(false);
	if (!Other)
	{
		return;
	}

	Other->CarriedBy = nullptr;
	Other->bStruggling = false;
	Other->StruggleTime = 0.f;
	Other->ApplyCarriedLocalState(nullptr);

	Carried->SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
	Other->bAwaitingBounce = bThrown;
	if (UTN_ShellComponent* Shell = Carried->GetShellComponent())
	{
		// Lanzada: no sale del caparazón hasta el rebote. Soltada: puede salir ya.
		Shell->SetExitLocked(bThrown);
	}
	if (!Velocity.IsNearlyZero())
	{
		Carried->LaunchCharacter(Velocity, true, true);
	}
	Other->ClientApplyThrow(Location, Velocity, bThrown);
}

void UTN_CarryComponent::ClientApplyThrow_Implementation(FVector StartLocation, FVector Velocity, bool bBounce)
{
	ATortugaCharacter* Self = GetTurtle();
	if (!Self || Self->HasAuthority())
	{
		// En el servidor (o el host) el impulso ya se aplicó en Release.
		return;
	}
	ApplyCarriedLocalState(nullptr);
	Self->SetActorLocation(StartLocation, false, nullptr, ETeleportType::TeleportPhysics);
	bAwaitingBounce = bBounce;
	if (!Velocity.IsNearlyZero())
	{
		Self->LaunchCharacter(Velocity, true, true);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Estado local (todas las máquinas)
// ─────────────────────────────────────────────────────────────────────────────

void UTN_CarryComponent::OnRep_CarriedTurtle()
{
	ApplyCarrierLocalState(CarriedTurtle != nullptr);
}

void UTN_CarryComponent::OnRep_CarriedBy()
{
	ApplyCarriedLocalState(CarriedBy);
}

void UTN_CarryComponent::ApplyCarrierLocalState(bool bCarrying)
{
	ATortugaCharacter* Self = GetTurtle();
	if (!Self || bCarrierStateApplied == bCarrying)
	{
		return;
	}
	bCarrierStateApplied = bCarrying;
	if (UTN_StaminaComponent* Stamina = Self->GetStaminaComponent())
	{
		if (bCarrying) { Stamina->SetSpeedCap(CarrySpeedCap); }
		else { Stamina->ClearSpeedCap(); }
	}
	if (!bCarrying)
	{
		Self->SetCarryShake(FRotator::ZeroRotator);
	}
}

void UTN_CarryComponent::ApplyCarriedLocalState(ATortugaCharacter* Carrier)
{
	ATortugaCharacter* Self = GetTurtle();
	if (!Self)
	{
		return;
	}
	UCharacterMovementComponent* Move = Self->GetCharacterMovement();

	if (Carrier)
	{
		if (Move)
		{
			Move->StopMovementImmediately();
			Move->DisableMovement();
		}
		Self->GetCapsuleComponent()->IgnoreActorWhenMoving(Carrier, true);
		Carrier->GetCapsuleComponent()->IgnoreActorWhenMoving(Self, true);
		Self->AttachToActor(Carrier, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		Self->SetActorRelativeLocation(FVector(0.f, 0.f, CarryHeight));
		Self->SetActorRelativeRotation(FRotator::ZeroRotator);
		LastCarrier = Carrier;
		bCarriedStateApplied = true;
		return;
	}

	if (!bCarriedStateApplied)
	{
		return;
	}
	bCarriedStateApplied = false;
	Self->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Self->SetActorRotation(FRotator(0.f, Self->GetActorRotation().Yaw, 0.f));
	if (Move)
	{
		Move->SetMovementMode(MOVE_Falling);
	}

	// La colisión con quien lo llevaba vuelve cuando ya se ha separado.
	TWeakObjectPtr<UTN_CarryComponent> WeakThis(this);
	TWeakObjectPtr<ATortugaCharacter> WeakCarrier = LastCarrier;
	FTimerHandle Handle;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(Handle, [WeakThis, WeakCarrier]()
		{
			if (WeakThis.IsValid() && WeakCarrier.IsValid())
			{
				WeakThis->RestoreCollisionWith(WeakCarrier.Get());
			}
		}, 0.45f, false);
	}
}

void UTN_CarryComponent::RestoreCollisionWith(ATortugaCharacter* Other)
{
	ATortugaCharacter* Self = GetTurtle();
	if (!Self || !Other || IsBeingCarried())
	{
		return;
	}
	Self->GetCapsuleComponent()->IgnoreActorWhenMoving(Other, false);
	Other->GetCapsuleComponent()->IgnoreActorWhenMoving(Self, false);
}

// ─────────────────────────────────────────────────────────────────────────────
// Aterrizaje del lanzado
// ─────────────────────────────────────────────────────────────────────────────

void UTN_CarryComponent::NotifyLanded()
{
	if (!bAwaitingBounce)
	{
		return;
	}
	bAwaitingBounce = false;
	ATortugaCharacter* Self = GetTurtle();
	if (!Self)
	{
		return;
	}
	// Rebote siempre vertical donde cae: se estira en el aire y aterriza de pie.
	Self->LaunchCharacter(FVector(0.f, 0.f, BounceVelocity), true, true);
	if (Self->HasAuthority())
	{
		if (UTN_ShellComponent* Shell = Self->GetShellComponent())
		{
			Shell->SetExitLocked(false);
			Shell->ForceExitShell();
		}
	}
}

void UTN_CarryComponent::NotifyEnteredWater()
{
	if (!bAwaitingBounce)
	{
		return;
	}
	bAwaitingBounce = false;
	ATortugaCharacter* Self = GetTurtle();
	if (Self && Self->HasAuthority())
	{
		if (UTN_ShellComponent* Shell = Self->GetShellComponent())
		{
			Shell->SetExitLocked(false);
			Shell->ForceExitShell();
		}
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Tick
// ─────────────────────────────────────────────────────────────────────────────

void UTN_CarryComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	ATortugaCharacter* Self = GetTurtle();
	if (!Self)
	{
		return;
	}

	if (Self->HasAuthority())
	{
		// Llevado: forcejeo continuo → se libera.
		if (IsBeingCarried())
		{
			StruggleTime = bStruggling ? StruggleTime + DeltaTime : 0.f;
			if (StruggleTime >= SecondsToEscape)
			{
				if (UTN_CarryComponent* CarrierComp = CarriedBy->GetCarryComponent())
				{
					CarrierComp->ForceRelease(true);
				}
			}
		}

		// Portador: si él o su carga dejan de poder seguir así, se suelta.
		if (IsCarrying())
		{
			ATortugaCharacter* Carried = CarriedTurtle;
			if (!IsValid(Carried) || Carried->IsDead() || Self->IsDead() || Self->IsKnockedDown())
			{
				ForceRelease(false);
			}
		}
	}

	// Temblor de cámara del portador mientras su carga forcejea (solo local).
	if (Self->IsLocallyControlled() && IsCarrying())
	{
		if (bCarriedStruggling)
		{
			ShakeTime += DeltaTime;
			Self->SetCarryShake(FRotator(FMath::Sin(ShakeTime * 41.f) * StruggleShakeDegrees,
				FMath::Sin(ShakeTime * 33.f + 1.3f) * StruggleShakeDegrees, 0.f));
		}
		else
		{
			Self->SetCarryShake(FRotator::ZeroRotator);
		}
	}
}
