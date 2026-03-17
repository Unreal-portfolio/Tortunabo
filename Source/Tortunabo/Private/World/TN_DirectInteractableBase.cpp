#include "World/TN_DirectInteractableBase.h"

bool ATN_DirectInteractableBase::CanInteract(APawn* Interactor) const
{
	if (!Super::CanInteract(Interactor) || !GetWorld())
	{
		return false;
	}

	const float CurrentTime = GetWorld()->GetTimeSeconds();
	return (CurrentTime - LastInteractionServerTime) >= CooldownSeconds;
}

void ATN_DirectInteractableBase::Interact(APawn* Interactor)
{
	if (!HasAuthority() || !CanInteract(Interactor) || !GetWorld())
	{
		return;
	}

	LastInteractionServerTime = GetWorld()->GetTimeSeconds();
	OnDirectInteraction(Interactor);
	Super::Interact(Interactor);
}

