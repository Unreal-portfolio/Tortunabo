#pragma once

#include "CoreMinimal.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "Fonts/SlateFontInfo.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"

/**
 * Estilo del HUD de la partida (TN_RunHUDWidget): paneles azul marino translúcidos con esquinas redondeadas y
 * un filo turquesa, textos blancos con contorno, arena para lo que se gana y coral para los avisos. Todo en código,
 * sin texturas: pinceles redondeados de Slate y la fuente por defecto del motor.
 */
namespace TNHUDStyle
{
	inline const FLinearColor Panel(0.012f, 0.045f, 0.08f, 0.74f);
	inline const FLinearColor PanelSoft(0.012f, 0.045f, 0.08f, 0.5f);
	inline const FLinearColor Edge(0.35f, 0.9f, 0.82f, 0.35f);
	inline const FLinearColor Accent(0.1f, 0.86f, 0.72f, 1.f);
	inline const FLinearColor Sand(1.f, 0.84f, 0.5f, 1.f);
	inline const FLinearColor Coral(1.f, 0.36f, 0.28f, 1.f);
	inline const FLinearColor Text(0.97f, 0.98f, 1.f, 1.f);
	inline const FLinearColor TextDim(0.7f, 0.8f, 0.86f, 1.f);

	inline FSlateBrush Rounded(const FLinearColor& Fill, float Radius, const FLinearColor& Outline = FLinearColor::Transparent, float OutlineWidth = 0.f)
	{
		return FSlateRoundedBoxBrush(Fill, Radius, Outline, OutlineWidth);
	}

	/** Fuente del motor (Regular, Bold, Light...) con contorno oscuro para leerse sobre cualquier fondo. */
	inline FSlateFontInfo Font(FName Weight, int32 Size, bool bOutline = true)
	{
		FSlateFontInfo F = FCoreStyle::GetDefaultFontStyle(Weight, Size);
		if (bOutline)
		{
			F.OutlineSettings.OutlineSize = FMath::Max(1, Size / 14);
			F.OutlineSettings.OutlineColor = FLinearColor(0.f, 0.02f, 0.05f, 0.8f);
		}
		return F;
	}

	inline void StyleText(UTextBlock* T, FName Weight, int32 Size, const FLinearColor& Color, bool bOutline = true)
	{
		if (!T) { return; }
		T->SetFont(Font(Weight, Size, bOutline));
		T->SetColorAndOpacity(FSlateColor(Color));
		T->SetShadowOffset(FVector2D(0.f, 2.f));
		T->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.45f));
	}

	inline void StylePanel(UBorder* B, const FLinearColor& Fill, float Radius, const FMargin& Padding, const FLinearColor& Outline = Edge, float OutlineWidth = 1.5f)
	{
		if (!B) { return; }
		B->SetBrush(Rounded(Fill, Radius, Outline, OutlineWidth));
		B->SetPadding(Padding);
	}

	/** Barra redondeada: fondo oscuro y relleno del color que se le ponga (FillColorAndOpacity). */
	inline FProgressBarStyle Bar(float Radius)
	{
		FProgressBarStyle S;
		S.SetBackgroundImage(Rounded(FLinearColor(0.f, 0.02f, 0.04f, 0.65f), Radius, FLinearColor(1.f, 1.f, 1.f, 0.12f), 1.f));
		S.SetFillImage(Rounded(FLinearColor::White, Radius));
		S.SetMarqueeImage(Rounded(FLinearColor::White, Radius));
		return S;
	}
}
