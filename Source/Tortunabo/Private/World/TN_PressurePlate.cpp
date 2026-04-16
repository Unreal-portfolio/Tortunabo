#include "World/TN_PressurePlate.h"
#include "World/TN_ButtonGroupManager.h"   // FTN_TransformAction
#include "Player/TortugaCharacter.h"
#include "Core/TN_CoopPlayerState.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Net/UnrealNetwork.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"

// ────────────────────────────────────────────────────────────────────────────
// ATN_PressurePlate
// ────────────────────────────────────────────────────────────────────────────

ATN_PressurePlate::ATN_PressurePlate()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(false);

	PlateMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlateMesh"));
	SetRootComponent(PlateMesh);
	PlateMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	PlateMesh->SetCollisionResponseToAllChannels(ECR_Block);

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	TriggerBox->SetupAttachment(PlateMesh);
	TriggerBox->SetBoxExtent(FVector(75.f, 75.f, 30.f));
	TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerBox->SetCollisionObjectType(ECC_WorldDynamic);
	TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerBox->SetGenerateOverlapEvents(true);
}

void ATN_PressurePlate::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ATN_PressurePlate::OnTriggerBeginOverlap);
		TriggerBox->OnComponentEndOverlap.AddDynamic(this, &ATN_PressurePlate::OnTriggerEndOverlap);
	}
}

void ATN_PressurePlate::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	OccupyingCharacters.Empty();
	Super::EndPlay(EndPlayReason);
}

void ATN_PressurePlate::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATN_PressurePlate, bOccupied);
}

// ── Overlaps ───────────────────────────────────────────────────────────────────

void ATN_PressurePlate::OnTriggerBeginOverlap(UPrimitiveComponent* /*OverlappedComp*/, AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/,
	bool /*bFromSweep*/, const FHitResult& /*SweepResult*/)
{
	if (!HasAuthority()) { return; }
	if (ATortugaCharacter* C = Cast<ATortugaCharacter>(OtherActor))
	{
		OccupyingCharacters.Add(C);
		RefreshOccupancy();
	}
}

void ATN_PressurePlate::OnTriggerEndOverlap(UPrimitiveComponent* /*OverlappedComp*/, AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/)
{
	if (!HasAuthority()) { return; }
	if (ATortugaCharacter* C = Cast<ATortugaCharacter>(OtherActor))
	{
		OccupyingCharacters.Remove(C);
		RefreshOccupancy();
	}
}

void ATN_PressurePlate::RefreshOccupancy()
{
	// Limpiar referencias inválidas (pawn destruido mientras estaba encima)
	OccupyingCharacters.Remove(nullptr);

	// Comprobar si alguno de los personajes encima está vivo
	bool bAnyAliveOccupying = false;
	for (const TWeakObjectPtr<ATortugaCharacter>& Weak : OccupyingCharacters)
	{
		if (ATortugaCharacter* C = Weak.Get())
		{
			if (ATN_CoopPlayerState* PS = C->GetPlayerState<ATN_CoopPlayerState>())
			{
				if (PS->bIsAlive && !PS->bIsEliminated)
				{
					bAnyAliveOccupying = true;
					break;
				}
			}
		}
	}

	if (bAnyAliveOccupying != bOccupied)
	{
		bOccupied = bAnyAliveOccupying;
		MulticastOnOccupancyChanged(bOccupied);
		OnOccupancyChanged.Broadcast(this, bOccupied);
	}
}

void ATN_PressurePlate::OnRep_bOccupied()
{
	// Visual feedback local (sin lógica de juego en clientes)
}

void ATN_PressurePlate::MulticastOnOccupancyChanged_Implementation(bool bNewOccupied)
{
	if (bNewOccupied) { OnPlatePressed(); }
	else              { OnPlateReleased(); }
}

// ────────────────────────────────────────────────────────────────────────────
// ATN_PressurePlateGroupManager
// ────────────────────────────────────────────────────────────────────────────

ATN_PressurePlateGroupManager::ATN_PressurePlateGroupManager()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
}

void ATN_PressurePlateGroupManager::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority()) { return; }

	for (ATN_PressurePlate* Plate : ManagedPlates)
	{
		if (Plate)
		{
			Plate->OnOccupancyChanged.AddUObject(this, &ATN_PressurePlateGroupManager::OnOccupancyChanged);
		}
	}
}

void ATN_PressurePlateGroupManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(HoldTimerHandle);
	for (ATN_PressurePlate* Plate : ManagedPlates)
	{
		if (Plate) { Plate->OnOccupancyChanged.RemoveAll(this); }
	}
	Super::EndPlay(EndPlayReason);
}

void ATN_PressurePlateGroupManager::OnOccupancyChanged(ATN_PressurePlate* /*Plate*/, bool /*bOccupied*/)
{
	if (!HasAuthority() || bTriggered) { return; }
	EvaluateAndHandleCondition();
}

void ATN_PressurePlateGroupManager::EvaluateAndHandleCondition()
{
	const bool bNowMet = EvaluateCondition();

	if (bNowMet && !bConditionMet)
	{
		// Condición recién cumplida — arrancar hold timer
		bConditionMet = true;

		if (HoldDurationRequired <= 0.f)
		{
			OnHoldTimerExpired();
		}
		else
		{
			FTimerDelegate Del = FTimerDelegate::CreateUObject(this, &ATN_PressurePlateGroupManager::OnHoldTimerExpired);
			GetWorldTimerManager().SetTimer(HoldTimerHandle, Del, HoldDurationRequired, false);
		}
	}
	else if (!bNowMet && bConditionMet)
	{
		// Condición perdida — cancelar hold
		bConditionMet = false;
		GetWorldTimerManager().ClearTimer(HoldTimerHandle);
		MulticastNotifyConditionChange(false);
	}
}

void ATN_PressurePlateGroupManager::OnHoldTimerExpired()
{
	if (!bConditionMet || bTriggered) { return; }

	// Re-verificar por si alguien salió entre el último callback y el timer
	if (!EvaluateCondition())
	{
		bConditionMet = false;
		MulticastNotifyConditionChange(false);
		return;
	}

	bTriggered = bOneShot;
	ApplyTriggerActions();
	MulticastNotifyConditionChange(true);
}

bool ATN_PressurePlateGroupManager::EvaluateCondition() const
{
	if (ManagedPlates.Num() == 0) { return false; }

	// Contar jugadores vivos
	int32 AlivePlayers = 0;
	if (UWorld* World = GetWorld())
	{
		if (AGameStateBase* GS = World->GetGameState())
		{
			for (APlayerState* PS : GS->PlayerArray)
			{
				if (ATN_CoopPlayerState* TNPS = Cast<ATN_CoopPlayerState>(PS))
				{
					if (TNPS->bIsAlive && !TNPS->bIsEliminated) { ++AlivePlayers; }
				}
			}
		}
	}

	if (AlivePlayers == 0) { return false; }

	// Contar cuántos jugadores vivos están en alguna placa
	int32 PlayersOnPlates = 0;
	for (const ATN_PressurePlate* Plate : ManagedPlates)
	{
		if (Plate && Plate->IsOccupied()) { ++PlayersOnPlates; }
	}

	return PlayersOnPlates >= AlivePlayers;
}

void ATN_PressurePlateGroupManager::ApplyTriggerActions()
{
	FTN_TransformAction::ApplyAll(TriggerActions, true);
}

void ATN_PressurePlateGroupManager::MulticastNotifyConditionChange_Implementation(bool bMet)
{
	if (bMet) { OnConditionMet(); }
	else      { OnConditionLost(); }
}
