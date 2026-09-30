#include "VR/TN_VRGrabComponent.h"
#include "VR/TN_VRMath.h"
#include "Core/TN_Log.h"
#include "Player/TN_ShellBody.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"

namespace TNVRGrabDetail
{
	/** Veces por segundo que el dueño manda la mano al servidor mientras lleva algo. */
	constexpr double SendRate = 30.0;
	/** Cuánto se tolera de más en la distancia de la mano al objeto al aceptarlo en el servidor (latencia). */
	constexpr float ServerGrabSlack = 60.f;

	bool IsValidHand(int32 Hand)
	{
		return Hand == 0 || Hand == 1;
	}
}

UTN_VRGrabComponent::UTN_VRGrabComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UTN_VRGrabComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (int32 Hand = 0; Hand < 2; ++Hand)
	{
		ReleaseHere(Hand, FVector::ZeroVector);
	}
	Super::EndPlay(EndPlayReason);
}

bool UTN_VRGrabComponent::IsGrabbable(const UPrimitiveComponent* Component, const AActor* ByActor, float MaxMassKg)
{
	if (!Component || !Component->IsSimulatingPhysics() || Component->Mobility != EComponentMobility::Movable)
	{
		return false;
	}
	const AActor* Owner = Component->GetOwner();
	// Tortugas, enemigos y caparazones tienen sus reglas (coger compañeros, derribos): la mano no los mueve.
	if (!Owner || Owner == ByActor || Owner->IsA<APawn>() || Owner->IsA<ATN_ShellBody>() || Owner->ActorHasTag(TEXT("NoVRGrab")))
	{
		return false;
	}
	return Component->GetMass() <= MaxMassKg;
}

UPrimitiveComponent* UTN_VRGrabComponent::FindGrabbable(const FVector& At) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	TArray<FOverlapResult> Overlaps;
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_PhysicsBody);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TNVRGrab), false, GetOwner());
	World->OverlapMultiByObjectType(Overlaps, At, FQuat::Identity, Objects, FCollisionShape::MakeSphere(GrabRadius), Params);
	UPrimitiveComponent* Best = nullptr;
	float BestDistance = TNumericLimits<float>::Max();
	for (const FOverlapResult& Overlap : Overlaps)
	{
		UPrimitiveComponent* Candidate = Overlap.GetComponent();
		if (!IsGrabbable(Candidate, GetOwner(), MaxMass))
		{
			continue;
		}
		FVector Closest;
		const float Distance = Candidate->GetClosestPointOnCollision(At, Closest);
		// Dentro del objeto (0) o sin colisión simple (-1): cuenta como tocándolo.
		const float Score = Distance < 0.f ? static_cast<float>(FVector::Dist(At, Candidate->GetComponentLocation())) : Distance;
		if (Score < BestDistance)
		{
			BestDistance = Score;
			Best = Candidate;
		}
	}
	return Best;
}

UPhysicsHandleComponent* UTN_VRGrabComponent::GetHandle(int32 Hand)
{
	TObjectPtr<UPhysicsHandleComponent>& Handle = Hand == 0 ? LeftHandle : RightHandle;
	if (!Handle && GetOwner())
	{
		Handle = NewObject<UPhysicsHandleComponent>(GetOwner(), Hand == 0 ? TEXT("VRGrabHandleLeft") : TEXT("VRGrabHandleRight"), RF_Transient);
		// Firme pero con algo de muelle: lo cogido no atraviesa las paredes y pesa un poco en la mano.
		Handle->RegisterComponent();
		Handle->SetLinearStiffness(1500.f);
		Handle->SetLinearDamping(120.f);
		Handle->SetAngularStiffness(1200.f);
		Handle->SetAngularDamping(300.f);
		Handle->SetInterpolationSpeed(40.f);
	}
	return Handle;
}

bool UTN_VRGrabComponent::IsGrabbing(int32 Hand) const
{
	return TNVRGrabDetail::IsValidHand(Hand) && Held[Hand].IsValid();
}

