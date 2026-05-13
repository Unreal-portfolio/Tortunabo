#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "TN_ProcAnimInstance.generated.h"

/**
 * @brief AnimInstance mínimo que permite a C++ controlar transforms de huesos sin necesidad de un AnimBP.
 *
 * ATortugaCharacter escribe en BoneQuat/BoneLoc/BoneScale cada Tick;
 * NativePostEvaluateAnimation los inyecta en el buffer editable component-space
 * justo antes de FinalizeBoneTransform.
 */
UCLASS()
class TORTUNABO_API UTN_ProcAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	/** @brief Overrides de rotación por hueso (component-space). Key = nombre resuelto del hueso. */
	TMap<FName, FQuat>   BoneQuat;

	/** @brief Overrides de posición por hueso (component-space). */
	TMap<FName, FVector> BoneLoc;

	/** @brief Overrides de escala por hueso (component-space). */
	TMap<FName, FVector> BoneScale;

protected:
	/** @brief Inyecta los overrides en el buffer de bones antes de finalizar la pose. */
	virtual void NativePostEvaluateAnimation() override;
};
