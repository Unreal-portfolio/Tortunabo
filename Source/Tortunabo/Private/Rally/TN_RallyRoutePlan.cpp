#include "Rally/TN_RallyRoutePlan.h"

namespace TNRally
{
	namespace
	{
		/** Paso con el que se busca sitio para una puerta adelantada (cm). */
		constexpr double RouteGateSearchStepCm = 200.0;
		/** Margen de la meta respecto al final del camino (cm). */
		constexpr double RouteFinishEndMarginCm = 1000.0;
		/** Distancia mínima del eje a la que se queda una columna de la parrilla (cm). */
		constexpr double RouteGridMinHalfCm = 180.0;
		/** Margen de la línea del piloto IA respecto al borde de la calzada (cm). */
		constexpr double RouteLineEdgeMarginCm = 250.0;

		/** Índice del segmento [I, I + 1] que contiene el arco S (recortado a la polilínea). */
		int32 SegmentAt(const TArray<double>& Arc, double S)
		{
			if (Arc.Num() < 2)
			{
				return 0;
			}
			const int32 Upper = Algo::UpperBound(Arc, S);
			return FMath::Clamp(Upper - 1, 0, Arc.Num() - 2);
		}

		double Alpha(const TArray<double>& Arc, int32 Index, double S)
		{
			const double Span = Arc[Index + 1] - Arc[Index];
			return Span > KINDA_SMALL_NUMBER ? FMath::Clamp((S - Arc[Index]) / Span, 0.0, 1.0) : 0.0;
		}
	}

	TArray<double> CumulativeArc(const TArray<FVector>& Points)
	{
		TArray<double> Arc;
		Arc.SetNumUninitialized(Points.Num());
		double Total = 0.0;
		for (int32 Index = 0; Index < Points.Num(); ++Index)
		{
			if (Index > 0)
			{
				Total += FVector::Dist(Points[Index - 1], Points[Index]);
			}
			Arc[Index] = Total;
		}
		return Arc;
	}

	double FinishArcForPath(const TArray<FRouteSample>& Samples, const TArray<double>& Arc, const FRoutePlanParams& Params)
	{
		if (Samples.Num() < 2 || Arc.Num() != Samples.Num())
		{
			return 0.0;
		}
		const double Total = Arc.Last();
		double Limit = FMath::Max(0.0, Total - RouteFinishEndMarginCm);
		// Donde el suelo baja de la cota mínima (la orilla) ya no puede ir la meta.
		if (Params.MinFinishZ > -1.0e9)
		{
			for (int32 Index = 1; Index < Samples.Num(); ++Index)
			{
				if (Samples[Index].bShore && Samples[Index].Location.Z < Params.MinFinishZ)
				{
					Limit = FMath::Min(Limit, FMath::Max(0.0, Arc[Index - 1] - 200.0));
					break;
				}
			}
		}
		for (int32 Index = 0; Index < Samples.Num(); ++Index)
		{
			if (Samples[Index].bShore)
			{
				return FMath::Min(Arc[Index] + Params.FinishIntoShoreCm, Limit);
			}
		}
		return FMath::Max(0.0, Total - Params.RunOffCm);
	}

	TArray<double> PlanGateArcs(double StartArc, double FinishArc, const FRoutePlanParams& Params, TFunctionRef<bool(double)> IsForbidden)
	{
		TArray<double> Gates;
		if (FinishArc <= StartArc)
		{
			return Gates;
		}
		Gates.Add(StartArc);
		const double Spacing = FMath::Max(Params.GateSpacingCm, 1000.0);
		const double MinGap = FMath::Clamp(Params.MinGateGapCm, 500.0, Spacing);
		double Last = StartArc;
		for (double Wanted = StartArc + Spacing; Wanted < FinishArc - MinGap; Wanted += Spacing)
		{
			// Adelanta la puerta hasta un sitio que valga (fuera de cuevas y estructuras), sin pasarse.
			double Chosen = -1.0;
			for (double Probe = Wanted; Probe <= Wanted + Params.MaxGateShiftCm; Probe += RouteGateSearchStepCm)
			{
				if (!IsForbidden(Probe))
				{
					Chosen = Probe;
					break;
				}
			}
			if (Chosen < 0.0 || Chosen - Last < MinGap || FinishArc - Chosen < MinGap)
			{
				continue;
			}
			Gates.Add(Chosen);
			Last = Chosen;
			// La siguiente cuenta desde la puerta puesta, no desde la que se quería.
			Wanted = Chosen;
		}
		Gates.Add(FinishArc);
		return Gates;
	}