bool UTN_VRGrabComponent::TryGrab(int32 Hand, const FTransform& HandWorld)
{
	if (!TNVRGrabDetail::IsValidHand(Hand) || IsGrabbing(Hand))
	{
		return false;
	}
	UPrimitiveComponent* Target = FindGrabbable(HandWorld.GetLocation());
	if (!Target)
	{
		return false;
	}
	const AActor* TargetActor = Target->GetOwner();
	const bool bViaServer = TargetActor && TargetActor->GetIsReplicated() && GetOwner() && !GetOwner()->HasAuthority();
	if (bViaServer)
	{
		// Lo mueve el servidor; aquí solo se recuerda para mandarle la mano y soltarlo.
		Held[Hand] = Target;
		bHeldByServer[Hand] = true;
		LastMoveSent[Hand] = -1.0;
		ServerGrab(static_cast<uint8>(Hand), Target, HandWorld.GetLocation(), HandWorld.Rotator());
		return true;
	}
	return GrabHere(Hand, Target, HandWorld);
}

void UTN_VRGrabComponent::UpdateGrab(int32 Hand, const FTransform& HandWorld)
{
	if (!IsGrabbing(Hand))
	{
		return;
	}
	if (!bHeldByServer[Hand])
	{
		MoveHere(Hand, HandWorld);
		return;
	}
	const UWorld* World = GetWorld();
	const double Now = World ? World->GetRealTimeSeconds() : 0.0;
	if (LastMoveSent[Hand] < 0.0 || Now - LastMoveSent[Hand] >= 1.0 / TNVRGrabDetail::SendRate)
	{
		LastMoveSent[Hand] = Now;
		ServerMoveGrab(static_cast<uint8>(Hand), HandWorld.GetLocation(), HandWorld.Rotator());
	}
}

void UTN_VRGrabComponent::Release(int32 Hand, const FVector& HandVelocity)
{
	if (!TNVRGrabDetail::IsValidHand(Hand) || !Held[Hand].IsValid())
	{
		if (TNVRGrabDetail::IsValidHand(Hand))
		{
			Held[Hand].Reset();
			bHeldByServer[Hand] = false;
		}
		return;
	}
	const FVector Velocity = TNVRMath::ThrowVelocity(HandVelocity);
	if (bHeldByServer[Hand])
	{
		Held[Hand].Reset();
		bHeldByServer[Hand] = false;
		ServerRelease(static_cast<uint8>(Hand), Velocity);
		return;
	}
	ReleaseHere(Hand, Velocity);
}

bool UTN_VRGrabComponent::GrabHere(int32 Hand, UPrimitiveComponent* Target, const FTransform& HandWorld)
{
	UPhysicsHandleComponent* Handle = GetHandle(Hand);
	if (!Handle || !Target)
	{
		return false;
	}
	if (Handle->GetGrabbedComponent())
	{
		Handle->ReleaseComponent();
	}
	// Se coge por el punto más cercano a la mano, con el giro que tiene: no salta a la palma.
	FVector GrabPoint;
	if (Target->GetClosestPointOnCollision(HandWorld.GetLocation(), GrabPoint) < 0.f)
	{
		GrabPoint = Target->GetComponentLocation();
	}
	const FTransform GrabWorld(Target->GetComponentQuat(), GrabPoint);
	HeldFromHand[Hand] = GrabWorld.GetRelativeTransform(HandWorld);
	Target->WakeAllRigidBodies();
	Handle->GrabComponentAtLocationWithRotation(Target, NAME_None, GrabPoint, Target->GetComponentRotation());
	Held[Hand] = Target;
	bHeldByServer[Hand] = false;
	UE_LOG(LogTortunabo, Verbose, TEXT("[VR] %s coge %s con la aleta %s."), *GetNameSafe(GetOwner()), *GetNameSafe(Target->GetOwner()),
		Hand == 0 ? TEXT("izquierda") : TEXT("derecha"));
	return true;
}

