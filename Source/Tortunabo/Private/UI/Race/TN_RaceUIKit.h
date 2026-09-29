#pragma once

#include "CoreMinimal.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Core/TN_LocText.h"
#include "Styling/SlateBrush.h"
#include "../HUD/TN_HUDArt.h"
#include "../HUD/TN_HUDStyle.h"

/**
 * Piezas para montar en código las pantallas del modo carrera con el estilo del HUD Tortunavy (las mismas que usa
 * TN_RunHUDWidget.cpp): textos con contorno, imágenes, carteles que se estiran como caja y colocación en lienzos y
 * superposiciones.
 */
namespace TNRaceUI
{
	/** Márgenes de caja (fracción de la textura) de los carteles con arte de TNHUDArt. */
	inline const FMargin CardMargin(0.16f, 0.2f, 0.16f, 0.34f);
	inline const FMargin RibbonMargin(0.14f, 0.f, 0.14f, 0.f);
	inline const FMargin TagMargin(0.2f, 0.f, 0.2f, 0.f);

	template <typename T>
	T* Make(UWidgetTree* Tree, const TCHAR* Name = nullptr)
	{
		return Tree->ConstructWidget<T>(T::StaticClass(), Name ? FName(Name) : NAME_None);
	}

	inline UTextBlock* MakeText(UWidgetTree* Tree, const FText& Content, FName Weight, int32 Size, const FLinearColor& Color, bool bOutline = true)
	{
		UTextBlock* Label = Make<UTextBlock>(Tree);
		Label->SetText(Content);
		TNHUDStyle::StyleText(Label, Weight, Size, Color, bOutline);
		if (!bOutline) { Label->SetShadowColorAndOpacity(FLinearColor::Transparent); }
		return Label;
	}

	inline FSlateBrush TextureBrush(UTexture2D* Tex, const FVector2D& Size)
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(Tex);
		Brush.ImageSize = Size;
		return Brush;
	}

	inline UImage* MakeImage(UWidgetTree* Tree, UTexture2D* Tex, const FVector2D& Size)
	{
		UImage* Img = Make<UImage>(Tree);
		Img->SetBrush(TextureBrush(Tex, Size));
		return Img;
	}

	/** Cambia la textura de una imagen conservando su tamaño. */
	inline void SetImageTexture(UImage* Img, UTexture2D* Tex)
	{
		if (!Img) { return; }
		FSlateBrush Brush = Img->GetBrush();
		if (Brush.GetResourceObject() == Tex) { return; }
		Brush.SetResourceObject(Tex);
		Img->SetBrush(Brush);
	}

	/** Pincel de caja con el arte de TNHUDArt (los bordes con dibujo quedan al tamaño real de la textura). */
	inline FSlateBrush BoxBrush(UTexture2D* Tex, const FMargin& Margin, const FLinearColor& Tint = FLinearColor::White)
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(Tex);
		Brush.DrawAs = ESlateBrushDrawType::Box;
		Brush.Margin = Margin;
		Brush.TintColor = FSlateColor(Tint);
		if (Tex) { Brush.ImageSize = FVector2D(Tex->GetSizeX(), Tex->GetSizeY()); }
		return Brush;
	}

	/** Cartel con arte que se estira como caja, con el contenido centrado y su relleno. */
	inline UBorder* MakeCard(UWidgetTree* Tree, UTexture2D* Tex, const FMargin& Margin, UWidget* Content, const FMargin& Padding)
	{
		UBorder* Card = Make<UBorder>(Tree);
		Card->SetBrush(BoxBrush(Tex, Margin));
		Card->SetPadding(Padding);
		Card->SetHorizontalAlignment(HAlign_Center);
		Card->SetVerticalAlignment(VAlign_Center);
		if (Content) { Card->SetContent(Content); }
		return Card;
	}

	inline USizeBox* MakeSize(UWidgetTree* Tree, UWidget* Content, float W, float H)
	{
		USizeBox* Box = Make<USizeBox>(Tree);
		if (W > 0.f) { Box->SetWidthOverride(W); }
		if (H > 0.f) { Box->SetHeightOverride(H); }
		if (Content) { Box->SetContent(Content); }
		return Box;
	}

	/** Añade a un Overlay con alineación y relleno. */
	inline UOverlaySlot* AddAt(UOverlay* Parent, UWidget* Child, EHorizontalAlignment H, EVerticalAlignment V, const FMargin& Padding = FMargin(0.f))
	{
		UOverlaySlot* OverlaySlot = Parent->AddChildToOverlay(Child);
		if (OverlaySlot)
		{
			OverlaySlot->SetHorizontalAlignment(H);
			OverlaySlot->SetVerticalAlignment(V);
			OverlaySlot->SetPadding(Padding);
		}
		return OverlaySlot;
	}

	/** Coloca en un lienzo con ancla y alineación en el mismo punto (Anchor) y a su tamaño. */
	inline UCanvasPanelSlot* Place(UCanvasPanel* Canvas, UWidget* Child, const FVector2D& Anchor, const FVector2D& Offset)
	{
		UCanvasPanelSlot* CanvasSlot = Canvas->AddChildToCanvas(Child);
		CanvasSlot->SetAnchors(FAnchors(Anchor.X, Anchor.Y));
		CanvasSlot->SetAlignment(Anchor);
		CanvasSlot->SetPosition(Offset);
		CanvasSlot->SetAutoSize(true);
		return CanvasSlot;
	}

	/** Ocupa todo el lienzo. */
	inline UCanvasPanelSlot* Fill(UCanvasPanel* Canvas, UWidget* Child)
	{
		UCanvasPanelSlot* CanvasSlot = Canvas->AddChildToCanvas(Child);
		CanvasSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		CanvasSlot->SetOffsets(FMargin(0.f));
		return CanvasSlot;
	}

	/** Coloca en un lienzo en una posición fija (desde arriba a la izquierda), con tamaño y alineación dados. */
	inline UCanvasPanelSlot* PlaceAt(UCanvasPanel* Canvas, UWidget* Child, const FVector2D& Position, const FVector2D& Size, const FVector2D& Alignment)
	{
		UCanvasPanelSlot* CanvasSlot = Canvas->AddChildToCanvas(Child);
		CanvasSlot->SetAnchors(FAnchors(0.f, 0.f));
		CanvasSlot->SetAlignment(Alignment);
		CanvasSlot->SetPosition(Position);
		CanvasSlot->SetSize(Size);
		return CanvasSlot;
	}

	/** Rebote de entrada: 0 → 1 con un pasito de más (T en 0..1). */
	inline float PopIn(float T)
	{
		const float X = FMath::Clamp(T, 0.f, 1.f);
		const float C1 = 1.9f;
		const float C3 = C1 + 1.f;
		return 1.f + C3 * FMath::Pow(X - 1.f, 3.f) + C1 * FMath::Pow(X - 1.f, 2.f);
	}

	/** Suave al empezar y al acabar (T en 0..1). */
	inline float Smooth(float T)
	{
		const float X = FMath::Clamp(T, 0.f, 1.f);
		return X * X * (3.f - 2.f * X);
	}
}
