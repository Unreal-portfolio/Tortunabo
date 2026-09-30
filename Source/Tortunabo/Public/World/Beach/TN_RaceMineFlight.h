#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"

/**
 * Vuelo de la mina de arena de la carrera (ATN_RaceMine, Docs/Modo_Carrera.md): lógica pura de un paso, sin mundo. El
 * actor la usa en el servidor con la cota de la arena del generador; los tests de Tortunabo.RaceItems.Mine la usan con
 * suelos de prueba, así cubren el código real y no una copia.
 */
namespace TNRaceMineFlight
{
	/** Gravedad de la mina (cm/s²): 1,3 veces la normal. */
	constexpr double GravityCm = 980.0 * 1.3;
	/** Radio con el que toca el suelo (cm). */
	constexpr double MineRadius = 30.0;
	/** Primer contacto con la arena: lo que conserva de la velocidad vertical y lo que pierde de la horizontal. */
	constexpr double Restitution = 0.35;
	constexpr double BounceFriction = 0.5;
	/** Rebotes que da antes de irse rodando hasta quedar quieta. */
	constexpr int32 MaxBounces = 1;
	/** Si el rebote sale con menos velocidad vertical (cm/s), ya no rebota. */
	constexpr double MinBounceSpeed = 90.0;
	/** Al rodar: frenado (cm/s²), velocidad a la que se da por quieta (cm/s) y desnivel (cm) a partir del cual se despega del suelo. */
	constexpr double SlideDecel = 900.0;
	constexpr double RestSpeed = 25.0;
	constexpr double SlideDetach = 30.0;
	/** Paso máximo de la simulación (s): con un tirón de fotograma no atraviesa la arena. */
	constexpr double MaxStepSeconds = 0.05;

	/** Estado de la simulación (el centro de la mina y su velocidad, en cm y cm/s). */
	struct FState
	{
		FVector Pos = FVector::ZeroVector;
		FVector Vel = FVector::ZeroVector;
		int32 Bounces = 0;
		bool bSliding = false;
	};

	struct FStepResult
	{
		/** Donde tocaría el centro de la mina en la arena bajo su sitio nuevo (cm). */
		double GroundZ = 0.0;
		/** Se ha quedado quieta rodando (State.Pos es donde se para y State.Vel, cero). */
		bool bAtRest = false;
	};

	/** Cota de la arena bajo Where (cm); FallbackZ si no hay suelo. */
	using FGroundFn = TFunctionRef<double(const FVector& Where, double FallbackZ)>;

	/**
	 * @brief Un paso de vuelo: cae, rebota una vez en la arena y se va rodando hasta quedar quieta.
	 * @note Cualquier paso que acabe con el centro por debajo de la arena la deja encima: también subiendo (una cuesta o el
	 *       borde de una meseta que suben más deprisa que ella), que antes no se miraba y la mina cruzaba la meseta por dentro.
	 */
	inline FStepResult Step(FState& State, double DeltaSeconds, FGroundFn GroundAt)
	{
		FStepResult Result;
		const double StepSeconds = FMath::Min(DeltaSeconds, MaxStepSeconds);
		if (!State.bSliding)
		{
			State.Vel.Z -= GravityCm * StepSeconds;
		}
		FVector Next = State.Pos + State.Vel * StepSeconds;
		const double GroundZ = GroundAt(Next, State.Pos.Z - MineRadius) + MineRadius;
		Result.GroundZ = GroundZ;
		if (State.bSliding && Next.Z - GroundZ > SlideDetach)
		{
			// Se acaba el suelo (un escalón, el borde de algo): vuelve a caer.
			State.bSliding = false;
		}
		if (State.bSliding)
		{
			// Rodando por la arena: pegada al suelo y frenando hasta quedar quieta.
			Next.Z = GroundZ;
			State.Vel.Z = 0.0;
			const double Speed = State.Vel.Size2D();
			const double SlowedSpeed = FMath::Max(0.0, Speed - SlideDecel * StepSeconds);
			State.Vel = Speed > UE_KINDA_SMALL_NUMBER ? State.Vel * (SlowedSpeed / Speed) : FVector::ZeroVector;
			if (SlowedSpeed < RestSpeed)
			{
				State.Pos = Next;
				State.Vel = FVector::ZeroVector;
				State.bSliding = false;
				Result.bAtRest = true;
				return Result;
			}
		}
		else if (Next.Z <= GroundZ)
		{
			Next.Z = GroundZ;
			if (State.Vel.Z > 0.0)
			{
				// Subiendo con la arena por encima: se queda encima sin rebotar y el paso siguiente la vuelve a posar.
				State.Vel.Z = 0.0;
			}
			else
			{
				// Toca la arena bajando: el primer contacto rebota (restitución y fricción); el siguiente ya rueda.
				if (State.Bounces < MaxBounces && -State.Vel.Z * Restitution >= MinBounceSpeed)
				{
					++State.Bounces;
					State.Vel.Z = -State.Vel.Z * Restitution;
				}
				else
				{
					State.bSliding = true;
					State.Vel.Z = 0.0;
				}
				State.Vel.X *= 1.0 - BounceFriction;
				State.Vel.Y *= 1.0 - BounceFriction;
			}
		}
		State.Pos = Next;
		return Result;
	}
}
