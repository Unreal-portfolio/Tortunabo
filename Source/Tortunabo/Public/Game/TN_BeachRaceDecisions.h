#pragma once

#include "CoreMinimal.h"

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
}
