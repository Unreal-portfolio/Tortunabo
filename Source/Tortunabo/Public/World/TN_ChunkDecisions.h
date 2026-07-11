#pragma once

#include "CoreMinimal.h"
#include "World/TN_ChunkManager.h" // ETNChunkDifficulty

/**
 * Decisiones de selección de chunk como funciones PURAS (Fase 4.3): sin UWorld,
 * sin actores, sin estado — entradas escalares, salida determinista.
 * ATN_ChunkManager delega aquí para que la lógica testeada por el Automation
 * framework sea EXACTAMENTE la que corre en juego, no una copia.
 *
 * Regla: nada de side effects ni lecturas de singletons. La aleatoriedad se
 * INYECTA como functor (el manager pasa FMath::RandRange; los tests, secuencias
 * deterministas).
 */

namespace TNChunkLogic
{
	/**
	 * Dificultad según el progreso. Borde: count == umbral pasa al tier siguiente
	 * (los umbrales usan <, no <=).
	 */
	inline ETNChunkDifficulty ComputeDifficultyFromProgress(int32 PassedChunkCount,
		int32 EasyToMediumThreshold, int32 MediumToHardThreshold)
	{
		if (PassedChunkCount < EasyToMediumThreshold) { return ETNChunkDifficulty::Easy; }
		if (PassedChunkCount < MediumToHardThreshold) { return ETNChunkDifficulty::Medium; }
		return ETNChunkDifficulty::Hard;
	}

	/** Dificultad de la secuencia personalizada; fuera de rango → Hard como fallback. */
	inline ETNChunkDifficulty ResolveCustomSequenceDifficulty(
		const TArray<ETNChunkDifficulty>& Sequence, int32 Index)
	{
		return Sequence.IsValidIndex(Index) ? Sequence[Index] : ETNChunkDifficulty::Hard;
	}

	/** Orden de pools a intentar: primario y dos fallbacks si están vacíos. */
	struct FPoolFallbackOrder
	{
		ETNChunkDifficulty Primary;
		ETNChunkDifficulty Fallback1;
		ETNChunkDifficulty Fallback2;
	};

	/** Easy→(E,M,H) · Medium→(M,E,H) · Hard→(H,M,E). */
	inline FPoolFallbackOrder GetPoolFallbackOrder(ETNChunkDifficulty Difficulty)
	{
		switch (Difficulty)
		{
			case ETNChunkDifficulty::Easy:
				return { ETNChunkDifficulty::Easy, ETNChunkDifficulty::Medium, ETNChunkDifficulty::Hard };
			case ETNChunkDifficulty::Medium:
				return { ETNChunkDifficulty::Medium, ETNChunkDifficulty::Easy, ETNChunkDifficulty::Hard };
			default:
				return { ETNChunkDifficulty::Hard, ETNChunkDifficulty::Medium, ETNChunkDifficulty::Easy };
		}
	}

	/**
	 * Índice aleatorio evitando repetir el anterior (hasta 10 reintentos; si el RNG
	 * insiste, se acepta la repetición — nunca cuelga).
	 * @param RandRange  Functor (Min, Max) → int32 en [Min, Max], ambos inclusive.
	 * @return INDEX_NONE si el pool está vacío; 0 si tiene un solo elemento.
	 */
	inline int32 SelectIndexAvoidingRepeat(int32 PoolSize, int32 LastIndex,
		TFunctionRef<int32(int32 Min, int32 Max)> RandRange)
	{
		if (PoolSize <= 0) { return INDEX_NONE; }
		if (PoolSize == 1) { return 0; }

		int32 NewIndex = LastIndex;
		int32 Attempts = 0;
		while (NewIndex == LastIndex && Attempts < 10)
		{
			NewIndex = RandRange(0, PoolSize - 1);
			++Attempts;
		}
		return NewIndex;
	}

	/** true cuando el progreso alcanza el total (>=): toca spawnear el chunk final. */
	inline bool ShouldSpawnFinalChunk(int32 PassedChunkCount, int32 TotalChunksBeforeFinal)
	{
		return PassedChunkCount >= TotalChunksBeforeFinal;
	}
}
