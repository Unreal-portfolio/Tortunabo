#pragma once

#include "CoreMinimal.h"

/**
 * Lógica pura del vaivén de las algas (ATN_BeachSeaweed): cada cuánto se reconstruye su malla procedural en CPU.
 * Enrolladas en una tortuga, cada frame (es jugabilidad). En reposo, cada frame solo cerca de la cámara; más lejos,
 * a menos frecuencia (el vaivén es de 22 cm en la punta y a esa distancia no se distingue) y, muy lejos o fuera de
 * pantalla, nada. Sin mundo ni componentes, para que Tortunabo.Seaweed.RebuildRate cubra la regla real.
 */
namespace TNSeaweedLogic
{
	/** Hasta aquí (cm) el vaivén se reconstruye cada frame: el aspecto a distancia de juego no cambia. */
	constexpr double FULL_RATE_DISTANCE = 2500.0;

	/** Hasta aquí (cm), a MID_RATE_INTERVAL. */
	constexpr double MID_RATE_DISTANCE = 5000.0;

	/** Hasta aquí (cm), a FAR_RATE_INTERVAL; más lejos la malla se queda quieta. */
	constexpr double FREEZE_DISTANCE = 9000.0;

	constexpr float MID_RATE_INTERVAL = 1.f / 20.f;
	constexpr float FAR_RATE_INTERVAL = 1.f / 8.f;

	/** Intervalo que significa «no reconstruir». */
	constexpr float NEVER = -1.f;

	/**
	 * @brief Segundos entre reconstrucciones de la malla viva (0 = cada frame, NEVER = no reconstruir).
	 * @param bWrapping          Alguna tortuga enrollada (o soltándose): siempre cada frame.
	 * @param bRecentlyRendered  La malla se ha dibujado hace poco.
	 * @param DistanceToViewer   Distancia (cm) a la cámara local más cercana.
	 */
	inline float RebuildInterval(bool bWrapping, bool bRecentlyRendered, double DistanceToViewer)
	{
		if (bWrapping)
		{
			return 0.f;
		}
		if (!bRecentlyRendered || DistanceToViewer > FREEZE_DISTANCE)
		{
			return NEVER;
		}
		if (DistanceToViewer <= FULL_RATE_DISTANCE)
		{
			return 0.f;
		}
		return DistanceToViewer <= MID_RATE_DISTANCE ? MID_RATE_INTERVAL : FAR_RATE_INTERVAL;
	}

	/** @brief Toca reconstruir si hay intervalo y ha pasado desde la última reconstrucción. */
	inline bool ShouldRebuild(float Interval, float SecondsSinceRebuild)
	{
		return Interval >= 0.f && SecondsSinceRebuild >= Interval;
	}
}
