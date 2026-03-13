#pragma once

#include "CoreMinimal.h"
#include "Engine/TriggerBox.h"
#include "TN_FinishLineVolume.generated.h"

class ATN_RunGameMode;

UCLASS()
class TORTUNABO_API ATN_FinishLineVolume : public ATriggerBox
{
	GENERATED_BODY()

public:
	ATN_FinishLineVolume();

private:
	UFUNCTION()
	void OnFinishBeginOverlap(AActor* OverlappedActor, AActor* OtherActor);

	ATN_RunGameMode* ResolveRunGameMode() const;
};

