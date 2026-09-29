#include "Settings/TN_LanguageSettings.h"
#include "Components/TextRenderComponent.h"
#include "Core/TN_Log.h"
#include "Engine/World.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/TextLocalizationManager.h"
#include "Kismet/KismetInternationalizationLibrary.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UObject/UObjectIterator.h"

// Con nombre (no anónimo): en la compilación por bloques (unity) los nombres de un espacio anónimo se ven en el resto
// del bloque.
namespace TNLanguageDetail
{
	/** El último idioma aplicado (vacío: todavía ninguno). Solo se toca desde el hilo del juego. */
	FString GActiveLanguage;

	FSimpleMulticastDelegate GAppliedEvent;

	/**
	 * El idioma acaba de cambiar: los carteles 3D (UTextRenderComponent) guardan el texto ya dibujado y no se enteran solos, así
	 * que se les avisa de que se repinten con el texto nuevo; y quien tenga cadenas montadas a partir de textos se rehace.
	 */
	void NotifyApplied()
	{
		for (TObjectIterator<UTextRenderComponent> It; It; ++It)
		{
			UTextRenderComponent* Text = *It;
			const UWorld* World = Text->GetWorld();
			if (Text->IsTemplate() || !World || !World->IsGameWorld() || !Text->IsRegistered())
			{
				continue;
			}
			Text->MarkRenderStateDirty();
		}
		GAppliedEvent.Broadcast();
	}

	FTNLanguageEntry MakeEntry(const TCHAR* Culture, const TCHAR* Name, const TCHAR* FontRegular = TEXT(""), const TCHAR* FontBold = TEXT(""),
		const TCHAR* FontScript = TEXT(""))
	{
		FTNLanguageEntry Entry;
		Entry.Culture = Culture;
		Entry.NativeName = Name;
		Entry.FontRegular = FontRegular;
		Entry.FontBold = FontBold;
		Entry.FontScript = FontScript;
		return Entry;
	}

	/**
	 * Los trece idiomas de serie (los mismos que Config/DefaultGame.ini). Los CJK ya traen el nombre del archivo de su fuente de
	 * reserva (Noto Sans en Content/Slate/Fonts): mientras el archivo no exista, no pasa nada y se ven con la de reserva del motor
	 * (Docs/Localizacion.md, «Fuentes»). El nombre para el jugador sale de KnownName.
	 */
	TArray<FTNLanguageEntry> DefaultLanguages()
	{
		TArray<FTNLanguageEntry> Out;
		Out.Add(MakeEntry(TEXT("es-ES"), TEXT("")));
		Out.Add(MakeEntry(TEXT("en"), TEXT("")));
		Out.Add(MakeEntry(TEXT("fr"), TEXT("")));
		Out.Add(MakeEntry(TEXT("de"), TEXT("")));
		Out.Add(MakeEntry(TEXT("it"), TEXT("")));
		Out.Add(MakeEntry(TEXT("pt-BR"), TEXT("")));
		Out.Add(MakeEntry(TEXT("ru"), TEXT("")));
		Out.Add(MakeEntry(TEXT("pl"), TEXT("")));
		Out.Add(MakeEntry(TEXT("tr"), TEXT("")));
		Out.Add(MakeEntry(TEXT("ja"), TEXT(""), TEXT("NotoSansJP-Regular.ttf"), TEXT("NotoSansJP-Bold.ttf"), TEXT("CJK")));
		Out.Add(MakeEntry(TEXT("ko"), TEXT(""), TEXT("NotoSansKR-Regular.ttf"), TEXT("NotoSansKR-Bold.ttf"), TEXT("CJK")));
		Out.Add(MakeEntry(TEXT("zh-Hans"), TEXT(""), TEXT("NotoSansSC-Regular.ttf"), TEXT("NotoSansSC-Bold.ttf"), TEXT("CJK")));
		Out.Add(MakeEntry(TEXT("zh-Hant"), TEXT(""), TEXT("NotoSansTC-Regular.ttf"), TEXT("NotoSansTC-Bold.ttf"), TEXT("CJK")));
		return Out;
	}

