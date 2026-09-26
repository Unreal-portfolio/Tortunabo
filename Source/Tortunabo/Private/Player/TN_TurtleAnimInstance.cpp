#include "Player/TN_TurtleAnimInstance.h"
#include "Player/TortugaCharacter.h"
#include "Player/TN_CarryComponent.h"
#include "Player/TN_StaminaComponent.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimSequence.h"
#include "BoneContainer.h"
#include "BonePose.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace TNTurtleAnim
{
	/** Ejes del espacio de la malla: mira a +Y, arriba +Z, su izquierda +X. */
	const FVector AxisX(1.0, 0.0, 0.0);
	const FVector AxisY(0.0, 1.0, 0.0);
	const FVector AxisZ(0.0, 0.0, 1.0);

	/** Velocidades (cm/s) a las que los clips de andar y correr no patinan. */
	constexpr float WalkNatural = 380.f;
	constexpr float RunNatural = 720.f;
	constexpr float TwoPiF = 6.2831853f;

	struct FBones
	{
		FCompactPoseBoneIndex Hips = FCompactPoseBoneIndex(INDEX_NONE);
		FCompactPoseBoneIndex Spine = FCompactPoseBoneIndex(INDEX_NONE);
		FCompactPoseBoneIndex Spine1 = FCompactPoseBoneIndex(INDEX_NONE);
		FCompactPoseBoneIndex Spine2 = FCompactPoseBoneIndex(INDEX_NONE);
		FCompactPoseBoneIndex Neck = FCompactPoseBoneIndex(INDEX_NONE);
		FCompactPoseBoneIndex Head = FCompactPoseBoneIndex(INDEX_NONE);
		FCompactPoseBoneIndex LArm = FCompactPoseBoneIndex(INDEX_NONE);
		FCompactPoseBoneIndex LFore = FCompactPoseBoneIndex(INDEX_NONE);
		FCompactPoseBoneIndex RArm = FCompactPoseBoneIndex(INDEX_NONE);
		FCompactPoseBoneIndex RFore = FCompactPoseBoneIndex(INDEX_NONE);
		FCompactPoseBoneIndex LUp = FCompactPoseBoneIndex(INDEX_NONE);
		FCompactPoseBoneIndex LLeg = FCompactPoseBoneIndex(INDEX_NONE);
		FCompactPoseBoneIndex RUp = FCompactPoseBoneIndex(INDEX_NONE);
		FCompactPoseBoneIndex RLeg = FCompactPoseBoneIndex(INDEX_NONE);
	};

	FCompactPoseBoneIndex FindBone(const FBoneContainer& Bones, const TCHAR* Name)
	{
		const int32 MeshIndex = Bones.GetPoseBoneIndexForBoneName(FName(Name));
		return MeshIndex == INDEX_NONE ? FCompactPoseBoneIndex(INDEX_NONE) : Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex));
	}

	FBones ResolveBones(const FBoneContainer& Bones)
	{
		FBones Out;
		Out.Hips = FindBone(Bones, TEXT("Hips"));
		Out.Spine = FindBone(Bones, TEXT("Spine"));
		Out.Spine1 = FindBone(Bones, TEXT("Spine1"));
		Out.Spine2 = FindBone(Bones, TEXT("Spine2"));
		Out.Neck = FindBone(Bones, TEXT("Neck"));
		Out.Head = FindBone(Bones, TEXT("Head"));
		Out.LArm = FindBone(Bones, TEXT("LeftArm"));
		Out.LFore = FindBone(Bones, TEXT("LeftForeArm"));
		Out.RArm = FindBone(Bones, TEXT("RightArm"));
		Out.RFore = FindBone(Bones, TEXT("RightForeArm"));
		Out.LUp = FindBone(Bones, TEXT("LeftUpLeg"));
		Out.LLeg = FindBone(Bones, TEXT("LeftLeg"));
		Out.RUp = FindBone(Bones, TEXT("RightUpLeg"));
		Out.RLeg = FindBone(Bones, TEXT("RightLeg"));
		return Out;
	}

	/** Giro del padre en el espacio de la malla (producto de los giros locales desde la raíz). */
	FQuat ParentRotation(const FCompactPose& Pose, FCompactPoseBoneIndex Bone)
	{
		FQuat Q = FQuat::Identity;
		for (FCompactPoseBoneIndex P = Pose.GetParentBoneIndex(Bone); P.IsValid(); P = Pose.GetParentBoneIndex(P))
		{
			Q = Pose[P].GetRotation() * Q;
		}
		return Q;
	}

	/** Gira un hueso Degrees alrededor de un eje del espacio de la malla que pasa por su articulación (los hijos le siguen). */
	void Turn(FCompactPose& Pose, FCompactPoseBoneIndex Bone, const FVector& Axis, float Degrees)
	{
		if (!Bone.IsValid() || FMath::Abs(Degrees) < 0.01f) { return; }
		const FQuat Parent = ParentRotation(Pose, Bone);
		const FQuat Delta(Axis, FMath::DegreesToRadians(Degrees));
		FTransform& Local = Pose[Bone];
		Local.SetRotation((Parent.Inverse() * Delta * Parent * Local.GetRotation()).GetNormalized());
	}

	/** A = mezcla de A con B (peso de B), hueso a hueso en espacio local. */
	void BlendInto(FCompactPose& A, const FCompactPose& B, float Weight)
	{
		const float W = FMath::Clamp(Weight, 0.f, 1.f);
		if (W <= 0.001f) { return; }
		for (const FCompactPoseBoneIndex I : A.ForEachBoneIndex())
		{
			const FTransform Current = A[I];
			A[I].Blend(Current, B[I], W);
		}
	}

	void SampleClip(const UAnimSequence* Clip, float Time, FPoseContext& Out)
	{
		if (!Clip)
		{
			Out.ResetToRefPose();
			return;
		}
		const float Length = FMath::Max(0.01f, Clip->GetPlayLength());
		FAnimationPoseData Data(Out);
		Clip->GetAnimationPose(Data, FAnimExtractContext(static_cast<double>(FMath::Fmod(Time, Length)), false));
	}

	/** Deja la cadera de los clips en su sitio (sin avance propio: lo mueve el personaje). */
	void KeepHipsInPlace(FCompactPose& Pose, const FBones& B)
	{
		if (!B.Hips.IsValid()) { return; }
		const FTransform& Ref = Pose.GetBoneContainer().GetRefPoseTransform(B.Hips);
		FVector T = Pose[B.Hips].GetTranslation();
		T.X = Ref.GetTranslation().X;
		T.Y = Ref.GetTranslation().Y;
		Pose[B.Hips].SetTranslation(T);
	}

	void Lift(FCompactPose& Pose, const FBones& B, float Units)
	{
		if (B.Hips.IsValid()) { Pose[B.Hips].AddToTranslation(FVector(0.0, 0.0, Units)); }
	}

	// ── Poses (sobre la postura en T; brazo izquierdo a lo largo de +X, derecho de -X, piernas hacia -Z) ──
	// Brazo izquierdo: abajo = +Y, arriba = -Y, adelante = +Z. Derecho: al revés en Y y en Z.
	// Piernas: adelante = +X. Rodilla (pierna baja hacia atrás) = -X. Espalda hacia delante = -X. Cabeza arriba = +X.

	void ArmsRelaxed(FCompactPose& P, const FBones& B, float Down = 68.f)
	{
		Turn(P, B.LArm, AxisY, Down);
		Turn(P, B.RArm, AxisY, -Down);
	}

	void PoseAir(FCompactPose& P, const FBones& B, const FTNTurtleAnimFrame& F)
	{
		const float Flail = 8.f * FMath::Sin(F.Clock * 10.f);
		const float Up = 35.f + 25.f * F.Falling;
		Turn(P, B.LArm, AxisY, -Up + Flail);
		Turn(P, B.LArm, AxisZ, 20.f);
		Turn(P, B.RArm, AxisY, Up + Flail);
		Turn(P, B.RArm, AxisZ, -20.f);
		Turn(P, B.LFore, AxisZ, 25.f);
		Turn(P, B.RFore, AxisZ, -25.f);
		Turn(P, B.LUp, AxisX, 50.f - 15.f * F.Falling);
		Turn(P, B.RUp, AxisX, 38.f - 10.f * F.Falling);
		Turn(P, B.LLeg, AxisX, -80.f);
		Turn(P, B.RLeg, AxisX, -65.f);
		Turn(P, B.Spine, AxisX, 6.f);
	}

	/** Panzazo (el personaje ya tumba la malla -80°): brazos por encima de la cabeza (hacia delante en el mundo), piernas estiradas. */
	void PoseDive(FCompactPose& P, const FBones& B, const FTNTurtleAnimFrame& F)
	{
		Turn(P, B.LArm, AxisY, -86.f);
		Turn(P, B.RArm, AxisY, 86.f);
		const float Kick = 9.f * FMath::Sin(F.Clock * 14.f);
		Turn(P, B.LUp, AxisX, Kick);
		Turn(P, B.RUp, AxisX, -Kick);
		Turn(P, B.Neck, AxisX, 24.f);
		Turn(P, B.Head, AxisX, 18.f);
	}

	void PoseSwim(FCompactPose& P, const FBones& B, const FTNTurtleAnimFrame& F)
	{
		const float T = F.Clock * TwoPiF * 0.85f;
		Turn(P, B.Spine, AxisX, -22.f);
		Turn(P, B.Head, AxisX, 24.f);
		Turn(P, B.LArm, AxisY, 12.f + 30.f * FMath::Cos(T));
		Turn(P, B.LArm, AxisZ, 50.f + 35.f * FMath::Sin(T));
		Turn(P, B.RArm, AxisY, -12.f - 30.f * FMath::Cos(T));
		Turn(P, B.RArm, AxisZ, -50.f - 35.f * FMath::Sin(T));
		Turn(P, B.LFore, AxisZ, 20.f * (1.f - FMath::Sin(T)));
		Turn(P, B.RFore, AxisZ, -20.f * (1.f - FMath::Sin(T)));
		Turn(P, B.LUp, AxisX, 25.f * FMath::Sin(3.f * T));
		Turn(P, B.RUp, AxisX, -25.f * FMath::Sin(3.f * T));
		Turn(P, B.LLeg, AxisX, -20.f);
		Turn(P, B.RLeg, AxisX, -20.f);
	}

	/** Lleva a otra tortuga en alto. */
	void PoseCarry(FCompactPose& P, const FBones& B)
	{
		Turn(P, B.LArm, AxisY, -80.f);
		Turn(P, B.LArm, AxisZ, 10.f);
		Turn(P, B.RArm, AxisY, 80.f);
		Turn(P, B.RArm, AxisZ, -10.f);
		Turn(P, B.LFore, AxisY, -25.f);
		Turn(P, B.RFore, AxisY, 25.f);
		Turn(P, B.Spine, AxisX, 4.f);
		Turn(P, B.LUp, AxisX, 10.f);
		Turn(P, B.RUp, AxisX, 10.f);
		Turn(P, B.LLeg, AxisX, -15.f);
		Turn(P, B.RLeg, AxisX, -15.f);
	}

	/** La llevan en alto y patalea. */
	void PoseCarried(FCompactPose& P, const FBones& B, const FTNTurtleAnimFrame& F)
	{
		const float T = F.Clock;
		Turn(P, B.LArm, AxisY, 20.f + 35.f * FMath::Sin(T * 12.f));
		Turn(P, B.LArm, AxisZ, 20.f * FMath::Cos(T * 9.f));
		Turn(P, B.RArm, AxisY, -20.f - 35.f * FMath::Sin(T * 12.f + 1.f));
		Turn(P, B.RArm, AxisZ, -20.f * FMath::Cos(T * 9.f + 0.5f));
		Turn(P, B.LUp, AxisX, 40.f * FMath::Sin(T * 10.f));
		Turn(P, B.RUp, AxisX, -40.f * FMath::Sin(T * 10.f));
		Turn(P, B.LLeg, AxisX, -30.f);
		Turn(P, B.RLeg, AxisX, -30.f);
		Turn(P, B.Head, AxisY, 15.f * FMath::Sin(T * 7.f));
	}

	/** Tumbada: brazos y patas flojos y abiertos, la cabeza caída a un lado. */
	void PoseDown(FCompactPose& P, const FBones& B)
	{
		Turn(P, B.LArm, AxisY, 22.f);
		Turn(P, B.LArm, AxisZ, -10.f);
		Turn(P, B.RArm, AxisY, -22.f);
		Turn(P, B.RArm, AxisZ, 10.f);
		Turn(P, B.LUp, AxisY, -20.f);
		Turn(P, B.RUp, AxisY, 20.f);
		Turn(P, B.Head, AxisY, 25.f);
		Turn(P, B.Head, AxisX, -10.f);
	}

	/** Emotes del catálogo (0-9) salvo la fiesta (9), que es el clip de gritar (con rebote aparte). */
	void PoseEmote(FCompactPose& P, const FBones& B, const FTNTurtleAnimFrame& F)
	{
		const float T = F.EmoteTime;
		switch (F.Emote)
		{
		case 0: // Saludar: brazo derecho arriba y la mano que va y viene.
			ArmsRelaxed(P, B);
			Turn(P, B.RArm, AxisY, 68.f + 125.f);
			Turn(P, B.RFore, AxisY, 25.f * FMath::Sin(T * 14.f));
			Turn(P, B.Head, AxisY, -8.f);
			break;
		case 1: // Aplauso: brazos en V en alto y palmadas rápidas.
			Turn(P, B.LArm, AxisY, -55.f);
			Turn(P, B.RArm, AxisY, 55.f);
			Turn(P, B.LFore, AxisY, -(30.f + 25.f * (0.5f + 0.5f * FMath::Sin(T * 16.f))));
			Turn(P, B.RFore, AxisY, 30.f + 25.f * (0.5f + 0.5f * FMath::Sin(T * 16.f)));
			Lift(P, B, 1.5f * FMath::Abs(FMath::Sin(T * 8.f)));
			break;
		case 2: // Helicóptero: brazos en cruz y el cuerpo de arriba girando sin parar.
			Turn(P, B.Spine, AxisZ, FMath::Fmod(T * 720.f, 360.f));
			Lift(P, B, 2.f * FMath::Abs(FMath::Sin(T * 6.f)));
			break;
		case 3: // Palmada potente: brazos delante y una palmada fuerte cada 0,7 s.
		{
			const float Phase = FMath::Fmod(T, 0.7f) / 0.7f;
			const float Clap = Phase < 0.25f ? Phase / 0.25f : FMath::Max(0.f, 1.f - (Phase - 0.25f) / 0.5f);
			Turn(P, B.LArm, AxisY, 10.f);
			Turn(P, B.LArm, AxisZ, 55.f + 35.f * Clap);
			Turn(P, B.RArm, AxisY, -10.f);
			Turn(P, B.RArm, AxisZ, -55.f - 35.f * Clap);
			Turn(P, B.Spine, AxisX, -8.f * Clap);
			break;
		}
		case 4: // Aplaudir: palmaditas rápidas delante del pecho.
			Turn(P, B.LArm, AxisY, 35.f);
			Turn(P, B.LArm, AxisZ, 70.f + 15.f * FMath::Sin(T * 18.f));
			Turn(P, B.RArm, AxisY, -35.f);
			Turn(P, B.RArm, AxisZ, -70.f - 15.f * FMath::Sin(T * 18.f));
			Turn(P, B.LFore, AxisZ, 40.f);
			Turn(P, B.RFore, AxisZ, -40.f);
			break;
		case 5: // Baile irlandés: brazos tiesos pegados al cuerpo y patadas alternas.
		{
			const float S = FMath::Sin(T * TwoPiF * 2.5f);
			Turn(P, B.LArm, AxisY, 85.f);
			Turn(P, B.RArm, AxisY, -85.f);
			Turn(P, B.LUp, AxisX, 50.f * FMath::Max(0.f, S));
			Turn(P, B.RUp, AxisX, 50.f * FMath::Max(0.f, -S));
			Turn(P, B.LLeg, AxisX, -20.f * FMath::Max(0.f, S));
			Turn(P, B.RLeg, AxisX, -20.f * FMath::Max(0.f, -S));
			Lift(P, B, 2.f * FMath::Abs(S));
			break;
		}
		case 6: // Flotar (Superman): la pose del panzazo meciéndose.
			PoseDive(P, B, F);
			Lift(P, B, 3.f * FMath::Sin(T * 3.f));
			break;
		case 7: // Señalar: brazo derecho al frente.
			ArmsRelaxed(P, B);
			Turn(P, B.RArm, AxisY, -68.f + 10.f);
			Turn(P, B.RArm, AxisZ, -85.f);
			Turn(P, B.Head, AxisZ, -10.f);
			break;
		case 8: // Modo loco: todo se mueve a su aire.
			Turn(P, B.LArm, AxisY, 60.f * FMath::Sin(T * 7.f));
			Turn(P, B.LArm, AxisZ, 50.f * FMath::Sin(T * 5.3f));
			Turn(P, B.RArm, AxisY, -60.f * FMath::Sin(T * 6.1f + 1.f));
			Turn(P, B.RArm, AxisZ, -50.f * FMath::Sin(T * 4.7f));
			Turn(P, B.LUp, AxisX, 45.f * FMath::Sin(T * 8.2f));
			Turn(P, B.RUp, AxisX, -45.f * FMath::Sin(T * 7.7f));
			Turn(P, B.Spine, AxisZ, 30.f * FMath::Sin(T * 3.f));
			Turn(P, B.Head, AxisX, 25.f * FMath::Sin(T * 9.f));
			break;
		default:
			break;
		}
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Evaluación
// ─────────────────────────────────────────────────────────────────────────────

bool FTNTurtleAnimProxy::Evaluate(FPoseContext& Output)
{
	using namespace TNTurtleAnim;
	const FTNTurtleAnimFrame& F = Frame;
	const FBones B = ResolveBones(Output.Pose.GetBoneContainer());

	// 1. Locomoción: espera, andar y correr (como ABS_Walk, por velocidad).
	SampleClip(IdleClip, F.IdleTime, Output);
	if (F.WalkW > 0.01f)
	{
		FPoseContext Walk(Output);
		SampleClip(WalkClip, F.WalkTime, Walk);
		if (F.RunW > 0.01f)
		{
			FPoseContext Run(Output);
			SampleClip(RunClip, F.RunTime, Run);
			BlendInto(Walk.Pose, Run.Pose, F.RunW);
		}
		BlendInto(Output.Pose, Walk.Pose, F.WalkW);
	}
	KeepHipsInPlace(Output.Pose, B);

	// 2. La fiesta es el clip de gritar (con rebote).
	if (F.Emote == 9 && F.EmoteW > 0.01f && CheerClip)
	{
		FPoseContext Cheer(Output);
		SampleClip(CheerClip, F.EmoteTime, Cheer);
		KeepHipsInPlace(Cheer.Pose, B);
		Lift(Cheer.Pose, B, 2.f * FMath::Abs(FMath::Sin(F.EmoteTime * TwoPiF * 2.f)));
		BlendInto(Output.Pose, Cheer.Pose, F.EmoteW);
	}

	// 3. Poses de estado sobre la postura en T, mezcladas por su peso.
	auto Layer = [&](float Weight, TFunctionRef<void(FCompactPose&)> Build)
	{
		if (Weight < 0.01f) { return; }
		FPoseContext Target(Output);
		Target.ResetToRefPose();
		Build(Target.Pose);
		BlendInto(Output.Pose, Target.Pose, Weight);
	};
	Layer(F.AirW, [&](FCompactPose& P) { PoseAir(P, B, F); });
	Layer(F.SwimW, [&](FCompactPose& P) { PoseSwim(P, B, F); });
	Layer(F.DiveW, [&](FCompactPose& P) { PoseDive(P, B, F); });
	Layer(F.CarryW, [&](FCompactPose& P) { PoseCarry(P, B); });
	Layer(F.CarriedW, [&](FCompactPose& P) { PoseCarried(P, B, F); });
	Layer(F.DownW, [&](FCompactPose& P) { PoseDown(P, B); });
	if (F.Emote >= 0 && F.Emote != 9)
	{
		Layer(F.EmoteW, [&](FCompactPose& P) { PoseEmote(P, B, F); });
	}

	// 4. Capas encima de lo que haya: inclinación al correr y en las curvas, cansancio, lanzamiento y caparazón.
	Turn(Output.Pose, B.Spine, AxisX, F.LeanPitch);
	Turn(Output.Pose, B.Spine, AxisY, F.LeanRoll);
	if (F.TiredW > 0.01f)
	{
		Turn(Output.Pose, B.Spine1, AxisX, -12.f * F.TiredW);
		Turn(Output.Pose, B.Spine2, AxisX, -3.f * FMath::Sin(F.Clock * 5.f) * F.TiredW);
		Turn(Output.Pose, B.Head, AxisX, -10.f * F.TiredW);
	}
	if (F.ThrowT >= 0.f && F.ThrowT <= 0.45f)
	{
		// Toma impulso con el brazo arriba y atrás y lo suelta hacia delante.
		const float Ph = F.ThrowT / 0.45f;
		const float Raise = Ph < 0.3f ? Ph / 0.3f : 1.f - (Ph - 0.3f) / 0.7f;
		const float Swing = Ph < 0.3f ? 30.f * Ph / 0.3f : 30.f - 100.f * (Ph - 0.3f) / 0.7f;
		Turn(Output.Pose, B.RArm, AxisY, 110.f * Raise);
		Turn(Output.Pose, B.RArm, AxisZ, Swing);
		if (F.bThrowBoth)
		{
			Turn(Output.Pose, B.LArm, AxisY, -110.f * Raise);
			Turn(Output.Pose, B.LArm, AxisZ, -Swing);
		}
	}
	if (F.ShellW > 0.01f)
	{
		// Se mete en el caparazón: cabeza, brazos y patas encogen hacia el cuerpo y el cuerpo baja al suelo.
		const FVector Tiny(FMath::Lerp(1.f, 0.12f, F.ShellW));
		for (const FCompactPoseBoneIndex Limb : { B.LArm, B.RArm, B.LUp, B.RUp, B.Neck })
		{
			if (Limb.IsValid()) { Output.Pose[Limb].SetScale3D(Tiny); }
		}
		Turn(Output.Pose, B.Spine, AxisX, -10.f * F.ShellW);
		Lift(Output.Pose, B, -14.f * F.ShellW);
	}
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Hilo de juego
// ─────────────────────────────────────────────────────────────────────────────

void UTN_TurtleAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	IdleAnim = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Animations/Character/TortugaDemo/Anim/Old_Man_Idle.Old_Man_Idle"));
	WalkAnim = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Animations/Character/TortugaDemo/Anim/Walking.Walking"));
	RunAnim = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Animations/Character/TortugaDemo/Anim/Drunk_Run_Forward.Drunk_Run_Forward"));
	CheerAnim = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Animations/Character/TortugaDemo/Anim/Yelling.Yelling"));
	if (const APawn* Owner = TryGetPawnOwner()) { PrevYaw = Owner->GetActorRotation().Yaw; }
}

FAnimInstanceProxy* UTN_TurtleAnimInstance::CreateAnimInstanceProxy()
{
	return new FTNTurtleAnimProxy(this);
}

void UTN_TurtleAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
	delete static_cast<FTNTurtleAnimProxy*>(InProxy);
}

