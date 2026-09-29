// Recursos que el código carga sin colgar de un mapa: si alguien mueve o renombra el Blueprint, este test falla
// en vez de jugarse la ronda sin conchas de puntos.
//   UnrealEditor-Cmd <uproject> -ExecCmds="Automation RunTests Tortunabo.Assets; Quit" -nullrhi -unattended

#include "Misc/AutomationTest.h"
#include "Settings/TN_GameplayAssetSettings.h"
#include "World/TN_ScorePickup.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNScorePickupClassTest,
	"Tortunabo.Assets.ScorePickupClass",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNScorePickupClassTest::RunTest(const FString& Parameters)
{
	const UTN_GameplayAssetSettings* Settings = GetDefault<UTN_GameplayAssetSettings>();
	if (!TestNotNull(TEXT("Existen los ajustes de recursos"), Settings))
	{
		return false;
	}
	TestFalse(TEXT("ScorePickupClass configurada en DefaultGame.ini"), Settings->ScorePickupClass.IsNull());

	const UClass* Class = UTN_GameplayAssetSettings::GetScorePickupClass();
	TestNotNull(TEXT("La clase se resuelve"), Class);
	TestTrue(TEXT("Es una concha de puntos"), Class && Class->IsChildOf(ATN_ScorePickup::StaticClass()));
	TestTrue(TEXT("Es el Blueprint, no la clase nativa de reserva"), Class != ATN_ScorePickup::StaticClass());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