	/**
	 * El nombre de un idioma escrito en su propio idioma, para los que ya se conocen. Así Config/DefaultGame.ini puede llevar solo el
	 * código de cultura (sin letras fuera de ASCII en el ini). Cualquier otro idioma que se añada: su NativeName en la configuración
	 * o, si no lo trae, el que da el motor.
	 */
	const TCHAR* KnownName(const FString& Culture)
	{
		static const TMap<FString, const TCHAR*> Names = {
			{ TEXT("es-es"), TEXT("Español (España)") }, { TEXT("en"), TEXT("English") }, { TEXT("fr"), TEXT("Français") },
			{ TEXT("de"), TEXT("Deutsch") }, { TEXT("it"), TEXT("Italiano") }, { TEXT("pt-br"), TEXT("Português (Brasil)") },
			{ TEXT("ru"), TEXT("Русский") }, { TEXT("pl"), TEXT("Polski") }, { TEXT("tr"), TEXT("Türkçe") },
			{ TEXT("ja"), TEXT("日本語") }, { TEXT("ko"), TEXT("한국어") }, { TEXT("zh-hans"), TEXT("简体中文") }, { TEXT("zh-hant"), TEXT("繁體中文") },
		};
		const TCHAR* const* Found = Names.Find(Culture.ToLower());
		return Found ? *Found : nullptr;
	}
}

UTN_LanguageSettings::UTN_LanguageSettings()
{
	NativeCulture = TEXT("es-ES");
	Languages = TNLanguageDetail::DefaultLanguages();
}

namespace TNLanguage
{
	const TArray<FTNLanguageEntry>& GetLanguages()
	{
		const UTN_LanguageSettings* Settings = GetDefault<UTN_LanguageSettings>();
		if (Settings && Settings->Languages.Num() > 0)
		{
			return Settings->Languages;
		}
		// Configuración vacía: al menos el español, para que nada se quede sin idioma.
		static const TArray<FTNLanguageEntry> Fallback = { TNLanguageDetail::MakeEntry(TEXT("es-ES"), TEXT("")) };
		return Fallback;
	}

