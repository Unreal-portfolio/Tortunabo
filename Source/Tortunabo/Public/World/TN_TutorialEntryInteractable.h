#pragma once

#include "CoreMinimal.h"
#include "World/TN_DirectInteractableBase.h"
#include "TN_TutorialEntryInteractable.generated.h"

/**
 * Interactable placed in the HQ lobby that teleports the activating player to the
 * tutorial zone.  Available at any time so players can revisit the tutorial after
 * completing it.
 *
 * Setup:
 *   1. Place this actor (or its BP child) somewhere visible in LVL_HQ.
 *   2. Ensure at least one APlayerStart in LVL_HQ has PlayerStartTag = "TutorialStart"
 *      (or the value set in TutorialStartTag).
 *   3. Assign a prompt mesh and text in the BP child class defaults.
 */
UCLASS()
class TORTUNABO_API ATN_TutorialEntryInteractable : public ATN_DirectInteractableBase
{
	GENERATED_BODY()

public:
	ATN_TutorialEntryInteractable();

	virtual void Interact(APawn* Interactor) override;

protected:
	/**
	 * PlayerStartTag value that identifies spawn points inside the tutorial zone.
	 * Must match the tag set on the tutorial APlayerStart actors in the level.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tutorial")
	FName TutorialStartTag = TEXT("TutorialStart");
};
