#include "STN_EggLoadingScreen.h"

#include "../HUD/TN_HUDArt.h"
#include "Engine/Texture2D.h"
#include "Fonts/SlateFontInfo.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

// ─────────────────────────────────────────────────────────────────────────────
// Arte (hilo de juego, una vez)
// ─────────────────────────────────────────────────────────────────────────────

namespace TNEggLoadingDetail
{
	constexpr float EggCx = 180.f;
	constexpr float EggCy = 275.f;
	constexpr float EggTexW = 360.f;
	constexpr float EggTexH = 480.f;

	/** Huevo: elipse más alta y algo más estrecha por arriba que por abajo (px del lienzo de 360 x 480). */
	float EggSdf(float x, float y)
	{
		const float Ry = y < EggCy ? 235.f : 190.f;
		const float Narrow = 1.f + 0.2f * FMath::Max(0.f, (EggCy - y) / Ry);
		return TNHUDArt::Ellipse(EggCx + (x - EggCx) * Narrow, y, EggCx, EggCy, 148.f, Ry);
	}

	/** Zigzag por el que se parte el huevo (y en función de x). */
	float ZigY(float x)
	{
		constexpr float Period = 60.f;
		const float Phase = FMath::Fmod(x + 100.f * Period, Period) / Period;
		return 255.f + 20.f * (1.f - 4.f * FMath::Abs(Phase - 0.5f));
	}

	FLinearColor EggColor(float x, float y)
	{
		const float Nx = (x - EggCx) / 148.f;
		const float Ny = (y - EggCy) / 235.f;
		const float Shade = FMath::Clamp(0.5f * Nx + 0.6f * Ny + 0.25f, 0.f, 1.f);
		return TNHUDArt::Mix(TNHUDArt::Cream, TNHUDArt::Hex(0xE9D5B0), 0.8f * Shade);
	}

	UTexture2D* MakeEggHalf(bool bTop, const TCHAR* Name)
	{
		TNHUDArt::FPainter P(static_cast<int32>(EggTexW), static_cast<int32>(EggTexH));
		auto HalfSdf = [bTop](float x, float y) { return bTop ? (y - ZigY(x)) : (ZigY(x) - y); };
		auto OuterSdf = [&](float x, float y) { return FMath::Max(EggSdf(x, y) - 6.f, HalfSdf(x, y)); };
		// El cuerpo deja 3 px de tinta en el borde del zigzag: cerrado se ve la costura entre las dos mitades.
		auto BodySdf = [&](float x, float y) { return FMath::Max(EggSdf(x, y), HalfSdf(x, y) + 3.f); };
		P.Fill(OuterSdf, TNHUDArt::Ink);
		P.Layer(BodySdf, [](float x, float y) { return EggColor(x, y); });

		// Motas turquesa, coral y doradas (huevo de dibujo).
		struct FSpot { float X; float Y; float R; uint32 Color; };
		static const FSpot Spots[] = {
			{ 120.f, 150.f, 16.f, 0x7FD8C8 }, { 236.f, 118.f, 11.f, 0xFFB4A2 }, { 262.f, 214.f, 19.f, 0x7FD8C8 },
			{ 98.f, 262.f, 12.f, 0xFFB4A2 }, { 176.f, 330.f, 22.f, 0xFFB4A2 }, { 252.f, 356.f, 13.f, 0x7FD8C8 },
			{ 108.f, 380.f, 15.f, 0x7FD8C8 }, { 200.f, 196.f, 9.f, 0xFFCB3D }, { 146.f, 430.f, 9.f, 0xFFCB3D },
			{ 290.f, 300.f, 8.f, 0xFFCB3D }, { 160.f, 80.f, 8.f, 0xFFB4A2 } };
		for (const FSpot& Spot : Spots)
		{
			P.Fill([&](float x, float y) { return FMath::Max(TNHUDArt::Circle(x, y, Spot.X, Spot.Y, Spot.R), BodySdf(x, y) + 2.f); },
				TNHUDArt::Hex(Spot.Color, 0.85f));
		}
		// Brillo arriba a la izquierda.
		P.Fill([&](float x, float y) { return FMath::Max(TNHUDArt::Ellipse(x, y, 116.f, 150.f, 22.f, 46.f), BodySdf(x, y)); },
			FLinearColor(1.f, 1.f, 1.f, 0.65f));
		return P.ToTexture(Name);
	}

