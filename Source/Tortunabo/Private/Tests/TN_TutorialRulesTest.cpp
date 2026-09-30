// Reglas del recorrido del tutorial (issue #17): TNTutorialRules (TN_TutorialRules.h), las que usa el servidor en
// ServerGoToStation. Correr desde Session Frontend (categoría "Tortunabo.Tutorial") o headless:
//   UnrealEditor-Cmd <uproject> -ExecCmds="Automation RunTests Tortunabo.Tutorial; Quit" -nullrhi -unattended

#include "Misc/AutomationTest.h"
#include "Lobby/TN_TutorialRules.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNTutorialGoToStationTest,
	"Tortunabo.Tutorial.GoToStation",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNTutorialGoToStationTest::RunTest(const FString& Parameters)
{
	using namespace TNTutorialRules;
	constexpr int32 NumStations = 19;

	// Un cliente cualquiera (sin el salto de pruebas del anfitrión).
	TestFalse(TEXT("Estación negativa: rechazada"), CanGoToStation(-1, NumStations, 5, false));
	TestFalse(TEXT("Estación que no existe: rechazada"), CanGoToStation(NumStations, NumStations, 5, false));
	TestFalse(TEXT("Valor arbitrario enorme: rechazado"), CanGoToStation(2147483647, NumStations, 18, false));
	TestFalse(TEXT("Fuera del tutorial: no entra por aquí"), CanGoToStation(0, NumStations, INDEX_NONE, false));
	TestFalse(TEXT("Adelantarse a una estación no alcanzada: rechazado"), CanGoToStation(12, NumStations, 5, false));
	TestTrue(TEXT("Volver a una ya alcanzada: sí"), CanGoToStation(3, NumStations, 5, false));
	TestTrue(TEXT("Repetir la actual: sí"), CanGoToStation(5, NumStations, 5, false));

	// El anfitrión en una build de pruebas (TN.Tutorial.Station): a cualquiera que exista, aunque no esté dentro.
	TestTrue(TEXT("Anfitrión de pruebas: a la 12 sin haber llegado"), CanGoToStation(11, NumStations, INDEX_NONE, true));
	TestFalse(TEXT("Anfitrión de pruebas: tampoco a una que no existe"), CanGoToStation(NumStations, NumStations, 0, true));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNTutorialCheckpointStationTest,
	"Tortunabo.Tutorial.CheckpointStation",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNTutorialCheckpointStationTest::RunTest(const FString& Parameters)
{
	using namespace TNTutorialRules;
	constexpr int32 Catapult = 11;
	TestEqual(TEXT("Antes del cañón, cada punto es su estación"), StationOfCheckpoint(4, Catapult), 4);
	TestEqual(TEXT("La catapulta"), StationOfCheckpoint(Catapult, Catapult), Catapult);
	TestEqual(TEXT("El punto de pasado el cañón sigue siendo de la catapulta"), StationOfCheckpoint(Catapult + 1, Catapult), Catapult);
	TestEqual(TEXT("Después, uno menos (nadar)"), StationOfCheckpoint(Catapult + 2, Catapult), Catapult + 1);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
