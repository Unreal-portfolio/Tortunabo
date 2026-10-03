#pragma once

#include "CoreMinimal.h"

/**
 * Lógica pura de la patada de la tormenta de bañistas (ATN_BeachStorm): el arco de la bola y qué se hace cuando no hay
 * arco libre o la bola no llega. Sin mundo ni actores: la usa la tormenta en el servidor y la prueban los tests
 * (Tortunabo.Beach.StormKick).
 *
 * La patada siempre se ve: en bola por el aire hasta su sitio. Si ningún arco está libre (una pared delante, un sitio
 * estrecho, nadando o muy lejos), la bola vuela igual atravesando lo que haya (ATN_ShellBody::SetPassThrough) y vuelve
 * a chocar al bajar sobre su sitio. Si una bola no llega (atascada, hundida, lejos o detrás del frente), otra patada
 * visible la lleva desde donde está. Solo sin bola posible, o tras MaxHops patadas que no llegan, se la pone en su sitio.
 */
namespace TNBeachStormKick
{
	/** Patadas de más (desde donde se ha quedado la bola) antes de ponerla en su sitio sin vuelo. */
	constexpr int32 MaxHops = 2;

	/** Con la bola atravesando, antes de esta parte del vuelo no se mira si ya puede volver a chocar (va subiendo). */
	constexpr float PassThroughEarliest = 0.5f;

	/** Y desde esta parte del vuelo vuelve a chocar sí o sí: el último tramo baja a la arena ya comprobada de su sitio. */
	constexpr float PassThroughLatest = 0.88f;

	/**
	 * Velocidad inicial para ir de From a To en Flight segundos con gravedad G y la amortiguación lineal de la caja (C, 1/s;
	 * frena un poco en el aire): x(t) = v/C·(1 - e^-Ct) y z(t) = (vz + G/C)/C·(1 - e^-Ct) - G·t/C.
	 */
	inline FVector BallisticLaunch(const FVector& From, const FVector& To, float Flight, float G, float C)
	{
		const FVector D = To - From;
		const double T = FMath::Max(0.1, static_cast<double>(Flight));
		if (C < 0.01f)
		{
			return FVector(D.X / T, D.Y / T, D.Z / T + 0.5 * G * T);
		}
		const double K = (1.0 - FMath::Exp(-C * T)) / C;
		return FVector(D.X / K, D.Y / K, (D.Z + G * T / C) / K - G / C);
	}

	/** Punto del arco a los T segundos (misma cuenta que BallisticLaunch). */
	inline FVector BallisticPoint(const FVector& From, const FVector& Launch, float T, float G, float C)
	{
		if (C < 0.01f)
		{
			return From + Launch * T - FVector(0.0, 0.0, 0.5 * G * T * T);
		}
		const double E = (1.0 - FMath::Exp(-C * T)) / C;
		return From + FVector(Launch.X * E, Launch.Y * E, (Launch.Z + G / C) * E - G * T / C);
	}

	/**
	 * Con la bola atravesando lo que haya: si ya puede volver a chocar. Bajando, pasada PassThroughEarliest del vuelo y
	 * sin nada que la pare alrededor (bOverlapping false); o, pase lo que pase, desde PassThroughLatest del vuelo.
	 */
	inline bool ShouldEndPassThrough(float Elapsed, float Flight, float VelocityZ, bool bOverlapping)
	{
		const float Safe = FMath::Max(Flight, 0.1f);
		if (Elapsed >= Safe * PassThroughLatest)
		{
			return true;
		}
		return Elapsed >= Safe * PassThroughEarliest && VelocityZ < 0.f && !bOverlapping;
	}

	/** Qué se hace con una patada que no ha llegado a su sitio. */
	enum class EFailedLanding : uint8
	{
		/** Otra patada visible desde donde está, atravesando lo que haya. */
		Hop,
		/** Ponerla en su sitio sin vuelo (último recurso). */
		Place,
	};

	/** Lleva Hops patadas de más: otra mientras queden (MaxHops); si no, a su sitio. */
	inline EFailedLanding ResolveFailedLanding(int32 Hops)
	{
		return Hops < MaxHops ? EFailedLanding::Hop : EFailedLanding::Place;
	}
}