void UTN_VRGrabComponent::MoveHere(int32 Hand, const FTransform& HandWorld)
{
	UPhysicsHandleComponent* Handle = Hand == 0 ? LeftHandle.Get() : RightHandle.Get();
	UPrimitiveComponent* Target = Held[Hand].Get();
	if (!Handle || !Target || Handle->GetGrabbedComponent() != Target || !Target->IsSimulatingPhysics())
	{
		// Se ha roto, lo ha cogido otro sistema o ya no tiene física: se suelta.
		ReleaseHere(Hand, FVector::ZeroVector);
		return;
	}
	const FTransform Goal = HeldFromHand[Hand] * HandWorld;
	Handle->SetTargetLocationAndRotation(Goal.GetLocation(), Goal.Rotator());
}

void UTN_VRGrabComponent::ReleaseHere(int32 Hand, const FVector& Velocity)
{
	if (!TNVRGrabDetail::IsValidHand(Hand))
	{
		return;
	}
	UPhysicsHandleComponent* Handle = Hand == 0 ? LeftHandle.Get() : RightHandle.Get();
	UPrimitiveComponent* Target = Held[Hand].Get();
	if (Handle && Handle->GetGrabbedComponent())
	{
		Handle->ReleaseComponent();
	}
	if (Target && Target->IsSimulatingPhysics() && !Velocity.IsNearlyZero())
	{
		Target->SetPhysicsLinearVelocity(Velocity);
	}
	Held[Hand].Reset();
	bHeldByServer[Hand] = false;
}

void UTN_VRGrabComponent::ServerGrab_Implementation(uint8 Hand, UPrimitiveComponent* Target, FVector_NetQuantize10 HandLocation, FRotator HandRotation)
{
	const AActor* Owner = GetOwner();
	if (!TNVRGrabDetail::IsValidHand(Hand) || !Owner || !IsGrabbable(Target, Owner, MaxMass))
	{
		return;
	}
	// Al alcance de la tortuga y de su mano (con margen por la latencia).
	FVector Closest;
	float HandDistance = Target->GetClosestPointOnCollision(HandLocation, Closest);
	if (HandDistance < 0.f)
	{
		HandDistance = static_cast<float>(FVector::Dist(HandLocation, Target->GetComponentLocation()));
	}
	if (FVector::Dist(Owner->GetActorLocation(), HandLocation) > MaxServerReach
		|| HandDistance > GrabRadius + TNVRGrabDetail::ServerGrabSlack)
	{
		UE_LOG(LogTortunabo, Verbose, TEXT("[VR] %s: agarre de %s rechazado (lejos)."), *GetNameSafe(Owner), *GetNameSafe(Target->GetOwner()));
		return;
	}
	GrabHere(Hand, Target, FTransform(HandRotation, HandLocation));
}

void UTN_VRGrabComponent::ServerMoveGrab_Implementation(uint8 Hand, FVector_NetQuantize10 HandLocation, FRotator HandRotation)
{
	const AActor* Owner = GetOwner();
	if (!TNVRGrabDetail::IsValidHand(Hand) || !Owner || !Held[Hand].IsValid())
	{
		return;
	}
	// La mano no se aleja de la tortuga más de lo que llega (un cliente no arrastra cosas por el mapa).
	FVector Location = HandLocation;
	const FVector FromOwner = Location - Owner->GetActorLocation();
	if (FromOwner.SizeSquared() > FMath::Square(MaxServerReach))
	{
		Location = Owner->GetActorLocation() + FromOwner.GetSafeNormal() * MaxServerReach;
	}
	MoveHere(Hand, FTransform(HandRotation, Location));
}

void UTN_VRGrabComponent::ServerRelease_Implementation(uint8 Hand, FVector_NetQuantize10 Velocity)
{
	ReleaseHere(Hand, TNVRMath::ThrowVelocity(Velocity));
}
