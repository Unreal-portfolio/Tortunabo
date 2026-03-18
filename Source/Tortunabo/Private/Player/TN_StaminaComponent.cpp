#include "Player/TN_StaminaComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Net/UnrealNetwork.h"

UTN_StaminaComponent::UTN_StaminaComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UTN_StaminaComponent::BeginPlay()
{
	Super::BeginPlay();

	CurrentStamina = MaxStamina;

	if (const ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		if (const USkeletalMeshComponent* CharacterMesh = Character->GetMesh())
		{
			CachedMeshRelativeRotation = CharacterMesh->GetRelativeRotation();
			bHasCachedMeshRotation = true;
		}
	}

	ApplyMovementSpeed();
	ApplySprintVisual();
}

void UTN_StaminaComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	if (!Owner->HasAuthority())
	{
		return;
	}

	TickUnlimitedTimer(DeltaTime);
	TickStamina(DeltaTime);
}

void UTN_StaminaComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(UTN_StaminaComponent, CurrentStamina, COND_OwnerOnly);
	DOREPLIFETIME(UTN_StaminaComponent, bIsSprinting);
	DOREPLIFETIME(UTN_StaminaComponent, bSprintRequested);
	DOREPLIFETIME_CONDITION(UTN_StaminaComponent, bUnlimitedStamina, COND_OwnerOnly);
}

void UTN_StaminaComponent::SetSprintRequested(bool bRequested)
{
	bSprintRequested = bRequested;
	RecomputeSprintState();

	if (GetOwner() && !GetOwner()->HasAuthority())
	{
		ServerSetSprintRequested(bRequested);
	}
}

void UTN_StaminaComponent::GrantUnlimitedStamina(float DurationSeconds)
{
	if (DurationSeconds <= 0.0f)
	{
		return;
	}

	if (!GetOwner())
	{
		return;
	}

	if (!GetOwner()->HasAuthority())
	{
		ServerGrantUnlimitedStamina(DurationSeconds);
		return;
	}

	bUnlimitedStamina = true;
	UnlimitedStaminaRemaining = DurationSeconds;
	CurrentStamina = MaxStamina;
	RecomputeSprintState();
}

void UTN_StaminaComponent::ServerSetSprintRequested_Implementation(bool bRequested)
{
	bSprintRequested = bRequested;
	RecomputeSprintState();
}

void UTN_StaminaComponent::ServerGrantUnlimitedStamina_Implementation(float DurationSeconds)
{
	GrantUnlimitedStamina(DurationSeconds);
}

void UTN_StaminaComponent::OnRep_CurrentStamina()
{
}

void UTN_StaminaComponent::OnRep_IsSprinting()
{
	ApplyMovementSpeed();
	ApplySprintVisual();
}

void UTN_StaminaComponent::OnRep_UnlimitedStamina()
{
	if (bUnlimitedStamina)
	{
		CurrentStamina = MaxStamina;
	}
}

void UTN_StaminaComponent::TickUnlimitedTimer(float DeltaTime)
{
	if (!bUnlimitedStamina)
	{
		return;
	}

	UnlimitedStaminaRemaining -= DeltaTime;
	CurrentStamina = MaxStamina;
	if (UnlimitedStaminaRemaining <= 0.0f)
	{
		bUnlimitedStamina = false;
		UnlimitedStaminaRemaining = 0.0f;
	}
}

void UTN_StaminaComponent::TickStamina(float DeltaTime)
{
	if (bIsSprinting)
	{
		TimeSinceSprintStopped = 0.0f;
		RechargeElapsed = 0.0f;

		if (!bUnlimitedStamina)
		{
			CurrentStamina = FMath::Max(0.0f, CurrentStamina - (SprintDrainPerSecond * DeltaTime));
		}
	}
	else
	{
		TimeSinceSprintStopped += DeltaTime;
		if (TimeSinceSprintStopped >= RechargeDelaySeconds)
		{
			RechargeElapsed += DeltaTime;
			const float RechargeRate = RechargeBasePerSecond * FMath::Exp(RechargeExponentGrowth * RechargeElapsed);
			CurrentStamina = FMath::Min(MaxStamina, CurrentStamina + (RechargeRate * DeltaTime));
		}
	}

	RecomputeSprintState();
}

void UTN_StaminaComponent::RecomputeSprintState()
{
	bool bCanSprint = bSprintRequested;

	if (!bUnlimitedStamina)
	{
		bCanSprint = bCanSprint && (CurrentStamina > KINDA_SMALL_NUMBER);
	}

	if (bIsSprinting != bCanSprint)
	{
		bIsSprinting = bCanSprint;
		ApplySprintVisual();
	}

	ApplyMovementSpeed();
}

void UTN_StaminaComponent::ApplyMovementSpeed() const
{
	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		{
			Movement->MaxWalkSpeed = bIsSprinting ? SprintSpeed : WalkSpeed;
		}
	}
}

void UTN_StaminaComponent::ApplySprintVisual() const
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character)
	{
		return;
	}

	USkeletalMeshComponent* CharacterMesh = Character->GetMesh();
	if (!CharacterMesh)
	{
		return;
	}

	const FRotator BaseRotation = bHasCachedMeshRotation ? CachedMeshRelativeRotation : CharacterMesh->GetRelativeRotation();
	if (bIsSprinting)
	{
		CharacterMesh->SetRelativeRotation(FRotator(BaseRotation.Pitch + SprintMeshPitchDegrees, BaseRotation.Yaw, BaseRotation.Roll));
	}
	else
	{
		CharacterMesh->SetRelativeRotation(BaseRotation);
	}
}

