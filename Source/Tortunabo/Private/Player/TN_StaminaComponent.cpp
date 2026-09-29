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
	SyncStaminaShared();
}

void UTN_StaminaComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// La stamina exacta, solo al dueño; los demás (espectadores que la miran, las caras del HUD y el foley) la reciben en un
	// byte que solo se manda al cambiar (StaminaShared). bIsExhausted, a todos.
	DOREPLIFETIME_CONDITION(UTN_StaminaComponent, CurrentStamina, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UTN_StaminaComponent, StaminaShared, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(UTN_StaminaComponent, bIsSprinting, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(UTN_StaminaComponent, bSprintRequested, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UTN_StaminaComponent, bUnlimitedStamina, COND_OwnerOnly);
	DOREPLIFETIME(UTN_StaminaComponent, bIsExhausted);
	DOREPLIFETIME_CONDITION(UTN_StaminaComponent, bPostBoostPenaltyActive, COND_OwnerOnly);
}

void UTN_StaminaComponent::SetSprintRequested(bool bRequested)
{
	const bool bChanged = bRequested != bSprintRequested;
	bSprintRequested = bRequested;
	RecomputeSprintState();

	// Al servidor solo cuando cambia: el personaje lo pide en cada fotograma mientras se mueve (RefreshSprintRequest) y un
	// RPC fiable por fotograma y jugador llenaba el búfer de fiables del cliente. Si el servidor lo cambia por su cuenta (al
	// meterse en el caparazón), la réplica (solo al dueño) lo trae aquí y la siguiente petición vuelve a salir.
	if (bChanged && GetOwner() && !GetOwner()->HasAuthority())
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

bool UTN_StaminaComponent::ServerGrantUnlimitedStamina_Validate(float DurationSeconds)
{
	// Cota generosa: rechaza (el engine desconecta) clientes que envíen valores
	// absurdos/NaN. El clamp real a 15s lo hace el _Implementation; esta validación
	// es una red de seguridad a nivel de engine que no rechaza tráfico legítimo.
	return DurationSeconds >= 0.f && DurationSeconds <= 300.f;
}

void UTN_StaminaComponent::ServerGrantUnlimitedStamina_Implementation(float DurationSeconds)
{
	// Clamp to prevent clients from granting themselves permanent unlimited stamina.
	constexpr float MaxGrantDuration = 15.f;
	GrantUnlimitedStamina(FMath::Clamp(DurationSeconds, 0.f, MaxGrantDuration));
}

void UTN_StaminaComponent::RestoreStaminaToFull()
{
	if (GetOwner() && !GetOwner()->HasAuthority())
	{
		// El servidor aplica el efecto; el cliente solo lo solicita.
		// Reutilizamos el patron de GrantUnlimitedStamina sin RPC dedicada:
		// el item pickup ya debería ejecutarse via Server RPC en el pickup base.
		return;
	}

	CurrentStamina    = GetEffectiveMaxStamina();
	bIsExhausted      = false;
	ExhaustionTimer   = 0.f;
	RechargeElapsed   = 0.f;
	TimeSinceSprintStopped = 0.f;
	RecomputeSprintState();
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

void UTN_StaminaComponent::OnRep_IsSprinting()
{
	ApplyMovementSpeed();
}

void UTN_StaminaComponent::SyncStaminaShared()
{
	const float Fraction = FMath::Clamp(CurrentStamina / FMath::Max(1.f, MaxStamina), 0.f, 1.f);
	const uint8 Byte = static_cast<uint8>(FMath::RoundToInt(Fraction * 255.f));
	if (Byte != StaminaShared)
	{
		StaminaShared = Byte;
	}
}

void UTN_StaminaComponent::OnRep_StaminaShared()
{
	CurrentStamina = StaminaShared / 255.f * MaxStamina;
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

		// Penalización blanda post-boost: durante PostBoostExhaustionSeconds el
		// jugador se mueve más lento y drena stamina x2. NO se bloquea recuperación
		// ni se resetea CurrentStamina — queremos desincentivar el sprint sin castrar
		// al jugador.
		if (PostBoostExhaustionSeconds > 0.f)
		{
			bPostBoostPenaltyActive = true;
			PostBoostPenaltyTimer   = PostBoostExhaustionSeconds;
		}
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

	// Penalización blanda post-boost corre en paralelo — no bloquea nada,
	// solo escala drain y velocidad mientras el timer expira.
	if (bPostBoostPenaltyActive)
	{
		PostBoostPenaltyTimer -= DeltaTime;
		if (PostBoostPenaltyTimer <= 0.0f)
		{
			bPostBoostPenaltyActive = false;
			PostBoostPenaltyTimer   = 0.0f;
			// ApplyMovementSpeed al quitar el flag — restaura velocidad completa.
			ApplyMovementSpeed();
		}
	}

	if (bIsSprinting)
	{
		TimeSinceSprintStopped = 0.0f;
		RechargeElapsed = 0.0f;

		if (!bUnlimitedStamina)
		{
			const float DrainMul = bPostBoostPenaltyActive ? PostBoostDrainMultiplier : 1.0f;
			CurrentStamina = FMath::Max(0.0f, CurrentStamina - (SprintDrainPerSecond * DrainMul * DeltaTime));

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
				bIsExhausted            = false;
				ExhaustionTimer         = 0.0f;
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
			float BaseSpeed = bIsSprinting ? SprintSpeed : WalkSpeed;
			if (bPostBoostPenaltyActive)
			{
				BaseSpeed *= PostBoostSpeedMultiplier;
			}
			if (RaceSpeedMultiplier > 1.0f)
			{
				// Turbo de carrera: al menos la velocidad de correr, por el multiplicador (sin la penalización de después).
				BaseSpeed = FMath::Max(BaseSpeed, SprintSpeed) * RaceSpeedMultiplier;
			}
			Movement->MaxWalkSpeed = FMath::Min(BaseSpeed, ActiveSpeedCap);
		}
	}
}

void UTN_StaminaComponent::SetSpeedCap(float Cap)
{
	ActiveSpeedCap = Cap;
	ApplyMovementSpeed();
}

void UTN_StaminaComponent::ClearSpeedCap()
{
	ActiveSpeedCap = TNumericLimits<float>::Max();
	ApplyMovementSpeed();
}

void UTN_StaminaComponent::SetRaceSpeedMultiplier(float Multiplier)
{
	const float NewMultiplier = FMath::Clamp(Multiplier, 1.0f, 4.0f);
	if (FMath::IsNearlyEqual(NewMultiplier, RaceSpeedMultiplier))
	{
		return;
	}
	if (const ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		{
			// La aceleración sube con la velocidad (el doble de rápido: el triple de aceleración) para que el empujón del
			// turbo sea casi inmediato; al acabar vuelve la de antes.
			if (RaceSpeedMultiplier <= 1.0f + KINDA_SMALL_NUMBER)
			{
				RaceBaseAcceleration = Movement->MaxAcceleration;
			}
			Movement->MaxAcceleration = NewMultiplier > 1.0f
				? RaceBaseAcceleration * (1.0f + (NewMultiplier - 1.0f) * 2.0f)
				: RaceBaseAcceleration;
		}
	}
	RaceSpeedMultiplier = NewMultiplier;
	ApplyMovementSpeed();
}

