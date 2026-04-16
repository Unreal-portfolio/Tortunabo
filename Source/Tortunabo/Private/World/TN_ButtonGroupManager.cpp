#include "World/TN_ButtonGroupManager.h"
#include "World/TN_ButtonInteractable.h"

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
	if (!HasAuthority() || bTriggered) { return; }
	CheckAndTrigger();
}

void ATN_ButtonGroupManager::CheckAndTrigger()
{
	// Comprobar si todos los botones gestionados están activados
	for (const ATN_ButtonInteractable* Button : ManagedButtons)
	{
		if (!Button || !Button->IsActivated()) { return; }
	}

	// Todos activados
	bTriggered = bOneShot;
	ApplyTriggerActions(true);
	MulticastNotifyActivated();
}

void ATN_ButtonGroupManager::ApplyTriggerActions(bool bForward)
{
	for (const FTN_TransformAction& Action : TriggerActions)
	{
		if (!Action.TargetActor) { continue; }

		AActor* Target = Action.TargetActor;
		const FVector  LocDelta = bForward ? Action.LocationOffset : -Action.LocationOffset;
		const FRotator RotDelta = bForward
			? Action.RotationOffset
			: FRotator(-Action.RotationOffset.Pitch, -Action.RotationOffset.Yaw, -Action.RotationOffset.Roll);
		FVector  NewLoc = Target->GetActorLocation() + LocDelta;
		FRotator NewRot = Target->GetActorRotation() + RotDelta;

		if (Action.TransitionDuration <= 0.f)
		{
			Target->SetActorLocation(NewLoc);
			Target->SetActorRotation(NewRot);
		}
		else
		{
			// Lerp-based move: use a timer to interpolate over TransitionDuration
			// For simplicity: snap immediately (designers can override with BP via OnAllButtonsActivated)
			Target->SetActorLocation(NewLoc);
			Target->SetActorRotation(NewRot);
		}
	}
}

void ATN_ButtonGroupManager::MulticastNotifyActivated_Implementation()
{
	OnAllButtonsActivated();
}

void ATN_ButtonGroupManager::MulticastNotifyDeactivated_Implementation()
{
	OnGroupDeactivated();
}
