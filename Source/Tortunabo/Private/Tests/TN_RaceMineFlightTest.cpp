// Vuelo de la mina de arena de la carrera (issue #84): lógica pura de TN_RaceMineFlight.h, la misma que usa ATN_RaceMine
// en el servidor, con suelos de prueba. Correr desde Session Frontend (categoría "Tortunabo.RaceItems.Mine") o headless:
//   UnrealEditor-Cmd <uproject> -ExecCmds="Automation RunTests Tortunabo.RaceItems.Mine; Quit" -nullrhi -unattended

#include "Misc/AutomationTest.h"
#include "World/Beach/TN_RaceMineFlight.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace TNRaceMineFlightTestHelpers
{
	struct FFlightRun
	{
		/** Lo más hondo (cm) que ha quedado el centro de la mina bajo la arena más su radio en algún paso. */
		double WorstSink = 0.0;
		TNRaceMineFlight::FState End;
		bool bAtRest = false;
	};

	/** Lanza la mina desde Pos con Vel y la sigue a 60 pasos por segundo hasta que se para (o 6 s, como el actor). */
	static FFlightRun Fly(TFunctionRef<double(double X)> GroundAtX, const FVector& Pos, const FVector& Vel)
	{
		using namespace TNRaceMineFlight;
		FFlightRun Run;
		Run.End.Pos = Pos;
		Run.End.Vel = Vel;
		const auto Ground = [&GroundAtX](const FVector& Where, double) { return GroundAtX(Where.X); };
		for (int32 Frame = 0; Frame < 6 * 60 && !Run.bAtRest; ++Frame)
		{
			Run.bAtRest = Step(Run.End, 1.0 / 60.0, Ground).bAtRest;
			Run.WorstSink = FMath::Max(Run.WorstSink, GroundAtX(Run.End.Pos.X) + MineRadius - Run.End.Pos.Z);
		}
		return Run;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNRaceMineRisingSlopeTest,
	"Tortunabo.RaceItems.Mine.RisingSlope",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNRaceMineRisingSlopeTest::RunTest(const FString& Parameters)
{
	using namespace TNRaceMineFlight;
	using namespace TNRaceMineFlightTestHelpers;

	// Llano hasta 2 m, cuesta de 45° que sube 2 m y meseta. La mina va subiendo cuando llega a la cuesta, y la cuesta sube
	// más deprisa que ella: antes no se miraba el suelo al subir y cruzaba la meseta por dentro (casi 1 m bajo la arena).
	const auto Ramp = [](double X) { return X < 200.0 ? 0.0 : (X < 400.0 ? X - 200.0 : 200.0); };
	{
		const FFlightRun Run = Fly(Ramp, FVector(0.0, 0.0, MineRadius + 20.0), FVector(1500.0, 0.0, 300.0));
		TestTrue(TEXT("Subiendo contra la cuesta no se mete bajo la arena"), Run.WorstSink <= 1.0);
		TestTrue(TEXT("Se para"), Run.bAtRest);
		TestTrue(TEXT("Se para encima de la meseta"), Run.End.Pos.X > 400.0 && FMath::IsNearlyEqual(Run.End.Pos.Z, 200.0 + MineRadius, 1.0));
	}

	// El borde de una meseta de 1,5 m (un escalón): tampoco lo cruza por dentro.
	const auto Cliff = [](double X) { return X < 300.0 ? 0.0 : 150.0; };
	{
		const FFlightRun Run = Fly(Cliff, FVector(0.0, 0.0, MineRadius + 40.0), FVector(1200.0, 0.0, 500.0));
		TestTrue(TEXT("Subiendo contra un escalón no se mete bajo la arena"), Run.WorstSink <= 1.0);
		TestTrue(TEXT("Se para encima del escalón"), Run.bAtRest && FMath::IsNearlyEqual(Run.End.Pos.Z, 150.0 + MineRadius, 1.0));
	}

	// Un paso suelto: subiendo con la arena por encima, se queda encima y deja de subir, sin rebotar.
	{
		FState State;
		State.Pos = FVector(0.0, 0.0, 100.0);
		State.Vel = FVector(600.0, 0.0, 400.0);
		const FStepResult Result = Step(State, 1.0 / 60.0, [](const FVector&, double) { return 300.0; });
		TestEqual(TEXT("Queda a ras de la arena"), State.Pos.Z, 300.0 + MineRadius, 0.01);
		TestEqual(TEXT("Deja de subir"), State.Vel.Z, 0.0, 0.01);
		TestEqual(TEXT("No cuenta como rebote"), State.Bounces, 0);
		TestFalse(TEXT("No se para en el aire"), Result.bAtRest);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNRaceMineFlatLandingTest,
	"Tortunabo.RaceItems.Mine.FlatLanding",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNRaceMineFlatLandingTest::RunTest(const FString& Parameters)
{
	using namespace TNRaceMineFlight;
	using namespace TNRaceMineFlightTestHelpers;

	// Lo de siempre: cae en llano, rebota una vez y se va rodando hasta quedar quieta a ras de la arena.
	const FFlightRun Run = Fly([](double) { return 0.0; }, FVector(0.0, 0.0, 500.0), FVector(300.0, 0.0, 0.0));
	TestTrue(TEXT("Se para antes de 6 s"), Run.bAtRest);
	TestEqual(TEXT("Rebota una vez"), Run.End.Bounces, MaxBounces);
	TestEqual(TEXT("Queda a ras de la arena"), Run.End.Pos.Z, MineRadius, 0.01);
	TestTrue(TEXT("No se mete bajo la arena"), Run.WorstSink <= 0.01);
	TestTrue(TEXT("Rueda hacia delante"), Run.End.Pos.X > 300.0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