	FRoutePlan PlanRouteFromPath(const TArray<FRouteSample>& Samples, const FRoutePlanParams& Params)
	{
		FRoutePlan Plan;
		if (Samples.Num() < 2)
		{
			return Plan;
		}
		TArray<FVector> Points;
		Points.Reserve(Samples.Num());
		for (const FRouteSample& Sample : Samples)
		{
			Points.Add(Sample.Location);
		}
		const TArray<double> Arc = CumulativeArc(Points);
		const double Total = Arc.Last();
		const double StartArc = FMath::Clamp(Params.StartGateArcCm, 0.0, Total);
		const double FinishArc = FinishArcForPath(Samples, Arc, Params);
		if (FinishArc - StartArc < FMath::Max(Params.MinGateGapCm, 1000.0))
		{
			return Plan;
		}

		auto SampleAt = [&Samples, &Arc](double S, FVector& OutLocation, double& OutWidth, bool& bOutNoGate)
		{
			const int32 Index = SegmentAt(Arc, S);
			const double T = Alpha(Arc, Index, S);
			OutLocation = FMath::Lerp(Samples[Index].Location, Samples[Index + 1].Location, T);
			OutWidth = FMath::Lerp(Samples[Index].WidthCm, Samples[Index + 1].WidthCm, T);
			bOutNoGate = Samples[Index].bNoGate || Samples[Index + 1].bNoGate;
		};

		// Eje de la spline cada RoadStep, de la primera muestra a la meta más la escapatoria.
		const double RoadEnd = FMath::Min(Total, FinishArc + Params.RunOffCm);
		const double Step = FMath::Max(Params.RoadStepCm, 100.0);
		for (double S = 0.0;; S += Step)
		{
			const double Clamped = FMath::Min(S, RoadEnd);
			FVector Location;
			double Width = 0.0;
			bool bNoGate = false;
			SampleAt(Clamped, Location, Width, bNoGate);
			Plan.Road.Add(Location);
			Plan.RoadWidthCm.Add(Width);
			if (Clamped >= RoadEnd)
			{
				break;
			}
		}
		Plan.RoadArcCm = CumulativeArc(Plan.Road);
		Plan.LengthCm = Plan.RoadArcCm.Last();

		// Puertas sobre el camino original (sus arcos casi coinciden con los del eje remuestreado).
		const TArray<double> GateArcs = PlanGateArcs(StartArc, FinishArc, Params, [&SampleAt](double S)
		{
			FVector Location;
			double Width = 0.0;
			bool bNoGate = false;
			SampleAt(S, Location, Width, bNoGate);
			return bNoGate;
		});
		for (const double GateArc : GateArcs)
		{
			FVector Location;
			double Width = 0.0;
			bool bNoGate = false;
			SampleAt(GateArc, Location, Width, bNoGate);
			FVector Ahead;
			FVector Behind;
			double Ignored = 0.0;
			SampleAt(FMath::Min(GateArc + 400.0, Total), Ahead, Ignored, bNoGate);
			SampleAt(FMath::Max(GateArc - 400.0, 0.0), Behind, Ignored, bNoGate);
			FGateDef Gate;
			Gate.Location = Location;
			const FVector Direction = (Ahead - Behind).GetSafeNormal2D();
			Gate.YawDeg = Direction.IsNearlyZero() ? 0.0 : Direction.Rotation().Yaw;
			Gate.bHasYaw = true;
			Gate.WidthCm = FMath::Clamp(Width + Params.GateWidthMarginCm, Params.MinGateWidthCm, Params.MaxGateWidthCm);
			Plan.Gates.Add(Gate);
			Plan.GateArcCm.Add(GateArc);
		}
		Plan.FinishArcCm = FinishArc;
		Plan.bValid = Plan.Gates.Num() >= 2 && Plan.Road.Num() >= 2;
		return Plan;
	}

	FVector2D GridSlotOffsetForWidth(int32 Slot, double RoadWidthCm)
	{
		const FVector2D Base = GridSlotOffset(Slot);
		if (RoadWidthCm <= 0.0)
		{
			return Base;
		}
		const double Half = FMath::Clamp(RoadWidthCm * 0.25, RouteGridMinHalfCm, GridHalfSpacingCm);
		return FVector2D(Base.X, Base.Y < 0.0 ? -Half : Half);
	}

	TArray<double> PlanItemRowArcs(double StartArc, double EndArc, const TArray<double>& GateArcs, double SpacingCm, double MinFromGateCm)
	{
		TArray<double> Rows;
		const double Spacing = FMath::Max(SpacingCm, 2000.0);
		for (double Arc = StartArc + 0.5 * Spacing; Arc < EndArc; Arc += Spacing)
		{
			double Placed = Arc;
			// Lejos de las puertas: se adelanta hasta quedar a MinFromGateCm de la que tiene cerca.
			for (int32 Guard = 0; Guard < 4; ++Guard)
			{
				bool bMoved = false;
				for (const double Gate : GateArcs)
				{
					if (FMath::Abs(Placed - Gate) < MinFromGateCm)
					{
						Placed = Gate + MinFromGateCm;
						bMoved = true;
					}
				}
				if (!bMoved)
				{
					break;
				}
			}
			if (Placed < EndArc && (Rows.Num() == 0 || Placed - Rows.Last() >= 0.5 * Spacing))
			{
				Rows.Add(Placed);
			}
		}
		return Rows;
	}

