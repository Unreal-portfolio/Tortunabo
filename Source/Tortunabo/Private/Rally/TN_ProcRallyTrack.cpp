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
		// Validación del camino para el buggy (en el log): las rampas más empinadas, los escalones y el paso más estrecho.
		{
			double MaxSlope = 0.0, MaxSlopeArc = 0.0, MinWidth = TNumericLimits<double>::Max(), MinWidthArc = 0.0;
			int32 Steep = 0;
			for (int32 Index = 1; Index < Plan.Road.Num(); ++Index)
			{
				const double Run = FMath::Max(1.0, FVector::Dist2D(Plan.Road[Index], Plan.Road[Index - 1]));
				const double Slope = FMath::Abs(Plan.Road[Index].Z - Plan.Road[Index - 1].Z) / Run;
				Steep += Slope > 0.3 ? 1 : 0;
				if (Slope > MaxSlope) { MaxSlope = Slope; MaxSlopeArc = Plan.RoadArcCm[Index]; }
				if (Plan.RoadWidthCm[Index] < MinWidth) { MinWidth = Plan.RoadWidthCm[Index]; MinWidthArc = Plan.RoadArcCm[Index]; }
			}
			UE_LOG(LogTNRally, Log, TEXT("[ProcRally] Camino: pendiente máxima %.0f %% en el arco %.0f m, %d tramos de más del 30 %%, paso más estrecho %.1f m en el arco %.0f m."),
				100.0 * MaxSlope, MaxSlopeArc / 100.0, Steep, MinWidth / 100.0, MinWidthArc / 100.0);
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
		const bool bBuilt = Track.BuildFromRoutePlan(Plan, true, SeaZ, Obstacles);
		if (bBuilt && UE_LOG_ACTIVE(LogTNRally, Verbose))
		{
			for (const FVector4& Obstacle : Raw)
			{
				const FVector Center(Obstacle.X, Obstacle.Y, Obstacle.Z);
				const double Arc = Track.FindArcGlobal(Center);
				UE_LOG(LogTNRally, Verbose, TEXT("[ProcRally] Obstáculo en el arco %.0f m, a %.1f m del eje (calzada de %.0f m), radio %.1f m; la línea IA pasa a %.1f m."),
					Arc / 100.0, FVector::Dist2D(Center, Track.GetLocationAtArc(Arc)) / 100.0, 2.0 * Track.GetRoadHalfWidthAtArc(Arc) / 100.0,
					Obstacle.W / 100.0, FVector::Dist2D(Center, Track.GetRacingLineLocationAtArc(Arc)) / 100.0);
			}
		}
		return bBuilt;
	}
}
