// Fase 1 de la mecánica de caparazón (issue #6): lógica pura de entrada y salida.
// Sin mundo, sin componentes — se testean las funciones de TN_ShellDecisions.h que
// UTN_ShellComponent usa en producción. Correr desde Session Frontend (categoría
// "Tortunabo.Shell") o headless:
//   UnrealEditor-Cmd <uproject> -ExecCmds="Automation RunTests Tortunabo.Shell; Quit" -nullrhi -unattended

#include "Misc/AutomationTest.h"
#include "Player/TN_ShellDecisions.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace TNShellTestHelpers
{
	/** Contexto de un personaje sano, de pie y con las manos libres: puede entrar. */
	static TNShellLogic::FShellEnterContext MakeValidContext()
	{
		TNShellLogic::FShellEnterContext Context;
		Context.bOnGround = true;
		Context.bIsDead = false;
		Context.bIsKnockedDown = false;
		Context.bIsDiving = false;
		Context.bHasEquippedItem = false;
		return Context;
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Entrar al caparazón
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNShellEnterTest,
	"Tortunabo.Shell.Enter",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNShellEnterTest::RunTest(const FString& Parameters)
{
	using namespace TNShellLogic;
	using namespace TNShellTestHelpers;

	TestTrue(TEXT("De pie, sano y con las manos libres → entra"),
		CanEnterShell(MakeValidContext()));

	{
		FShellEnterContext Context = MakeValidContext();
		Context.bOnGround = false;
		TestFalse(TEXT("En el aire no se entra"), CanEnterShell(Context));
	}

	{
		FShellEnterContext Context = MakeValidContext();
		Context.bIsDead = true;
		TestFalse(TEXT("Muerto no se entra"), CanEnterShell(Context));
	}

	{
		FShellEnterContext Context = MakeValidContext();
		Context.bIsKnockedDown = true;
		TestFalse(TEXT("Derribado no se entra"), CanEnterShell(Context));
	}

	{
		FShellEnterContext Context = MakeValidContext();
		Context.bIsDiving = true;
		TestFalse(TEXT("Buceando no se entra"), CanEnterShell(Context));
	}

	{
		FShellEnterContext Context = MakeValidContext();
		Context.bHasEquippedItem = true;
		TestFalse(TEXT("Con un objeto en la mano no se entra"), CanEnterShell(Context));
	}

	{
		// Varias condiciones a la vez: el resultado sigue siendo negativo.
		FShellEnterContext Context = MakeValidContext();
		Context.bOnGround = false;
		Context.bIsDead = true;
		Context.bHasEquippedItem = true;
		TestFalse(TEXT("Varios impedimentos a la vez → no entra"), CanEnterShell(Context));
	}

	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Salir del caparazón — permanencia mínima
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNShellExitTest,
	"Tortunabo.Shell.Exit",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNShellExitTest::RunTest(const FString& Parameters)
{
	using namespace TNShellLogic;

	constexpr float MinTime = 0.3f;

	TestFalse(TEXT("Salir en el mismo instante de entrar → bloqueado"),
		CanExitShell(0.f, MinTime));

	TestFalse(TEXT("Salir antes del mínimo → bloqueado"),
		CanExitShell(0.29f, MinTime));

	TestTrue(TEXT("Salir justo en el mínimo → permitido"),
		CanExitShell(MinTime, MinTime));

	TestTrue(TEXT("Salir pasado el mínimo → permitido"),
		CanExitShell(5.f, MinTime));

	TestTrue(TEXT("Sin permanencia mínima configurada se sale siempre"),
		CanExitShell(0.f, 0.f));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
