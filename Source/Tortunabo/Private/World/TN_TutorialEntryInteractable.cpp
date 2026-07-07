#include "World/TN_TutorialEntryInteractable.h"
#include "Core/TN_Log.h"
#include "GameFramework/PlayerStart.h"
#include "EngineUtils.h"

ATN_TutorialEntryInteractable::ATN_TutorialEntryInteractable()
{
	PromptText        = FText::FromString(TEXT("Repetir Tutorial"));
	InteractionDistance = 300.f;
	CooldownSeconds   = 1.0f;
}

void ATN_TutorialEntryInteractable::Interact(APawn* Interactor)
{
	if (!HasAuthority() || !Interactor || !CanInteract(Interactor))
	{
		return;
	}

	// Find the nearest PlayerStart tagged as tutorial zone entry
	APlayerStart* TutorialStart = nullptr;
	float BestDistSq = MAX_FLT;
	const FVector InteractorLoc = Interactor->GetActorLocation();

	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		APlayerStart* PS = *It;
		if (PS && PS->PlayerStartTag == TutorialStartTag)
		{
			const float DistSq = FVector::DistSquared(InteractorLoc, PS->GetActorLocation());
			if (DistSq < BestDistSq)
			{
				BestDistSq    = DistSq;
				TutorialStart = PS;
			}
		}
	}

	if (!TutorialStart)
	{
		UE_LOG(LogTortunabo, Warning,
			TEXT("[TutorialEntryInteractable] No PlayerStart with tag '%s' found in level — cannot teleport."),
			*TutorialStartTag.ToString());
		return;
	}

	Interactor->TeleportTo(TutorialStart->GetActorLocation(), TutorialStart->GetActorRotation());

	UE_LOG(LogTortunabo, Log,
		TEXT("[TutorialEntryInteractable] Teleported '%s' to tutorial zone at %s."),
		*GetNameSafe(Interactor), *TutorialStart->GetActorLocation().ToString());

	// Propagate to base: updates LastInteractionServerTime and fires OnDirectInteraction BP event
	Super::Interact(Interactor);
}
