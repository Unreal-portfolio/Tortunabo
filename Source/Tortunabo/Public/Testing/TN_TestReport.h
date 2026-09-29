#pragma once

#include "CoreMinimal.h"

class FJsonObject;
class UWorld;

/** Tiempos de fotograma reales (ms entre dos Tick consecutivos), con tope de muestras. */
class FTNFrameRecorder
{
public:
	void Tick()
	{
		const double Now = FPlatformTime::Seconds();
		if (Last > 0.0 && Samples.Num() < 400000)
		{
			Samples.Add(static_cast<float>((Now - Last) * 1000.0));
		}
		Last = Now;
	}
	void Reset() { Samples.Reset(); Last = 0.0; }
	const TArray<float>& GetSamples() const { return Samples; }

private:
	TArray<float> Samples;
	double Last = 0.0;
};

/** Utilidades comunes de los informes JSON de las pruebas de monkey y de estrés. */
namespace TNTestReport
{
	/** Saved/<Folder>/<fecha>.json (fecha local aaaa-mm-dd_hh-mm-ss). */
	TORTUNABO_API FString DefaultPath(const TCHAR* Folder);

	/** Escribe el objeto con sangría; crea la carpeta. false si no se ha podido. */
	TORTUNABO_API bool Save(const FJsonObject& Object, const FString& Path);

	/** Valor de «-Clave=valor» de la línea de órdenes (vacío si no está). */
	TORTUNABO_API FString CommandLineValue(const TCHAR* Key);

	/** Nombre del modo de compilación y de red para el informe. */
	TORTUNABO_API FString BuildConfigName();
	TORTUNABO_API FString NetModeName(const UWorld* World);

	/** Memoria física en uso (MB). */
	TORTUNABO_API double UsedPhysicalMB();
	TORTUNABO_API double PeakPhysicalMB();
}
