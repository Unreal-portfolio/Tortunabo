// Conchas de puntos dormidas de lejos (#59): despiertan dentro de su WakeDistance y, para dormirse, hay que alejarse un
// poco más (sin parpadeo en el borde). Se testea la función de TN_ScorePickupWakeSubsystem.h.
//   UnrealEditor-Cmd <uproject> -ExecCmds="Automation RunTests Tortunabo.Perf.ScorePickupWake; Quit" -nullrhi -unattended

#include "Misc/AutomationTest.h"
#include "World/TN_ScorePickupWakeSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNScorePickupWakeTest,
	"Tortunabo.Perf.ScorePickupWake",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNScorePickupWakeTest::RunTest(const FString& Parameters)
{
	using namespace TNScorePickupWake;
	constexpr float Wake = 6000.f;
	const auto Sq = [](double D) { return D * D; };

	TestTrue(TEXT("Dormida y cerca → despierta"), ShouldBeAwake(Sq(5000.0), Wake, false));
	TestFalse(TEXT("Dormida y lejos → sigue dormida"), ShouldBeAwake(Sq(7000.0), Wake, false));
	TestFalse(TEXT("Dormida justo pasado el borde → no despierta"), ShouldBeAwake(Sq(6100.0), Wake, false));
	TestTrue(TEXT("Despierta justo pasado el borde → sigue despierta (margen)"), ShouldBeAwake(Sq(6100.0), Wake, true));
	TestFalse(TEXT("Despierta y más allá del margen → se duerme"), ShouldBeAwake(Sq(Wake * SleepFactor + 10.0), Wake, true));
	return true;
}

#endif
