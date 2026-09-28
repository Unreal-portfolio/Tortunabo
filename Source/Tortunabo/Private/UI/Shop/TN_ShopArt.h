#pragma once

#include "CoreMinimal.h"
#include "../HUD/TN_HUDArt.h"

/**
 * Arte de la tienda y del probador, pintado en código con el pintor del HUD (TNHUDArt): cartas del catálogo, botones
 * de píldora y flechas. Todo pequeño: Slate mide los márgenes de las cajas con el tamaño real de la textura.
 */
namespace TNShopArt
{
	using namespace TNHUDArt;

	/** Carta del catálogo: crema con filo azul marino; la elegida, con filo dorado grueso. Márgenes 0.3. */
	inline UTexture2D* ItemCard(bool bSelected)
	{
		return Cached(bSelected ? TEXT("ShopCardOn") : TEXT("ShopCardOff"), [bSelected]
		{
			FPainter P(96, 112);
			auto Card = [](float x, float y) { return Box(x, y, 48.f, 54.f, 42.f, 48.f, 14.f); };
			P.Sticker(Card, bSelected ? 4.5f : 3.f, 4.f, FVector2f(1.f, 3.f));
			P.Fill(Card, bSelected ? Gold : Navy);
			P.Layer([&](float x, float y) { return Card(x, y) + (bSelected ? 4.f : 2.5f); },
				[](float x, float y) { return Mix(Cream, SandLight, (y - 10.f) / 96.f); });
			return P.ToTexture(bSelected ? TEXT("TN_Shop_CardOn") : TEXT("TN_Shop_CardOff"));
		});
	}

	/** Botón de píldora con degradado, brillo arriba y filo azul marino. Márgenes (0.4, 0, 0.4, 0). */
	inline UTexture2D* Pill(uint32 TopHex, uint32 BottomHex)
	{
		const FName Key(*FString::Printf(TEXT("ShopPill_%06X_%06X"), TopHex, BottomHex));
		return Cached(Key, [Key, TopHex, BottomHex]
		{
			FPainter P(112, 52);
			auto Shape = [](float x, float y) { return Box(x, y, 56.f, 25.f, 50.f, 19.f, 19.f); };
			P.Sticker(Shape, 3.f, 3.f, FVector2f(1.f, 3.f));
			P.Fill(Shape, NavyDeep);
			const FLinearColor Top = Hex(TopHex), Bottom = Hex(BottomHex);
			P.Layer([&](float x, float y) { return Shape(x, y) + 2.5f; }, [Top, Bottom](float x, float y) { return Mix(Top, Bottom, (y - 8.f) / 34.f); });
			P.Fill([&](float x, float y) { return FMath::Max(FMath::Abs(y - 13.f) - 2.f, Shape(x, y) + 9.f); }, Hex(0xFFFFFF, 0.32f));
			return P.ToTexture(*FString::Printf(TEXT("TN_Shop_%s"), *Key.ToString()));
		});
	}

	/** Flecha de cambiar (probador): triángulo crema con filo azul marino. */
	inline UTexture2D* Arrow(bool bRight)
	{
		return Cached(bRight ? TEXT("ShopArrowR") : TEXT("ShopArrowL"), [bRight]
		{
			FPainter P(56, 56);
			const float S = bRight ? 1.f : -1.f;
			const TArray<FVector2f> Tri = { { 28.f - 11.f * S, 12.f }, { 28.f + 15.f * S, 28.f }, { 28.f - 11.f * S, 44.f } };
			auto Shape = [&](float x, float y) { return Polygon(x, y, Tri) - 3.f; };
			P.Sticker(Shape, 3.f, 3.f, FVector2f(1.f, 2.f));
			P.Fill(Shape, Navy);
			P.Layer([&](float x, float y) { return Shape(x, y) + 3.f; }, [](float x, float y) { return Mix(Gold, Hex(0xF2A93B), (y - 14.f) / 28.f); });
			return P.ToTexture(bRight ? TEXT("TN_Shop_ArrowR") : TEXT("TN_Shop_ArrowL"));
		});
	}

	/** Panel de arena para las filas del probador. Márgenes 0.3. */
	inline UTexture2D* SandPanel(bool bFocused)
	{
		return Cached(bFocused ? TEXT("ShopSandOn") : TEXT("ShopSandOff"), [bFocused]
		{
			FPainter P(96, 80);
			auto Shape = [](float x, float y) { return Box(x, y, 48.f, 38.f, 42.f, 32.f, 16.f); };
			P.Sticker(Shape, bFocused ? 4.f : 2.5f, 4.f, FVector2f(1.f, 3.f));
			P.Fill(Shape, bFocused ? Gold : Hex(0xC89B5E));
			P.Layer([&](float x, float y) { return Shape(x, y) + (bFocused ? 4.f : 2.5f); }, [](float x, float y) { return Mix(SandLight, SandC, (y - 8.f) / 64.f); });
			return P.ToTexture(bFocused ? TEXT("TN_Shop_SandOn") : TEXT("TN_Shop_SandOff"));
		});
	}
}
