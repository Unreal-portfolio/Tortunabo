// Dónde va el montículo de un rebuscable de la playa (#254): la regla pura de TN_BeachLoot.h que usa
// UTN_BeachLootSubsystem al repartir el botín. Un decorado sin sitio libre para su montículo no es rebuscable (antes se dejaba
// el montículo en un lado ocupado y el aviso salía sin montículo a la vista). Correr desde Session Frontend (categoría
// "Tortunabo.Beach.MoundSite") o sin ventana:
//   UnrealEditor-Cmd <uproject> -ExecCmds="Automation RunTests Tortunabo.Beach.MoundSite; Quit" -nullrhi -unattended

#include "Misc/AutomationTest.h"
#include "World/Beach/TN_BeachLoot.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNBeachMoundSiteTest,
	"Tortunabo.Beach.MoundSite",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNBeachMoundSiteTest::RunTest(const FString& Parameters)
{
	using namespace TNBeachLoot;

	// El primer lado libre, por orden.
	TestEqual(TEXT("Todo libre: el lado por el que se llega"), PickMoundSide([](int32) { return true; }), 0);
	TestEqual(TEXT("Ocupado el primero: el siguiente"), PickMoundSide([](int32 Side) { return Side != 0; }), 1);
	TestEqual(TEXT("Solo libre la última diagonal"), PickMoundSide([](int32 Side) { return Side == NumMoundSides - 1; }), NumMoundSides - 1);

	// Sin ningún lado libre no hay montículo, y sin él, ni rebuscable.
	TestEqual(TEXT("Ningún lado libre: sin sitio"), PickMoundSide([](int32) { return false; }), static_cast<int32>(INDEX_NONE));

	// No se pregunta de más (IsClearOfLayout recorre el reparto): se para en el primero libre.
	int32 Asked = 0;
	const int32 Picked = PickMoundSide([&Asked](int32 Side) { ++Asked; return Side == 2; });
	TestEqual(TEXT("Elige el lado libre"), Picked, 2);
	TestEqual(TEXT("Y deja de preguntar en él"), Asked, 3);

	// Los lados: los cuatro de siempre primero (llegada, costados y espalda) y después las diagonales, todos distintos.
	TestEqual(TEXT("Hay ocho lados"), NumMoundSides, 8);
	TestEqual(TEXT("1.º: por el que se llega"), MoundSideTurns[0], 0.0);
	TestEqual(TEXT("2.º: un costado"), MoundSideTurns[1], 90.0);
	TestEqual(TEXT("3.º: el otro costado"), MoundSideTurns[2], -90.0);
	TestEqual(TEXT("4.º: la espalda"), MoundSideTurns[3], 180.0);
	for (int32 First = 0; First < NumMoundSides; ++First)
	{
		for (int32 Second = First + 1; Second < NumMoundSides; ++Second)
		{
			const double Gap = FMath::Abs(FMath::FindDeltaAngleDegrees(MoundSideTurns[First], MoundSideTurns[Second]));
			TestTrue(*FString::Printf(TEXT("Los lados %d y %d no coinciden"), First, Second), Gap > 1.0);
		}
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