	int32 IndexOf(const FString& Culture)
	{
		if (Culture.IsEmpty())
		{
			return INDEX_NONE;
		}
		const TArray<FTNLanguageEntry>& List = GetLanguages();
		for (int32 Index = 0; Index < List.Num(); ++Index)
		{
			if (List[Index].Culture.Equals(Culture, ESearchCase::IgnoreCase))
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	FString GetNativeCulture()
	{
		const UTN_LanguageSettings* Settings = GetDefault<UTN_LanguageSettings>();
		if (Settings && !Settings->NativeCulture.IsEmpty())
		{
			return Settings->NativeCulture;
		}
		return TEXT("es-ES");
	}

	FString GetDisplayName(const FTNLanguageEntry& Entry)
	{
		if (!Entry.NativeName.IsEmpty())
		{
			return Entry.NativeName;
		}
		if (const TCHAR* Known = TNLanguageDetail::KnownName(Entry.Culture))
		{
			return Known;
		}
		// Ni en la configuración ni entre los conocidos: el que da el motor para esa cultura, con la primera letra en mayúscula.
		const FCulturePtr Culture = FInternationalization::Get().GetCulture(Entry.Culture);
		FString Name = Culture.IsValid() ? Culture->GetNativeName() : Entry.Culture;
		if (Name.Len() > 0)
		{
			Name[0] = FChar::ToUpper(Name[0]);
		}
		return Name;
	}

	FString FindSystemLanguage()
	{
		const TArray<FTNLanguageEntry>& List = GetLanguages();
		FInternationalization& I18N = FInternationalization::Get();
		const FCultureRef System = I18N.GetDefaultLanguage();

		// 1) El más específico de los que el sistema admite que esté en la lista (es-MX → es-419 → es, zh-Hant-TW → zh-Hant...).
		for (const FString& Name : I18N.GetPrioritizedCultureNames(System->GetName()))
		{
			const int32 Found = IndexOf(Name);
			if (Found != INDEX_NONE)
			{
				return List[Found].Culture;
			}
		}

		// 2) El chino sin escritura: por región (Taiwán, Hong Kong y Macao usan el tradicional).
		if (System->GetTwoLetterISOLanguageName() == TEXT("zh"))
		{
			const FString Region = System->GetRegion();
			const bool bTraditional = System->GetScript() == TEXT("Hant") || Region == TEXT("TW") || Region == TEXT("HK") || Region == TEXT("MO");
			const int32 Found = IndexOf(bTraditional ? TEXT("zh-Hant") : TEXT("zh-Hans"));
			if (Found != INDEX_NONE)
			{
				return List[Found].Culture;
			}
		}

		// 3) El mismo idioma con otra región (es-AR → es-ES, pt-PT → pt-BR, en-GB → en): el primero de la lista.
		const FString Language = System->GetTwoLetterISOLanguageName();
		for (const FTNLanguageEntry& Entry : List)
		{
			const FCulturePtr Candidate = I18N.GetCulture(Entry.Culture);
			const bool bSameLanguage = Candidate.IsValid() ? Candidate->GetTwoLetterISOLanguageName() == Language : Entry.Culture.StartsWith(Language, ESearchCase::IgnoreCase);
			if (bSameLanguage)
			{
				return Entry.Culture;
			}
		}

		// 4) Ninguno: el español, que es en lo que está escrito todo.
		const int32 Native = IndexOf(GetNativeCulture());
		return Native != INDEX_NONE ? List[Native].Culture : GetNativeCulture();
	}

	FString ResolveLanguage(const FString& Saved)
	{
		const int32 Found = IndexOf(Saved);
		if (Found != INDEX_NONE)
		{
			return GetLanguages()[Found].Culture;
		}
		return FindSystemLanguage();
	}

	bool Apply(const FString& InCulture)
	{
		const int32 Found = IndexOf(InCulture);
		const FString Culture = Found != INDEX_NONE ? GetLanguages()[Found].Culture : GetNativeCulture();

		// Un idioma forzado por la línea de comandos (-culture=ja, o las de prueba del motor: -culture=LEET pone todos los textos
		// localizables en «leet» y -culture=keys enseña sus claves) manda sobre el ajuste: sirve para probar.
		FString Forced;
		if (FParse::Value(FCommandLine::Get(), TEXT("CULTURE="), Forced) || FParse::Value(FCommandLine::Get(), TEXT("LANGUAGE="), Forced))
		{
			TNLanguageDetail::GActiveLanguage = FInternationalization::Get().GetCurrentLanguage()->GetName();
			UE_LOG(LogTortunabo, Log, TEXT("[Idioma] Forzado por la línea de comandos (%s): no se aplica el ajuste (%s)."), *Forced, *Culture);
			return true;
		}

#if WITH_EDITOR
		if (GIsEditor)
		{
			// El idioma del editor (sus menús) no se toca: solo los textos del juego, como la previsualización del idioma del juego
			// de las preferencias del editor. Al acabar PIE, el propio editor la vuelve a apagar.
			FTextLocalizationManager::Get().EnableGameLocalizationPreview(Culture);
			const bool bChangedInEditor = TNLanguageDetail::GActiveLanguage != Culture;
			TNLanguageDetail::GActiveLanguage = Culture;
			if (bChangedInEditor)
			{
				TNLanguageDetail::NotifyApplied();
			}
			UE_LOG(LogTortunabo, Log, TEXT("[Idioma] Textos del juego en %s (previsualización del editor)."), *Culture);
			return true;
		}
#endif

		FInternationalization& I18N = FInternationalization::Get();
		if (!I18N.GetCulture(Culture).IsValid())
		{
			// Sin datos de esa cultura en la compilación (falta en «Languages to Package» o en el preajuste de internacionalización).
			UE_LOG(LogTortunabo, Warning, TEXT("[Idioma] La cultura %s no está disponible en esta compilación: se queda %s."), *Culture, *I18N.GetCurrentLanguage()->GetName());
			return false;
		}
		// Sin guardarla en la configuración del motor: el idioma que manda es el de los ajustes del juego.
		const bool bAlreadyThere = I18N.GetCurrentLanguage()->GetName() == Culture;
		const bool bApplied = bAlreadyThere || UKismetInternationalizationLibrary::SetCurrentCulture(Culture, false);
		if (bApplied)
		{
			TNLanguageDetail::GActiveLanguage = Culture;
			UE_LOG(LogTortunabo, Log, TEXT("[Idioma] Juego en %s."), *Culture);
			if (!bAlreadyThere)
			{
				TNLanguageDetail::NotifyApplied();
			}
		}
		else
		{
			UE_LOG(LogTortunabo, Warning, TEXT("[Idioma] No se pudo poner la cultura %s."), *Culture);
		}
		return bApplied;
	}

	FSimpleMulticastDelegate& OnApplied()
	{
		return TNLanguageDetail::GAppliedEvent;
	}

	FString GetActive()
	{
		return TNLanguageDetail::GActiveLanguage.IsEmpty() ? ResolveLanguage(FString()) : TNLanguageDetail::GActiveLanguage;
	}

	bool IsActiveLanguage(const TCHAR* TwoLetterCode)
	{
		const FString Active = GetActive();
		const int32 Length = FCString::Strlen(TwoLetterCode);
		return Length > 0 && Active.StartsWith(TwoLetterCode, ESearchCase::IgnoreCase) && (Active.Len() == Length || Active[Length] == TEXT('-'));
	}
}
