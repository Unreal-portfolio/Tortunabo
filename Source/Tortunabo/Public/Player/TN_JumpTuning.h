#pragma once

#include "CoreMinimal.h"

/**
 * Cálculo del salto como función PURA: sin UWorld, sin actores, sin estado.
 * ATortugaCharacter::ApplyJumpTuning delega aquí para que lo que testea el
 * Automation framework sea exactamente lo que corre en juego.
 *
 * El salto se define por resultado (altura y distancia en llano) y de ahí se
 * derivan los dos campos del CharacterMovement que lo producen.
 */

namespace TNJumpLogic
{
	struct FJumpTuning
	{
		/** Tiempo total en el aire de un salto en llano (s). */
		float AirTime = 0.f;
		/** Valor para UCharacterMovementComponent::JumpZVelocity (cm/s). */
		float JumpZVelocity = 0.f;
		/** Valor para UCharacterMovementComponent::GravityScale. */
		float GravityScale = 1.f;
	};

	/**
	 * Tiro parabólico en llano con gravedad efectiva g: T = D / v_h, h = g·T² / 8, v_z = g·T / 2.
	 * Fijados h y T: v_z = 4h / T, g = 8h / T².
	 *
	 * @param JumpHeight          Altura máxima deseada (cm).
	 * @param JumpDistance        Distancia horizontal deseada a ReferenceSpeed (cm).
	 * @param ReferenceSpeed      Velocidad horizontal a la que se mide JumpDistance (cm/s).
	 * @param WorldGravityZ       Gravedad del mundo (cm/s²); se usa su valor absoluto.
	 * @param OutTuning           Resultado; no se toca si la función devuelve false.
	 * @return false si alguna entrada no es positiva (no hay salto físicamente definible).
	 */
	inline bool ComputeJumpTuning(float JumpHeight, float JumpDistance, float ReferenceSpeed, float WorldGravityZ, FJumpTuning& OutTuning)
	{
		const float Gravity = FMath::Abs(WorldGravityZ);
		if (JumpHeight <= KINDA_SMALL_NUMBER || JumpDistance <= KINDA_SMALL_NUMBER
			|| ReferenceSpeed <= KINDA_SMALL_NUMBER || Gravity <= KINDA_SMALL_NUMBER)
		{
			return false;
		}

		const float AirTime = JumpDistance / ReferenceSpeed;
		OutTuning.AirTime       = AirTime;
		OutTuning.JumpZVelocity = 4.f * JumpHeight / AirTime;
		OutTuning.GravityScale  = (8.f * JumpHeight / FMath::Square(AirTime)) / Gravity;
		return true;
	}

	/** Distancia horizontal recorrida en llano a Speed con el tiempo de vuelo dado (cm). */
	inline float ComputeJumpDistance(float Speed, float AirTime)
	{
		return FMath::Max(0.f, Speed) * FMath::Max(0.f, AirTime);
	}
}
