#include "Player/TN_ProcAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"

void UTN_ProcAnimInstance::NativePostEvaluateAnimation()
{
	Super::NativePostEvaluateAnimation();

	USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
	if (!Mesh) { return; }

	// GetEditableComponentSpaceTransforms() holds the freshly evaluated pose.
	// Overriding here runs BEFORE FinalizeBoneTransform() flips the buffers,
	// so our values end up in the readable buffer this frame.
	TArray<FTransform>& Transforms = Mesh->GetEditableComponentSpaceTransforms();

	for (const auto& [BoneName, Quat] : BoneQuat)
	{
		const int32 Idx = Mesh->GetBoneIndex(BoneName);
		if (Transforms.IsValidIndex(Idx)) { Transforms[Idx].SetRotation(Quat); }
	}

	for (const auto& [BoneName, Loc] : BoneLoc)
	{
		const int32 Idx = Mesh->GetBoneIndex(BoneName);
		if (Transforms.IsValidIndex(Idx)) { Transforms[Idx].SetTranslation(Loc); }
	}
}
