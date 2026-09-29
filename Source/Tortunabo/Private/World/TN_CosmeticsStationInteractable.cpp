#include "World/TN_CosmeticsStationInteractable.h"
#include "Player/MP_GamePlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

ATN_CosmeticsStationInteractable::ATN_CosmeticsStationInteractable()
{
	PromptText = NSLOCTEXT("Tortunabo", "CosmeticsStationPrompt", "Abrir cosméticos");
}

void ATN_CosmeticsStationInteractable::Interact(APawn* Interactor)
{
	if (!HasAuthority() || !CanInteract(Interactor) || !GetWorld())
	{
		return;
	}

	LastInteractionServerTime = GetWorld()->GetTimeSeconds();

	if (Interactor)
	{
		if (APlayerController* PC = Cast<APlayerController>(Interactor->GetController()))
		{
			if (AMP_GamePlayerController* TNPC = Cast<AMP_GamePlayerController>(PC))
			{
				TNPC->ClientOpenCosmeticsMenu();
			}
		}
	}

	Super::Interact(Interactor);
}

