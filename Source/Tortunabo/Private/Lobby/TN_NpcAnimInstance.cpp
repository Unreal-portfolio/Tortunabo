#include "Lobby/TN_NpcAnimInstance.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimSequence.h"
#include "BonePose.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"

namespace TNNpcAnim
{
	/** Segundos con los que se funde el final de la espera con su principio. */
	constexpr float LoopFadeSeconds = 0.3f;

	float SmoothStep(float X)
	{
		const float T = FMath::Clamp(X, 0.f, 1.f);
		return T * T * (3.f - 2.f * T);
	}

	/** Pose del clip en el tiempo Time; en bucle, el final se funde con el principio al volver a empezar. */
	void SampleClip(const UAnimSequence* Clip, float Time, bool bLoop, FPoseContext& Out)
	{
		if (!Clip)
		{
			Out.ResetToRefPose();
			return;
		}
		const float Length = FMath::Max(0.01f, Clip->GetPlayLength());
		if (!bLoop)
		{
			FAnimationPoseData Data(Out);
			Clip->GetAnimationPose(Data, FAnimExtractContext(static_cast<double>(FMath::Clamp(Time, 0.f, Length)), false));
			return;
		}
		const float Fade = FMath::Min(LoopFadeSeconds, Length * 0.25f);
		const float Loop = FMath::Max(0.01f, Length - Fade);
		const float T = FMath::Fmod(FMath::Max(0.f, Time), Loop);
		FAnimationPoseData Data(Out);
		Clip->GetAnimationPose(Data, FAnimExtractContext(static_cast<double>(T), false));
		if (T < Fade && Fade > 0.001f)
		{
			FPoseContext Tail(Out);
			FAnimationPoseData TailData(Tail);
			Clip->GetAnimationPose(TailData, FAnimExtractContext(static_cast<double>(T + Loop), false));
			const float StartW = SmoothStep(T / Fade);
			for (const FCompactPoseBoneIndex I : Out.Pose.ForEachBoneIndex())
			{
				const FTransform Start = Out.Pose[I];
				Out.Pose[I].Blend(Tail.Pose[I], Start, StartW);
			}
		}
	}
}

bool FTNNpcAnimProxy::Evaluate(FPoseContext& Output)
{
	using namespace TNNpcAnim;
	SampleClip(IdleClip, Frame.IdleTime, true, Output);
	if (GestureClip && Frame.GestureW > 0.01f)
	{
		FPoseContext Gesture(Output);
		SampleClip(GestureClip, Frame.GestureTime, false, Gesture);
		const float W = FMath::Clamp(Frame.GestureW, 0.f, 1.f);
		for (const FCompactPoseBoneIndex I : Output.Pose.ForEachBoneIndex())
		{
			const FTransform Current = Output.Pose[I];
			Output.Pose[I].Blend(Current, Gesture.Pose[I], W);
		}
	}
	return true;
}

FAnimInstanceProxy* UTN_NpcAnimInstance::CreateAnimInstanceProxy()
{
	return new FTNNpcAnimProxy(this);
}

void UTN_NpcAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
	delete static_cast<FTNNpcAnimProxy*>(InProxy);
}

void UTN_NpcAnimInstance::SetIdle(UAnimSequence* InIdle)
{
	IdleAnim = InIdle;
}

void UTN_NpcAnimInstance::PlayGesture(UAnimSequence* InGesture, float BlendIn, float BlendOut)
{
	if (!InGesture) { return; }
	// Si ya estaba haciendo uno, el nuevo arranca desde el peso que llevaba (sin saltos).
	GestureAnim = InGesture;
	GestureBlendIn = FMath::Max(0.01f, BlendIn);
	GestureBlendOut = FMath::Max(0.01f, BlendOut);
	Frame.GestureTime = 0.f;
	bGestureActive = true;
}

UTN_NpcAnimInstance* UTN_NpcAnimInstance::SetupOn(USkeletalMeshComponent* Mesh, UAnimationAsset* Idle)
{
	if (!Mesh) { return nullptr; }
	UAnimSequence* IdleSequence = Cast<UAnimSequence>(Idle);
	if (!IdleSequence)
	{
		if (Idle) { Mesh->PlayAnimation(Idle, true); }
		return nullptr;
	}
	Mesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	Mesh->SetAnimInstanceClass(UTN_NpcAnimInstance::StaticClass());
	UTN_NpcAnimInstance* Npc = Cast<UTN_NpcAnimInstance>(Mesh->GetAnimInstance());
	if (!Npc)
	{
		Mesh->PlayAnimation(Idle, true);
		return nullptr;
	}
	Npc->SetIdle(IdleSequence);
	return Npc;
}

void UTN_NpcAnimInstance::PlayGestureOn(USkeletalMeshComponent* Mesh, UAnimationAsset* Gesture)
{
	if (!Mesh || !Gesture) { return; }
	UTN_NpcAnimInstance* Npc = Cast<UTN_NpcAnimInstance>(Mesh->GetAnimInstance());
	UAnimSequence* GestureSequence = Cast<UAnimSequence>(Gesture);
	if (Npc && GestureSequence)
	{
		Npc->PlayGesture(GestureSequence);
		return;
	}
	Mesh->PlayAnimation(Gesture, false);
}

void UTN_NpcAnimInstance::ReturnToIdleOn(USkeletalMeshComponent* Mesh, UAnimationAsset* Idle)
{
	if (!Mesh || !Idle || Cast<UTN_NpcAnimInstance>(Mesh->GetAnimInstance())) { return; }
	Mesh->PlayAnimation(Idle, true);
}

void UTN_NpcAnimInstance::PreviewInEditor(USkeletalMeshComponent* Mesh, UAnimationAsset* Idle)
{
#if WITH_EDITOR
	const UWorld* World = Mesh ? Mesh->GetWorld() : nullptr;
	if (!World || World->WorldType != EWorldType::Editor || !Idle) { return; }
	Mesh->SetUpdateAnimationInEditor(true);
	SetupOn(Mesh, Idle);
#endif
}

float UTN_NpcAnimInstance::GetGestureTimeLeft() const
{
	if (!bGestureActive || !GestureAnim) { return 0.f; }
	return FMath::Max(0.f, GestureAnim->GetPlayLength() - Frame.GestureTime);
}

void UTN_NpcAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	using namespace TNNpcAnim;
	Super::NativeUpdateAnimation(DeltaSeconds);
	const float Dt = FMath::Max(DeltaSeconds, 0.f);
	Frame.IdleTime += Dt;

	float Target = 0.f;
	if (bGestureActive && GestureAnim)
	{
		Frame.GestureTime += Dt;
		const float Length = FMath::Max(0.01f, GestureAnim->GetPlayLength());
		// Entra en BlendIn y sale en los últimos BlendOut del clip; al acabar, vuelve a la espera.
		Target = SmoothStep(FMath::Min(Frame.GestureTime / GestureBlendIn, (Length - Frame.GestureTime) / GestureBlendOut));
		if (Frame.GestureTime >= Length) { bGestureActive = false; Target = 0.f; }
	}
	// El peso sigue a su objetivo sin saltar (si un gesto nuevo corta a otro, parte del peso que llevaba).
	Frame.GestureW = FMath::FInterpTo(Frame.GestureW, Target, Dt, 14.f);

	FTNNpcAnimProxy& Proxy = GetProxyOnGameThread<FTNNpcAnimProxy>();
	Proxy.Frame = Frame;
	Proxy.IdleClip = IdleAnim;
	Proxy.GestureClip = GestureAnim;
}
