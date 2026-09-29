#pragma once

#include "CoreMinimal.h"

/**
 * Cifras y cuentas puras de las gaviotas de la carrera: la zona de gaviotas (ATN_BeachGullZone: picado y cagada) y la
 * gaviota justiciera del objeto de carrera (ATN_RaceGullStrike). Sin mundo ni objetos: las usan los dos actores y las
 * pruebas Tortunabo.Beach.Gull (Private/Tests/TN_BeachGullTuningTest.cpp).
 *
 * Ronda 4, tarea 5 (nerf, Docs/Modo_Carrera.md «Nerf de la gaviota y de su caca», con los valores de antes): casi no se
 * podían esquivar. Lo que se pidió: andando te pilla; corriendo y cambiando de dirección en el momento justo, o tirándote en
 * plancha a tiempo, te libras. Con las velocidades de verdad de la tortuga (las del Blueprint: andando 200 cm/s, corriendo
 * 400; la plancha sale a 350 + la velocidad del salto, 750 corriendo), el blanco:
 *  1. sigue a la tortuga un poco más rápido de lo que corre (ChaseSpeed): ni andando ni corriendo en línea recta se despega;
 *  2. los últimos CommitSeconds, el pájaro (o lo que cae) ya va lanzado por la línea que llevaba la tortuga en ese momento:
 *     por esa línea la acompaña (hasta su velocidad de entonces, nunca hacia atrás) y hacia los lados apenas corrige
 *     (LateCorrection). Corriendo en línea recta, te pilla; si al lanzarse giras corriendo (60° o más), te das la vuelta o
 *     sales corriendo de parada, te libras; andando no da tiempo a salir del golpe.
 * La plancha en el momento justo libra además de las cagadas (TNBeach::IsDodgingByBellyDive) y del picado entero (el
 * panzazo); el golpe (lo que coge el pico, la mancha) es el de la sombra dura.
 */
namespace TNBeachGullTuning
{
	// ── Velocidades de la tortuga con las que se ajusta (las del Blueprint, Docs/Biblia_Tortunavy.md §13) ──

	constexpr float TurtleWalkSpeed = 200.f;
	constexpr float TurtleRunSpeed = 400.f;

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
	 * El blanco del picado sigue a la tortuga a DiveChaseSpeed cm/s como mucho (antes 625, pensado para andar a 450 y correr
	 * a 800). Los últimos DiveCommitSeconds antes de llegar abajo (cuando pliega las alas del todo) va lanzado por la línea
	 * de la tortuga y hacia los lados corrige a DiveLateCorrection (antes seguía igual hasta el final).
	 */
	constexpr float DiveChaseSpeed = 420.f;
	constexpr float DiveCommitSeconds = 1.5f;
	constexpr float DiveLateCorrection = 75.f;

	/** Coge a la tortuga cuyo centro está a menos de GrabRadius × tamaño + GrabPad cm del blanco (antes 300 y 45). */
	constexpr float GrabRadius = 220.f;
	constexpr float GrabPad = 25.f;

	// ── Cagada de la zona ──

	/**
	 * Mientras la gaviota vuela hasta encima y mientras cae, el blanco sigue a la tortuga a PoopChaseSpeed (antes 625). Los
	 * últimos PoopCommitSeconds (el «!» deja de parpadear y se queda fijo) cae por la línea que llevaba la tortuga y hacia
	 * los lados corrige a PoopLateCorrection.
	 */
	constexpr float PoopChaseSpeed = 420.f;
	constexpr float PoopCommitSeconds = 1.5f;
	constexpr float PoopLateCorrection = 75.f;

	/** Mancha (cm, por el tamaño) y holgura del golpe (cm): antes 280 y 45. La sombra dura crece hasta la mancha. */
	constexpr float SplatRadius = 200.f;
	constexpr float SplatPad = 35.f;

	// ── Gaviota justiciera (objeto de carrera, ATN_RaceGullStrike) ──

	/**
	 * El blanco sigue a la víctima a StrikeChaseSpeed (antes 700 hasta el golpe). Los últimos StrikeCommitSeconds (desde
	 * justo después de soltarla) la cagada cae por la línea que llevaba la víctima y hacia los lados corrige a
	 * StrikeLateCorrection.
	 */
	constexpr float StrikeChaseSpeed = 420.f;
	constexpr float StrikeCommitSeconds = 1.5f;
	constexpr float StrikeLateCorrection = 75.f;

	/** Radio del impacto en planta (cm; antes 330). */
	constexpr float StrikeImpactRadius = 240.f;

	// ── Plancha ──

