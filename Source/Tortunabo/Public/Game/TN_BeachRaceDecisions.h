#pragma once

#include "CoreMinimal.h"
#include "Game/TN_BeachRaceGameState.h"

/**
 * Reglas de la carrera en la playa (ATN_BeachRaceGameMode) como lógica pura: sin mundo ni controladores, todo entra por
 * parámetro. El modo de juego las usa tal cual y los tests de Tortunabo.BeachRace cubren el código real.
 */
namespace TNBeachRaceRules
{
	/** Una jugadora al acabarse el tiempo de la ronda sin nadie en el agua. */
	struct FTimeLimitCandidate
	{
		/** Puede ganar: tiene estado de jugadora, no es espectadora y, en el sprint final, es finalista. */
		bool bEligible = false;
		/** Tiene tortuga: sin ella (caída bajo KillZ, esperando su huevo) no se sabe lo cerca que está del mar. */
		bool bHasPawn = false;
		/** Lo que ha avanzado hacia el mar (GetCourseProgress). */
		float Progress = 0.f;
	};

	/**
	 * @brief Quién gana al acabarse el tiempo sin nadie en el agua: la elegible con tortuga más cerca del mar.
	 * @param bAnyEligibleWithoutPawn Si ninguna elegible tiene tortuga, la primera elegible. En el sprint final hace falta
	 *        una ganadora: sin ella la ronda ya estaba parada y la partida se quedaba sin terminar (#56). En una ronda normal
	 *        se acaba sin ganadora.
	 * @return Índice en Candidates o INDEX_NONE si no gana nadie.
	 */
	inline int32 PickTimeLimitWinner(const TArray<FTimeLimitCandidate>& Candidates, bool bAnyEligibleWithoutPawn)
	{
		int32 Best = INDEX_NONE;
		float BestProgress = -TNumericLimits<float>::Max();
		for (int32 Index = 0; Index < Candidates.Num(); ++Index)
		{
			const FTimeLimitCandidate& Candidate = Candidates[Index];
			if (Candidate.bEligible && Candidate.bHasPawn && Candidate.Progress > BestProgress)
			{
				BestProgress = Candidate.Progress;
				Best = Index;
			}
		}
		if (Best == INDEX_NONE && bAnyEligibleWithoutPawn)
		{
			Best = Candidates.IndexOfByPredicate([](const FTimeLimitCandidate& Candidate) { return Candidate.bEligible; });
		}
		return Best;
	}

	/**
	 * @brief ¿Se quita este objeto suelto al preparar la ronda siguiente? (#71)
	 *
	 * Lo suelto es de la ronda en que apareció: los objetos que se sueltan (pickups), las bolas que se paran, las conchas
	 * trampa y las cajas de objetos no pasan a la ronda N+1 como objetos gratis o trampas (la tortuga que los dejó ya no
	 * existe y la playa es otra). Se respeta lo que se colocó a mano en el nivel (no es de ninguna ronda) y lo que ya se
	 * está destruyendo.
	 * @param bPlacedInLevel El actor viene del nivel (AActor::IsNetStartupActor), no se creó jugando.
	 * @param bBeingDestroyed Ya no es válido o se está destruyendo (IsValid, IsActorBeingDestroyed).
	 */
	inline bool ShouldClearRoundLeftover(bool bPlacedInLevel, bool bBeingDestroyed)
	{
		return !bPlacedInLevel && !bBeingDestroyed;
	}

	/**
	 * @brief ¿La carrera deja jugar? (#72) Los enemigos, las trampas y los objetos de carrera (mina, cangrejo, gaviota, disco,
	 *        protector solar, nube de tormenta...) lo miran antes de golpear.
	 *
	 * No deja jugar al acabar la cuenta de meta («¡TIEMPO!» y «¡TODAS AL AGUA!»: gusanos y recuento), en el recuento de
	 * conchas, en el título del sprint final ni en el podio. A propósito, la fase Waiting cuenta como carrera (3, 2, 1
	 * incluidos): si la fase se quedara en Waiting por lo que sea, se sigue atacando.
	 */
	inline bool IsRaceLive(ETNBeachRacePhase Phase, ETNBeachFinishCountdown Countdown)
	{
		return Phase != ETNBeachRacePhase::RoundResults && Phase != ETNBeachRacePhase::Champion
			&& Phase != ETNBeachRacePhase::SprintIntro && Countdown != ETNBeachFinishCountdown::TimeUp
			&& Countdown != ETNBeachFinishCountdown::AllIn;
	}
}
