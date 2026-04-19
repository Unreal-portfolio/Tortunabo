#include "World/TN_ButtonGroupManager.h"
#include "World/TN_ButtonInteractable.h"

// ── FTN_TransformAction ───────────────────────────────────────────────────────

void FTN_TransformAction::ApplyAll(const TArray<FTN_TransformAction>& Actions, bool bForward)
{
	for (const FTN_TransformAction& Action : Actions)
	{
		if (!Action.TargetActor) { continue; }

		AActor* Target = Action.TargetActor;
		const FVector  LocDelta = bForward ? Action.LocationOffset : -Action.LocationOffset;
		const FRotator RotDelta = bForward
			? Action.RotationOffset
			: FRotator(-Action.RotationOffset.Pitch, -Action.RotationOffset.Yaw, -Action.RotationOffset.Roll);

		Target->SetActorLocation(Target->GetActorLocation() + LocDelta);
		Target->SetActorRotation(Target->GetActorRotation() + RotDelta);
	}
}

ATN_ButtonGroupManager::ATN_ButtonGroupManager()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
}

void ATN_ButtonGroupManager::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority()) { return; }

	// Suscribirse al delegate de cada botón gestionado
	for (ATN_ButtonInteractable* Button : ManagedButtons)
	{
		if (!Button) { continue; }
		Button->OnActivationChanged.AddUObject(this, &ATN_ButtonGroupManager::OnButtonActivationChanged);
	}
}

void ATN_ButtonGroupManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (ATN_ButtonInteractable* Button : ManagedButtons)
	{
		if (Button)
		{
			Button->OnActivationChanged.RemoveAll(this);
		}
	}
	Super::EndPlay(EndPlayReason);
}

// ── Lógica ─────────────────────────────────────────────────────────────────────

void ATN_ButtonGroupManager::OnButtonActivationChanged(ATN_ButtonInteractable* /*Button*/, bool /*bActivated*/)
{
	if (!HasAuthority()) { return; }

	// Si ya se disparó y es one-shot, no procesar más cambios
	if (bTriggered) { return; }

	CheckAndTrigger();
}

void ATN_ButtonGroupManager::CheckAndTrigger()
{
	// Contar cuántos botones están activados actualmente
	int32 ActiveCount = 0;
	for (const ATN_ButtonInteractable* Button : ManagedButtons)
	{
		if (Button && Button->IsActivated()) { ++ActiveCount; }
	}

	const int32 Required = GetEffectiveThreshold();
	const bool  bMet     = ActiveCount >= Required;

	if (bMet && !bCurrentlyActivated)
	{
		// Umbral alcanzado — disparar
		bCurrentlyActivated = true;
		bTriggered = bOneShot;
		ApplyTriggerActions(true);
		MulticastNotifyActivated();
	}
	else if (!bMet && bCurrentlyActivated && !bOneShot)
	{
		// Bajamos de umbral y no es one-shot — revertir
		bCurrentlyActivated = false;
		ApplyTriggerActions(false);
		MulticastNotifyDeactivated();
	}
}

int32 ATN_ButtonGroupManager::GetEffectiveThreshold() const
{
	const int32 N = ManagedButtons.Num();
	if (N <= 0) { return 1; }
	if (TriggerThreshold <= 0) { return N; }
	return FMath::Clamp(TriggerThreshold, 1, N);
}

void ATN_ButtonGroupManager::ApplyTriggerActions(bool bForward)
{
	FTN_TransformAction::ApplyAll(TriggerActions, bForward);
}

void ATN_ButtonGroupManager::MulticastNotifyActivated_Implementation()
{
	OnAllButtonsActivated();
}

void ATN_ButtonGroupManager::MulticastNotifyDeactivated_Implementation()
{
	OnGroupDeactivated();
}
