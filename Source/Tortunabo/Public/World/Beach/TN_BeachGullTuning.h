#pragma once

#include "CoreMinimal.h"

/**
 * Cifras y cuentas puras de las gaviotas de la carrera: la zona de gaviotas (ATN_BeachGullZone: picado y cagada) y la
 * gaviota justiciera del objeto de carrera (ATN_RaceGullStrike). Sin mundo ni objetos: las usan los dos actores y las
 * pruebas Tortunabo.Beach.Gull (Private/Tests/TN_BeachGullTuningTest.cpp).
 *
 * Ronda 4, tarea 5 (nerf, Docs/Modo_Carrera.md «Nerf de la gaviota y de su caca», con los valores de antes): casi no se
 * podían esquivar. Ahora:
 *  - atacan desde menos lejos y cada algo más de tiempo;
 *  - el blanco sigue a la tortuga algo más despacio y, en el último tramo (picado lanzado, cagada a punto de caer), casi
 *    nada: un cambio de dirección o un sprint a tiempo la hacen fallar;
 *  - el golpe (lo que coge el pico, la mancha de la cagada) es más pequeño y es lo que marca la sombra dura;
 *  - la plancha en el momento justo (en el aire o aún arrastrándose deprisa) libra de la cagada.
 * Velocidades de la tortuga para las cuentas: andando 450 cm/s, corriendo 800 (UTN_StaminaComponent).
 */
namespace TNBeachGullTuning
{
	// ── Líneas de tiempo (s desde que empieza el ataque; sin cambios en el nerf) ──

	/** Cagada de la zona: vuela hasta encima de la tortuga y la suelta a PoopDropTime; tarda PoopFallTime en caer. */
	constexpr float PoopDropTime = 1.5f;
	constexpr float PoopFallTime = 2.1f;

	/** Picado de la zona: sube y se coloca en DiveClimbTime y baja en picado DiveTime (desde que aparece la sombra). */
	constexpr float DiveClimbTime = 1.f;
	constexpr float DiveTime = 2.3f;

	/** Gaviota justiciera: llega sobre la víctima y suelta la cagada a StrikeReleaseAge; tarda StrikeFallSeconds en caer. */
	constexpr float StrikeReleaseAge = 3.2f;
	constexpr float StrikeFallSeconds = 1.7f;

	// ── Zona de gaviotas: a quién ataca ──

	/** Radio de ataque (cm) = huella de la zona × AttackFootprintScale + AttackRadiusPad (antes: huella + 800). */
	constexpr float AttackFootprintScale = 0.8f;
	constexpr float AttackRadiusPad = 0.f;

	/** Tiempo entre ataques (s) mientras haya tortugas debajo (antes 3-6). */
	constexpr float AttackIntervalMin = 4.f;
	constexpr float AttackIntervalMax = 7.f;

	// ── Picado de la zona ──

	/**
	 * El blanco del picado sigue a la tortuga a DiveChaseSpeed cm/s como mucho (antes 625): más que andando, menos que
	 * corriendo. Los últimos DiveCommitSeconds antes de llegar abajo ya va lanzado y apenas corrige (DiveLateChaseSpeed;
	 * antes seguía igual hasta el final).
	 */
	constexpr float DiveChaseSpeed = 540.f;
	constexpr float DiveCommitSeconds = 0.6f;
	constexpr float DiveLateChaseSpeed = 120.f;

	/** Coge a la tortuga cuyo centro está a menos de GrabRadius × tamaño + GrabPad cm del blanco (antes 300 y 45). */
	constexpr float GrabRadius = 220.f;
	constexpr float GrabPad = 25.f;

	// ── Cagada de la zona ──

	/**
	 * Mientras la gaviota vuela hasta encima, el blanco sigue a la tortuga a PoopChaseSpeed (antes 625); mientras la cagada
	 * cae, a PoopFallChaseSpeed (lo que se anda: andando en línea recta no se gana terreno; antes 625), y los últimos
	 * PoopLockSeconds ya no se mueve (antes seguía hasta el golpe).
	 */
	constexpr float PoopChaseSpeed = 540.f;
	constexpr float PoopFallChaseSpeed = 450.f;
	constexpr float PoopLockSeconds = 0.45f;

	/** Mancha (cm, por el tamaño) y holgura del golpe (cm): antes 280 y 45. La sombra dura crece hasta la mancha. */
	constexpr float SplatRadius = 200.f;
	constexpr float SplatPad = 25.f;

