// Lógica pura del buggy del Rally (sin mundo ni física): derrape asistido, golpe de rueda, enderezado, reaparición
// pulsando, tinte por equipo y frenado del charco. Port de FHYDriftMath y FHYBumpMath (HellYeah) con tests en
// Tortunabo.Rally.Buggy.*.
#pragma once

#include "CoreMinimal.h"

DECLARE_LOG_CATEGORY_EXTERN(LogTNBuggy, Log, All);

namespace TNBuggy
{
	// ── Derrape asistido ────────────────────────────────────────────────────────

	/** Por debajo de esta velocidad horizontal (cm/s) no se mide deriva. */
	constexpr float MinSlipSpeed = 100.f;

	/** Deriva a partir de la cual se deja de asistir (marcha atrás o trompo). */
	constexpr float MaxAssistedSlipDeg = 90.f;

	/**
	 * Ángulo con signo, en grados y en (-180, 180], entre el morro y la velocidad, ambos en el plano XY. Positivo si la
	 * velocidad apunta a la derecha del morro. 0 si la velocidad horizontal es menor que MinSlipSpeed.
	 */
	TORTUNABO_API float SlipAngleDeg(const FVector& Forward, const FVector& Velocity);

	/**
	 * Dirección final en [-1, 1]: la pedida más un contravolante proporcional a la deriva (Assist a MaxAngleDeg o más).
	 * Sin corrección si la deriva supera MaxAssistedSlipDeg.
	 */
	TORTUNABO_API float AssistSteer(float Input, float SlipDeg, float Assist, float MaxAngleDeg);

	// ── Golpe de rueda contra un escalón ────────────────────────────────────────

	struct FBumpTuning
	{
		/** Subida mínima del suelo (cm) entre dos frames para contar como subida. */
		float MinStepCm = 4.f;
		/** Fracción de la velocidad de subida teórica (avance × escalón / radio) que pasa al chasis. */
		float Scale = 0.15f;
		/** Tope del golpe (cm/s hacia arriba en la rueda). */
		float MaxKick = 200.f;
	};

	struct FWheelTrack
	{
		bool bPrevRise = false;
		float PendingKick = 0.f;
	};

	struct FBumpStep
	{
		FWheelTrack Track;
		/** Golpe que hay que aplicar en este frame (cm/s); 0 si ninguno. */
		float Kick = 0.f;
	};

	TORTUNABO_API bool IsRise(float StepCm, bool bBothInContact, const FBumpTuning& Tuning);
	TORTUNABO_API float KickSpeed(float StepCm, float ForwardSpeed, float WheelRadius, const FBumpTuning& Tuning);

	/**
	 * Avanza una rueda un frame. Una subida aislada (escalón) golpea un frame después; si sigue subiendo (rampa) se
	 * descarta.
	 */
	TORTUNABO_API FBumpStep BumpStep(const FWheelTrack& Track, float StepCm, float ForwardSpeed, float WheelRadius,
		bool bBothInContact, const FBumpTuning& Tuning);

	// ── Enderezado y reaparición ────────────────────────────────────────────────

	/** Volcado: el eje Z del buggy apunta por debajo de este valor. */
	constexpr float FlippedUpZ = 0.3f;

	enum class ESelfRight : uint8
	{
		None,
		/** La ocupante pulsó R (o Y) con el buggy volcado. */
		Manual,
		/** Lleva AutoDelay segundos volcado. */
		Auto
	};

	TORTUNABO_API bool IsFlipped(float UpZ);

	/** Segundos seguidos volcado tras un frame: suma Dt si está volcado; si no, vuelve a 0. */
	TORTUNABO_API float AdvanceFlipped(float FlippedSeconds, float UpZ, float Dt);

	/**
	 * Qué enderezado toca. Manual si se pide y lleva al menos ManualDelay volcado; Auto a los AutoDelay segundos
	 * volcado aunque nadie lo pida; None en otro caso.
	 */
	TORTUNABO_API ESelfRight DecideSelfRight(float FlippedSeconds, bool bRequested, float ManualDelay, float AutoDelay);

	/** Transform del enderezado: Lift cm más arriba, con solo la guiñada (cabeceo y alabeo a 0). */
	TORTUNABO_API FTransform SelfRightTransform(const FTransform& Current, float LiftCm);

	/** Botón de reaparecer: true el frame en que se cumplen HoldSeconds pulsado (una vez por pulsación). */
	struct FHold
	{
		float Held = 0.f;
		bool bFired = false;
	};

	TORTUNABO_API bool AdvanceHold(FHold& Hold, bool bPressed, float Dt, float HoldSeconds);

	// ── Tinte, charco y bamboleo ────────────────────────────────────────────────

	/** Color de la carrocería del equipo Index (8 colores que se repiten; los negativos, blanco). */
	TORTUNABO_API FLinearColor TeamColor(int32 Index);

	/**
	 * Deceleración (cm/s², hacia atrás) para no pasar de Cap: 0 por debajo, Gain × exceso por encima. La aplica cada
	 * máquina que simula el buggy mientras está en un charco.
	 */
	TORTUNABO_API float SpeedCapDecel(float Speed, float Cap, float Gain);

	/** Dirección extra del bamboleo del coco: seno de Frequency Hz que se apaga linealmente hasta TimeLeft = 0. */
	TORTUNABO_API float SteerWobble(float TimeLeft, float Duration, float Amplitude, float Frequency);
}
