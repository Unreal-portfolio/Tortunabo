// Algas: la malla del vaivén no se reconstruye en CPU cada frame salvo cerca de la cámara o enrollando a alguien.
// Se testea la regla de TN_SeaweedDecisions.h que usa ATN_BeachSeaweed::Tick.
//   UnrealEditor-Cmd <uproject> -ExecCmds="Automation RunTests Tortunabo.Seaweed; Quit" -nullrhi -unattended

#include "Misc/AutomationTest.h"
#include "World/TN_SeaweedDecisions.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNSeaweedRebuildRateTest,
	"Tortunabo.Seaweed.RebuildRate",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNSeaweedRebuildRateTest::RunTest(const FString& Parameters)
{
	using namespace TNSeaweedLogic;

	TestEqual(TEXT("Enrollando: cada frame aunque esté lejos"), RebuildInterval(true, true, 20000.0), 0.f);
	TestEqual(TEXT("Enrollando: cada frame aunque no se vea"), RebuildInterval(true, false, 100.0), 0.f);
	TestEqual(TEXT("En reposo y cerca: cada frame (mismo aspecto)"), RebuildInterval(false, true, 800.0), 0.f);
	TestEqual(TEXT("En reposo a media distancia: 20 Hz"), RebuildInterval(false, true, 4000.0), MID_RATE_INTERVAL);
	TestEqual(TEXT("En reposo lejos: 8 Hz"), RebuildInterval(false, true, 7000.0), FAR_RATE_INTERVAL);
	TestEqual(TEXT("Muy lejos: quieta"), RebuildInterval(false, true, 12000.0), NEVER);
	TestEqual(TEXT("Fuera de pantalla: quieta"), RebuildInterval(false, false, 500.0), NEVER);

	TestTrue(TEXT("Intervalo 0: siempre"), ShouldRebuild(0.f, 0.f));
	TestFalse(TEXT("20 Hz: no antes de 50 ms"), ShouldRebuild(MID_RATE_INTERVAL, 0.016f));
	TestTrue(TEXT("20 Hz: sí a los 50 ms"), ShouldRebuild(MID_RATE_INTERVAL, 0.051f));
	TestFalse(TEXT("NEVER: nunca"), ShouldRebuild(NEVER, 100.f));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
