#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"

/**
 * Fuentes de la interfaz de Tortunavy (Docs/Localizacion.md, «Fuentes»).
 *
 * La de serie es la del motor (Roboto: latín, latín extendido y cirílico; su fuente de reserva, Droid Sans Fallback, trae
 * los caracteres CJK). Para que un idioma se vea mejor que con esa reserva, basta con dejar su fuente en Content/Slate/Fonts
 * y apuntarla en su línea de la lista de idiomas (UTN_LanguageSettings: FontRegular, FontBold, FontScript): esta fuente
 * compuesta añade una fuente de reserva por idioma, que el motor elige solo cuando el juego está en ese idioma. Sin el
 * archivo, no cambia nada.
 */
namespace TNHUDFonts
{
	/** La fuente compuesta del juego: la del motor más las fuentes de reserva por idioma que existan en disco. */
	TSharedRef<const FCompositeFont> GetComposite();

	/** Una fuente de la interfaz: Weight es Regular, Bold, Black o Light (como en la fuente del motor). */
	FSlateFontInfo Make(FName Weight, int32 Size);
}