	UTexture2D* MakeTurtle(int32 ColorIndex, int32 Frame, const TCHAR* Name)
	{
		static const uint32 ShellColors[TNEggLoadingArt::NumTurtles] = { 0x2EC4B6, 0xFF6A52, 0xFFCB3D, 0x9B5DE5 };
		const FLinearColor ShellColor = TNHUDArt::Hex(ShellColors[FMath::Clamp(ColorIndex, 0, TNEggLoadingArt::NumTurtles - 1)]);
		const FLinearColor ShellDark(ShellColor.R * 0.7f, ShellColor.G * 0.7f, ShellColor.B * 0.7f, 1.f);
		const FLinearColor Skin = TNHUDArt::Hex(0x9CD66C);
		const FLinearColor SkinDark = TNHUDArt::Hex(0x6DAF45);

		TNHUDArt::FPainter P(160, 110);
		const float Swing = Frame == 0 ? 9.f : -8.f;
		auto LegSdf = [](float x, float y, float Hip, float Reach) { return TNHUDArt::Segment(x, y, Hip, 68.f, Hip + Reach, 94.f, 8.f); };
		auto FarLegs = [&](float x, float y) { return FMath::Min(LegSdf(x, y, 96.f, -Swing), LegSdf(x, y, 52.f, Swing)); };
		auto NearLegs = [&](float x, float y) { return FMath::Min(LegSdf(x, y, 108.f, Swing), LegSdf(x, y, 40.f, -Swing)); };
		auto ShellSdf = [](float x, float y) { return FMath::Max(TNHUDArt::Ellipse(x, y, 72.f, 66.f, 50.f, 42.f), y - 70.f); };
		auto RimSdf = [](float x, float y) { return TNHUDArt::Box(x, y, 72.f, 71.f, 53.f, 6.f, 5.f); };
		auto HeadSdf = [](float x, float y)
		{
			return FMath::Min(TNHUDArt::Circle(x, y, 130.f, 56.f, 16.f), TNHUDArt::Segment(x, y, 106.f, 66.f, 122.f, 60.f, 9.f));
		};
		const TArray<FVector2f> TailPoints = { FVector2f(26.f, 66.f), FVector2f(8.f, 74.f), FVector2f(28.f, 76.f) };
		auto TailSdf = [&](float x, float y) { return TNHUDArt::Polygon(x, y, TailPoints); };
		auto AllSdf = [&](float x, float y)
		{
			return FMath::Min(FMath::Min(FMath::Min(FarLegs(x, y), NearLegs(x, y)), FMath::Min(ShellSdf(x, y), RimSdf(x, y))),
				FMath::Min(HeadSdf(x, y), TailSdf(x, y)));
		};

		// Sombra, contorno y piezas de atrás hacia delante.
		P.Fill([](float x, float y) { return TNHUDArt::Ellipse(x, y, 76.f, 101.f, 58.f, 6.f); }, TNHUDArt::Hex(0x0A1C38, 0.35f));
		P.Fill([&](float x, float y) { return AllSdf(x, y) - 3.5f; }, TNHUDArt::Ink);
		P.Fill(FarLegs, SkinDark);
		P.Fill(TailSdf, Skin);
		P.Fill(NearLegs, Skin);
		P.Fill(HeadSdf, Skin);
		P.Fill(ShellSdf, ShellColor);
		// Placas: anillo central y dos radios hacia el borde; borde de la concha y brillo.
		P.Fill([&](float x, float y) { return FMath::Max(FMath::Abs(TNHUDArt::Circle(x, y, 72.f, 60.f, 22.f)) - 2.2f, ShellSdf(x, y) + 3.f); }, ShellDark);
		P.Fill([&](float x, float y)
		{
			return FMath::Max(FMath::Min(TNHUDArt::Segment(x, y, 52.f, 70.f, 58.f, 52.f, 2.2f), TNHUDArt::Segment(x, y, 92.f, 70.f, 86.f, 52.f, 2.2f)),
				ShellSdf(x, y) + 3.f);
		}, ShellDark);
		P.Fill(RimSdf, ShellDark);
		P.Fill([&](float x, float y) { return FMath::Max(TNHUDArt::Ellipse(x, y, 56.f, 42.f, 13.f, 7.f), ShellSdf(x, y) + 4.f); }, FLinearColor(1.f, 1.f, 1.f, 0.45f));
		// Ojo con brillo, moflete y sonrisa.
		P.Fill([](float x, float y) { return TNHUDArt::Circle(x, y, 134.f, 52.f, 5.5f); }, FLinearColor::White);
		P.Fill([](float x, float y) { return TNHUDArt::Circle(x, y, 136.f, 52.5f, 3.2f); }, TNHUDArt::Ink);
		P.Fill([](float x, float y) { return TNHUDArt::Circle(x, y, 137.f, 51.f, 1.2f); }, FLinearColor::White);
		P.Fill([](float x, float y) { return TNHUDArt::Circle(x, y, 127.f, 62.f, 3.4f); }, TNHUDArt::Hex(0xFF9A80, 0.7f));
		P.Fill([](float x, float y) { return TNHUDArt::Arc(x, y, 134.f, 59.f, 5.f, 0.35f, 1.9f, 1.2f); }, TNHUDArt::Ink);
		return P.ToTexture(Name);
	}

