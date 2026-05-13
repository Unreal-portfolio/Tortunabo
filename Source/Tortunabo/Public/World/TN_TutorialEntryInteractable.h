#pragma once

#include "CoreMinimal.h"
#include "World/TN_DirectInteractableBase.h"
#include "TN_TutorialEntryInteractable.generated.h"

/**
 * @brief Interactable del lobby HQ que teletransporta al jugador a la zona de tutorial.
 *
 * Disponible en cualquier momento — los jugadores pueden revisitar el tutorial
 * tras haberlo completado.
 *
 * Setup:
 *   1. Coloca este actor (o su BP hijo) en una zona visible de LVL_HQ.
 *   2. Asegúrate de que al menos un APlayerStart en LVL_HQ tenga PlayerStartTag = "TutorialStart"
 *      (o el valor establecido en TutorialStartTag).
 *   3. Asigna un prompt mesh y texto en los defaults del BP hijo.
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
