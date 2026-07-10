#pragma once

#include "CoreMinimal.h"

/**
 * Decisiones de enemigos como funciones PURAS (Fase 4.3): sin UWorld, sin actores,
 * sin estado — entradas escalares, salida determinista. Los actores
 * (ATN_EnemySeagull, ATN_CrabActor) delegan aquí para que la lógica testeada por
 * el Automation framework sea EXACTAMENTE la que corre en juego, no una copia.
 *
 * Regla: nada de side effects ni lecturas de singletons. Si una decisión necesita
 * algo del mundo (distancias, techo, vida del target), el actor lo calcula y lo
 * pasa como parámetro.
 */

namespace TNSeagullLogic
{
	/** Resultado de resolver el ataque al expirar el countdown (orden = precedencia). */
	enum class EAttackDecision : uint8
	{
		Retreat_TargetLost, // El target ya no existe/es inválido
		Retreat_Roof,       // Hay cubierta entre gaviota y target
		Retreat_Escaped,    // El target está fuera del radio de kill
		Strike              // Picotazo
	};

	/**
	 * Countdown restante derivado del timestamp replicado.
	 * @param AttackStartServerTime  Instante (reloj del servidor) en que arrancó; <0 = sin inicializar.
	 * @param ServerNow              Reloj sincronizado actual.
	 * @param AttackTimerSeconds     Duración total del cronómetro.
	 * @return Restante en [0, AttackTimerSeconds]. Sin inicializar → duración completa.
	 */
	inline float ComputeCountdownRemaining(float AttackStartServerTime, float ServerNow, float AttackTimerSeconds)
	{
		if (AttackStartServerTime < 0.f) { return AttackTimerSeconds; }
		return FMath::Clamp(AttackTimerSeconds - (ServerNow - AttackStartServerTime), 0.f, AttackTimerSeconds);
	}

	/**
	 * Radio de peligro interpolado con el countdown: empieza en MaxDangerRadius y
	 * se cierra hasta MinKillRadius al expirar.
	 */
	inline float ComputeDangerRadius(float CountdownRemaining, float AttackTimerSeconds,
		float MinKillRadius, float MaxDangerRadius)
	{
		if (AttackTimerSeconds <= 0.f) { return MinKillRadius; }
		const float NormT = FMath::Clamp(CountdownRemaining / AttackTimerSeconds, 0.f, 1.f);
		return FMath::Lerp(MinKillRadius, MaxDangerRadius, NormT);
	}

	/**
	 * Decisión al expirar el countdown (precedencia: target > techo > distancia).
	 * Borde: DistXY == MinKillRadius NO es escape (el escape exige estrictamente >).
	 */
	inline EAttackDecision DecideAttack(bool bTargetValid, bool bRoofBetween,
		float DistXY, float MinKillRadius)
	{
		if (!bTargetValid)          { return EAttackDecision::Retreat_TargetLost; }
		if (bRoofBetween)           { return EAttackDecision::Retreat_Roof; }
		if (DistXY > MinKillRadius) { return EAttackDecision::Retreat_Escaped; }
		return EAttackDecision::Strike;
	}

	/**
	 * Avance del temporizador de escape: fuera del radio acumula, dentro resetea.
	 * @param bOutAbort  true si el acumulado alcanza EscapeSeconds (la gaviota se retira).
	 * @return Nuevo valor de TimeOutsideShadow.
	 */
	inline float AdvanceEscapeTimer(float DistXY, float DangerRadius, float TimeOutsideShadow,
		float DeltaTime, float EscapeSeconds, bool& bOutAbort)
	{
		if (DistXY > DangerRadius)
		{
			const float NewTime = TimeOutsideShadow + DeltaTime;
			bOutAbort = NewTime >= EscapeSeconds;
			return NewTime;
		}
		bOutAbort = false;
		return 0.f;
	}
}

namespace TNCrabLogic
{
	/** Transición desde el estado Chase (orden de checks = precedencia del código). */
	enum class EChaseTransition : uint8
	{
		ReturnToPatrol_TargetLost, // Target inválido/muerto/eliminado
		ReturnToPatrol_OutOfZone,  // Target salió de la zona del cangrejo
		StartAttack,               // Target dentro del radio de ataque
		KeepChasing
	};

	/** Segundos restantes de un efecto (stun/blind) derivados del timestamp replicado. */
	inline float ComputeEffectRemaining(float EffectEndServerTime, float ServerNow)
	{
		return FMath::Max(0.f, EffectEndServerTime - ServerNow);
	}

	/**
	 * Nuevo instante de expiración al aplicar un efecto de Duration segundos.
	 * Max: un efecto nuevo no acorta uno en curso más largo.
	 */
	inline float ExtendEffectEndTime(float CurrentEndServerTime, float ServerNow, float Duration)
	{
		return FMath::Max(CurrentEndServerTime, ServerNow + Duration);
	}

	/**
	 * Decisión de transición durante Chase.
	 * @param bTargetAliveAndValid   IsAliveAndValid(target) calculado por el actor.
	 * @param DistTargetFromSpawn    Dist2D del target al spawn del cangrejo (su zona).
	 * @param DistToTarget           Dist2D del cangrejo al target.
	 * Borde: DistToTarget == AttackRadius SÍ ataca (el código usa <=).
	 */
	inline EChaseTransition DecideChaseTransition(bool bTargetAliveAndValid,
		float DistTargetFromSpawn, float MaxChaseDistance,
		float DistToTarget, float AttackRadius)
	{
		if (!bTargetAliveAndValid)                  { return EChaseTransition::ReturnToPatrol_TargetLost; }
		if (DistTargetFromSpawn > MaxChaseDistance) { return EChaseTransition::ReturnToPatrol_OutOfZone; }
		if (DistToTarget <= AttackRadius)           { return EChaseTransition::StartAttack; }
		return EChaseTransition::KeepChasing;
	}
}
