#include "Vehicles/TN_BuggyMath.h"

DEFINE_LOG_CATEGORY(LogTNBuggy);

namespace TNBuggy
{
	float SlipAngleDeg(const FVector& Forward, const FVector& Velocity)
	{
		const FVector Flat(Velocity.X, Velocity.Y, 0.f);
		if (Flat.Size() < MinSlipSpeed)
		{
			return 0.f;
		}
		const FVector F = FVector(Forward.X, Forward.Y, 0.f).GetSafeNormal();
		const FVector V = Flat.GetSafeNormal();
		return FMath::RadiansToDegrees(FMath::Atan2(FVector::CrossProduct(F, V).Z, FVector::DotProduct(F, V)));
	}

	float AssistSteer(float Input, float SlipDeg, float Assist, float MaxAngleDeg)
	{
		if (FMath::Abs(SlipDeg) > MaxAssistedSlipDeg || MaxAngleDeg <= 0.f)
		{
			return FMath::Clamp(Input, -1.f, 1.f);
		}
		const float Correction = Assist * FMath::Clamp(SlipDeg / MaxAngleDeg, -1.f, 1.f);
		return FMath::Clamp(Input + Correction, -1.f, 1.f);
	}

	bool IsRise(float StepCm, bool bBothInContact, const FBumpTuning& Tuning)
	{
		return bBothInContact && StepCm >= Tuning.MinStepCm;
	}

	float KickSpeed(float StepCm, float ForwardSpeed, float WheelRadius, const FBumpTuning& Tuning)
	{
		if (WheelRadius <= 0.f || StepCm <= 0.f)
		{
			return 0.f;
		}
		const float Severity = FMath::Min(StepCm / WheelRadius, 1.f);
		return FMath::Clamp(Tuning.Scale * FMath::Abs(ForwardSpeed) * Severity, 0.f, Tuning.MaxKick);
	}

	FBumpStep BumpStep(const FWheelTrack& Track, float StepCm, float ForwardSpeed, float WheelRadius,
		bool bBothInContact, const FBumpTuning& Tuning)
	{
		const bool bRise = IsRise(StepCm, bBothInContact, Tuning);
		FBumpStep Out;
		Out.Kick = bRise ? 0.f : Track.PendingKick;
		Out.Track.bPrevRise = bRise;
		Out.Track.PendingKick = (bRise && !Track.bPrevRise) ? KickSpeed(StepCm, ForwardSpeed, WheelRadius, Tuning) : 0.f;
		return Out;
	}

	bool IsFlipped(float UpZ)
	{
		return UpZ < FlippedUpZ;
	}

	float AdvanceFlipped(float FlippedSeconds, float UpZ, float Dt)
	{
		return IsFlipped(UpZ) ? FlippedSeconds + FMath::Max(Dt, 0.f) : 0.f;
	}

	ESelfRight DecideSelfRight(float FlippedSeconds, bool bRequested, float ManualDelay, float AutoDelay)
	{
		if (FlippedSeconds <= 0.f)
		{
			return ESelfRight::None;
		}
		if (bRequested && FlippedSeconds >= ManualDelay)
		{
			return ESelfRight::Manual;
		}
		return FlippedSeconds >= AutoDelay ? ESelfRight::Auto : ESelfRight::None;
	}

	FTransform SelfRightTransform(const FTransform& Current, float LiftCm)
	{
		// La guiñada sale del morro proyectado en horizontal: boca abajo, el rotador puede traerla girada 180 grados.
		const FVector Forward = Current.GetRotation().GetForwardVector();
		const FVector FlatForward(Forward.X, Forward.Y, 0.f);
		const double Yaw = FlatForward.IsNearlyZero(0.01f) ? Current.Rotator().Yaw : FlatForward.Rotation().Yaw;
		const FRotator Upright(0.f, Yaw, 0.f);
		return FTransform(Upright, Current.GetLocation() + FVector::UpVector * LiftCm, Current.GetScale3D());
	}