	UTexture2D* MakeSky()
	{
		TNHUDArt::FPainter P(8, 256);
		P.Layer([](float, float) { return -1.f; }, [](float, float y)
		{
			const float T = y / 256.f;
			return T < 0.7f ? TNHUDArt::Mix(TNHUDArt::Hex(0x071428), TNHUDArt::Hex(0x12305A), T / 0.7f)
				: TNHUDArt::Mix(TNHUDArt::Hex(0x12305A), TNHUDArt::Hex(0x1B5C7E), (T - 0.7f) / 0.3f);
		});
		return P.ToTexture(TEXT("TN_EggLoading_Sky"));
	}

	UTexture2D* MakeSand()
	{
		TNHUDArt::FPainter P(512, 96);
		auto EdgeY = [](float x) { return 16.f + 5.f * FMath::Sin(x / 512.f * 2.f * PI * 4.f) + 2.f * FMath::Sin(x / 512.f * 2.f * PI * 9.f + 1.f); };
		P.Layer([&](float x, float y) { return EdgeY(x) - y; }, [](float, float y) { return TNHUDArt::Mix(TNHUDArt::SandC, TNHUDArt::Hex(0xE3BE82), y / 96.f); });
		P.Fill([&](float x, float y) { return FMath::Max(FMath::Abs(y - EdgeY(x) - 7.f) - 2.5f, EdgeY(x) - y); }, TNHUDArt::Hex(0xC89B5E, 0.45f));
		for (int32 i = 0; i < 70; ++i)
		{
			const float Sx = FMath::Frac(i * 0.618034f) * 512.f;
			const float Sy = 30.f + FMath::Frac(i * 0.371f) * 60.f;
			P.Fill([Sx, Sy](float x, float y) { return TNHUDArt::Circle(x, y, Sx, Sy, 1.6f); }, TNHUDArt::Hex(0xC89B5E, 0.6f));
		}
		return P.ToTexture(TEXT("TN_EggLoading_Sand"));
	}

	UTexture2D* MakeDot()
	{
		TNHUDArt::FPainter P(16, 16);
		P.Layer([](float x, float y) { return TNHUDArt::Circle(x, y, 8.f, 8.f, 7.f); }, [](float x, float y)
		{
			const float D = FMath::Clamp(FMath::Sqrt(FMath::Square(x - 8.f) + FMath::Square(y - 8.f)) / 7.f, 0.f, 1.f);
			return FLinearColor(1.f, 1.f, 1.f, FMath::Square(1.f - D));
		});
		P.Fill([](float x, float y) { return TNHUDArt::Circle(x, y, 8.f, 8.f, 2.2f); }, FLinearColor::White);
		return P.ToTexture(TEXT("TN_EggLoading_Dot"));
	}

	UTexture2D* MakeShard()
	{
		TNHUDArt::FPainter P(48, 48);
		const TArray<FVector2f> Points = { FVector2f(7.f, 40.f), FVector2f(24.f, 7.f), FVector2f(41.f, 33.f), FVector2f(28.f, 43.f) };
		P.Fill([&](float x, float y) { return TNHUDArt::Polygon(x, y, Points) - 3.f; }, TNHUDArt::Ink);
		P.Fill([&](float x, float y) { return TNHUDArt::Polygon(x, y, Points); }, TNHUDArt::Cream);
		return P.ToTexture(TEXT("TN_EggLoading_Shard"));
	}

