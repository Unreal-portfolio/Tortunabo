#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "TN_ProcAnimInstance.generated.h"

/**
 * Minimal AnimInstance that lets C++ drive bone transforms without an AnimBP.
 * ATortugaCharacter writes to BoneQuat/BoneLoc each Tick; NativePostEvaluateAnimation
 * injects them into the editable component-space buffer before FinalizeBoneTransform.
 */
UCLASS()
class TORTUNABO_API UTN_ProcAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	// Component-space bone overrides — keyed by resolved bone name (not socket name).
	TMap<FName, FQuat>   BoneQuat;
	TMap<FName, FVector> BoneLoc;

protected:
	virtual void NativePostEvaluateAnimation() override;
};
