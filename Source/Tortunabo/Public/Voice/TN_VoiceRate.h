#pragma once

#include "CoreMinimal.h"

/**
 * Frecuencia de muestreo real de la captura de voz (#154). La que dice el dispositivo (PreferredSampleRate) no siempre es
 * la del flujo que entrega: con la cancelación de eco de Windows, un micrófono de 48 kHz puede llegar a 16 kHz en mono. Si
 * la voz sale etiquetada con la del dispositivo, el que la oye la reproduce más deprisa y más aguda (efecto ardilla). Aquí
 * se mide con cuántas muestras llegan de verdad por segundo. Lógica pura: los tests de Tortunabo.Voice la cubren.
 */
namespace TNVoiceRate
{
	/** Frecuencias de muestreo habituales de un micrófono. */
	inline const TArray<int32>& StandardRates()
	{
		static const TArray<int32> Rates = { 8000, 11025, 16000, 22050, 24000, 32000, 44100, 48000, 88200, 96000 };
		return Rates;
	}

	/**
	 * @brief La frecuencia estándar más cercana a la medida (en proporción), o 0 si ninguna está a menos de Tolerance.
	 *        44,1 y 48 kHz se distinguen: están a un 8,8 % la una de la otra.
	 */
	inline int32 SnapToStandardRate(double MeasuredHz, double Tolerance = 0.06)
	{
		if (MeasuredHz <= 0.0)
		{
			return 0;
		}
		int32 Best = 0;
		double BestError = TNumericLimits<double>::Max();
		for (const int32 Rate : StandardRates())
		{
			const double Error = FMath::Abs(MeasuredHz / Rate - 1.0);
			if (Error < BestError)
			{
				BestError = Error;
				Best = Rate;
			}
		}
		return BestError <= Tolerance ? Best : 0;
	}

	/**
	 * Cuenta las muestras (ya en mono) que llegan de la captura en ventanas de WindowSeconds y confirma la frecuencia
	 * cuando dos ventanas seguidas dan la misma: un tirón o una ventana con muestras perdidas no la cambia.
	 */
	struct FCaptureRateMeter
	{
		static constexpr double WindowSeconds = 2.0;

		/** Frecuencia confirmada (Hz); 0 mientras no se sabe. */
		int32 Rate = 0;

		/**
		 * @brief Anota Samples muestras recibidas en el instante Now (s). Las de la primera llamada solo marcan el inicio
		 *        (no se sabe desde cuándo estaban en el búfer).
		 * @return true si la frecuencia confirmada ha cambiado.
		 */
		bool Add(int32 Samples, double Now)
		{
			if (WindowStart < 0.0)
			{
				WindowStart = Now;
				WindowSamples = 0;
				return false;
			}
			WindowSamples += FMath::Max(0, Samples);
			const double Elapsed = Now - WindowStart;
			if (Elapsed < WindowSeconds)
			{
				return false;
			}
			const int32 Snapped = SnapToStandardRate(static_cast<double>(WindowSamples) / Elapsed);
			WindowStart = Now;
			WindowSamples = 0;
			const bool bConfirmed = Snapped > 0 && Snapped == LastSnapped && Snapped != Rate;
			LastSnapped = Snapped;
			if (bConfirmed)
			{
				Rate = Snapped;
			}
			return bConfirmed;
		}

	private:
		double WindowStart = -1.0;
		int64 WindowSamples = 0;
		int32 LastSnapped = 0;
	};
}
