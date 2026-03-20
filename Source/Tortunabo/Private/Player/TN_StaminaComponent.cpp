#include "Player/TN_StaminaComponent.h"
#include "Player/TN_InventoryComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
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

	ApplyMovementSpeed();
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
	DOREPLIFETIME_CONDITION(UTN_StaminaComponent, bIsSprinting, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(UTN_StaminaComponent, bSprintRequested, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(UTN_StaminaComponent, bUnlimitedStamina, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UTN_StaminaComponent, bIsExhausted, COND_OwnerOnly);
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

void UTN_StaminaComponent::SetInventoryComponent(UTN_InventoryComponent* InvComp)
{
	InventoryComponentRef = InvComp;
}

float UTN_StaminaComponent::GetEffectiveMaxStamina() const
{
	float TotalWeight = 0.f;
	if (InventoryComponentRef.IsValid())
	{
		TotalWeight = InventoryComponentRef->GetTotalCarriedWeight();
	}
	const float Penalty = TotalWeight * StaminaPerWeightUnit;
	// La stamina efectiva nunca puede bajar de 1 (evitar división por cero en la UI)
	return FMath::Max(1.f, MaxStamina - Penalty);
}

void UTN_StaminaComponent::OnRep_CurrentStamina()
{
}

void UTN_StaminaComponent::OnRep_IsSprinting()
{
	ApplyMovementSpeed();
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
	// Techo dinámico por peso — si el jugador coge un objeto pesado,
	// la stamina se recorta inmediatamente al nuevo máximo efectivo.
	const float EffMax = GetEffectiveMaxStamina();
	if (CurrentStamina > EffMax)
	{
		CurrentStamina = EffMax;
	}

	if (bIsSprinting)
	{
		TimeSinceSprintStopped = 0.0f;
		RechargeElapsed = 0.0f;

		if (!bUnlimitedStamina)
		{
			CurrentStamina = FMath::Max(0.0f, CurrentStamina - (SprintDrainPerSecond * DeltaTime));

			// Marcar agotamiento cuando la stamina llega a cero
			if (CurrentStamina <= 0.0f && !bIsExhausted)
			{
				bIsExhausted = true;
				ExhaustionTimer = ExhaustionPenaltySeconds;
			}
		}
	}
	else
	{
		// Contar la penalización de agotamiento antes del delay normal de recarga
		if (bIsExhausted)
		{
			ExhaustionTimer -= DeltaTime;
			if (ExhaustionTimer <= 0.0f)
			{
				bIsExhausted = false;
				ExhaustionTimer = 0.0f;
				// Reiniciar timer de delay normal también
				TimeSinceSprintStopped = 0.0f;
				RechargeElapsed = 0.0f;
			}
			// Bloquear recuperación durante la penalización
			RecomputeSprintState();
			return;
		}

		TimeSinceSprintStopped += DeltaTime;
		if (TimeSinceSprintStopped >= RechargeDelaySeconds)
		{
			RechargeElapsed += DeltaTime;
			const float RechargeRate = RechargeBasePerSecond * FMath::Exp(RechargeExponentGrowth * RechargeElapsed);
			// La recarga se limita al effective max (techo de peso), no al MaxStamina base.
			CurrentStamina = FMath::Min(EffMax, CurrentStamina + (RechargeRate * DeltaTime));
		}
	}

	RecomputeSprintState();
}

void UTN_StaminaComponent::RecomputeSprintState()
{
	bool bCanSprint = bSprintRequested;

	if (!bUnlimitedStamina)
	{
		// Puede sprintar si la stamina actual supera un mínimo — comprobamos contra 0,
		// ya que GetEffectiveMaxStamina es el techo, no el suelo.
		bCanSprint = bCanSprint && (CurrentStamina > KINDA_SMALL_NUMBER);
	}

	if (bIsSprinting != bCanSprint)
	{
		bIsSprinting = bCanSprint;
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
	// La inclinación del mesh al sprintar se ha eliminado.
	// El feedback visual de sprint viene únicamente del aumento de amplitud de las piernas.
}