	/**
	 * La plancha libra de la cagada si al caer la tortuga va en plancha (pose de panzazo) en el aire, o arrastrándose sobre
	 * la tripa aún a BellyDodgeMinSpeed cm/s o más: tirarse en el momento justo, no tumbarse a esperar.
	 */
	constexpr float BellyDodgeMinSpeed = 250.f;

	// ── Cuentas ──

	/** Cómo persigue el blanco en un ataque: hasta CommitAt, a ChaseSpeed; de CommitAt a EndAt, lanzado; después, quieto. */
	struct FChasePlan
	{
		float CommitAt = 0.f;
		float EndAt = 0.f;
		float ChaseSpeed = 0.f;
		float LateCorrection = 0.f;
	};

	/** Lo que el blanco recuerda al lanzarse: la línea (unitaria; cero si la tortuga estaba parada) y su velocidad por ella. */
	struct FChaseState
	{
		bool bCommitted = false;
		FVector2D Dir = FVector2D::ZeroVector;
		float Speed = 0.f;
	};

	/** Picado de la zona (T: segundos desde que empieza el ataque; llega abajo a DiveClimbTime + DiveTime). */
	inline FChasePlan DivePlan()
	{
		const float Strike = DiveClimbTime + DiveTime;
		return FChasePlan{ Strike - DiveCommitSeconds, Strike, DiveChaseSpeed, DiveLateCorrection };
	}

	/** Cagada de la zona (T: segundos desde que empieza el ataque; cae a PoopDropTime + PoopFallTime). */
	inline FChasePlan PoopPlan()
	{
		const float Impact = PoopDropTime + PoopFallTime;
		return FChasePlan{ Impact - PoopCommitSeconds, Impact, PoopChaseSpeed, PoopLateCorrection };
	}

	/** Gaviota justiciera (T: segundos desde que nace; cae a StrikeReleaseAge + StrikeFallSeconds). */
	inline FChasePlan StrikePlan()
	{
		const float Impact = StrikeReleaseAge + StrikeFallSeconds;
		return FChasePlan{ Impact - StrikeCommitSeconds, Impact, StrikeChaseSpeed, StrikeLateCorrection };
	}

	/** La más rápida de todas (para el suavizado del blanco en los clientes). */
	constexpr float MaxChaseSpeed()
	{
		return DiveChaseSpeed > PoopChaseSpeed ? (DiveChaseSpeed > StrikeChaseSpeed ? DiveChaseSpeed : StrikeChaseSpeed)
			: (PoopChaseSpeed > StrikeChaseSpeed ? PoopChaseSpeed : StrikeChaseSpeed);
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

	/**
	 * Un paso del blanco Aim a los T segundos del ataque, con la tortuga en Target yendo a TargetVelocity (cm/s, en planta).
	 * Antes de lanzarse, hacia ella a Plan.ChaseSpeed; al lanzarse apunta en State su línea y su velocidad (como mucho
	 * ChaseSpeed) y desde ahí la acompaña por esa línea (lo que ella avance por ella, nunca hacia atrás ni más que entonces)
	 * y hacia ella corrige a Plan.LateCorrection. Pasado Plan.EndAt, quieto.
	 */
	inline FVector2D StepAim(const FChasePlan& Plan, float T, FChaseState& State, const FVector2D& Aim, const FVector2D& Target,
		const FVector2D& TargetVelocity, float DeltaSeconds)
	{
		if (T >= Plan.EndAt)
		{
			return Aim;
		}
		if (T < Plan.CommitAt)
		{
			return StepToward(Aim, Target, Plan.ChaseSpeed, DeltaSeconds);
		}
		if (!State.bCommitted)
		{
			State.bCommitted = true;
			const double Speed = TargetVelocity.Size();
			State.Speed = static_cast<float>(FMath::Min(Speed, static_cast<double>(Plan.ChaseSpeed)));
			State.Dir = Speed > 1.0 ? TargetVelocity / Speed : FVector2D::ZeroVector;
		}
		const double Along = FMath::Clamp(FVector2D::DotProduct(TargetVelocity, State.Dir), 0.0, static_cast<double>(State.Speed));
		return StepToward(Aim + State.Dir * (Along * static_cast<double>(DeltaSeconds)), Target, Plan.LateCorrection, DeltaSeconds);
	}

	/** true si ya va lanzado (a los T segundos del ataque): la gaviota pliega las alas; el «!» de la cagada se queda fijo. */
	inline bool IsCommitted(const FChasePlan& Plan, float T)
	{
		return T >= Plan.CommitAt;
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
