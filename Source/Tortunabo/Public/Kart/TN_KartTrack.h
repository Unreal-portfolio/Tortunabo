// Pista de los karts sobre el mapa generado del cooperativo (#291): el camino principal de ATN_ProcMapGenerator pasa por
// TNKart::PlanRouteFromPath (puertas cada 250 m con la regla del 60 %, salida en el claro inicial y meta en la playa
// final) y la spline de ATN_RallyTrack es la línea del piloto IA (TNKart::PlanRacingLineOffsets), que rodea los obstáculos
// del camino y pasa centrada por las puertas, los arcos y los géiseres. Sin cajas de munición del Rally. Cada máquina la
// construye con su generador (el mismo mapa con la semilla replicada), así que todas tienen la misma pista.
#pragma once

#include "CoreMinimal.h"
#include "Kart/TN_KartRoutePlan.h"
#include "Rally/TN_RallyTrack.h"
#include "TN_KartTrack.generated.h"

class ATN_ProcMapGenerator;
struct FTNProcPathPoint;

namespace TNKart
{
	/** Parámetros del plan: puertas cada 250 m, salida a 42 m del principio y meta en la playa, por encima del mar. */
	TORTUNABO_API FRoutePlanParams MakePlanParams(double SeaLevelZ);

	/** Muestras del plan desde el camino del generador: sin puertas en cuevas ni estructuras; la playa final, marcada. */
	TORTUNABO_API TArray<FRouteSample> RouteSamplesFrom(const TArray<FTNProcPathPoint>& Points);

	/**
	 * Sin puerta a menos de ClearCm (más su radio) de un obstáculo que la línea del piloto rodea: así la línea pasa por el
	 * centro de todas las puertas y el arco no se planta junto a una roca.
	 */
	TORTUNABO_API void BlockGatesNearObstacles(TArray<FRouteSample>& Samples, const TArray<FLineObstacle>& Obstacles, double ClearCm);
}

UCLASS()
class TORTUNABO_API ATN_KartTrack : public ATN_RallyTrack
{
	GENERATED_BODY()

public:
	ATN_KartTrack();

	/** Construye la pista con el mapa que tiene Generator en esta máquina. False si no hay mapa o no da para una pista. */
	bool BuildFromMap(ATN_ProcMapGenerator& Generator);

	/** Generación del mapa con la que se hizo (0 = sin pista). */
	int32 GetMapGeneration() const { return MapGeneration; }

	/** Arco de la meta en la spline (cm). */
	double GetFinishArc() const { return GetGateArc(GetGateCount() - 1); }

	/** Semiancho del camino en ese arco de la spline (cm). */
	double GetRoadHalfWidthAtArc(double Arc) const;

	/** Holgura de la línea del piloto IA alrededor de los obstáculos (cm) y largo de la rampa para volver al eje (cm). */
	UPROPERTY(EditAnywhere, Category = "Karts|Línea")
	float LineClearanceCm = 450.f;

	UPROPERTY(EditAnywhere, Category = "Karts|Línea")
	float LineRampCm = 3000.f;

	/** Sin puertas a menos de esto de un obstáculo (cm, más su radio). */
	UPROPERTY(EditAnywhere, Category = "Karts|Línea")
	float GateObstacleClearCm = 2500.f;

	/** La línea pasa por el centro de cada puerta en este radio (cm). */
	UPROPERTY(EditAnywhere, Category = "Karts|Línea")
	float GateCenteredRadiusCm = 1200.f;

private:
	/** Arco (en el eje del plan) y semiancho de cada punto del eje, para GetRoadHalfWidthAtArc. */
	TArray<double> RoadArcs;
	TArray<double> RoadHalfWidths;
	int32 MapGeneration = 0;
};