	// ── Gaviota justiciera (objeto de carrera, ATN_RaceGullStrike) ──

	/**
	 * Hasta soltar la cagada, el blanco sigue a la víctima a StrikeChaseSpeed (antes 700); mientras cae, a
	 * StrikeFallChaseSpeed (antes 700), y los últimos StrikeLockSeconds ya no se mueve (antes seguía hasta el golpe).
	 */
	constexpr float StrikeChaseSpeed = 600.f;
	constexpr float StrikeFallChaseSpeed = 450.f;
	constexpr float StrikeLockSeconds = 0.5f;

	/** Radio del impacto en planta (cm; antes 330). */
	constexpr float StrikeImpactRadius = 240.f;

	// ── Plancha ──

	/**
	 * La plancha libra de la cagada si al caer la tortuga va en plancha (pose de panzazo) en el aire, o arrastrándose sobre
	 * la tripa aún a BellyDodgeMinSpeed cm/s o más: tirarse en el momento justo, no tumbarse a esperar.
	 */
	constexpr float BellyDodgeMinSpeed = 250.f;

	// ── Cuentas ──

	/**
	 * Velocidad máxima del blanco (cm/s) a los T segundos del ataque: Early hasta SwitchAt, Late hasta LockAt y 0 después
	 * (quieto).
	 */
	inline float ChaseSpeedAt(float T, float SwitchAt, float LockAt, float Early, float Late)
	{
		if (T < SwitchAt)
		{
			return Early;
		}
		return T < LockAt ? Late : 0.f;
	}

	/** Picado de la zona: a qué velocidad sigue el blanco a los Tau s del ataque, si llega abajo a StrikeTime s. */
	inline float DiveChaseSpeedAt(float Tau, float StrikeTime)
	{
		return ChaseSpeedAt(Tau, StrikeTime - DiveCommitSeconds, StrikeTime, DiveChaseSpeed, DiveLateChaseSpeed);
	}

	/** Cagada de la zona: la suelta a DropTime s y cae en FallTime s. */
	inline float PoopChaseSpeedAt(float Tau, float DropTime, float FallTime)
	{
		return ChaseSpeedAt(Tau, DropTime, DropTime + FallTime - PoopLockSeconds, PoopChaseSpeed, PoopFallChaseSpeed);
	}

	/** Gaviota justiciera: la suelta a los ReleaseAge s y cae en FallSeconds s. */
	inline float StrikeChaseSpeedAt(float Age, float ReleaseAge, float FallSeconds)
	{
		return ChaseSpeedAt(Age, ReleaseAge, ReleaseAge + FallSeconds - StrikeLockSeconds, StrikeChaseSpeed, StrikeFallChaseSpeed);
	}

	/** La más rápida de todas (para el suavizado del blanco en los clientes). */
	constexpr float MaxChaseSpeed()
	{
		return DiveChaseSpeed > PoopChaseSpeed ? DiveChaseSpeed : PoopChaseSpeed;
	}

	/** El blanco From, hacia Target a MaxSpeed cm/s como mucho durante DeltaSeconds (en planta). */
	inline FVector2D StepToward(const FVector2D& From, const FVector2D& Target, float MaxSpeed, float DeltaSeconds)
	{
		const FVector2D Delta = Target - From;
		const double Dist = Delta.Size();
		const double MaxStep = FMath::Max(0.0, static_cast<double>(MaxSpeed) * static_cast<double>(DeltaSeconds));
		if (Dist <= MaxStep || Dist <= UE_KINDA_SMALL_NUMBER)
		{
			return Target;
		}
		return From + Delta * (MaxStep / Dist);
	}

	/** true si lo que cae en planta a Dist2D cm de la tortuga le da con el radio Radius × SizeK + Pad. */
	inline bool IsInsideHit(double Dist2D, float Radius, float SizeK, float Pad)
	{
		return Dist2D <= static_cast<double>(Radius * SizeK + Pad);
	}

	/** true si la plancha la libra: pose de panzazo y, además, en el aire o arrastrándose aún deprisa. */
	inline bool DodgesByBellyDive(bool bBellyPose, bool bAirborne, float Speed2D)
	{
		return bBellyPose && (bAirborne || Speed2D >= BellyDodgeMinSpeed);
	}
}
