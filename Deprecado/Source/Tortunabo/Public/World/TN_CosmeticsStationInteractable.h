#pragma once

#include "CoreMinimal.h"
#include "World/TN_DirectInteractableBase.h"
#include "TN_CosmeticsStationInteractable.generated.h"

/**
 * @brief Estación de cosméticos del lobby: al interactuar abre el menú de cosméticos en el cliente owner.
 *        Hereda de TN_DirectInteractableBase para usar el cooldown anti-spam.
 */
UCLASS()
class TORTUNABO_API ATN_CosmeticsStationInteractable : public ATN_DirectInteractableBase
{
	GENERATED_BODY()

public:
	ATN_CosmeticsStationInteractable();

	/** @brief Dispara ClientOpenCosmeticsMenu en el PlayerController del interactuante. */
	virtual void Interact(APawn* Interactor) override;
};