	UTexture2D* MakeBurst()
	{
		TNHUDArt::FPainter P(256, 256);
		const TArray<FVector2f> Outer = TNHUDArt::StarPoints(128.f, 128.f, 120.f, 0.72f, 12);
		const TArray<FVector2f> Inner = TNHUDArt::StarPoints(128.f, 128.f, 90.f, 0.7f, 12);
		P.Fill([&](float x, float y) { return TNHUDArt::Polygon(x, y, Outer) - 5.f; }, TNHUDArt::Ink);
		P.Fill([&](float x, float y) { return TNHUDArt::Polygon(x, y, Outer); }, TNHUDArt::Gold);
		P.Fill([&](float x, float y) { return TNHUDArt::Polygon(x, y, Inner); }, TNHUDArt::CoralLight);
		return P.ToTexture(TEXT("TN_EggLoading_Burst"));
	}

	void SetBrush(FSlateBrush& Brush, UTexture2D* Texture)
	{
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.Tiling = ESlateBrushTileType::NoTile;
		if (Texture)
		{
			Brush.SetResourceObject(Texture);
			Brush.ImageSize = FVector2D(static_cast<double>(Texture->GetSizeX()), static_cast<double>(Texture->GetSizeY()));
		}
	}

	/** Grietas que salen de la costura al romperse (px del lienzo del huevo). */
	struct FCrack
	{
		float StartX;
		FVector2f Mid;
		FVector2f End;
	};
	const FCrack Cracks[] = {
		{ 120.f, FVector2f(104.f, 205.f), FVector2f(118.f, 172.f) },
		{ 212.f, FVector2f(228.f, 302.f), FVector2f(214.f, 330.f) },
		{ 272.f, FVector2f(290.f, 216.f), FVector2f(278.f, 188.f) },
		{ 82.f, FVector2f(70.f, 300.f), FVector2f(86.f, 322.f) },
		{ 168.f, FVector2f(160.f, 212.f), FVector2f(176.f, 186.f) },
	};
}

namespace TNEggLoadingArt
{
	UTexture2D* EggTop() { return TNHUDArt::Cached(TEXT("EggLoading_EggTop"), [] { return TNEggLoadingDetail::MakeEggHalf(true, TEXT("TN_EggLoading_EggTop")); }); }
	UTexture2D* EggBottom() { return TNHUDArt::Cached(TEXT("EggLoading_EggBottom"), [] { return TNEggLoadingDetail::MakeEggHalf(false, TEXT("TN_EggLoading_EggBottom")); }); }

	UTexture2D* Turtle(int32 ColorIndex, int32 Frame)
	{
		const FString Key = FString::Printf(TEXT("EggLoading_Turtle_%d_%d"), ColorIndex, Frame);
		return TNHUDArt::Cached(FName(*Key), [ColorIndex, Frame, &Key] { return TNEggLoadingDetail::MakeTurtle(ColorIndex, Frame, *Key); });
	}

	UTexture2D* Sky() { return TNHUDArt::Cached(TEXT("EggLoading_Sky"), [] { return TNEggLoadingDetail::MakeSky(); }); }
	UTexture2D* Sand() { return TNHUDArt::Cached(TEXT("EggLoading_Sand"), [] { return TNEggLoadingDetail::MakeSand(); }); }
	UTexture2D* Dot() { return TNHUDArt::Cached(TEXT("EggLoading_Dot"), [] { return TNEggLoadingDetail::MakeDot(); }); }
	UTexture2D* Shard() { return TNHUDArt::Cached(TEXT("EggLoading_Shard"), [] { return TNEggLoadingDetail::MakeShard(); }); }
	UTexture2D* Burst() { return TNHUDArt::Cached(TEXT("EggLoading_Burst"), [] { return TNEggLoadingDetail::MakeBurst(); }); }

