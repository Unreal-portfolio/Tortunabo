#include "Player/TN_LegAnimComponent.h"
#include "Player/TN_StaminaComponent.h"
#include "GameFramework/Character.h"
#include "Components/SceneComponent.h"

UTN_LegAnimComponent::UTN_LegAnimComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(false);
}

void UTN_LegAnimComponent::BeginPlay()
{
	Super::BeginPlay();

	if (const ACharacter* Char = Cast<ACharacter>(GetOwner()))
	{
		CachedStamina = Char->FindComponentByClass<UTN_StaminaComponent>();
	}

	CacheRestRotations();
}

void UTN_LegAnimComponent::CacheRestRotations()
{
	RestRotations.Reset();

	auto CacheName = [this](const FName& Name)
	{
		if (Name.IsNone()) { return; }
		if (USceneComponent* Comp = FindChildComponent(Name))
		{
			RestRotations.Add(Name, Comp->GetRelativeRotation());
		}
	};

	for (const FName& N : GroupA) { CacheName(N); }
	for (const FName& N : GroupB) { CacheName(N); }
}

void UTN_LegAnimComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const AActor* Owner = GetOwner();
	if (!Owner) { return; }

	const float Speed          = Owner->GetVelocity().Size2D();
	const bool  bIsSprinting   = CachedStamina.IsValid() && CachedStamina->IsSprinting();
	const float TargetAmplitude = bIsSprinting ? SprintAmplitudeDegrees : WalkAmplitudeDegrees;
	const float TargetFrequency = bIsSprinting ? SprintCyclesPerSecond  : WalkCyclesPerSecond;

	const float TargetMult = (Speed > MinSpeedToAnimate) ? 1.f : 0.f;
	AmplitudeMultiplier = FMath::FInterpTo(AmplitudeMultiplier, TargetMult, DeltaTime, AmplitudeFadeSpeed);

	if (Speed > MinSpeedToAnimate)
	{
		PhaseAccumulator += TargetFrequency * DeltaTime;
		PhaseAccumulator  = FMath::Fmod(PhaseAccumulator, 1.f);
	}

	const float Angle = TargetAmplitude * AmplitudeMultiplier * FMath::Sin(PhaseAccumulator * 2.f * PI);
	ApplyAngleToGroup(GroupA,  Angle);
	ApplyAngleToGroup(GroupB, -Angle);
}

void UTN_LegAnimComponent::ApplyAngleToGroup(const TArray<FName>& Names, float AngleDeg)
{
	for (const FName& Name : Names) { ApplyAngleToName(Name, AngleDeg); }
}

void UTN_LegAnimComponent::ApplyAngleToName(const FName& Name, float AngleDeg)
{
	if (Name.IsNone()) { return; }

	USceneComponent* Comp = FindChildComponent(Name);
	if (!Comp) { return; }

	const FRotator* RestPtr = RestRotations.Find(Name);
	const FQuat     RestQuat = RestPtr ? FQuat(*RestPtr) : FQuat::Identity;
	const FQuat     SwingQuat(SwingAxisLocal.GetSafeNormal(), FMath::DegreesToRadians(AngleDeg));
	Comp->SetRelativeRotation((RestQuat * SwingQuat).Rotator());
}

USceneComponent* UTN_LegAnimComponent::FindChildComponent(const FName& Name) const
{
	const AActor* Owner = GetOwner();
	if (!Owner) { return nullptr; }

	for (UActorComponent* Comp : Owner->GetComponents())
	{
		if (Comp && Comp->GetFName() == Name)
		{
			return Cast<USceneComponent>(Comp);
		}
	}
	return nullptr;
}

