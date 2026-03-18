#pragma once

#include "CoreMinimal.h"
#include "World/TN_DirectInteractableBase.h"
#include "TN_CosmeticsStationInteractable.generated.h"

UCLASS()
class TORTUNABO_API ATN_CosmeticsStationInteractable : public ATN_DirectInteractableBase
{
	GENERATED_BODY()

public:
	ATN_CosmeticsStationInteractable();

	virtual void Interact(APawn* Interactor) override;
};