	void Warm()
	{
		EggTop();
		EggBottom();
		Sky();
		Sand();
		Dot();
		Shard();
		Burst();
		for (int32 i = 0; i < NumTurtles; ++i)
		{
			Turtle(i, 0);
			Turtle(i, 1);
		}
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Pintor
// ─────────────────────────────────────────────────────────────────────────────

void STN_EggPainter::Construct(const FArguments& InArgs, const TSharedRef<FTNEggTimeline>& InTimeline)
{
	Timeline = InTimeline;
	TNEggLoadingDetail::SetBrush(SkyBrush, TNEggLoadingArt::Sky());
	TNEggLoadingDetail::SetBrush(SandBrush, TNEggLoadingArt::Sand());
	TNEggLoadingDetail::SetBrush(DotBrush, TNEggLoadingArt::Dot());
	TNEggLoadingDetail::SetBrush(EggTopBrush, TNEggLoadingArt::EggTop());
	TNEggLoadingDetail::SetBrush(EggBottomBrush, TNEggLoadingArt::EggBottom());
	TNEggLoadingDetail::SetBrush(ShardBrush, TNEggLoadingArt::Shard());
	TNEggLoadingDetail::SetBrush(BurstBrush, TNEggLoadingArt::Burst());
	for (int32 i = 0; i < TNEggLoadingArt::NumTurtles; ++i)
	{
		TNEggLoadingDetail::SetBrush(TurtleBrushes[i][0], TNEggLoadingArt::Turtle(i, 0));
		TNEggLoadingDetail::SetBrush(TurtleBrushes[i][1], TNEggLoadingArt::Turtle(i, 1));
	}
}

int32 STN_EggPainter::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	if (!Timeline.IsValid())
	{
		return LayerId;
	}
	const double Now = FPlatformTime::Seconds();
	const FTNEggTimeline& Line = *Timeline;
	const float Time = static_cast<float>(Now - Line.ShowTime);
	const float BreakT = Line.BreakElapsed(Now);
	const float Alpha = InWidgetStyle.GetColorAndOpacityTint().A;
	const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
	const float Sw = static_cast<float>(LocalSize.X);
	const float Sh = static_cast<float>(LocalSize.Y);
	const float Px = Sh / 1080.f;
	int32 Layer = LayerId;

	auto DrawBrush = [&](const FSlateBrush& Brush, float X, float Y, float W, float H, FLinearColor Tint)
	{
		Tint.A *= Alpha;
		if (Tint.A <= 0.002f || W <= 0.f || H <= 0.f)
		{
			return;
		}
		FSlateDrawElement::MakeBox(OutDrawElements, Layer, AllottedGeometry.ToPaintGeometry(FVector2f(W, H), FSlateLayoutTransform(FVector2f(X, Y))),
			&Brush, ESlateDrawEffect::None, Tint);
	};

	// Cielo de noche con estrellas que titilan.
	DrawBrush(SkyBrush, 0.f, 0.f, Sw, Sh, FLinearColor::White);
	++Layer;
	for (int32 i = 0; i < 26; ++i)
	{
		const float Size = (7.f + (i % 3) * 4.f) * Px;
		const float X = FMath::Frac(i * 0.754877f) * Sw;
		const float Y = FMath::Frac(i * 0.569840f + 0.1f) * Sh * 0.6f;
		const float Twinkle = 0.35f + 0.35f * FMath::Sin(Time * 1.7f + i * 2.1f);
		DrawBrush(DotBrush, X - Size * 0.5f, Y - Size * 0.5f, Size, Size, FLinearColor(1.f, 0.97f, 0.85f, Twinkle));
	}

	// Arena y las cuatro tortugas que caminan hacia la derecha.
	++Layer;
	const float SandTop = Sh * 0.78f;
	DrawBrush(SandBrush, 0.f, SandTop, Sw, Sh - SandTop, FLinearColor::White);
	++Layer;
	const float TurtleH = Sh * 0.1f;
	const float TurtleW = TurtleH * 160.f / 110.f;
	const float FeetY = Sh * 0.875f;
	for (int32 i = 0; i < TNEggLoadingArt::NumTurtles; ++i)
	{
		const float Phase = FMath::Frac(Time * 0.045f + i * 0.25f);
		const float X = -TurtleW + Phase * (Sw + TurtleW);
		const int32 Frame = static_cast<int32>(Time * 5.f + i * 0.5f) % 2;
		const float Bob = FMath::Abs(FMath::Sin(Time * 5.f * PI + i)) * TurtleH * 0.035f;
		DrawBrush(TurtleBrushes[i][Frame], X, FeetY - TurtleH * (100.f / 110.f) - Bob, TurtleW, TurtleH, FLinearColor::White);
	}

	// Huevo: entra cerrándose, se balancea y, al romperse, tiembla, se agrieta y revienta.
	++Layer;
	const float EggH = Sh * 0.5f;
	const float EggW = EggH * (TNEggLoadingDetail::EggTexW / TNEggLoadingDetail::EggTexH);
	const float EggCenterX = Sw * 0.5f;
	const float EggCenterY = Sh * 0.44f;
	const float Left = EggCenterX - EggW * 0.5f;
	const float Top = EggCenterY - EggH * 0.5f;

	float TopDx = 0.f, TopDy = 0.f, BottomDx = 0.f, BottomDy = 0.f;
	if (!Line.bStartClosed && Time < 0.55f)
	{
		// Las dos mitades llegan desde fuera de la pantalla con un pequeño rebote al encajar.
		const float K = FMath::Clamp(Time / 0.55f, 0.f, 1.f) - 1.f;
		const float Ease = 1.f + 2.70158f * K * K * K + 1.70158f * K * K;
		TopDy = -(1.f - Ease) * (Top + EggH * 0.6f);
		BottomDy = (1.f - Ease) * (Sh - Top - EggH * 0.4f);
	}
	else if (BreakT < 0.f)
	{
		const float Wobble = FMath::Sin(Time * 2.2f) * EggW * 0.012f;
		const float Bob = FMath::Sin(Time * 1.6f) * EggH * 0.01f;
		TopDx = BottomDx = Wobble;
		TopDy = BottomDy = Bob;
	}
	if (BreakT >= 0.f && BreakT < FTNEggTimeline::PopAt)
	{
		const float Amp = (2.f + 12.f * BreakT / FTNEggTimeline::PopAt) * Px;
		const float ShakeX = FMath::Sin(BreakT * 70.f) * Amp;
		const float ShakeY = FMath::Cos(BreakT * 55.f) * Amp * 0.4f;
		TopDx = BottomDx = ShakeX;
		TopDy = BottomDy = ShakeY;
	}
	const float PopT = BreakT - FTNEggTimeline::PopAt;
	if (PopT >= 0.f)
	{
		// La mitad de arriba sale disparada hacia arriba y la de abajo cae.
		TopDy = -Sh * 1.6f * PopT + 0.5f * Sh * 2.5f * PopT * PopT;
		TopDx = Sw * 0.08f * PopT;
		BottomDy = 0.5f * Sh * 3.f * PopT * PopT;
		BottomDx = -Sw * 0.02f * PopT;
	}
	DrawBrush(EggBottomBrush, Left + BottomDx, Top + BottomDy, EggW, EggH, FLinearColor::White);
	DrawBrush(EggTopBrush, Left + TopDx, Top + TopDy, EggW, EggH, FLinearColor::White);

	// Grietas que crecen desde la costura antes del «¡pum!».
	++Layer;
	if (BreakT >= 0.f && PopT < 0.f)
	{
		auto ToScreen = [&](float Ex, float Ey) { return FVector2D(Left + TopDx + Ex / TNEggLoadingDetail::EggTexW * EggW, Top + TopDy + Ey / TNEggLoadingDetail::EggTexH * EggH); };
		const int32 NumCracks = UE_ARRAY_COUNT(TNEggLoadingDetail::Cracks);
		for (int32 c = 0; c < NumCracks; ++c)
		{
			const float Grow = FMath::Clamp((BreakT - c * 0.13f) / 0.14f, 0.f, 1.f);
			if (Grow <= 0.f)
			{
				continue;
			}
			const TNEggLoadingDetail::FCrack& Crack = TNEggLoadingDetail::Cracks[c];
			const FVector2f Start(Crack.StartX, TNEggLoadingDetail::ZigY(Crack.StartX));
			TArray<FVector2D> Points;
			Points.Add(ToScreen(Start.X, Start.Y));
			if (Grow < 0.5f)
			{
				const FVector2f P = FMath::Lerp(Start, Crack.Mid, Grow * 2.f);
				Points.Add(ToScreen(P.X, P.Y));
			}
			else
			{
				Points.Add(ToScreen(Crack.Mid.X, Crack.Mid.Y));
				const FVector2f P = FMath::Lerp(Crack.Mid, Crack.End, (Grow - 0.5f) * 2.f);
				Points.Add(ToScreen(P.X, P.Y));
			}
			FSlateDrawElement::MakeLines(OutDrawElements, Layer, AllottedGeometry.ToPaintGeometry(), Points, ESlateDrawEffect::None,
				FLinearColor(TNHUDArt::Ink.R, TNHUDArt::Ink.G, TNHUDArt::Ink.B, Alpha), true, 4.f * Px);
		}
	}

	// «¡pum!»: fogonazo, trozos de cáscara y la estrella con el rótulo.
	if (PopT >= 0.f)
	{
		++Layer;
		if (PopT < 0.25f)
		{
			DrawBrush(FlashBrush, 0.f, 0.f, Sw, Sh, FLinearColor(1.f, 0.98f, 0.9f, 0.55f * (1.f - PopT / 0.25f)));
		}
		++Layer;
		const float ShardAlpha = 1.f - FMath::Clamp(PopT / 0.7f, 0.f, 1.f);
		for (int32 i = 0; i < 14; ++i)
		{
			const float Angle = i / 14.f * 2.f * PI + 0.3f * FMath::Sin(i * 7.f);
			const float Speed = Sh * (0.55f + 0.35f * FMath::Frac(i * 0.61f));
			const float Size = Sh * 0.035f * (0.7f + 0.6f * FMath::Frac(i * 0.37f));
			const float X = EggCenterX + FMath::Cos(Angle) * Speed * PopT;
			const float Y = EggCenterY + FMath::Sin(Angle) * Speed * PopT + 0.5f * Sh * 1.8f * PopT * PopT;
			DrawBrush(ShardBrush, X - Size * 0.5f, Y - Size * 0.5f, Size, Size, FLinearColor(1.f, 1.f, 1.f, ShardAlpha));
		}
		++Layer;
		const float Pop = PopT < 0.12f ? PopT / 0.12f * 1.15f : 1.15f - FMath::Min(0.15f, (PopT - 0.12f) * 0.8f);
		const float BurstAlpha = PopT < 0.45f ? 1.f : 1.f - FMath::Clamp((PopT - 0.45f) / 0.3f, 0.f, 1.f);
		const float BurstSize = Sh * 0.34f * Pop;
		DrawBrush(BurstBrush, EggCenterX - BurstSize * 0.5f, EggCenterY - BurstSize * 0.5f, BurstSize, BurstSize, FLinearColor(1.f, 1.f, 1.f, BurstAlpha));
		++Layer;
		FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", 64);
		Font.OutlineSettings.OutlineSize = 4;
		Font.OutlineSettings.OutlineColor = TNHUDArt::Ink;
		const float TextScale = FMath::Max(0.05f, Pop * Px);
		// Ancho aproximado de «¡PUM!» a 64 pt (5 letras gruesas): se centra a ojo en la estrella.
		const float TextX = EggCenterX - 108.f * TextScale;
		const float TextY = EggCenterY - 46.f * TextScale;
		FSlateDrawElement::MakeText(OutDrawElements, Layer,
			AllottedGeometry.ToPaintGeometry(FVector2f(300.f, 120.f), FSlateLayoutTransform(TextScale, FVector2f(TextX, TextY))),
			FString(TEXT("¡PUM!")), Font, ESlateDrawEffect::None, FLinearColor(TNHUDArt::Navy.R, TNHUDArt::Navy.G, TNHUDArt::Navy.B, BurstAlpha * Alpha));
	}
	return Layer;
}

// ─────────────────────────────────────────────────────────────────────────────
// Pantalla
// ─────────────────────────────────────────────────────────────────────────────

void STN_EggLoadingScreen::Construct(const FArguments& InArgs)
{
	TNEggLoadingArt::Warm();
	Timeline->ShowTime = FPlatformTime::Seconds();
	Timeline->bStartClosed = InArgs._StartClosed;
	Status = InArgs._Status.IsEmpty() ? NSLOCTEXT("TNLoading", "Default", "Incubando la partida") : InArgs._Status;

	FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 56);
	TitleFont.OutlineSettings.OutlineSize = 4;
	TitleFont.OutlineSettings.OutlineColor = TNHUDArt::Ink;
	FSlateFontInfo StatusFont = FCoreStyle::GetDefaultFontStyle("Bold", 26);
	StatusFont.OutlineSettings.OutlineSize = 2;
	StatusFont.OutlineSettings.OutlineColor = TNHUDArt::NavyDeep;
	FSlateFontInfo TipFont = FCoreStyle::GetDefaultFontStyle("Regular", 18);

	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(STN_EggPainter, Timeline)
		]
		+ SOverlay::Slot()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().FillHeight(0.063f)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("TNLoading", "Title", "Tortunavy"))
				.Font(TitleFont)
				.ColorAndOpacity(this, &STN_EggLoadingScreen::GetTitleColor)
				.ShadowOffset(FVector2D(0.0, 3.0))
				.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.4f))
			]
			+ SVerticalBox::Slot().FillHeight(0.678f)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Text(this, &STN_EggLoadingScreen::GetStatusText)
				.Font(StatusFont)
				.ColorAndOpacity(this, &STN_EggLoadingScreen::GetStatusColor)
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(40.f, 6.f, 40.f, 0.f))
			[
				SNew(STextBlock)
				.Text(this, &STN_EggLoadingScreen::GetTipText)
				.Font(TipFont)
				.ColorAndOpacity(this, &STN_EggLoadingScreen::GetTipColor)
				.Justification(ETextJustify::Center)
			]
			+ SVerticalBox::Slot().FillHeight(0.259f)
		]
	];

	// Repinta cada fotograma (también en el hilo de carga de MoviePlayer) y lleva la opacidad general.
	RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateLambda([WeakTimeline = TWeakPtr<FTNEggTimeline>(Timeline), this](double, float)
	{
		if (const TSharedPtr<FTNEggTimeline> Line = WeakTimeline.Pin())
		{
			SetRenderOpacity(Line->Opacity(FPlatformTime::Seconds()));
		}
		return EActiveTimerReturnType::Continue;
	}));
}

