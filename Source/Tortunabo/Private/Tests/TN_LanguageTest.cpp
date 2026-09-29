// Idiomas del juego (Settings/TN_LanguageSettings.h): la lista de idiomas de la configuración y cómo se elige el que toca.
// No cambia el idioma del proceso (Apply toca la localización del editor): solo lee y resuelve. Correr desde Session Frontend
// (categoría "Tortunabo.Settings") o headless:
//   UnrealEditor-Cmd <uproject> -ExecCmds="Automation RunTests Tortunabo.Settings.Language; Quit" -nullrhi -unattended

#include "Internationalization/Internationalization.h"
#include "Misc/AutomationTest.h"
#include "Settings/TN_LanguageSettings.h"

#if WITH_DEV_AUTOMATION_TESTS

// ─────────────────────────────────────────────────────────────────────────────
// La lista: los trece idiomas, sin repetidos, con nombre y con cultura que el motor conoce
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNLanguageListTest,
	"Tortunabo.Settings.Language.List",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNLanguageListTest::RunTest(const FString& Parameters)
{
	const TArray<FTNLanguageEntry>& List = TNLanguage::GetLanguages();
	TestTrue(TEXT("La lista de idiomas no está vacía"), List.Num() > 0);

	// Los trece que pidió el proyecto (la configuración puede añadir más).
	static const TCHAR* Required[] = { TEXT("es-ES"), TEXT("en"), TEXT("fr"), TEXT("de"), TEXT("it"), TEXT("pt-BR"), TEXT("ru"), TEXT("pl"),
		TEXT("tr"), TEXT("ja"), TEXT("ko"), TEXT("zh-Hans"), TEXT("zh-Hant") };
	for (const TCHAR* Culture : Required)
	{
		TestTrue(FString::Printf(TEXT("Está %s en la lista de idiomas"), Culture), TNLanguage::IndexOf(Culture) != INDEX_NONE);
	}
	TestTrue(TEXT("El idioma de los textos origen está en la lista"), TNLanguage::IndexOf(TNLanguage::GetNativeCulture()) != INDEX_NONE);

	TSet<FString> Seen;
	for (const FTNLanguageEntry& Entry : List)
	{
		bool bAlreadyInSet = false;
		Seen.Add(Entry.Culture.ToLower(), &bAlreadyInSet);
		TestFalse(FString::Printf(TEXT("Cultura repetida: %s"), *Entry.Culture), bAlreadyInSet);
		TestFalse(FString::Printf(TEXT("%s tiene código"), *Entry.Culture), Entry.Culture.IsEmpty());
		TestFalse(FString::Printf(TEXT("%s tiene nombre para el jugador"), *Entry.Culture), TNLanguage::GetDisplayName(Entry).IsEmpty());
		TestTrue(FString::Printf(TEXT("El motor conoce la cultura %s"), *Entry.Culture), FInternationalization::Get().GetCulture(Entry.Culture).IsValid());
		// Una fuente de reserva sin archivo de negrita se apaña con la normal, pero la negrita sola no tiene sentido.
		TestTrue(FString::Printf(TEXT("%s: la negrita solo con fuente normal"), *Entry.Culture), Entry.FontBold.IsEmpty() || !Entry.FontRegular.IsEmpty());
	}
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Elegir el idioma: el guardado si está en la lista; si no, el del sistema (que siempre es uno de la lista)
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNLanguageResolveTest,
	"Tortunabo.Settings.Language.Resolve",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNLanguageResolveTest::RunTest(const FString& Parameters)
{
	const FString System = TNLanguage::FindSystemLanguage();
	TestTrue(FString::Printf(TEXT("El idioma del sistema (%s) es uno de la lista"), *System), TNLanguage::IndexOf(System) != INDEX_NONE);

	TestEqual(TEXT("Sin elegir toca el del sistema"), TNLanguage::ResolveLanguage(FString()), System);
	TestEqual(TEXT("Uno que no existe, el del sistema"), TNLanguage::ResolveLanguage(TEXT("klingon")), System);
	TestEqual(TEXT("Un idioma de la lista se respeta"), TNLanguage::ResolveLanguage(TEXT("ja")), FString(TEXT("ja")));
	TestEqual(TEXT("Sin distinguir mayúsculas se devuelve el de la lista"), TNLanguage::ResolveLanguage(TEXT("PT-br")), FString(TEXT("pt-BR")));
	TestEqual(TEXT("El vacío no está en la lista"), TNLanguage::IndexOf(FString()), (int32)INDEX_NONE);

	// El idioma activo es el que toca mientras no se aplique otro, y se reconoce por sus dos primeras letras.
	const FString Active = TNLanguage::GetActive();
	TestTrue(TEXT("El idioma activo es uno de la lista"), TNLanguage::IndexOf(Active) != INDEX_NONE);
	TestTrue(TEXT("IsActiveLanguage reconoce el idioma activo"), TNLanguage::IsActiveLanguage(*Active.Left(2)));
	TestFalse(TEXT("IsActiveLanguage no acepta el vacío"), TNLanguage::IsActiveLanguage(TEXT("")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
