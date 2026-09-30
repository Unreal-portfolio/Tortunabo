// Grupo de botones (issue #55): reglas de TNButtonGroupRules (TN_ButtonGroupManager.h), las mismas que usa
// ATN_ButtonGroupManager en el servidor y en el OnRep de los clientes. Correr desde Session Frontend (categoría
// "Tortunabo.World.ButtonGroup") o headless:
//   UnrealEditor-Cmd <uproject> -ExecCmds="Automation RunTests Tortunabo.World.ButtonGroup; Quit" -nullrhi -unattended

#include "Misc/AutomationTest.h"
#include "World/TN_ButtonGroupManager.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNButtonGroupDecideTest,
	"Tortunabo.World.ButtonGroup.Decide",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNButtonGroupDecideTest::RunTest(const FString& Parameters)
{
	using namespace TNButtonGroupRules;

	TestTrue(TEXT("Faltan botones: nada"), Decide(2, 3, false, true) == EGroupChange::None);
	TestTrue(TEXT("Todos pulsados: se activa"), Decide(3, 3, false, true) == EGroupChange::Activate);
	TestTrue(TEXT("Con umbral 2 de 4: se activa con 2"), Decide(2, 2, false, true) == EGroupChange::Activate);
	TestTrue(TEXT("Ya activado: nada"), Decide(3, 3, true, true) == EGroupChange::None);
	TestTrue(TEXT("One-shot activado y sueltan: sigue activado"), Decide(0, 3, true, true) == EGroupChange::None);
	TestTrue(TEXT("Sin one-shot y sueltan: se desactiva"), Decide(1, 3, true, false) == EGroupChange::Deactivate);
	TestTrue(TEXT("Sin one-shot, desactivado y faltan: nada"), Decide(1, 3, false, false) == EGroupChange::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNButtonGroupClientTargetsTest,
	"Tortunabo.World.ButtonGroup.ClientTargets",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNButtonGroupClientTargetsTest::RunTest(const FString& Parameters)
{
	using namespace TNButtonGroupRules;

	// Quien entra tarde recibe bGroupActive y en su OnRep mueve los objetivos que el servidor no le replica: así ve el
	// puzzle resuelto. Los que replican su movimiento ya le llegan movidos: moverlos también los desplazaría dos veces.
	TestTrue(TEXT("Objetivo sin réplica (puerta del nivel): lo mueve el cliente"), ClientMovesTarget(false, false));
	TestTrue(TEXT("Replicado sin movimiento replicado: lo mueve el cliente"), ClientMovesTarget(true, false));
	TestFalse(TEXT("Con movimiento replicado: lo mueve solo el servidor"), ClientMovesTarget(true, true));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