	int32 ItemBoxesForWidth(double RoadWidthCm)
	{
		return FMath::Clamp(FMath::FloorToInt32((RoadWidthCm - 200.0) / 400.0), 2, 6);
	}

	double ItemLateralSpacingCm(double RoadWidthCm, int32 Boxes)
	{
		return FMath::Min(450.0, FMath::Max(200.0, RoadWidthCm - 200.0) / FMath::Max(1, Boxes));
	}

	TArray<double> PlanRacingLineOffsets(const TArray<FVector>& Road, const TArray<double>& HalfWidthsCm,
		const TArray<FLineObstacle>& Obstacles, double ClearanceCm, double RampCm)
	{
		const int32 Num = Road.Num();
		TArray<double> Offsets;
		Offsets.SetNumZeroed(Num);
		if (Num < 2 || Obstacles.Num() == 0)
		{
			return Offsets;
		}
		const TArray<double> Arc = CumulativeArc(Road);
		auto Limit = [&HalfWidthsCm](int32 Index)
		{
			return HalfWidthsCm.IsValidIndex(Index) ? FMath::Max(0.0, HalfWidthsCm[Index] - RouteLineEdgeMarginCm) : 0.0;
		};
		auto DirAt = [&Road, Num](int32 Index)
		{
			const FVector From = Road[FMath::Max(0, Index - 1)];
			const FVector To = Road[FMath::Min(Num - 1, Index + 1)];
			return FVector2D(To - From).GetSafeNormal();
		};

		// Lo que pide cada obstáculo en el punto del eje más cercano: pasar por el lado con más sitio.
		struct FNeed
		{
			double Arc = 0.0;
			double Offset = 0.0;
		};
		TArray<FNeed> Needs;
		for (const FLineObstacle& Obstacle : Obstacles)
		{
			int32 Nearest = INDEX_NONE;
			double Best = TNumericLimits<double>::Max();
			for (int32 Index = 0; Index < Num; ++Index)
			{
				const double D = FVector2D::DistSquared(FVector2D(Road[Index]), Obstacle.Center);
				if (D < Best)
				{
					Best = D;
					Nearest = Index;
				}
			}
			const double Reach = Limit(Nearest) + Obstacle.RadiusCm + ClearanceCm + RouteLineEdgeMarginCm;
			if (Nearest == INDEX_NONE || Best > FMath::Square(Reach))
			{
				continue;
			}
			const FVector2D Dir = DirAt(Nearest);
			const FVector2D Right(-Dir.Y, Dir.X);
			const double Lateral = FVector2D::DotProduct(Obstacle.Center - FVector2D(Road[Nearest]), Right);
			const double Keep = Obstacle.RadiusCm + ClearanceCm;
			const double Edge = Limit(Nearest);
			// Hueco a cada lado del obstáculo dentro de la calzada.
			const double LeftRoom = (Lateral - Keep) - (-Edge);
			const double RightRoom = Edge - (Lateral + Keep);
			FNeed Need;
			Need.Arc = Arc[Nearest];
			Need.Offset = LeftRoom >= RightRoom ? FMath::Min(0.0, Lateral - Keep) : FMath::Max(0.0, Lateral + Keep);
			if (FMath::Abs(Need.Offset) > KINDA_SMALL_NUMBER)
			{
				Needs.Add(Need);
			}
		}

		// Rampa de entrada y salida; si dos se pisan, manda la que pide más.
		const double Ramp = FMath::Max(RampCm, 100.0);
		for (int32 Index = 0; Index < Num; ++Index)
		{
			double Chosen = 0.0;
			for (const FNeed& Need : Needs)
			{
				const double Weight = FMath::Clamp(1.0 - FMath::Abs(Arc[Index] - Need.Arc) / Ramp, 0.0, 1.0);
				// Meseta de un tercio de la rampa alrededor del obstáculo: lo rodea entero, no solo en su centro.
				const double Shaped = FMath::Clamp(Weight * 1.5, 0.0, 1.0);
				const double Value = Need.Offset * Shaped;
				if (FMath::Abs(Value) > FMath::Abs(Chosen))
				{
					Chosen = Value;
				}
			}
			const double Edge = Limit(Index);
			Offsets[Index] = FMath::Clamp(Chosen, -Edge, Edge);
		}
		return Offsets;
	}
}
