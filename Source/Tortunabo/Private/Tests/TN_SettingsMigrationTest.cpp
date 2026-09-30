// Versión de los ajustes guardados (#75): un guardado sin número cuenta como de la 1 y se migra sin tocar lo elegido;
// el de la versión actual se usa tal cual y el de una build más nueva también, con aviso.
// Se testean las funciones de TN_SettingsMigration.h que usa UTN_GameSettingsSubsystem::LoadSettings.
//   UnrealEditor-Cmd <uproject> -ExecCmds="Automation RunTests Tortunabo.Settings.Version; Quit" -nullrhi -unattended

#include "Misc/AutomationTest.h"
#include "Settings/TN_SettingsMigration.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNSettingsVersionResolveTest,
	"Tortunabo.Settings.Version.Resolve",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNSettingsVersionResolveTest::RunTest(const FString& Parameters)
{
	using namespace TNSettingsMigration;
	using TNSaveLogic::EMigration;

	TestEqual(TEXT("Sin número → la más antigua (1)"), ResolveSavedVersion(0), 1);
	TestEqual(TEXT("Número negativo (fichero raro) → 1"), ResolveSavedVersion(-4), 1);
	TestEqual(TEXT("Con número → ese"), ResolveSavedVersion(2), 2);

	FTNGameSettings Settings;
	TestTrue(TEXT("Sin número → migrar"), Migrate(Settings, 0) == EMigration::Upgrade);
	TestTrue(TEXT("De la 2 → migrar"), Migrate(Settings, 2) == EMigration::Upgrade);
	TestTrue(TEXT("La actual → tal cual"), Migrate(Settings, TNSaveLogic::SETTINGS_SAVE_VERSION) == EMigration::UpToDate);
	TestTrue(TEXT("De una build más nueva → tal cual, con aviso"),
		Migrate(Settings, TNSaveLogic::SETTINGS_SAVE_VERSION + 1) == EMigration::FromNewerBuild);

	UTN_SettingsSaveGame* Save = NewObject<UTN_SettingsSaveGame>();
	TestEqual(TEXT("Un guardado nuevo no tiene número hasta sellarlo (si no, no se escribiría)"), Save->Version, 0);
	Save->StampCurrentVersion();
	TestEqual(TEXT("Sellado con la actual"), Save->Version, TNSaveLogic::SETTINGS_SAVE_VERSION);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNSettingsVersionMigrateTest,
	"Tortunabo.Settings.Version.Migrate",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNSettingsVersionMigrateTest::RunTest(const FString& Parameters)
{
	using namespace TNSettingsMigration;

	// Un guardado de la 2: lo que eligió el jugador y, de los campos de la 3, lo de serie (no venían en el fichero).
	FTNGameSettings FromV2;
	FromV2.MusicVolume = 0.3f;
	FromV2.KeyOverrides.Add(TEXT("IA_Jump#0"), TEXT("SpaceBar"));
	FromV2.UIScale = 1.25f;
	Migrate(FromV2, 2);
	TestEqual(TEXT("2 → 3 conserva el volumen"), FromV2.MusicVolume, 0.3f);
	TestEqual(TEXT("2 → 3 conserva las teclas"), FromV2.KeyOverrides.Num(), 1);
	TestEqual(TEXT("2 → 3 conserva la escala"), FromV2.UIScale, 1.25f);
	TestTrue(TEXT("2 → 3: idioma del sistema"), FromV2.Language.IsEmpty());
	TestTrue(TEXT("2 → 3: ojo de pez de serie (encendido)"), FromV2.bFisheye);

	// Un guardado sin número que en realidad es de la 3: no se pierde lo elegido de la 3.
	FTNGameSettings Unnumbered;
	Unnumbered.Language = TEXT("en");
	Unnumbered.bFisheye = false;
	Migrate(Unnumbered, 0);
	TestEqual(TEXT("Sin número: se queda el idioma elegido"), Unnumbered.Language, FString(TEXT("en")));
	TestFalse(TEXT("Sin número: se queda el ojo de pez apagado"), Unnumbered.bFisheye);
	return true;
}

#endif
