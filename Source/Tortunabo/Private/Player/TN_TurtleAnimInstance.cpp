#include "Player/TN_TurtleAnimInstance.h"
#include "Player/TortugaCharacter.h"
#include "Player/TN_CarryComponent.h"
#include "Player/TN_StaminaComponent.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimSequence.h"
#include "BoneContainer.h"
#include "BonePose.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace TNTurtleAnim
{
	/** Ejes del espacio de la malla: mira a +Y, arriba +Z, su izquierda +X. */
	const FVector AxisX(1.0, 0.0, 0.0);
	const FVector AxisY(0.0, 1.0, 0.0);
	const FVector AxisZ(0.0, 0.0, 1.0);

	/**
	 * Velocidad (unidades de la malla por segundo) a la que el clip de andar no patina: el pie apoyado barre 41 u/s en
	 * el clip y la zancada se alarga un 25 % (Amplify). Se multiplica por la escala del componente (2,5 en el personaje).
	 */
	constexpr float WalkNaturalUnits = 51.f;
	/** Largo de la pierna (cadera a punta del pie), en unidades de la malla: da la amplitud del paso de la carrera. */
	constexpr float LegUnits = 24.f;
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
		FCompactPoseBoneIndex LFoot = FCompactPoseBoneIndex(INDEX_NONE);
		FCompactPoseBoneIndex RUp = FCompactPoseBoneIndex(INDEX_NONE);
		FCompactPoseBoneIndex RLeg = FCompactPoseBoneIndex(INDEX_NONE);
		FCompactPoseBoneIndex RFoot = FCompactPoseBoneIndex(INDEX_NONE);
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
		Out.LFoot = FindBone(Bones, TEXT("LeftFoot"));
		Out.RUp = FindBone(Bones, TEXT("RightUpLeg"));
		Out.RLeg = FindBone(Bones, TEXT("RightLeg"));
		Out.RFoot = FindBone(Bones, TEXT("RightFoot"));
		return Out;
	}

	/** Transformación de un hueso en el espacio de la malla (composición de las locales desde la raíz). */
	FTransform ComponentSpace(const FCompactPose& Pose, FCompactPoseBoneIndex Bone)
	{
		FTransform T = Pose[Bone];
		for (FCompactPoseBoneIndex P = Pose.GetParentBoneIndex(Bone); P.IsValid(); P = Pose.GetParentBoneIndex(P))
		{
			T = T * Pose[P];
		}
		return T;
	}

	/** Lleva la articulación de un hueso (con sus hijos) hacia un punto del espacio de la malla. */
	void MoveJointToward(FCompactPose& Pose, FCompactPoseBoneIndex Bone, const FVector& Target, float Alpha)
	{
		if (!Bone.IsValid() || Alpha <= 0.001f) { return; }
		const FCompactPoseBoneIndex Parent = Pose.GetParentBoneIndex(Bone);
		const FTransform ParentCS = Parent.IsValid() ? ComponentSpace(Pose, Parent) : FTransform::Identity;
		const FVector Current = (Pose[Bone] * ParentCS).GetLocation();
		Pose[Bone].SetTranslation(ParentCS.InverseTransformPosition(FMath::Lerp(Current, Target, static_cast<double>(Alpha))));
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

	/** Exagera el giro local de un hueso respecto de la postura de referencia (K > 1 alarga la zancada de un clip). */
	void Amplify(FCompactPose& Pose, FCompactPoseBoneIndex Bone, double K)
	{
		if (!Bone.IsValid()) { return; }
		const FQuat Ref = Pose.GetBoneContainer().GetRefPoseTransform(Bone).GetRotation();
		FQuat Delta = Pose[Bone].GetRotation() * Ref.Inverse();
		if (Delta.W < 0.0) { Delta = FQuat(-Delta.X, -Delta.Y, -Delta.Z, -Delta.W); }
		FVector Axis;
		double Angle = 0.0;
		Delta.ToAxisAndAngle(Axis, Angle);
		Pose[Bone].SetRotation((FQuat(Axis, Angle * K) * Ref).GetNormalized());
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

	/**
	 * Carrera del sprint (RunTime cuenta ciclos): zancada larga con fase de vuelo, la rodilla recogida mientras la
	 * pierna vuelve hacia delante, braceo contrario a las piernas con los codos doblados y el tronco hacia delante.
	 */
	void PoseRun(FCompactPose& P, const FBones& B, const FTNTurtleAnimFrame& F)
	{
		const float Ph = F.RunTime * TwoPiF;
		const float S = FMath::Sin(Ph);
		const float C = FMath::Cos(Ph);
		const float K = F.SprintW;
		Turn(P, B.Spine, AxisX, -(5.f + 5.f * K));
		Turn(P, B.Spine1, AxisZ, 8.f * S);
		Turn(P, B.Head, AxisX, 8.f + 4.f * K);
		// Piernas: la izquierda va hacia delante con S > 0 y recoge la rodilla mientras avanza (C > 0).
		const float Stride = F.RunStride;
		Turn(P, B.LUp, AxisX, 6.f + Stride * S);
		Turn(P, B.RUp, AxisX, 6.f - Stride * S);
		Turn(P, B.LLeg, AxisX, -(14.f + 80.f * FMath::Pow(FMath::Max(0.f, C), 1.4f)));
		Turn(P, B.RLeg, AxisX, -(14.f + 80.f * FMath::Pow(FMath::Max(0.f, -C), 1.4f)));
		Turn(P, B.LFoot, AxisX, -25.f * FMath::Max(0.f, C));
		Turn(P, B.RFoot, AxisX, -25.f * FMath::Max(0.f, -C));
		// Brazos abajo, codos a unos 80° y braceo al revés que las piernas.
		const float Arm = 38.f + 10.f * K;
		Turn(P, B.LArm, AxisY, 74.f);
		Turn(P, B.RArm, AxisY, -74.f);
		Turn(P, B.LArm, AxisX, -Arm * S);
		Turn(P, B.RArm, AxisX, Arm * S);
		Turn(P, B.LFore, AxisX, 80.f);
		Turn(P, B.RFore, AxisX, 80.f);
		// Más alta en el vuelo (piernas abiertas) y más baja al pisar.
		Lift(P, B, 1.6f * FMath::Abs(S) - 0.4f);
	}

	/**
	 * Panzazo (el personaje ya tumba la malla -80° y la aplasta un poco): brazos por delante de la cabeza apoyados en
	 * el suelo, que reman hacia los lados, y piernas estiradas hacia atrás que patalean con los pies en el suelo. En la
	 * malla, +Y (la tripa) mira al suelo: los giros hacia +Y bajan brazos y piernas hasta apoyarlos.
	 */
	void PoseDive(FCompactPose& P, const FBones& B, const FTNTurtleAnimFrame& F)
	{
		const float Paddle = 0.5f + 0.5f * FMath::Sin(F.Clock * 11.f);
		Turn(P, B.LArm, AxisY, -86.f + 32.f * Paddle);
		Turn(P, B.RArm, AxisY, 86.f - 32.f * Paddle);
		Turn(P, B.LArm, AxisX, -20.f);
		Turn(P, B.RArm, AxisX, -20.f);
		Turn(P, B.LFore, AxisY, -12.f * Paddle);
		Turn(P, B.RFore, AxisY, 12.f * Paddle);
		const float Kick = 14.f * FMath::Sin(F.Clock * 14.f);
		Turn(P, B.LUp, AxisX, 14.f + Kick);
		Turn(P, B.RUp, AxisX, 14.f - Kick);
		Turn(P, B.LFoot, AxisX, -60.f);
		Turn(P, B.RFoot, AxisX, -60.f);
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
		case 0: // WAZAAA: brazo derecho en alto (por fuera de la cabeza) y la mano que saluda.
			ArmsRelaxed(P, B);
			Turn(P, B.RArm, AxisY, 68.f + 58.f);
			Turn(P, B.RArm, AxisZ, -10.f);
			Turn(P, B.RFore, AxisY, -6.f + 20.f * FMath::Sin(T * 14.f));
			Turn(P, B.Head, AxisY, -8.f);
			break;
		case 1: // HAPPIE: brazos arriba y palmadas encima de la cabeza (las manos se juntan en cada una).
		{
			const float Pulse = 0.5f + 0.5f * FMath::Sin(T * 16.f);
			Turn(P, B.LArm, AxisY, -78.f);
			Turn(P, B.LArm, AxisZ, -10.f);
			Turn(P, B.RArm, AxisY, 78.f);
			Turn(P, B.RArm, AxisZ, 10.f);
			Turn(P, B.LFore, AxisY, -(26.f + 14.f * Pulse));
			Turn(P, B.RFore, AxisY, 26.f + 14.f * Pulse);
			Lift(P, B, 1.5f * FMath::Abs(FMath::Sin(T * 8.f)));
			break;
		}
		case 2: // PARACOPTER: brazos en cruz y el cuerpo de arriba girando sin parar (tres vueltas por segundo).
			Turn(P, B.Spine, AxisZ, FMath::Fmod(T * 1080.f, 360.f));
			Lift(P, B, 2.f * FMath::Abs(FMath::Sin(T * 6.f)));
			break;
		case 3: // SAX-O: brazos delante y una palmada fuerte cada 0,7 s con las manos juntas en el centro.
		{
			const float Phase = FMath::Fmod(T, 0.7f) / 0.7f;
			const float Clap = Phase < 0.25f ? Phase / 0.25f : FMath::Max(0.f, 1.f - (Phase - 0.25f) / 0.5f);
			Turn(P, B.LArm, AxisY, 12.f);
			Turn(P, B.LArm, AxisZ, 60.f + 42.f * Clap);
			Turn(P, B.RArm, AxisY, -12.f);
			Turn(P, B.RArm, AxisZ, -60.f - 42.f * Clap);
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
		case 5: // RUN: carrera ninja, el tronco hacia delante, los brazos estirados hacia atrás y rodillas arriba.
		{
			const float S = FMath::Sin(T * TwoPiF * 2.5f);
			Turn(P, B.Spine, AxisX, -20.f);
			Turn(P, B.Head, AxisX, 16.f);
			Turn(P, B.LArm, AxisZ, -78.f);
			Turn(P, B.LArm, AxisX, 18.f);
			Turn(P, B.RArm, AxisZ, 78.f);
			Turn(P, B.RArm, AxisX, 18.f);
			Turn(P, B.LUp, AxisX, 50.f * FMath::Max(0.f, S));
			Turn(P, B.RUp, AxisX, 50.f * FMath::Max(0.f, -S));
			Turn(P, B.LLeg, AxisX, -20.f * FMath::Max(0.f, S));
			Turn(P, B.RLeg, AxisX, -20.f * FMath::Max(0.f, -S));
			Lift(P, B, 2.f * FMath::Abs(S));
			break;
		}
		case 6: // SUPERKIRK: vuela como Superman, todo el cuerpo inclinado 45°, brazos al frente en V y pies en punta.
		{
			Turn(P, B.LArm, AxisY, -65.f);
			Turn(P, B.LArm, AxisZ, 10.f);
			Turn(P, B.RArm, AxisY, 65.f);
			Turn(P, B.RArm, AxisZ, -10.f);
			const float Kick = 6.f * FMath::Sin(T * 5.f);
			Turn(P, B.LUp, AxisX, Kick);
			Turn(P, B.RUp, AxisX, -Kick);
			Turn(P, B.LFoot, AxisX, -60.f);
			Turn(P, B.RFoot, AxisX, -60.f);
			Turn(P, B.Neck, AxisX, 14.f);
			Turn(P, B.Head, AxisX, 10.f);
			Turn(P, B.Hips, AxisX, -45.f);
			Lift(P, B, 8.f + 2.f * FMath::Sin(T * 3.f));
			break;
		}
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

	// 1. Locomoción: espera y andar con los clips (como ABS_Walk, por velocidad) y la carrera del sprint encima.
	SampleClip(IdleClip, F.IdleTime, Output);
	if (F.WalkW > 0.01f)
	{
		FPoseContext Walk(Output);
		SampleClip(WalkClip, F.WalkTime, Walk);
		// Zancada más larga que la del clip (las patas de la tortuga son cortas): con su ritmo, los pies no patinan.
		Amplify(Walk.Pose, B.LUp, 1.3);
		Amplify(Walk.Pose, B.RUp, 1.3);
		Amplify(Walk.Pose, B.LLeg, 1.15);
		Amplify(Walk.Pose, B.RLeg, 1.15);
		BlendInto(Output.Pose, Walk.Pose, F.WalkW);
	}
	KeepHipsInPlace(Output.Pose, B);
	if (F.RunW > 0.01f)
	{
		FPoseContext Run(Output);
		Run.ResetToRefPose();
		PoseRun(Run.Pose, B, F);
		BlendInto(Output.Pose, Run.Pose, F.RunW);
	}

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
		// Se mete en el caparazón: cabeza, brazos y patas encogen y se meten dentro del cuerpo (hacia el centro del
		// tronco) y el caparazón baja hasta apoyarse en el suelo.
		const FVector Tiny(FMath::Lerp(1.f, 0.08f, F.ShellW));
		const FVector Inside = B.Spine1.IsValid() ? ComponentSpace(Output.Pose, B.Spine1).GetLocation() : FVector(0.0, 1.0, 29.0);
		for (const FCompactPoseBoneIndex Limb : { B.LArm, B.RArm, B.LUp, B.RUp, B.Neck })
		{
			if (!Limb.IsValid()) { continue; }
			MoveJointToward(Output.Pose, Limb, Inside, 0.85f * F.ShellW);
			Output.Pose[Limb].SetScale3D(Tiny);
		}
		Turn(Output.Pose, B.Spine, AxisX, -6.f * F.ShellW);
		Lift(Output.Pose, B, -19.f * F.ShellW);
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

	// Locomoción: pesos por velocidad y fases que avanzan al ritmo de los pasos (andar hasta la velocidad normal,
	// 4,5 m/s; la carrera entra con el sprint).
	const USkeletalMeshComponent* SkelMesh = GetSkelMeshComponent();
	const float Scale = SkelMesh ? FMath::Max(0.1f, static_cast<float>(SkelMesh->GetComponentScale().Z)) : 2.5f;
	F.WalkW = FMath::FInterpTo(F.WalkW, FMath::Clamp(Speed / 150.f, 0.f, 1.f), Dt, 8.f);
	// La carrera entra con el sprint (o al pasar un 8 % de la velocidad de andar del nivel: en el lobby es 2 m/s).
	const UTN_StaminaComponent* StaminaComp = Turtle ? Turtle->GetStaminaComponent() : nullptr;
	const float WalkRef = StaminaComp ? FMath::Max(100.f, StaminaComp->GetWalkSpeed()) : 450.f;
	const bool bSprinting = StaminaComp && StaminaComp->IsSprinting() && Speed > WalkRef * 0.6f;
	const float RunTarget = FMath::Max(bSprinting ? 1.f : 0.f, FMath::Clamp((Speed - WalkRef * 1.08f) / (WalkRef * 0.3f), 0.f, 1.f));
	F.RunW = FMath::FInterpTo(F.RunW, RunTarget, Dt, 7.f);
	F.SprintW = FMath::FInterpTo(F.SprintW, FMath::Clamp((Speed - WalkRef * 1.3f) / (WalkRef * 0.6f), 0.f, 1.f), Dt, 4.f);
	F.WalkTime += Dt * FMath::Clamp(Speed / (WalkNaturalUnits * Scale), 0.5f, 4.5f);
	// Carrera: de 2 a 3,4 ciclos por segundo según la velocidad y la amplitud del paso que hace que el pie apoyado
	// barra el suelo a la velocidad del cuerpo (pierna de LegUnits * Scale).
	const float Cadence = FMath::Clamp(0.8f + Speed / 300.f, 2.f, 3.4f);
	F.RunTime += Dt * Cadence;
	F.RunStride = FMath::Clamp(FMath::RadiansToDegrees(Speed / (LegUnits * Scale * TwoPiF * Cadence)), 20.f, 48.f);

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
	Proxy.CheerClip = CheerAnim;
}
