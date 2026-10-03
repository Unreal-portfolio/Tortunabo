#include "Kart/TN_KartTrack.h"

#include "Algo/BinarySearch.h"
#include "Engine/World.h"
#include "World/ProcMap/TN_ProcMapGenerator.h"
#include "World/ProcMap/TN_ProcMapLayout.h"

namespace TNKart
{
	namespace
	{
		/** La meta, como poco esto por encima del mar (cm): en la arena seca, no en la orilla. */
		constexpr double FinishAboveSeaCm = 40.0;
		/** Sin línea desplazada antes de esto pasada la salida (cm): la parrilla queda en el eje. */
		constexpr double StraightStartCm = 2000.0;
	}

	FRoutePlanParams MakePlanParams(double SeaLevelZ)
	{
		FRoutePlanParams Params;
		Params.MinFinishZ = SeaLevelZ + FinishAboveSeaCm;
		// Un punto del eje por muestra del camino (4 m): la spline sigue el camino de verdad en las cuevas y las curvas
		// cerradas, sin recortarlas contra las paredes.
		Params.RoadStepCm = 400.0;
		return Params;
	}

	TArray<FRouteSample> RouteSamplesFrom(const TArray<FTNProcPathPoint>& Points)
	{
		using namespace TNProcMap;
		// Donde no cabe o no se vería el arco de una puerta: cuevas, tableros, torres, toboganes, géiseres, isletas,
		// pasarelas, huecos y puentes del río.
		constexpr uint32 NoGate = PathFlags::Tunnel | PathFlags::Elevated | PathFlags::Colossal | PathFlags::TowerTop | PathFlags::UnderTower
			| PathFlags::Slide | PathFlags::GeyserBase | PathFlags::Islet | PathFlags::Boardwalk | PathFlags::Gap | PathFlags::RiverCross;
		TArray<FRouteSample> Samples;
		Samples.Reserve(Points.Num());
		for (const FTNProcPathPoint& Point : Points)
		{
			FRouteSample& Sample = Samples.AddDefaulted_GetRef();
			Sample.Location = Point.Location;
			Sample.WidthCm = Point.Width;
			Sample.bNoGate = (Point.Flags & NoGate) != 0;
			Sample.bShore = (Point.Flags & PathFlags::Shore) != 0;
		}
		return Samples;
	}

	void BlockGatesNearObstacles(TArray<FRouteSample>& Samples, const TArray<FLineObstacle>& Obstacles, double ClearCm)
	{
		for (FRouteSample& Sample : Samples)
		{
			const FVector2D Point(Sample.Location);
			for (const FLineObstacle& Obstacle : Obstacles)
			{
				if (!Obstacle.bKeepCentered && FVector2D::Distance(Point, Obstacle.Center) <= Obstacle.RadiusCm + ClearCm)
				{
					Sample.bNoGate = true;
					break;
				}
			}
		}
	}
}

ATN_KartTrack::ATN_KartTrack()
{
	// Un punto de la spline por muestra del camino (4 m): no recorta las curvas cerradas ni las cuevas.
	RoadSampleStepCm = 400.f;
	// Sin cajas de munición del Rally.
	AmmoBoxesPerRow = 0;
}

double ATN_KartTrack::GetRoadHalfWidthAtArc(double Arc) const
{
	if (RoadArcs.Num() < 2 || RoadHalfWidths.Num() != RoadArcs.Num())
	{
		return 700.0;
	}
	// La spline va por la línea del piloto: su largo difiere un poco del eje; se lleva el arco a la escala del eje.
	const double SplineLength = FMath::Max(1.0, static_cast<double>(GetTrackLengthCm()));
	const double PlanArc = FMath::Clamp(Arc * RoadArcs.Last() / SplineLength, 0.0, RoadArcs.Last());
	const int32 Upper = FMath::Clamp(Algo::UpperBound(RoadArcs, PlanArc), 1, RoadArcs.Num() - 1);
	const double Span = FMath::Max(1.0, RoadArcs[Upper] - RoadArcs[Upper - 1]);
	return FMath::Lerp(RoadHalfWidths[Upper - 1], RoadHalfWidths[Upper], FMath::Clamp((PlanArc - RoadArcs[Upper - 1]) / Span, 0.0, 1.0));
}

