// Rally sobre el mapa generado del cooperativo (#291): el camino conducible del generador (TNProcMap::FGenParams::bDrivable)
// y la pista que sale de él (TNRally::PlanRouteFromPath: puertas, salida, meta, parrilla, cajas y línea del piloto IA).
// Lógica pura. Correr desde Session Frontend (categorías "Tortunabo.Rally.Route" y "Tortunabo.ProcMap.Drivable") o
// headless con UnrealEditor-Win64-DebugGame-Cmd <uproject> -ExecCmds="Automation RunTests Tortunabo.Rally; Quit".

#include "Misc/AutomationTest.h"
#include "Lobby/TN_LobbyMission.h"
#include "Rally/TN_RallyRoutePlan.h"
#include "World/ProcMap/TN_ProcMapGenerate.h"
#include "World/ProcMap/TN_ProcMapTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace TNRallyRouteTestHelpers
{
	/**
	 * Camino recto en X de Length cm con una muestra cada Step cm y ancho Width: la playa empieza en ShoreFrom (cm) y baja
	 * hasta el mar al final; las muestras de [TunnelFrom, TunnelTo] van marcadas sin puerta.
	 */
	static TArray<TNRally::FRouteSample> StraightPath(double Length, double Step, double Width, double ShoreFrom,
		double TunnelFrom = -1.0, double TunnelTo = -1.0)
	{
		TArray<TNRally::FRouteSample> Samples;
		for (double X = 0.0; X <= Length + 1.0; X += Step)
		{
			TNRally::FRouteSample& Sample = Samples.AddDefaulted_GetRef();
			// En la playa el suelo baja de 300 cm a -100 cm en los últimos metros (el mar a 0).
			const double Z = X < ShoreFrom ? 300.0 : FMath::Lerp(300.0, -100.0, (X - ShoreFrom) / FMath::Max(1.0, Length - ShoreFrom));
			Sample.Location = FVector(X, 0.0, Z);
			Sample.WidthCm = Width;
			Sample.bShore = X >= ShoreFrom;
			Sample.bNoGate = X >= TunnelFrom && X <= TunnelTo;
		}
		return Samples;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNRallyRouteGatesTest, "Tortunabo.Rally.Route.GatesFromPath",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNRallyRouteGatesTest::RunTest(const FString& Parameters)
{
	using namespace TNRally;
	// 3 km recto, cueva de 1000 a 1150 m, playa desde 2900 m.
	const TArray<FRouteSample> Samples = TNRallyRouteTestHelpers::StraightPath(300000.0, 400.0, 1600.0, 290000.0, 100000.0, 115000.0);
	FRoutePlanParams Params;
	Params.MinFinishZ = 40.0;
	const FRoutePlan Plan = PlanRouteFromPath(Samples, Params);
	TestTrue(TEXT("El plan vale"), Plan.bValid);
	TestEqual(TEXT("Una puerta por arco"), Plan.Gates.Num(), Plan.GateArcCm.Num());
	if (Plan.GateArcCm.Num() < 3)
	{
		AddError(TEXT("Hacen falta al menos salida, una puerta y meta"));
		return false;
	}
	TestEqual(TEXT("La salida, a 42 m del principio"), Plan.GateArcCm[0], Params.StartGateArcCm, 1.0);
	TestEqual(TEXT("La meta, 25 m dentro de la playa"), Plan.FinishArcCm, 290000.0 + Params.FinishIntoShoreCm, 1.0);
	TestEqual(TEXT("La última puerta es la meta"), Plan.GateArcCm.Last(), Plan.FinishArcCm, 1.0);
	for (int32 Index = 1; Index < Plan.GateArcCm.Num(); ++Index)
	{
		const double Gap = Plan.GateArcCm[Index] - Plan.GateArcCm[Index - 1];
		TestTrue(FString::Printf(TEXT("Puertas %d-%d a no menos de %.0f m"), Index - 1, Index, Params.MinGateGapCm / 100.0), Gap >= Params.MinGateGapCm - 1.0);
		TestTrue(FString::Printf(TEXT("Puertas %d-%d a no más de una separación y la cueva"), Index - 1, Index),
			Gap <= Params.GateSpacingCm + Params.MaxGateShiftCm + 1.0);
		const double Arc = Plan.GateArcCm[Index];
		TestFalse(FString::Printf(TEXT("La puerta %d no cae en la cueva (%.0f m)"), Index, Arc / 100.0), Arc >= 100000.0 - 400.0 && Arc <= 115000.0 + 400.0);
	}
	for (const FGateDef& Gate : Plan.Gates)
	{
		TestEqual(TEXT("Puerta del ancho del camino más el margen"), Gate.WidthCm, 1600.0 + Params.GateWidthMarginCm, 1.0);
		TestEqual(TEXT("Puerta mirando hacia delante"), Gate.YawDeg, 0.0, 0.5);
	}
	TestTrue(TEXT("El eje sigue pasada la meta (escapatoria)"), Plan.LengthCm > Plan.FinishArcCm);
	TestEqual(TEXT("Un ancho por punto del eje"), Plan.RoadWidthCm.Num(), Plan.Road.Num());

	// Camino muy corto: no da para salida y meta.
	const TArray<FRouteSample> Short = TNRallyRouteTestHelpers::StraightPath(8000.0, 400.0, 1600.0, 6000.0);
	TestFalse(TEXT("Un camino de 80 m no da para una pista"), PlanRouteFromPath(Short, Params).bValid);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNRallyRouteFinishTest, "Tortunabo.Rally.Route.FinishOnDryBeach",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNRallyRouteFinishTest::RunTest(const FString& Parameters)
{
	using namespace TNRally;
	// Playa corta: de 300 cm a -100 cm en 40 m; con la meta a 25 m caería en el agua, así que se queda antes de la orilla.
	const TArray<FRouteSample> Samples = TNRallyRouteTestHelpers::StraightPath(100000.0, 200.0, 1600.0, 96000.0);
	TArray<FVector> Points;
	for (const FRouteSample& Sample : Samples)
	{
		Points.Add(Sample.Location);
	}
	const TArray<double> Arc = CumulativeArc(Points);
	FRoutePlanParams Params;
	Params.MinFinishZ = 40.0;
	const double Finish = FinishArcForPath(Samples, Arc, Params);
	TestTrue(TEXT("La meta va en la playa"), Finish >= 96000.0);
	// Cota del suelo en la meta (recta y bajada lineal): por encima de la mínima.
	const double ZAtFinish = FMath::Lerp(300.0, -100.0, (Finish - 96000.0) / 4000.0);
	TestTrue(FString::Printf(TEXT("La meta queda en seco (%.0f cm sobre el mar)"), ZAtFinish), ZAtFinish >= Params.MinFinishZ - 20.0);

	// Sin playa: la meta, al final del camino menos la escapatoria.
	TArray<FRouteSample> NoShore = Samples;
	for (FRouteSample& Sample : NoShore)
	{
		Sample.bShore = false;
	}
	TestEqual(TEXT("Sin playa, al final menos la escapatoria"), FinishArcForPath(NoShore, Arc, Params), Arc.Last() - Params.RunOffCm, 1.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNRallyRouteGateShiftTest, "Tortunabo.Rally.Route.GateShift",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNRallyRouteGateShiftTest::RunTest(const FString& Parameters)
{
	using namespace TNRally;
	FRoutePlanParams Params;
	// Prohibido de 240 a 260 m: la puerta de 250 m (42 + 250 = 292 m, libre) no se mueve; con prohibido de 280 a 300 m se adelanta.
	const TArray<double> Free = PlanGateArcs(4200.0, 100000.0, Params, [](double) { return false; });
	TestEqual(TEXT("Sin estorbos: salida, cada 250 m y meta"), Free.Num(), 5);
	TestEqual(TEXT("Segunda puerta a 250 m de la salida"), Free[1], 4200.0 + 25000.0, 1.0);
	const TArray<double> Shifted = PlanGateArcs(4200.0, 100000.0, Params, [](double S) { return S >= 28000.0 && S <= 30000.0; });
	TestTrue(TEXT("La puerta que cae en la cueva se adelanta hasta salir de ella"), Shifted.Num() >= 2 && Shifted[1] > 30000.0 && Shifted[1] <= 30000.0 + 400.0);
	// Prohibido un tramo más largo que lo que se puede adelantar: esa puerta no se pone.
	const TArray<double> Dropped = PlanGateArcs(4200.0, 100000.0, Params, [](double S) { return S >= 28000.0 && S <= 45000.0; });
	TestTrue(TEXT("Sin sitio en 80 m, la puerta se quita"), Dropped.Num() >= 2 && (Dropped[1] < 28000.0 || Dropped[1] > 45000.0));
	TestEqual(TEXT("Siempre acaba en la meta"), Dropped.Last(), 100000.0, 1.0);
	TestEqual(TEXT("Meta antes de la salida: ninguna puerta"), PlanGateArcs(5000.0, 4000.0, Params, [](double) { return false; }).Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNRallyRouteRacingLineTest, "Tortunabo.Rally.Route.RacingLine",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNRallyRouteRacingLineTest::RunTest(const FString& Parameters)
{
	using namespace TNRally;
	TArray<FVector> Road;
	TArray<double> Half;
	for (int32 Index = 0; Index <= 60; ++Index)
	{
		Road.Add(FVector(Index * 1000.0, 0.0, 0.0));
		Half.Add(1500.0);
	}
	TestTrue(TEXT("Sin obstáculos, la línea va por el eje"), !PlanRacingLineOffsets(Road, Half, {}, 450.0, 4500.0).ContainsByPredicate(
		[](double Offset) { return !FMath::IsNearlyZero(Offset); }));

	// Pieza de 4 m de radio a 1 m a la izquierda del eje a 300 m: la línea pasa por la derecha, a 4 m + 4,5 m de su centro.
	const FLineObstacle Plaza{ FVector2D(30000.0, -100.0), 400.0 };
	const TArray<double> Offsets = PlanRacingLineOffsets(Road, Half, { Plaza }, 450.0, 4500.0);
	TestTrue(TEXT("Rodea por la derecha (más sitio)"), Offsets[30] > 0.0);
	TestTrue(TEXT("Pasa a la holgura pedida del obstáculo"), Offsets[30] - (-100.0) >= 400.0 + 450.0 - 1.0);
	TestTrue(TEXT("Rodea toda la pieza, no solo su centro"), Offsets[29] > 0.0 && Offsets[31] > 0.0);
	TestTrue(TEXT("Lejos del obstáculo, por el eje"), FMath::IsNearlyZero(Offsets[0]) && FMath::IsNearlyZero(Offsets[60]));
	for (int32 Index = 0; Index < Offsets.Num(); ++Index)
	{
		TestTrue(TEXT("Nunca fuera de la calzada"), FMath::Abs(Offsets[Index]) <= Half[Index] - 250.0 + 1.0);
	}

	// Dos obstáculos a 17 m uno del otro (lo que pasaba en la salida de un mapa normal): la línea los rodea a los dos.
	TArray<double> WideHalf;
	WideHalf.Init(1750.0, Road.Num());
	const FLineObstacle A{ FVector2D(30000.0, 550.0), 300.0 };
	const FLineObstacle B{ FVector2D(31700.0, 200.0), 170.0 };
	const TArray<double> Weave = PlanRacingLineOffsets(Road, WideHalf, { A, B }, 450.0, 4500.0);
	for (int32 Index = 0; Index < Road.Num(); ++Index)
	{
		for (const FLineObstacle& Obstacle : { A, B })
		{
			const double Along = FMath::Abs(Road[Index].X - Obstacle.Center.X);
			if (Along <= Obstacle.RadiusCm + 450.0)
			{
				TestTrue(FString::Printf(TEXT("A la altura de un obstáculo (punto %d), la línea pasa a su holgura"), Index),
					FMath::Abs(Weave[Index] - Obstacle.Center.Y) >= Obstacle.RadiusCm + 450.0 - 1.0);
			}
		}
	}

	// Un obstáculo fuera de la calzada no cambia nada.
	const FLineObstacle Far{ FVector2D(30000.0, 6000.0), 400.0 };
	TestTrue(TEXT("Obstáculo lejos del camino: línea por el eje"), FMath::IsNearlyZero(PlanRacingLineOffsets(Road, Half, { Far }, 450.0, 4500.0)[30]));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNRallyRouteGridAndOffTrackTest, "Tortunabo.Rally.Route.GridAndOffTrack",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNRallyRouteGridAndOffTrackTest::RunTest(const FString& Parameters)
{
	using namespace TNRally;
	TestEqual(TEXT("Calzada desconocida: la parrilla de siempre"), GridSlotOffsetForWidth(3, 0.0), GridSlotOffset(3));
	TestEqual(TEXT("Calzada ancha: 3,5 m a cada lado"), FMath::Abs(GridSlotOffsetForWidth(0, 3000.0).Y), GridHalfSpacingCm, 0.01);
	TestEqual(TEXT("Calzada de 9 m: columnas más juntas (2,25 m)"), FMath::Abs(GridSlotOffsetForWidth(1, 900.0).Y), 225.0, 0.01);
	TestEqual(TEXT("Nunca a menos de 1,8 m del eje"), FMath::Abs(GridSlotOffsetForWidth(2, 400.0).Y), 180.0, 0.01);
	TestTrue(TEXT("Izquierda y derecha alternas"), GridSlotOffsetForWidth(0, 900.0).Y < 0.0 && GridSlotOffsetForWidth(1, 900.0).Y > 0.0);
	TestEqual(TEXT("Las filas no cambian con el ancho"), GridSlotOffsetForWidth(5, 900.0).X, GridSlotOffset(5).X, 0.01);

	TestEqual(TEXT("Calzada estrecha: fuera de pista a 40 m"), OffTrackLimitCm(700.0), OffTrackDistanceCm, 0.01);
	TestEqual(TEXT("Explanada de 60 m: fuera de pista a 45 m"), OffTrackLimitCm(3000.0), 4500.0, 0.01);
	FOffTrackState State;
	TestFalse(TEXT("A 44 m en una explanada no está fuera"), UpdateOffTrack(State, 4400.0, OffTrackLimitCm(3000.0), 2.0));
	TestTrue(TEXT("A 46 m durante 1 s, sí"), UpdateOffTrack(State, 4600.0, OffTrackLimitCm(3000.0), 1.0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNProcMapDrivableTest, "Tortunabo.ProcMap.Drivable",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNProcMapDrivableTest::RunTest(const FString& Parameters)
{
	using namespace TNProcMap;
	constexpr uint32 NotDrivable = PathFlags::GeyserBase | PathFlags::Slide | PathFlags::CliffUp | PathFlags::Islet | PathFlags::Boardwalk
		| PathFlags::Gap | PathFlags::Elevated | PathFlags::Colossal | PathFlags::TowerTop | PathFlags::UnderTower;
	double PreviousLength = 0.0;
	for (int32 D = 0; D < 3; ++D)
	{
		const ETNProcDifficulty Difficulty = static_cast<ETNProcDifficulty>(D);
		double LengthSum = 0.0;
		for (const uint32 Seed : { 11u, 777u, 2027u })
		{
			// El perfil del cooperativo, como en el Rally (ATN_ProcMapGenerator::BuildLayout).
			FGenParams Params = TN_MakeDefaultProcProfile(ETNProcGameMode::Coop, Difficulty).ToGenParams(Seed);
			Params.bDrivable = true;
			FLayout Layout;
			if (!GenerateLayout(Params, Layout))
			{
				AddError(FString::Printf(TEXT("Sin layout conducible (semilla %u, dificultad %d): %hs"), Seed, D, Layout.FailReason));
				continue;
			}
			const FString Where = FString::Printf(TEXT("semilla %u, dificultad %d"), Seed, D);
			TestEqual(*FString::Printf(TEXT("Sin cruces colosales (%s)"), *Where), Layout.Crossings.Num(), 0);
			TestEqual(*FString::Printf(TEXT("Sin ramas (%s)"), *Where), Layout.Branches.Num(), 0);
			int32 Blocked = 0;
			double MinWidth = TNumericLimits<double>::Max();
			double MaxSlope = 0.0;
			for (int32 Index = 0; Index < Layout.Main.Num(); ++Index)
			{
				const FPathSample& Sample = Layout.Main[Index];
				Blocked += (Sample.Flags & NotDrivable) != 0 ? 1 : 0;
				if ((Sample.Flags & PathFlags::Shore) == 0)
				{
					MinWidth = FMath::Min(MinWidth, Sample.Width);
				}
				if (Index > 0 && (Sample.Flags & PathFlags::Shore) == 0)
				{
					const double Run = FMath::Max(1.0, Sample.S - Layout.Main[Index - 1].S);
					MaxSlope = FMath::Max(MaxSlope, FMath::Abs(Sample.Z - Layout.Main[Index - 1].Z) / Run);
				}
			}
			TestEqual(*FString::Printf(TEXT("Ni géiseres, ni toboganes, ni escalones, ni isletas, ni huecos (%s)"), *Where), Blocked, 0);
			TestTrue(*FString::Printf(TEXT("Sin pasos de menos de 6,5 m, tampoco en las cuevas (%s): %.0f cm"), *Where, MinWidth), MinWidth >= 650.0);
			TestTrue(*FString::Printf(TEXT("Pendiente conducible (%s): %.2f"), *Where, MaxSlope), MaxSlope <= Layout.Params.MaxPathSlope * 1.6);
			int32 Forbidden = 0;
			for (const FFeature& Feature : Layout.Features)
			{
				const bool bForbidden = Feature.Type == EFeature::Geyser || Feature.Type == EFeature::SlideZone || Feature.Type == EFeature::Gap
					|| Feature.Type == EFeature::EggNest || Feature.Type == EFeature::Log || Feature.Type == EFeature::PathProp
					|| Feature.Type == EFeature::Boulder || Feature.Type == EFeature::ClimbTower || Feature.Type == EFeature::BonusPickup
					|| Feature.Type == EFeature::Islet || Feature.Type == EFeature::Boardwalk || Feature.Type == EFeature::Tower;
				Forbidden += bForbidden ? 1 : 0;
			}
			TestEqual(*FString::Printf(TEXT("Nada de lo de las tortugas a pie en el camino (%s)"), *Where), Forbidden, 0);
			LengthSum += Layout.MainLength();
		}
		TestTrue(FString::Printf(TEXT("La dificultad %d alarga el camino como en el cooperativo"), D), LengthSum > PreviousLength);
		PreviousLength = LengthSum;
	}

	// Sin bDrivable la generación es la de siempre (mismos parámetros, mismo layout).
	const FGenParams Coop = TN_MakeDefaultProcProfile(ETNProcGameMode::Coop, ETNProcDifficulty::Easy).ToGenParams(11u);
	FLayout A;
	FLayout B;
	if (GenerateLayout(Coop, A) && GenerateLayout(Coop, B))
	{
		TestEqual(TEXT("El cooperativo no cambia: mismo camino"), A.Main.Num(), B.Main.Num());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNRallyLobbyModeTest, "Tortunabo.Rally.Route.LobbyMode",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNRallyLobbyModeTest::RunTest(const FString& Parameters)
{
	bool bInMenu = false;
	for (const ETNProcGameMode Mode : TNLobbyMission::MenuModes)
	{
		bInMenu |= Mode == ETNProcGameMode::Rally;
	}
	TestTrue(TEXT("El Rally se elige en el menú, la sala y el general"), bInMenu);
	TestEqual(TEXT("Una sala de Rally se queda en Rally"), TNLobbyMission::NormalizeMenuMode(ETNProcGameMode::Rally), ETNProcGameMode::Rally);
	TestFalse(TEXT("El Rally tiene nombre propio"), TNLobbyMission::ModeName(ETNProcGameMode::Rally).IsEmpty());
	TestTrue(TEXT("El Rally va después de los modos que ya había (el número se guarda en las salas)"),
		static_cast<int32>(ETNProcGameMode::Rally) > static_cast<int32>(ETNProcGameMode::Survival));
	return true;
}

#endif
