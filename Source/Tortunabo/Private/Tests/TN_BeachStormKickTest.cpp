// Patada de la tormenta de bañistas (issue #252): lógica pura de TN_BeachStormKick.h, la misma que usa ATN_BeachStorm en el
// servidor. La patada siempre se ve como un vuelo: el arco llega a su sitio, la bola que atraviesa vuelve a chocar al bajar y
// la que no llega recibe otra patada antes de ponerla en su sitio sin vuelo.
// Correr desde Session Frontend (categoría "Tortunabo.Beach.StormKick") o sin ventana:
//   UnrealEditor-Cmd <uproject> -ExecCmds="Automation RunTests Tortunabo.Beach.StormKick; Quit" -nullrhi -unattended

#include "Misc/AutomationTest.h"
#include "World/Beach/TN_BeachStormKick.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNBeachStormKickArcTest, "Tortunabo.Beach.StormKick.ArcReachesSpot",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNBeachStormKickArcTest::RunTest(const FString& Parameters)
{
	using namespace TNBeachStormKick;
	// El arco calculado acaba en su sitio con y sin amortiguación, en llano, cuesta arriba y cuesta abajo, corto y largo
	// (también un vuelo atravesando a 50 m, más allá de la distancia con arco comprobado).
	const FVector From(100.0, -40.0, 120.0);
	const FVector Targets[] = { FVector(2500.0, 0.0, 60.0), FVector(900.0, 400.0, 300.0), FVector(5000.0, -800.0, -150.0) };
	const float Flights[] = { 1.1f, 1.8f, 2.6f, 3.2f };
	const float Dampings[] = { 0.f, 0.25f };
	for (const FVector& To : Targets)
	{
		for (const float Flight : Flights)
		{
			for (const float Damping : Dampings)
			{
				const FVector Launch = BallisticLaunch(From, To, Flight, 980.f, Damping);
				const FVector End = BallisticPoint(From, Launch, Flight, 980.f, Damping);
				TestTrue(FString::Printf(TEXT("Llega a su sitio (vuelo %.1f s, amortiguación %.2f, a %.0f m)"), Flight, Damping, FVector::Dist(From, To) / 100.0),
					FVector::Dist(End, To) < 1.0);
				// Sale hacia arriba: se ve como una patada, no como un empujón por el suelo.
				TestTrue(TEXT("Sale hacia arriba"), Launch.Z > 0.0);
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNBeachStormKickPassThroughTest, "Tortunabo.Beach.StormKick.PassThroughEnds",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNBeachStormKickPassThroughTest::RunTest(const FString& Parameters)
{
	using namespace TNBeachStormKick;
	const float Flight = 2.6f;
	// Subiendo, sigue atravesando (la pared que tenía delante queda abajo).
	TestFalse(TEXT("Subiendo al principio, atraviesa"), ShouldEndPassThrough(0.3f * Flight, Flight, 800.f, false));
	// Bajando antes de la mitad del vuelo, tampoco (aún puede ir por encima de lo que cruza).
	TestFalse(TEXT("Bajando antes de la mitad, atraviesa"), ShouldEndPassThrough(0.45f * Flight, Flight, -200.f, false));
	// Bajando sobre su sitio con algo alrededor, sigue atravesando; sin nada, vuelve a chocar.
	TestFalse(TEXT("Bajando dentro de algo, atraviesa"), ShouldEndPassThrough(0.7f * Flight, Flight, -600.f, true));
	TestTrue(TEXT("Bajando sin nada alrededor, vuelve a chocar"), ShouldEndPassThrough(0.7f * Flight, Flight, -600.f, false));
	// Para el último tramo vuelve a chocar sí o sí: nunca cae a través de la arena.
	TestTrue(TEXT("En el último tramo vuelve a chocar aunque toque algo"), ShouldEndPassThrough(PassThroughLatest * Flight, Flight, -900.f, true));
	TestTrue(TEXT("Pasado el vuelo vuelve a chocar"), ShouldEndPassThrough(Flight + 0.5f, Flight, -900.f, true));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNBeachStormKickFailedLandingTest, "Tortunabo.Beach.StormKick.FailedLanding",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNBeachStormKickFailedLandingTest::RunTest(const FString& Parameters)
{
	using namespace TNBeachStormKick;
	// La bola que no llega recibe otra patada visible; solo tras MaxHops se la pone en su sitio sin vuelo.
	TestTrue(TEXT("Primera que no llega: otra patada"), ResolveFailedLanding(0) == EFailedLanding::Hop);
	for (int32 Hops = 1; Hops < MaxHops; ++Hops)
	{
		TestTrue(TEXT("Mientras queden patadas: otra patada"), ResolveFailedLanding(Hops) == EFailedLanding::Hop);
	}
	TestTrue(TEXT("Tras MaxHops: a su sitio"), ResolveFailedLanding(MaxHops) == EFailedLanding::Place);
	TestTrue(TEXT("Al menos una patada de más antes del último recurso"), MaxHops >= 1);
	return true;
}

#endif