bool ATN_KartTrack::BuildFromMap(ATN_ProcMapGenerator& Generator)
{
	MapGeneration = 0;
	if (!Generator.IsMapReady())
	{
		return false;
	}
	TArray<FTNProcPathPoint> Points;
	Generator.GetMainPathWorld(Points);
	const double SeaZ = Generator.GetSeaLevelWorldZ();

	TArray<FVector4> Raw;
	Generator.GetMainPathObstaclesWorld(Raw);
	TArray<TNKart::FLineObstacle> Obstacles;
	Obstacles.Reserve(Raw.Num());
	for (const FVector4& Obstacle : Raw)
	{
		Obstacles.Add({ FVector2D(Obstacle.X, Obstacle.Y), FMath::Abs(Obstacle.W), Obstacle.W < 0.0 });
	}

	TArray<TNKart::FRouteSample> Samples = TNKart::RouteSamplesFrom(Points);
	TNKart::BlockGatesNearObstacles(Samples, Obstacles, GateObstacleClearCm);
	const TNKart::FRoutePlan Plan = TNKart::PlanRouteFromPath(Samples, TNKart::MakePlanParams(SeaZ));
	if (!Plan.bValid)
	{
		UE_LOG(LogTNRally, Error, TEXT("[KartTrack] El camino del mapa (%d muestras) no da para una pista."), Points.Num());
		return false;
	}

	// Línea del piloto IA: alrededor de los obstáculos, centrada en las puertas y recta en la parrilla.
	TArray<TNKart::FLineObstacle> LineObstacles = Obstacles;
	for (const TNRally::FGateDef& Gate : Plan.Gates)
	{
		LineObstacles.Add({ FVector2D(Gate.Location), static_cast<double>(GateCenteredRadiusCm), true });
	}
	TArray<double> HalfWidths;
	HalfWidths.Reserve(Plan.RoadWidthCm.Num());
	for (const double Width : Plan.RoadWidthCm)
	{
		HalfWidths.Add(0.5 * Width);
	}
	TArray<double> Offsets = TNKart::PlanRacingLineOffsets(Plan.Road, HalfWidths, LineObstacles, LineClearanceCm, LineRampCm);
	const double StraightUntil = Plan.GateArcCm.Num() > 0 ? Plan.GateArcCm[0] + TNKart::StraightStartCm : 0.0;
	for (int32 Index = 0; Index < Offsets.Num() && Plan.RoadArcCm[Index] <= StraightUntil; ++Index)
	{
		Offsets[Index] = 0.0;
	}
	const TArray<FVector> Line = TNKart::OffsetRoad(Plan.Road, Offsets);

	if (!BuildFromGates(Plan.Gates, false, Line, 0.0))
	{
		return false;
	}
	RoadArcs = Plan.RoadArcCm;
	RoadHalfWidths = HalfWidths;
	MapGeneration = Generator.GetBuiltGeneration();

	// Validación del camino para el kart (en el log): las rampas más empinadas y el paso más estrecho.
	double MaxSlope = 0.0;
	double MaxSlopeArc = 0.0;
	double MinWidth = TNumericLimits<double>::Max();
	double MinWidthArc = 0.0;
	for (int32 Index = 1; Index < Plan.Road.Num(); ++Index)
	{
		const double Run = FMath::Max(1.0, FVector::Dist2D(Plan.Road[Index], Plan.Road[Index - 1]));
		const double Slope = FMath::Abs(Plan.Road[Index].Z - Plan.Road[Index - 1].Z) / Run;
		if (Slope > MaxSlope) { MaxSlope = Slope; MaxSlopeArc = Plan.RoadArcCm[Index]; }
		if (Plan.RoadWidthCm[Index] < MinWidth) { MinWidth = Plan.RoadWidthCm[Index]; MinWidthArc = Plan.RoadArcCm[Index]; }
	}
	UE_LOG(LogTNRally, Log, TEXT("[KartTrack] Mapa %d: %.2f km, %d puertas, %d obstáculos rodeados; pendiente máxima %.0f %% en %.0f m, paso más estrecho %.1f m en %.0f m."),
		MapGeneration, GetTrackLengthCm() / 100000.0, GetGateCount(), Obstacles.Num(), 100.0 * MaxSlope,
		MaxSlopeArc / 100.0, MinWidth / 100.0, MinWidthArc / 100.0);
	return true;
}
