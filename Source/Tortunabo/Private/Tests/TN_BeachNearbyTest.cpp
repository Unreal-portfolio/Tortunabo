// Trampas de la playa: solo miran a los personajes que tienen al alcance en planta (#60).
// Se testea la regla de TN_BeachNearby.h que usa TNBeachNearby::Gather.
//   UnrealEditor-Cmd <uproject> -ExecCmds="Automation RunTests Tortunabo.Beach.Nearby; Quit" -nullrhi -unattended

#include "Misc/AutomationTest.h"
#include "World/Beach/TN_BeachNearby.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNBeachNearbyTest,
	"Tortunabo.Beach.Nearby.Within2D",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNBeachNearbyTest::RunTest(const FString& Parameters)
{
	using namespace TNBeachNearby;

	const FVector Center(1000.0, -500.0, 200.0);
	TestTrue(TEXT("Encima"), IsWithin2D(Center, Center, 0.0));
	TestTrue(TEXT("Dentro del radio"), IsWithin2D(Center + FVector(300.0, 400.0, 0.0), Center, 500.0));
	TestTrue(TEXT("Justo en el borde"), IsWithin2D(Center + FVector(500.0, 0.0, 0.0), Center, 500.0));
	TestFalse(TEXT("Fuera del radio"), IsWithin2D(Center + FVector(300.0, 401.0, 0.0), Center, 500.0));
	// La altura la deciden las reglas de cada trampa (una tortuga que cae encima de la mina también cuenta).
	TestTrue(TEXT("La altura no cuenta"), IsWithin2D(Center + FVector(0.0, 0.0, 5000.0), Center, 10.0));
	TestFalse(TEXT("Radio negativo: nadie"), IsWithin2D(Center, Center, -1.0));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
