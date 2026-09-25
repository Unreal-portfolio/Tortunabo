#pragma once

#include "CoreMinimal.h"
#include "World/ProcMap/TN_ProcMapLayout.h"

/**
 * Zonas de exclusión de la vegetación y los props (círculos con cubos para consultas rápidas):
 * estructuras, huevos, géiseres, huecos, puzles y tramos no tallados del camino. La usan el
 * scatter de capas (TN_ProcMapGenerator_Spawn.cpp) y la vegetación procedural (…_Flora.cpp).
 */
struct FTNProcKeepOut
{
	double Cell = 5000.0;
	FVector2D Origin = FVector2D(-30000.0, -30000.0);
	int32 W = 0;
	int32 H = 0;
	TArray<FVector2D> Centers;
	TArray<double> Radii;
	TArray<TArray<int32>> Buckets;

	void Init(double WorldSize)
	{
		W = H = FMath::CeilToInt((WorldSize + 60000.0) / Cell) + 1;
		Buckets.SetNum(W * H);
	}

	void Add(const FVector2D& C, double R)
	{
		const int32 Idx = Centers.Add(C);
		Radii.Add(R);
		const int32 X0 = FMath::Clamp(FMath::FloorToInt((C.X - R - Origin.X) / Cell), 0, W - 1);
		const int32 X1 = FMath::Clamp(FMath::FloorToInt((C.X + R - Origin.X) / Cell), 0, W - 1);
		const int32 Y0 = FMath::Clamp(FMath::FloorToInt((C.Y - R - Origin.Y) / Cell), 0, H - 1);
		const int32 Y1 = FMath::Clamp(FMath::FloorToInt((C.Y + R - Origin.Y) / Cell), 0, H - 1);
		for (int32 y = Y0; y <= Y1; ++y) { for (int32 x = X0; x <= X1; ++x) { Buckets[y * W + x].Add(Idx); } }
	}

	bool Blocked(const FVector2D& P) const
	{
		const int32 X = FMath::Clamp(FMath::FloorToInt((P.X - Origin.X) / Cell), 0, W - 1);
		const int32 Y = FMath::Clamp(FMath::FloorToInt((P.Y - Origin.Y) / Cell), 0, H - 1);
		for (const int32 Idx : Buckets[Y * W + X])
		{
			if (FVector2D::DistSquared(P, Centers[Idx]) < Radii[Idx] * Radii[Idx]) { return true; }
		}
		return false;
	}

	/** Exclusiones del layout: estructuras, huevos, géiseres, huecos, puzles y tramos no tallados. */
	void AddLayout(const TNProcMap::FLayout& Layout)
	{
		using namespace TNProcMap;
		Init(Layout.WorldSize);
		for (const FFeature& F : Layout.Features)
		{
			const FVector2D C(F.Location.X, F.Location.Y);
			switch (F.Type)
			{
				case EFeature::EggNest:        Add(C, 800.0); break;
				case EFeature::Geyser:         Add(C, 700.0); break;
				case EFeature::Tower:          Add(C, F.Radius + 900.0); break;
				case EFeature::Gate:           Add(C, FMath::Max(F.Width, F.Length) * 0.5 + 1200.0); break;
				case EFeature::StartArea:      Add(C, F.Radius + 600.0); break;
				case EFeature::Gap:            Add(C, FMath::Max(F.Height, F.Width) * 0.5 + 400.0); break;
				case EFeature::ThrowWall:      Add(C, 1800.0); break;
				case EFeature::SabotageGate:   Add(C, 1200.0); break;
				case EFeature::SabotageSwitch: Add(C, 400.0); break;
				case EFeature::RiverBridge:    Add(C, F.Length * 0.5 + 300.0); break;
				case EFeature::LavaPool:       Add(C, F.Radius + 300.0); break;
				case EFeature::GiantTree:      Add(C, F.Radius * 3.0); break;
				default: break;
			}
		}
		// Las murallas son mucho más gruesas que su adarve: en talud, hasta 10-12 m del eje.
		TArray<FIntPoint> WallSpans;
		for (const FCrossing& C : Layout.Crossings)
		{
			if (C.Type == ETNProcCrossingType::Wall) { WallSpans.Add(FIntPoint(Layout.Route[C.HighStep].FirstSample, Layout.Route[C.HighStep].LastSample)); }
		}
		for (int32 i = 0; i < Layout.Main.Num(); ++i)
		{
			const FPathSample& S = Layout.Main[i];
			if ((S.Flags & (PathFlags::Elevated | PathFlags::Colossal | PathFlags::Islet | PathFlags::Boardwalk)) != 0)
			{
				bool bWall = false;
				for (const FIntPoint& Span : WallSpans) { if (i >= Span.X && i <= Span.Y) { bWall = true; break; } }
				Add(S.P, S.Width * 0.5 + (bWall ? 1400.0 : 350.0));
			}
		}
	}
};
