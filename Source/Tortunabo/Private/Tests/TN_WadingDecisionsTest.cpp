// Vadeo simple (no nado): lógica pura del multiplicador de velocidad según
// profundidad. Sin mundo, sin componentes — se testea la función de
// TN_WadingDecisions.h que UTN_WadingComponent usa en producción. Correr
// desde Session Frontend (categoría "Tortunabo.Wading") o headless:
//   UnrealEditor-Cmd <uproject> -ExecCmds="Automation RunTests Tortunabo.Wading; Quit" -nullrhi -unattended

#include "Misc/AutomationTest.h"
#include "Player/TN_WadingDecisions.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNWadingSpeedMultiplierTest,
	"Tortunabo.Wading.SpeedMultiplier",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNWadingSpeedMultiplierTest::RunTest(const FString& Parameters)
{
	using namespace TNWadingLogic;

	const float MinDepth = 10.f;
	const float FullDepth = 90.f;
	const float WadeMult = 0.55f;

	TestEqual(TEXT("Por debajo de MinDepth no hay efecto"),
		ComputeSpeedMultiplier(5.f, MinDepth, FullDepth, WadeMult), 1.f);

	TestEqual(TEXT("Justo en MinDepth tampoco hay efecto (borde)"),
		ComputeSpeedMultiplier(MinDepth, MinDepth, FullDepth, WadeMult), 1.f);

	TestEqual(TEXT("En FullDepth satura en WadeSpeedMultiplier"),
		ComputeSpeedMultiplier(FullDepth, MinDepth, FullDepth, WadeMult), WadeMult);

	TestEqual(TEXT("Por encima de FullDepth sigue saturado, no sigue bajando"),
		ComputeSpeedMultiplier(500.f, MinDepth, FullDepth, WadeMult), WadeMult);

	// Punto medio entre Min y Full → mitad de camino entre 1.0 y WadeSpeedMultiplier.
	const float MidDepth = (MinDepth + FullDepth) * 0.5f;
	const float Expected = (1.f + WadeMult) * 0.5f;
	TestEqual(TEXT("A mitad de rango interpola linealmente"),
		ComputeSpeedMultiplier(MidDepth, MinDepth, FullDepth, WadeMult), Expected, KINDA_SMALL_NUMBER);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
