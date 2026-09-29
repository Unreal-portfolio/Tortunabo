#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "TN_LanguageSettings.generated.h"

/**
 * Un idioma del juego (Docs/Localizacion.md): la cultura de la localización, su nombre escrito en su propio idioma (lo que
 * se ve en la fila «Idioma» del menú de pausa) y, si su escritura no la cubre la fuente de serie, una fuente de reserva.
 */
USTRUCT()
struct FTNLanguageEntry
{
	GENERATED_BODY()

	/** Código de cultura de la localización («es-ES», «en», «pt-BR», «zh-Hans»...); el mismo que el de Content/Localization/Game. */
	UPROPERTY(Config, EditAnywhere, Category = "Idioma")
	FString Culture;

	/**
	 * Nombre para el jugador, escrito en su propio idioma («Deutsch», «日本語»...). Vacío: el de los trece idiomas de serie
	 * (TNLanguageDetail::KnownName) o, si no es uno de ellos, el que da el motor para esa cultura. Solo hace falta ponerlo en el ini
	 * para cambiarlo (con letras que no son ASCII, el ini se guarda como UTF-8).
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Idioma")
	FString NativeName;

	/**
	 * Fuente de reserva para este idioma: nombre de un archivo .ttf u .otf dentro de Content/Slate/Fonts (vacío: solo las del
	 * motor). Si el archivo no existe se ignora sin más. Se usa para los caracteres que la fuente de serie no trae, con
	 * preferencia sobre la de reserva del motor. FontBold: la negrita (vacío: la misma).
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Fuente de reserva")
	FString FontRegular;

	UPROPERTY(Config, EditAnywhere, Category = "Fuente de reserva")
	FString FontBold;

	/**
	 * Qué caracteres cubre esa fuente: «CJK» (kanji, kana, hanzi, hangul y signos), «Cyrillic» o «Latin» (latín extendido).
	 * Vacío: «CJK».
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Fuente de reserva")
	FString FontScript;
};

/**
 * @brief Lista de idiomas del juego, sin tocar código: se edita en Config/DefaultGame.ini
 * ([/Script/Tortunabo.TN_LanguageSettings], o Ajustes del proyecto > Tortunavy > Idiomas). Añadir un idioma es una línea
 * más aquí, su cultura en «Languages to Package» (CulturesToStage) y su traducción (Docs/Localizacion.md).
 *
 * El primero de la lista y NativeCulture es el idioma en el que se escriben los textos del código (español de España).
 * Si la configuración no trae ninguno, el constructor pone los trece de serie.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Tortunavy - Idiomas"))
class TORTUNABO_API UTN_LanguageSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UTN_LanguageSettings();

	/** Cultura de los textos origen (los NSLOCTEXT del código): la nativa del objetivo «Game» de localización. */
	UPROPERTY(Config, EditAnywhere, Category = "Idiomas")
	FString NativeCulture;

	/** Idiomas que se pueden elegir, en el orden de la fila «Idioma». */
	UPROPERTY(Config, EditAnywhere, Category = "Idiomas")
	TArray<FTNLanguageEntry> Languages;
};

/**
 * Idioma del juego: qué idiomas hay, cuál toca por defecto y cómo se cambia en caliente. Lo usa UTN_GameSettingsSubsystem
 * (ajuste «Idioma»), TNRoomNames (nombres de sala) y las fuentes de la interfaz (TNHUDFonts).
 */
namespace TNLanguage
{
	/** Los idiomas de la configuración (nunca vacía: si no hay ninguno, el español). */
	TORTUNABO_API const TArray<FTNLanguageEntry>& GetLanguages();

	/** Posición en la lista de una cultura («es-ES», «pt-BR»...; sin distinguir mayúsculas) o INDEX_NONE. */
	TORTUNABO_API int32 IndexOf(const FString& Culture);

	/** La cultura de los textos origen del código («es-ES»). */
	TORTUNABO_API FString GetNativeCulture();

	/** Nombre para el jugador de un idioma de la lista, escrito en su idioma. */
	TORTUNABO_API FString GetDisplayName(const FTNLanguageEntry& Entry);

	/**
	 * El idioma del sistema, en términos de la lista: el más específico que esté (zh-TW → «zh-Hant», es-MX → «es-ES»,
	 * pt-PT → «pt-BR»...) o, si no hay ninguno, el nativo (español).
	 */
	TORTUNABO_API FString FindSystemLanguage();

	/** El idioma que toca: el guardado si está en la lista y, si no (o vacío), el del sistema. */
	TORTUNABO_API FString ResolveLanguage(const FString& Saved);

	/**
	 * Pone el idioma en el juego (una cultura de la lista): en el juego, la cultura del motor (SetCurrentCulture, sin
	 * guardarla en la configuración del motor: el ajuste propio manda); en el editor, solo los textos del juego (previsualización
	 * del idioma del juego, para no tocar el idioma del editor). true si se ha aplicado.
	 */
	TORTUNABO_API bool Apply(const FString& Culture);

	/** El último idioma aplicado; antes de aplicar ninguno, el que tocaría. */
	TORTUNABO_API FString GetActive();

	/** true si el idioma del juego es ese idioma (dos letras: «es», «en»...). */
	TORTUNABO_API bool IsActiveLanguage(const TCHAR* TwoLetterCode);
}
