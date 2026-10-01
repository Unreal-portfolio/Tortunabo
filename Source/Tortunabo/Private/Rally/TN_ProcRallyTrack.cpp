#include "Rally/TN_ProcRallyTrack.h"

#include "Rally/TN_RallyTrack.h"
#include "World/ProcMap/TN_ProcMapGenerator.h"
#include "World/ProcMap/TN_ProcMapLayout.h"

namespace TNProcRally
{
	namespace
	{
		/** La meta, como poco esto por encima del mar (cm): en la arena seca, no en la orilla. */
		constexpr double FinishAboveSeaCm = 40.0;
	}

	TNRally::FRoutePlanParams MakePlanParams(double SeaLevelZ)
	{
		TNRally::FRoutePlanParams Params;
		Params.MinFinishZ = SeaLevelZ + FinishAboveSeaCm;
		// Un punto del eje por muestra del camino (4 m): la spline sigue el camino de verdad en las cuevas y las curvas
		// cerradas, sin recortarlas contra las paredes.
		Params.RoadStepCm = 400.0;
		return Params;
	}

	TArray<TNRally::FRouteSample> RouteSamplesFrom(const TArray<FTNProcPathPoint>& Points)
	{
		using namespace TNProcMap;
		// Donde no cabe o no se vería el arco de una puerta: cuevas, tableros, torres, toboganes, isletas, pasarelas, huecos y
		// puentes del río (en el mapa del Rally casi todo eso no existe; las cuevas sí).
		constexpr uint32 NoGate = PathFlags::Tunnel | PathFlags::Elevated | PathFlags::Colossal | PathFlags::TowerTop | PathFlags::UnderTower
			| PathFlags::Slide | PathFlags::GeyserBase | PathFlags::Islet | PathFlags::Boardwalk | PathFlags::Gap | PathFlags::RiverCross;
		TArray<TNRally::FRouteSample> Samples;
		Samples.Reserve(Points.Num());
		for (const FTNProcPathPoint& Point : Points)
		{
			TNRally::FRouteSample& Sample = Samples.AddDefaulted_GetRef();
			Sample.Location = Point.Location;
			Sample.WidthCm = Point.Width;
			Sample.bNoGate = (Point.Flags & NoGate) != 0;
			Sample.bShore = (Point.Flags & PathFlags::Shore) != 0;
		}
		return Samples;
	}

	bool BuildTrackFromGenerator(ATN_RallyTrack& Track, ATN_ProcMapGenerator& Generator)
	{
		if (!Generator.IsMapReady())
		{
			return false;
		}
		TArray<FTNProcPathPoint> Points;
		Generator.GetMainPathWorld(Points);
		const double SeaZ = Generator.GetSeaLevelWorldZ();
		const TNRally::FRoutePlan Plan = TNRally::PlanRouteFromPath(RouteSamplesFrom(Points), MakePlanParams(SeaZ));
		if (!Plan.bValid)
		{
			UE_LOG(LogTNRally, Error, TEXT("[ProcRally] El camino del mapa (%d muestras) no da para una pista."), Points.Num());
			return false;
		}
		TArray<FVector4> Raw;
		Generator.GetMainPathObstaclesWorld(Raw);
		TArray<TNRally::FLineObstacle> Obstacles;
		Obstacles.Reserve(Raw.Num());
		for (const FVector4& Obstacle : Raw)
		{
			Obstacles.Add({ FVector2D(Obstacle.X, Obstacle.Y), Obstacle.W });
		}
		// Suelo sin trazas: la colisión del terreno se cocina en segundo plano y la parrilla y las cajas se colocan antes.
		TWeakObjectPtr<ATN_ProcMapGenerator> WeakGenerator(&Generator);
		Track.SetGroundProvider([WeakGenerator](const FVector& At, FVector& OutGround)
		{
			const ATN_ProcMapGenerator* Map = WeakGenerator.Get();
			if (!Map || !Map->IsMapReady())
			{
				return false;
			}
			OutGround = FVector(At.X, At.Y, Map->GetTerrainHeightAt(At));
			return true;
		}, true);
		return Track.BuildFromRoutePlan(Plan, true, SeaZ, Obstacles);
	}
}
