// Nombres de sala (TN_RoomNames.h): la tabla fija que viaja por índice en la sesión. Se comprueba lo que la interfaz y
// la red necesitan de ella: cantidad mínima, nada vacío, cabe en pantalla (28 caracteres), sin repetidos en ningún
// idioma, nombre de reserva fuera de rango y sorteo dentro de rango que respeta «otro nombre». Correr desde Session
// Frontend (categoría "Tortunabo.Multiplayer") o headless:
//   UnrealEditor-Cmd <uproject> -ExecCmds="Automation RunTests Tortunabo.Multiplayer.RoomNames; Quit" -nullrhi -unattended

#include "Misc/AutomationTest.h"
#include "Multiplayer/TN_RoomNames.h"

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
}

// ─────────────────────────────────────────────────────────────────────────────
// La tabla: cantidad, contenido, largo y repetidos
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTNRoomNamesTableTest,
	"Tortunabo.Multiplayer.RoomNames.Table",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTNRoomNamesTableTest::RunTest(const FString& Parameters)
{
	const int32 Count = TNRoomNames::Num();
	TestTrue(FString::Printf(TEXT("Hay al menos %d nombres (hay %d)"), TNRoomNamesTest::MinNames, Count),
		Count >= TNRoomNamesTest::MinNames);

	// Un conjunto por idioma; se compara sin distinguir mayúsculas para que «Sea You Later» y «sea you later» cuenten como uno.
	TSet<FString> SeenSpanish;
	TSet<FString> SeenEnglish;

	for (int32 Id = 0; Id < Count; ++Id)
	{
		for (const bool bSpanish : { true, false })
		{
			const TCHAR* Lang = bSpanish ? TEXT("ES") : TEXT("EN");
			const FString Name = TNRoomNames::GetIn(Id, bSpanish);

			TestTrue(FString::Printf(TEXT("%s #%d no está vacío"), Lang, Id), !Name.IsEmpty());
			TestTrue(FString::Printf(TEXT("%s #%d sin espacios sobrantes en los bordes"), Lang, Id),
				Name == Name.TrimStartAndEnd());
			TestTrue(FString::Printf(TEXT("%s #%d cabe en %d caracteres (%d): %s"), Lang, Id, TNRoomNamesTest::MaxLength, Name.Len(), *Name),
				Name.Len() <= TNRoomNamesTest::MaxLength);
			TestFalse(FString::Printf(TEXT("%s #%d sin comillas ni caracteres raros: %s"), Lang, Id, *Name),
				TNRoomNamesTest::HasForbiddenChar(Name));

			bool bAlreadyInSet = false;
			(bSpanish ? SeenSpanish : SeenEnglish).Add(Name.ToLower(), &bAlreadyInSet);
			TestFalse(FString::Printf(TEXT("%s #%d repetido: %s"), Lang, Id, *Name), bAlreadyInSet);
		}

		// La versión de FText sigue al idioma del juego y coincide con la de GetIn.
		TestTrue(FString::Printf(TEXT("Get(%d) es el nombre en el idioma del juego"), Id),
			TNRoomNames::Get(Id).ToString() == TNRoomNames::GetIn(Id, TNRoomNames::IsSpanish()));
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
		TestTrue(FString::Printf(TEXT("Índice %d fuera de rango → reserva en español"), Id),
			TNRoomNames::GetIn(Id, true) == TEXT("Sala sin nombre"));
		TestTrue(FString::Printf(TEXT("Índice %d fuera de rango → reserva en inglés"), Id),
			TNRoomNames::GetIn(Id, false) == TEXT("Nameless Nest"));
		TestFalse(FString::Printf(TEXT("Get(%d) fuera de rango nunca devuelve vacío"), Id),
			TNRoomNames::Get(Id).IsEmpty());
	}

	// Los extremos válidos siguen siendo nombres de la lista, no la reserva.
	TestTrue(TEXT("El primer índice (0) es un nombre de la lista"),
		TNRoomNames::GetIn(0, true) != TEXT("Sala sin nombre") && TNRoomNames::GetIn(0, false) != TEXT("Nameless Nest"));
	TestTrue(TEXT("El último índice (Num() - 1) es un nombre de la lista"),
		TNRoomNames::GetIn(Count - 1, true) != TEXT("Sala sin nombre") && TNRoomNames::GetIn(Count - 1, false) != TEXT("Nameless Nest"));

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
