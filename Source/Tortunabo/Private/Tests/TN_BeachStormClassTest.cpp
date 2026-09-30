// La carrera crea la tormenta de bañistas con una clase de verdad (TSubclassOf), no por nombre: si ATN_BeachStorm
// cambia de nombre deja de compilar, y si alguien vacía la clase este test lo avisa.
// Correr desde Session Frontend (categoría "Tortunabo.Beach.StormClass") o sin ventana:
//   UnrealEditor-Cmd <uproject> -ExecCmds="Automation RunTests Tortunabo.Beach.StormClass; Quit" -nullrhi -unattended

#include "Misc/AutomationTest.h"
#include "Game/TN_BeachRaceGameMode.h"
#include "World/Beach/TN_BeachStorm.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNBeachStormClassTest, "Tortunabo.Beach.StormClass",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTNBeachStormClassTest::RunTest(const FString& Parameters)
{
	const ATN_BeachRaceGameMode* Defaults = GetDefault<ATN_BeachRaceGameMode>();
	UClass* StormClass = Defaults ? Defaults->GetStormClass() : nullptr;
	TestNotNull(TEXT("La carrera tiene clase de tormenta"), StormClass);
	if (StormClass)
	{
		TestTrue(TEXT("La clase de la tormenta es ATN_BeachStorm o una hija"), StormClass->IsChildOf(ATN_BeachStorm::StaticClass()));
	}
	return true;
}

#endif
