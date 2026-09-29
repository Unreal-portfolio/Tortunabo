#pragma once

#include "CoreMinimal.h"
#include "Internationalization/Text.h"
#include "Internationalization/Internationalization.h"

/**
 * Ayudas para montar textos de pantalla localizables (Docs/Localizacion.md): números y tiempos con el formato de la cultura
 * activa (separador decimal, orden de los números), siempre como FText y con la frase entera en un solo NSLOCTEXT.
 *
 * Solo funciones en línea (sin estado): se pueden llamar desde cualquier sitio, también durante la construcción de un
 * widget. No hay NSLOCTEXT en inicializadores estáticos de archivo.
 */
namespace TNLocText
{
	/** Entero sin separador de millares («1000», no «1.000»): puestos, cuentas atrás, número de ronda, semillas. */
	inline FText Int(int32 Value)
	{
		return FText::AsNumber(Value, &FNumberFormattingOptions::DefaultNoGrouping());
	}

	/** Número con un decimal fijo y el separador de la cultura («12,3» en español, «12.3» en inglés). */
	inline FText OneDecimal(double Value)
	{
		FNumberFormattingOptions Options;
		Options.UseGrouping = false;
		Options.MinimumFractionalDigits = 1;
		Options.MaximumFractionalDigits = 1;
		return FText::AsNumber(Value, &Options);
	}

	/** Un texto que no se traduce (nombre de jugador, código de sala, símbolo): FText marcado como invariable. */
	inline FText Literal(const FString& Value)
	{
		return FText::AsCultureInvariant(Value);
	}

	/** Nombre de un jugador para pantalla; si no lo hay todavía (vacío), «Tortuga». El nombre es un dato del jugador: no se traduce. */
	inline FText PlayerName(const FString& Name)
	{
		return Name.IsEmpty() ? NSLOCTEXT("TNText", "DefaultPlayerName", "Tortuga") : FText::AsCultureInvariant(Name);
	}

	/** Lista con las conjunciones del idioma: «Ana», «Ana y Leo», «Ana, Leo y Bea». */
	inline FText JoinList(const TArray<FText>& Items)
	{
		if (Items.Num() == 0)
		{
			return FText::GetEmpty();
		}
		if (Items.Num() == 1)
		{
			return Items[0];
		}
		if (Items.Num() == 2)
		{
			return FText::Format(NSLOCTEXT("TNText", "ListPair", "{0} y {1}"), Items[0], Items[1]);
		}
		FText Head = Items[0];
		for (int32 Index = 1; Index < Items.Num() - 1; ++Index)
		{
			Head = FText::Format(NSLOCTEXT("TNText", "ListMiddle", "{0}, {1}"), Head, Items[Index]);
		}
		return FText::Format(NSLOCTEXT("TNText", "ListLast", "{0} y {1}"), Head, Items.Last());
	}

	/** Segundos como «m:ss» (el reloj de la ronda, la cuenta atrás de los avisos). Negativos se quedan en 0. */
	inline FText MinutesSeconds(int32 TotalSeconds)
	{
		const int32 Safe = FMath::Max(0, TotalSeconds);
		FNumberFormattingOptions TwoDigits;
		TwoDigits.UseGrouping = false;
		TwoDigits.MinimumIntegralDigits = 2;
		return FText::Format(NSLOCTEXT("TNTime", "MinutesSeconds", "{0}:{1}"), Int(Safe / 60), FText::AsNumber(Safe % 60, &TwoDigits));
	}
}