void STN_EggLoadingScreen::StartBreak()
{
	if (Timeline->BreakTime < 0.0)
	{
		Timeline->BreakTime = FPlatformTime::Seconds();
	}
}

FText STN_EggLoadingScreen::GetStatusText() const
{
	if (IsBreaking())
	{
		return NSLOCTEXT("TNLoading", "Hatch", "¡Allá vamos!");
	}
	const int32 Dots = 1 + static_cast<int32>((FPlatformTime::Seconds() - Timeline->ShowTime) * 2.5) % 3;
	return FText::FromString(Status.ToString() + FString::ChrN(Dots, TEXT('.')));
}

FText STN_EggLoadingScreen::GetTipText() const
{
	static const TCHAR* Tips[] = {
		TEXT("Métete en el caparazón: rodarás cuesta abajo y tus compañeros te podrán lanzar."),
		TEXT("El panzazo cruza huecos que andando no se cruzan."),
		TEXT("Si te noquean, espera a que se vayan los pajaritos."),
		TEXT("Lleva a un compañero en su caparazón y lánzalo hacia la meta."),
		TEXT("La tormenta avanza por el camino: no te quedes atrás."),
		TEXT("En el agua se nada; las corrientes también empujan."),
		TEXT("¿Aburrido en el cuartel? Prueba el parkour del castillo de arena."),
	};
	const int32 NumTips = UE_ARRAY_COUNT(Tips);
	const int32 Start = static_cast<int32>(FMath::Frac(Timeline->ShowTime * 0.37) * NumTips);
	const int32 Index = (Start + static_cast<int32>((FPlatformTime::Seconds() - Timeline->ShowTime) / 4.5)) % NumTips;
	return FText::FromString(FString(TEXT("Consejo: ")) + Tips[Index]);
}

FSlateColor STN_EggLoadingScreen::GetTitleColor() const
{
	return FSlateColor(TNHUDArt::Gold);
}

FSlateColor STN_EggLoadingScreen::GetStatusColor() const
{
	return FSlateColor(TNHUDArt::Cream);
}

FSlateColor STN_EggLoadingScreen::GetTipColor() const
{
	return FSlateColor(FLinearColor(TNHUDArt::Foam.R, TNHUDArt::Foam.G, TNHUDArt::Foam.B, 0.85f));
}