	bool AdvanceHold(FHold& Hold, bool bPressed, float Dt, float HoldSeconds)
	{
		if (!bPressed)
		{
			Hold = FHold();
			return false;
		}
		Hold.Held += FMath::Max(Dt, 0.f);
		if (!Hold.bFired && Hold.Held >= HoldSeconds)
		{
			Hold.bFired = true;
			return true;
		}
		return false;
	}

	namespace
	{
		/** Ángulo con signo (rad) de Axis alrededor del que Current se aparta de la vertical, con la zona libre Free (rad). */
		double ExcessTilt(const FVector& Axis, const FVector& Current, double Free)
		{
			const FVector Vertical = (FVector::UpVector - Axis * (FVector::UpVector | Axis)).GetSafeNormal();
			if (Vertical.IsNearlyZero())
			{
				return 0.0;
			}
			const double Angle = FMath::Atan2(Axis | (Vertical ^ Current), Vertical | Current);
			return FMath::Sign(Angle) * FMath::Max(0.0, FMath::Abs(Angle) - Free);
		}
	}

	FVector AntiRollAccel(const FVector& Forward, const FVector& Up, const FVector& AngularVelocityRad, bool bAirborne,
		const FAntiRollTuning& Tuning)
	{
		if (IsFlipped(static_cast<float>(Up.Z)) || Tuning.Stiffness <= 0.f)
		{
			return FVector::ZeroVector;
		}
		const FVector F = Forward.GetSafeNormal();
		const FVector R = (Up ^ F).GetSafeNormal();
		const double FreeRoll = bAirborne ? 0.0 : FMath::DegreesToRadians(Tuning.GroundFreeRollDeg);
		const double FreePitch = bAirborne ? 0.0 : FMath::DegreesToRadians(Tuning.GroundFreePitchDeg);
		const double Roll = ExcessTilt(F, Up, FreeRoll);
		const double Pitch = ExcessTilt(R, Up, FreePitch);
		// El amortiguador solo actúa mientras hay exceso (o en el aire): en el suelo no frena el balanceo normal.
		const double RollRate = (Roll != 0.0 || bAirborne) ? (AngularVelocityRad | F) : 0.0;
		const double PitchRate = (Pitch != 0.0 || bAirborne) ? (AngularVelocityRad | R) : 0.0;
		const FVector Accel = F * (-Tuning.Stiffness * Roll - Tuning.Damping * RollRate)
			+ R * (-Tuning.Stiffness * Pitch - Tuning.Damping * PitchRate);
		return Accel.GetClampedToMaxSize(FMath::Max(0.f, Tuning.MaxAccel));
	}

	FLinearColor TeamColor(int32 Index)
	{
		static const FLinearColor Palette[] = {
			FLinearColor(0.90f, 0.20f, 0.15f), // rojo
			FLinearColor(0.15f, 0.45f, 0.95f), // azul
			FLinearColor(0.20f, 0.80f, 0.25f), // verde
			FLinearColor(0.98f, 0.80f, 0.10f), // amarillo
			FLinearColor(0.70f, 0.25f, 0.90f), // morado
			FLinearColor(1.00f, 0.50f, 0.05f), // naranja
			FLinearColor(0.10f, 0.85f, 0.85f), // turquesa
			FLinearColor(0.95f, 0.40f, 0.70f), // rosa
		};
		if (Index < 0)
		{
			return FLinearColor::White;
		}
		return Palette[Index % UE_ARRAY_COUNT(Palette)];
	}

	float SpeedCapDecel(float Speed, float Cap, float Gain)
	{
		const float Excess = FMath::Abs(Speed) - FMath::Max(Cap, 0.f);
		return Excess > 0.f ? Excess * FMath::Max(Gain, 0.f) : 0.f;
	}

	float SteerWobble(float TimeLeft, float Duration, float Amplitude, float Frequency)
	{
		if (TimeLeft <= 0.f || Duration <= 0.f)
		{
			return 0.f;
		}
		const float Elapsed = Duration - FMath::Min(TimeLeft, Duration);
		const float Fade = FMath::Clamp(TimeLeft / Duration, 0.f, 1.f);
		return Amplitude * Fade * FMath::Sin(2.f * UE_PI * Frequency * Elapsed + UE_HALF_PI);
	}
}
