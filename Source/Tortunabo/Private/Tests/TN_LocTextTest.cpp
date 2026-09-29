// Ayudas de textos localizables (Core/TN_LocText.h) y nombres de teclas (UTN_GameSettingsSubsystem::KeyDisplayName). Las pruebas no
// dependen del idioma en marcha (el editor puede tener traducciones compiladas): miran la forma del resultado, no las palabras.
// Correr desde Session Frontend (categoría "Tortunabo.Settings.LocText") o headless:
//   UnrealEditor-Cmd <uproject> -ExecCmds="Automation RunTests Tortunabo.Settings.LocText; Quit" -nullrhi -unattended

#include "Core/TN_LocText.h"
#include "InputCoreTypes.h"
#include "Internationalization/Internationalization.h"
#include "Misc/AutomationTest.h"
#include "Settings/TN_GameSettingsSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

// ─────────────────────────────────────────────────────────────────────────────
// Números y tiempos
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNLocTextNumbersTest,
	"Tortunabo.Settings.LocText.Numbers",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNLocTextNumbersTest::RunTest(const FString& Parameters)
{
	// Sin separador de millares (puestos, cuentas atrás, semillas).
	TestEqual(TEXT("Int(1000) sin separador de millares"), TNLocText::Int(1000).ToString(), FString(TEXT("1000")));
	TestEqual(TEXT("Int(0)"), TNLocText::Int(0).ToString(), FString(TEXT("0")));

	// Un decimal: «12,3» o «12.3» según la cultura; nunca dos.
	const FString OneDecimal = TNLocText::OneDecimal(12.34).ToString();
	TestTrue(TEXT("OneDecimal empieza por 12"), OneDecimal.StartsWith(TEXT("12")));
	TestTrue(TEXT("OneDecimal acaba en un solo decimal (3)"), OneDecimal.EndsWith(TEXT("3")) && OneDecimal.Len() == 4);

	// «m:ss»: 65 s = 1 minuto y 05 segundos; los negativos se quedan en cero.
	const FString Clock = TNLocText::MinutesSeconds(65).ToString();
	TestTrue(TEXT("MinutesSeconds(65) lleva el minuto"), Clock.Contains(TEXT("1")));
	TestTrue(TEXT("MinutesSeconds(65) lleva los segundos con dos cifras"), Clock.Contains(TEXT("05")));
	TestTrue(TEXT("MinutesSeconds(-5) se queda en cero"), TNLocText::MinutesSeconds(-5).ToString().Contains(TEXT("00")));
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Listas de nombres y nombres de jugador
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNLocTextListTest,
	"Tortunabo.Settings.LocText.List",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNLocTextListTest::RunTest(const FString& Parameters)
{
	const FText Ana = INVTEXT("Ana");
	const FText Leo = INVTEXT("Leo");
	const FText Bea = INVTEXT("Bea");

	TestTrue(TEXT("Una lista vacía sale vacía"), TNLocText::JoinList({}).IsEmpty());
	TestEqual(TEXT("Un nombre sale tal cual"), TNLocText::JoinList({ Ana }).ToString(), FString(TEXT("Ana")));

	const FString Two = TNLocText::JoinList({ Ana, Leo }).ToString();
	TestTrue(TEXT("Dos nombres: los dos, en orden"), Two.Find(TEXT("Ana")) != INDEX_NONE && Two.Find(TEXT("Leo")) > Two.Find(TEXT("Ana")));

	const FString Three = TNLocText::JoinList({ Ana, Leo, Bea }).ToString();
	const int32 AtAna = Three.Find(TEXT("Ana"));
	const int32 AtLeo = Three.Find(TEXT("Leo"));
	const int32 AtBea = Three.Find(TEXT("Bea"));
	TestTrue(TEXT("Tres nombres: los tres, en orden"), AtAna != INDEX_NONE && AtLeo > AtAna && AtBea > AtLeo);

	// Un nombre de jugador no se traduce; sin nombre sale el de reserva.
	TestEqual(TEXT("PlayerName respeta el nombre"), TNLocText::PlayerName(TEXT("Perla")).ToString(), FString(TEXT("Perla")));
	TestFalse(TEXT("PlayerName sin nombre da un texto"), TNLocText::PlayerName(FString()).IsEmpty());
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Plurales con la sintaxis del motor
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNLocTextPluralTest,
	"Tortunabo.Settings.LocText.Plural",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNLocTextPluralTest::RunTest(const FString& Parameters)
{
	// Como «1 sala / 5 salas» del menú de salas: el número entero, para que el motor sepa la forma.
	const FText Pattern = INVTEXT("{0} {0}|plural(one=sala,other=salas)");
	const FString One = FText::Format(FTextFormat(Pattern), 1).ToString();
	const FString Five = FText::Format(FTextFormat(Pattern), 5).ToString();
	TestTrue(TEXT("Uno va en singular"), One.EndsWith(TEXT("sala")) && One.StartsWith(TEXT("1")));
	TestTrue(TEXT("Cinco va en plural"), Five.EndsWith(TEXT("salas")) && Five.StartsWith(TEXT("5")));
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Nombres de teclas
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNLocTextKeyNamesTest,
	"Tortunabo.Settings.LocText.KeyNames",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNLocTextKeyNamesTest::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("Sin tecla enseña un guion"), UTN_GameSettingsSubsystem::KeyDisplayName(FKey()).IsEmpty());

	// Las teclas y los botones que el juego nombra, y las que dejan al motor.
	const FKey Named[] = { EKeys::SpaceBar, EKeys::LeftShift, EKeys::Escape, EKeys::Enter, EKeys::LeftMouseButton, EKeys::MouseScrollUp,
		EKeys::Gamepad_LeftShoulder, EKeys::Gamepad_FaceButton_Bottom, EKeys::Gamepad_DPad_Up, EKeys::Gamepad_Special_Right,
		EKeys::E, EKeys::F1, EKeys::One };
	for (const FKey& Key : Named)
	{
		TestFalse(FString::Printf(TEXT("%s tiene nombre"), *Key.ToString()), UTN_GameSettingsSubsystem::KeyDisplayName(Key).IsEmpty());
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
