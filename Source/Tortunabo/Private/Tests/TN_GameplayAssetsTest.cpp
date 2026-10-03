// Recursos que el código carga sin colgar de un mapa: si alguien mueve o renombra el Blueprint, este test falla
// en vez de jugarse la ronda sin conchas de puntos.
//   UnrealEditor-Cmd <uproject> -ExecCmds="Automation RunTests Tortunabo.Assets; Quit" -nullrhi -unattended

#include "Misc/AutomationTest.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"
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

// #328: el CDO de los ajustes vive en el pool que el GC no recorre; si guarda una referencia fuerte a la clase cargada,
// la build empaquetada se cierra en el primer GC. La clase la retiene el handle del StreamableManager.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNScorePickupClassRetentionTest,
	"Tortunabo.Assets.ScorePickupClassRetention",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNScorePickupClassRetentionTest::RunTest(const FString& Parameters)
{
	UClass* Class = UTN_GameplayAssetSettings::GetScorePickupClass();
	if (!TestTrue(TEXT("La clase resuelta es el Blueprint"), Class && Class != ATN_ScorePickup::StaticClass()))
	{
		return false;
	}

	const UTN_GameplayAssetSettings* Settings = GetDefault<UTN_GameplayAssetSettings>();
	for (TPropertyValueIterator<FObjectPropertyBase> It(UTN_GameplayAssetSettings::StaticClass(), Settings); It; ++It)
	{
		const FObjectPropertyBase* Property = It.Key();
		if (Property->IsA<FSoftObjectProperty>())
		{
			continue;
		}
		TestTrue(*FString::Printf(TEXT("El CDO no referencia la clase por %s"), *Property->GetName()),
			Property->GetObjectPropertyValue(It.Value()) != Class);
	}

	TestTrue(TEXT("El handle de carga retiene la clase"), UTN_GameplayAssetSettings::IsScorePickupClassRetained());
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	TestTrue(TEXT("Tras un GC se resuelve la misma clase"), UTN_GameplayAssetSettings::GetScorePickupClass() == Class);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
