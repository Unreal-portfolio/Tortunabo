// Nombres de sala (TN_RoomNames.h): la tabla fija que viaja por índice en la sesión y que ahora vive en el canal de
// localización del motor. Se comprueba lo que la interfaz, la red y la localización necesitan de ella: cantidad mínima, nada
// vacío, cabe en pantalla (28 caracteres), sin repetidos, identidad estable de cada texto (espacio «TNRoomNames», clave
// «Room_NNN»), nombre de reserva fuera de rango, sorteo dentro de rango que respeta «otro nombre» y, con el CSV de las
// adaptaciones inglesas (Tools/Localization/room_names_en.csv), que el inglés cumple las mismas reglas y cuadra con el
// origen. Correr desde Session Frontend (categoría "Tortunabo.Multiplayer") o headless:
//   UnrealEditor-Cmd <uproject> -ExecCmds="Automation RunTests Tortunabo.Multiplayer.RoomNames; Quit" -nullrhi -unattended

#include "Internationalization/Text.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Multiplayer/TN_RoomNames.h"
#include "Settings/TN_LanguageSettings.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace TNRoomNamesTest
{
	/** Mínimo de nombres que tiene que ofrecer la lista. */
	constexpr int32 MinNames = 220;

	/** Largo máximo de un nombre en cada idioma (lo que cabe en la interfaz). */
	constexpr int32 MaxLength = 28;

	/** Comillas de todo tipo, caracteres de control y emojis (fuera del plano básico): nada de eso cabe en un nombre de sala. */
	bool HasForbiddenChar(const FString& Name)
	{
		static const TCHAR* Forbidden = TEXT("\"'`‘’“”«»");
		for (const TCHAR Ch : Name)
		{
			if (FCString::Strchr(Forbidden, Ch) != nullptr)
			{
				return true;
			}
			// Fuera del plano básico (emojis y similares) o caracteres de control.
			if (Ch < 0x20 || (Ch >= 0xD800 && Ch <= 0xDFFF))
			{
				return true;
			}
		}
		return false;
	}

	/** Las reglas de cualquier nombre en cualquier idioma: no vacío, sin espacios en los bordes, cabe y sin comillas ni emojis. */
	void CheckName(FAutomationTestBase& Test, const TCHAR* Lang, int32 Id, const FString& Name)
	{
		Test.TestTrue(FString::Printf(TEXT("%s #%d no está vacío"), Lang, Id), !Name.IsEmpty());
		Test.TestTrue(FString::Printf(TEXT("%s #%d sin espacios sobrantes en los bordes"), Lang, Id), Name == Name.TrimStartAndEnd());
		Test.TestTrue(FString::Printf(TEXT("%s #%d cabe en %d caracteres (%d): %s"), Lang, Id, MaxLength, Name.Len(), *Name), Name.Len() <= MaxLength);
		Test.TestFalse(FString::Printf(TEXT("%s #%d sin comillas ni caracteres raros: %s"), Lang, Id, *Name), HasForbiddenChar(Name));
	}

	/** Un CSV con todo entre comillas (como el que escribe Tools/Localization/room_names_en.csv): las filas, cada una con sus campos. */
	TArray<TArray<FString>> ParseCsv(const FString& Text)
	{
		TArray<TArray<FString>> Rows;
		TArray<FString> Row;
		FString Field;
		bool bQuoted = false;
		bool bAny = false;
		for (int32 i = 0; i < Text.Len(); ++i)
		{
			const TCHAR Ch = Text[i];
			if (bQuoted)
			{
				if (Ch == TEXT('"'))
				{
					if (i + 1 < Text.Len() && Text[i + 1] == TEXT('"')) { Field.AppendChar(TEXT('"')); ++i; }
					else { bQuoted = false; }
				}
				else { Field.AppendChar(Ch); }
				continue;
			}
			if (Ch == TEXT('"')) { bQuoted = true; bAny = true; }
			else if (Ch == TEXT(',')) { Row.Add(Field); Field.Reset(); bAny = true; }
			else if (Ch == TEXT('\n') || Ch == TEXT('\r'))
			{
				if (Ch == TEXT('\r') && i + 1 < Text.Len() && Text[i + 1] == TEXT('\n')) { ++i; }
				if (bAny || !Field.IsEmpty()) { Row.Add(Field); Rows.Add(Row); }
				Row.Reset();
				Field.Reset();
				bAny = false;
			}
			else { Field.AppendChar(Ch); }
		}
		if (bAny || !Field.IsEmpty()) { Row.Add(Field); Rows.Add(Row); }
		return Rows;
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// La tabla: cantidad, contenido, largo, repetidos e identidad en la localización
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNRoomNamesTableTest,
	"Tortunabo.Multiplayer.RoomNames.Table",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNRoomNamesTableTest::RunTest(const FString& Parameters)
{
	const int32 Count = TNRoomNames::Num();
	TestTrue(FString::Printf(TEXT("Hay al menos %d nombres (hay %d)"), TNRoomNamesTest::MinNames, Count),
		Count >= TNRoomNamesTest::MinNames);

	// Se compara sin distinguir mayúsculas para que «Sea You Later» y «sea you later» cuenten como uno.
	TSet<FString> SeenSource;
	TSet<FString> SeenKeys;

	for (int32 Id = 0; Id < Count; ++Id)
	{
		const FString Source = TNRoomNames::GetSource(Id);
		TNRoomNamesTest::CheckName(*this, TEXT("ES"), Id, Source);

		bool bAlreadyInSet = false;
		SeenSource.Add(Source.ToLower(), &bAlreadyInSet);
		TestFalse(FString::Printf(TEXT("ES #%d repetido: %s"), Id, *Source), bAlreadyInSet);

		// La identidad del texto en la localización: espacio «TNRoomNames» y una clave estable por índice.
		const FText Text = TNRoomNames::Get(Id);
		const FString Key = TNRoomNames::GetKey(Id);
		TestEqual(FString::Printf(TEXT("La clave de #%d"), Id), Key, FString::Printf(TEXT("Room_%03d"), Id));
		SeenKeys.Add(Key, &bAlreadyInSet);
		TestFalse(FString::Printf(TEXT("Clave repetida en #%d: %s"), Id, *Key), bAlreadyInSet);
		TestTrue(FString::Printf(TEXT("#%d es un texto localizable (FTextInspector::ShouldGatherForLocalization)"), Id), FTextInspector::ShouldGatherForLocalization(Text));
		TestEqual(FString::Printf(TEXT("Espacio de nombres de #%d"), Id), FTextInspector::GetNamespace(Text).Get(FString()), FString(TEXT("TNRoomNames")));
		TestEqual(FString::Printf(TEXT("Clave de localización de #%d"), Id), FTextInspector::GetKey(Text).Get(FString()), Key);
		const FString* TextSource = FTextInspector::GetSourceString(Text);
		TestTrue(FString::Printf(TEXT("El origen de #%d es el español"), Id), TextSource && *TextSource == Source);

		// El texto que se ve es el del idioma elegido: el mismo que el origen si el idioma es el español (o no hay traducción).
		TestFalse(FString::Printf(TEXT("Get(%d) no está vacío"), Id), Text.IsEmpty());
		TestTrue(FString::Printf(TEXT("GetIn(%d, false) es lo que se ve"), Id), TNRoomNames::GetIn(Id, false) == Text.ToString());
		TestTrue(FString::Printf(TEXT("GetIn(%d, true) es el origen"), Id), TNRoomNames::GetIn(Id, true) == Source);
	}

	// El idioma lo manda el ajuste del juego, no la cultura del motor.
	TestTrue(TEXT("IsSpanish sigue al idioma del juego"), TNRoomNames::IsSpanish() == TNLanguage::IsActiveLanguage(TEXT("es")));

	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Las adaptaciones inglesas (CSV para la fase de traducción): mismas reglas y cuadran con el origen
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNRoomNamesEnglishCsvTest,
	"Tortunabo.Multiplayer.RoomNames.EnglishCsv",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNRoomNamesEnglishCsvTest::RunTest(const FString& Parameters)
{
	const FString Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Tools/Localization/room_names_en.csv"));
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		// Un juego empaquetado no lleva las herramientas: no es un fallo.
		AddWarning(FString::Printf(TEXT("No está %s: no se comprueba el inglés."), *Path));
		return true;
	}
	if (Text.Len() > 0 && Text[0] == 0xFEFF)
	{
		Text.RightChopInline(1);
	}

	const TArray<TArray<FString>> Rows = TNRoomNamesTest::ParseCsv(Text);
	const int32 Count = TNRoomNames::Num();
	// La primera fila son los títulos de las columnas.
	TestEqual(TEXT("Filas del CSV (sin los títulos)"), Rows.Num() - 1, Count);

	TSet<FString> SeenEnglish;
	for (int32 Id = 0; Id < Count && Rows.IsValidIndex(Id + 1); ++Id)
	{
		const TArray<FString>& Row = Rows[Id + 1];
		if (!TestEqual(FString::Printf(TEXT("Campos de la fila de #%d"), Id), Row.Num(), 4))
		{
			continue;
		}
		TestEqual(FString::Printf(TEXT("Espacio de nombres de la fila #%d"), Id), Row[0], FString(TEXT("TNRoomNames")));
		TestEqual(FString::Printf(TEXT("Clave de la fila #%d"), Id), Row[1], TNRoomNames::GetKey(Id));
		TestEqual(FString::Printf(TEXT("El español de la fila #%d cuadra con el código"), Id), Row[2], TNRoomNames::GetSource(Id));
		TNRoomNamesTest::CheckName(*this, TEXT("EN"), Id, Row[3]);
		bool bAlreadyInSet = false;
		SeenEnglish.Add(Row[3].ToLower(), &bAlreadyInSet);
		TestFalse(FString::Printf(TEXT("EN #%d repetido: %s"), Id, *Row[3]), bAlreadyInSet);
	}
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Índices fuera de rango: nombre de reserva
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNRoomNamesFallbackTest,
	"Tortunabo.Multiplayer.RoomNames.Fallback",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNRoomNamesFallbackTest::RunTest(const FString& Parameters)
{
	const int32 Count = TNRoomNames::Num();
	const int32 OutOfRange[] = { -1, Count, Count + 1, MAX_int32, MIN_int32, INDEX_NONE };

	for (const int32 Id : OutOfRange)
	{
		TestTrue(FString::Printf(TEXT("Índice %d fuera de rango → reserva (origen en español)"), Id),
			TNRoomNames::GetSource(Id) == TEXT("Sala sin nombre") && TNRoomNames::GetIn(Id, true) == TEXT("Sala sin nombre"));
		TestFalse(FString::Printf(TEXT("Get(%d) fuera de rango nunca devuelve vacío"), Id),
			TNRoomNames::Get(Id).IsEmpty());
	}

	// Los extremos válidos siguen siendo nombres de la lista, no la reserva.
	TestTrue(TEXT("El primer índice (0) es un nombre de la lista"), TNRoomNames::GetSource(0) != TEXT("Sala sin nombre"));
	TestTrue(TEXT("El último índice (Num() - 1) es un nombre de la lista"), TNRoomNames::GetSource(Count - 1) != TEXT("Sala sin nombre"));

	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Sorteo: en rango y distinto del evitado
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNRoomNamesRandomTest,
	"Tortunabo.Multiplayer.RoomNames.Random",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNRoomNamesRandomTest::RunTest(const FString& Parameters)
{
	const int32 Count = TNRoomNames::Num();
	constexpr int32 Draws = 2000;

	// Sin nada que evitar (valor por defecto y valores que no son índices): siempre dentro de rango.
	const int32 NothingToAvoid[] = { INDEX_NONE, -7, Count, Count + 100 };
	for (const int32 Avoid : NothingToAvoid)
	{
		for (int32 i = 0; i < Draws; ++i)
		{
			const int32 Id = TNRoomNames::Random(Avoid);
			if (Id < 0 || Id >= Count)
			{
				AddError(FString::Printf(TEXT("Random(%d) devolvió %d, fuera de 0..%d"), Avoid, Id, Count - 1));
				break;
			}
		}
	}

	// Con un índice a evitar (extremos y uno del medio): en rango y nunca ese índice.
	const int32 Avoided[] = { 0, Count / 2, Count - 1 };
	for (const int32 Avoid : Avoided)
	{
		for (int32 i = 0; i < Draws; ++i)
		{
			const int32 Id = TNRoomNames::Random(Avoid);
			if (Id < 0 || Id >= Count)
			{
				AddError(FString::Printf(TEXT("Random(%d) devolvió %d, fuera de 0..%d"), Avoid, Id, Count - 1));
				break;
			}
			if (Id == Avoid)
			{
				AddError(FString::Printf(TEXT("Random(%d) devolvió el índice que debía evitar"), Avoid));
				break;
			}
		}
	}

	// El sorteo llega a todo el rango (con 8000 tiradas, no ver nunca el 0 o el último delataría un fallo en el salto).
	bool bSawFirst = false;
	bool bSawLast = false;
	for (int32 i = 0; i < Draws * 4; ++i)
	{
		const int32 Id = TNRoomNames::Random(INDEX_NONE);
		bSawFirst |= (Id == 0);
		bSawLast |= (Id == Count - 1);
	}
	TestTrue(TEXT("Random alcanza el primer índice"), bSawFirst);
	TestTrue(TEXT("Random alcanza el último índice"), bSawLast);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
