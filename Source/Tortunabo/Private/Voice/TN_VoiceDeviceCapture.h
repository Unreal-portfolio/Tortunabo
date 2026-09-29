#pragma once

#include "CoreMinimal.h"
#include "AudioCaptureCore.h"
#include "HAL/CriticalSection.h"
#include "Misc/ScopeLock.h"

/**
 * Captura de un micrófono concreto para UProximityVoiceComponent (el elegido en el menú de pausa). Hace lo mismo que
 * Audio::FAudioCaptureSynth, que solo sabe abrir el predeterminado: el hilo de captura de WASAPI deja las muestras en un
 * búfer con cerrojo y el componente las recoge cada fotograma. Llegan ya en mono.
 *
 * Igual que la del motor en el componente, nunca se para ni se destruye (parar o destruir una captura WASAPI abierta es
 * lo que rompe el juego al viajar): el componente la suelta y se queda viva hasta que se cierra el proceso. Para que una
 * suelta no crezca sin fin, el búfer no pasa de 2 s.
 *
 * El índice es el de los micrófonos activos de Windows, en el orden de Audio::FAudioCapture::GetCaptureDevicesAvailable
 * (WASAPI enumera los dos con eCapture y DEVICE_STATE_ACTIVE).
 */
class FTNVoiceDeviceCapture
{
public:
	/** Abre el micrófono CaptureDeviceIndex y empieza a capturar. */
	bool Open(int32 CaptureDeviceIndex)
	{
		Audio::FAudioCaptureDeviceParams Params;
		Params.DeviceIndex = CaptureDeviceIndex;
		// OpenAudioCaptureStream no se exporta del módulo del motor; OpenCaptureStream (obsoleta) sí y hace lo mismo.
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		const bool bOpened = Capture.OpenCaptureStream(Params,
			[this](const float* InAudio, int32 NumFrames, int32 NumChannels, int32 SampleRate, double StreamTime, bool bOverflow)
			{
				OnCapture(InAudio, NumFrames, NumChannels);
			}, 1024);
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
		return bOpened && Capture.StartStream();
	}

	/** Frecuencia de muestreo con la que se abrió (0 si no se sabe). */
	int32 GetSampleRate() const { return Capture.GetSampleRate(); }

	/** Añade a OutAudio lo capturado desde la última vez (mono). false si no había nada. */
	bool GetAudioData(TArray<float>& OutAudio)
	{
		FScopeLock Lock(&Section);
		if (Buffer.Num() == 0)
		{
			return false;
		}
		OutAudio.Append(Buffer);
		Buffer.Reset();
		return true;
	}

private:
	void OnCapture(const float* InAudio, int32 NumFrames, int32 NumChannels)
	{
		if (!InAudio || NumFrames <= 0 || NumChannels <= 0)
		{
			return;
		}
		FScopeLock Lock(&Section);
		if (Buffer.Num() + NumFrames > MaxSamples)
		{
			Buffer.Reset();
		}
		const int32 Start = Buffer.AddUninitialized(NumFrames);
		for (int32 Frame = 0; Frame < NumFrames; ++Frame)
		{
			float Sum = 0.f;
			for (int32 Channel = 0; Channel < NumChannels; ++Channel)
			{
				Sum += InAudio[Frame * NumChannels + Channel];
			}
			Buffer[Start + Frame] = Sum / NumChannels;
		}
	}

	/** Dos segundos a 48 kHz. */
	static constexpr int32 MaxSamples = 2 * 48000;

	Audio::FAudioCapture Capture;
	FCriticalSection Section;
	TArray<float> Buffer;
};
