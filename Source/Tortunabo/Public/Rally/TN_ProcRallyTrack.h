// Pista del Rally sobre el mapa generado del cooperativo (#291): pasa el camino principal de ATN_ProcMapGenerator a
// TNRally::PlanRouteFromPath y construye ATN_RallyTrack con el plan. La usan el servidor (ATN_ProcRallyGameMode) y los
// clientes (ATN_ProcRallyGameState) con los mismos datos, así que todas las máquinas tienen la misma pista.
#pragma once

#include "CoreMinimal.h"
#include "Rally/TN_RallyRoutePlan.h"

class ATN_ProcMapGenerator;
class ATN_RallyTrack;
struct FTNProcPathPoint;

namespace TNProcRally
{
	/** Parámetros del plan: puertas cada 250 m, salida a 42 m del principio y meta en la playa, por encima del mar. */
	TORTUNABO_API TNRally::FRoutePlanParams MakePlanParams(double SeaLevelZ);

	/** Muestras del plan desde el camino del generador: sin puertas en cuevas ni estructuras; la playa final, marcada. */
	TORTUNABO_API TArray<TNRally::FRouteSample> RouteSamplesFrom(const TArray<FTNProcPathPoint>& Points);

	/** Construye Track con el mapa que tiene Generator en esta máquina. False si no hay mapa o no da para una pista. */
	TORTUNABO_API bool BuildTrackFromGenerator(ATN_RallyTrack& Track, ATN_ProcMapGenerator& Generator);
}
