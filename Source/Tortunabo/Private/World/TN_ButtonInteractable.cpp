#include "World/TN_ButtonInteractable.h"
#include "Net/UnrealNetwork.h"

ATN_ButtonInteractable::ATN_ButtonInteractable()
{
	PrimaryActorTick.bCanEverTick = true;
	PromptText = FText::FromString(TEXT("Activar"));
}

void ATN_ButtonInteractable::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATN_ButtonInteractable, CurrentWaypointIndex);
}

void ATN_ButtonInteractable::Interact(APawn* Interactor)
{
	if (!HasAuthority() || !CanInteract(Interactor))
	{
		return;
	}

	if (!MoveTarget || Waypoints.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Button] '%s' — Sin MoveTarget o sin Waypoints."), *GetName());
		return;
	}

	// Avanzar al siguiente waypoint (wrap around)
	CurrentWaypointIndex = (CurrentWaypointIndex + 1) % Waypoints.Num();
	bIsMoving = true;

	MulticastPlayButtonFeedback();

	// Actualizar cooldown via padre (TN_DirectInteractableBase)
	Super::Interact(Interactor);

	UE_LOG(LogTemp, Log, TEXT("[Button] '%s' → Waypoint %d/%d"), *GetName(), CurrentWaypointIndex + 1, Waypoints.Num());
}

void ATN_ButtonInteractable::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Solo el servidor mueve el target; clients ven la posición via replicación de movimiento del target.
	if (!HasAuthority() || !bIsMoving || !MoveTarget || Waypoints.Num() == 0)
	{
		return;
	}

	if (!Waypoints.IsValidIndex(CurrentWaypointIndex))
	{
		bIsMoving = false;
		return;
	}

	const FTransform& TargetTransform = Waypoints[CurrentWaypointIndex];
	const FVector CurrentLoc = MoveTarget->GetActorLocation();
	const FRotator CurrentRot = MoveTarget->GetActorRotation();

	// Interpolar posición a velocidad constante
	const FVector NewLoc = FMath::VInterpConstantTo(CurrentLoc, TargetTransform.GetLocation(), DeltaTime, MoveSpeed);

	// Interpolar rotación a velocidad constante
	const FRotator NewRot = FMath::RInterpConstantTo(CurrentRot, TargetTransform.GetRotation().Rotator(), DeltaTime, RotateSpeed);

	MoveTarget->SetActorLocationAndRotation(NewLoc, NewRot);

	// Verificar si llegó al destino
	const float DistSq = FVector::DistSquared(NewLoc, TargetTransform.GetLocation());
	const float AngleDiff = (NewRot - TargetTransform.GetRotation().Rotator()).GetNormalized().GetManhattanDistance(FRotator::ZeroRotator);

	if (DistSq < 4.f && AngleDiff < 1.f) // 2cm y 1°
	{
		MoveTarget->SetActorTransform(TargetTransform);
		bIsMoving = false;
	}
}

void ATN_ButtonInteractable::MulticastPlayButtonFeedback_Implementation()
{
	// Override en Blueprint para añadir sonido/VFX/animación del botón.
}