void UTN_TurtleAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	using namespace TNTurtleAnim;
	Super::NativeUpdateAnimation(DeltaSeconds);
	const float Dt = FMath::Max(DeltaSeconds, 1e-4f);
	FTNTurtleAnimFrame& F = Frame;
	F.Clock += Dt;
	F.IdleTime += Dt;

	const ATortugaCharacter* Turtle = Cast<ATortugaCharacter>(TryGetPawnOwner());
	const UCharacterMovementComponent* Move = Turtle ? Turtle->GetCharacterMovement() : nullptr;
	const FVector Velocity = Turtle ? Turtle->GetVelocity() : FVector::ZeroVector;
	const float Speed = static_cast<float>(Velocity.Size2D());
	auto Ease = [Dt](float& Value, bool bOn, float Rate) { Value = FMath::FInterpTo(Value, bOn ? 1.f : 0.f, Dt, Rate); };

	// Locomoción: pesos por velocidad y fases que avanzan al ritmo de los pasos.
	F.WalkW = FMath::FInterpTo(F.WalkW, FMath::Clamp(Speed / 150.f, 0.f, 1.f), Dt, 8.f);
	F.RunW = FMath::FInterpTo(F.RunW, FMath::Clamp((Speed - 480.f) / 250.f, 0.f, 1.f), Dt, 6.f);
	F.WalkTime += Dt * FMath::Clamp(Speed / WalkNatural, 0.6f, 1.6f);
	F.RunTime += Dt * FMath::Clamp(Speed / RunNatural, 0.7f, 1.5f);

	const bool bSwim = Move && Move->IsSwimming();
	const bool bDive = Turtle && Turtle->IsDiving();
	const bool bAir = Move && Move->IsFalling() && !bDive && !bSwim;
	const UTN_CarryComponent* Carry = Turtle ? Turtle->GetCarryComponent() : nullptr;
	const UTN_StaminaComponent* Stamina = Turtle ? Turtle->GetStaminaComponent() : nullptr;
	const bool bCarrying = Carry && Carry->IsCarrying();
	Ease(F.AirW, bAir, 12.f);
	F.Falling = FMath::FInterpTo(F.Falling, (bAir && Velocity.Z < -200.0) ? 1.f : 0.f, Dt, 6.f);
	Ease(F.DiveW, bDive, 14.f);
	Ease(F.SwimW, bSwim, 6.f);
	Ease(F.ShellW, Turtle && Turtle->IsInShell(), 10.f);
	Ease(F.CarryW, bCarrying, 8.f);
	Ease(F.CarriedW, Carry && Carry->IsBeingCarried(), 8.f);
	Ease(F.DownW, Turtle && Turtle->IsKnockedDown(), 6.f);
	Ease(F.TiredW, Stamina && Stamina->IsExhausted(), 4.f);

	// Lanzamiento: al soltar a quien llevaba en alto, el golpe de brazos hacia delante.
	if (bWasCarrying && !bCarrying)
	{
		F.ThrowT = 0.f;
		F.bThrowBoth = true;
	}
	bWasCarrying = bCarrying;
	if (F.ThrowT >= 0.f)
	{
		F.ThrowT += Dt;
		if (F.ThrowT > 0.5f) { F.ThrowT = -1.f; }
	}

	// Emote: entra suave y, al acabar, sale suave con el último.
	const int32 Emote = Turtle ? Turtle->GetActiveEmoteIndex() : -1;
	if (Emote >= 0 && Emote <= 9)
	{
		if (Emote != LastEmote) { F.EmoteW = 0.f; }
		F.Emote = Emote;
		F.EmoteTime = Turtle->GetEmoteTime();
		LastEmote = Emote;
		Ease(F.EmoteW, true, 10.f);
	}
	else
	{
		Ease(F.EmoteW, false, 8.f);
		F.EmoteTime += Dt;
		if (F.EmoteW < 0.01f) { F.Emote = -1; LastEmote = -1; }
	}

	// Inclinación: hacia dentro de las curvas y un poco hacia delante al correr.
	const float Yaw = Turtle ? static_cast<float>(Turtle->GetActorRotation().Yaw) : PrevYaw;
	const float YawRate = FMath::FindDeltaAngleDegrees(PrevYaw, Yaw) / Dt;
	PrevYaw = Yaw;
	const float RollTarget = FMath::Clamp(-YawRate * 0.03f * FMath::Clamp(Speed / 400.f, 0.f, 1.f), -12.f, 12.f);
	F.LeanRoll = FMath::FInterpTo(F.LeanRoll, bSwim || bDive ? 0.f : RollTarget, Dt, 6.f);
	F.LeanPitch = FMath::FInterpTo(F.LeanPitch, -7.f * F.RunW, Dt, 4.f);

	FTNTurtleAnimProxy& Proxy = GetProxyOnGameThread<FTNTurtleAnimProxy>();
	Proxy.Frame = F;
	Proxy.IdleClip = IdleAnim;
	Proxy.WalkClip = WalkAnim;
	Proxy.RunClip = RunAnim;
	Proxy.CheerClip = CheerAnim;
}
