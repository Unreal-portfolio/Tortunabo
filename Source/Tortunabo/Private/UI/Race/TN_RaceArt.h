#pragma once

#include "CoreMinimal.h"
#include "Core/TN_CosmeticsTypes.h"
#include "Engine/Texture2D.h"
#include "Kismet/GameplayStatics.h"
#include "Multiplayer/MP_GameInstance.h"
#include "../HUD/TN_HUDArt.h"
#include "../HUD/TN_HUDFaces.h"

/**
 * Arte en código de las pantallas del modo carrera (recuento de conchas, campeón y podio), en el estilo Tortunavy del
 * HUD (TN_HUDArt.h): fondos, huecos de concha, corona, brillos, cielo, sol, nubes e iconos de los botones, más las caras
 * de tortuga del HUD (TN_HUDFaces.h) pintadas con el color de piel de cada jugador. Cada textura se dibuja una vez y
 * queda en la caché de TNHUDArt.
 */
namespace TNRaceArt
{
	/** Verde de serie del cuerpo (difuso de M_TortugaDemo2, lineal): las caras del HUD están dibujadas para él. */
	inline const FLinearColor SerieGreen(0.017144f, 0.090625f, 0.000829f, 1.f);

	/** Tono (grados), saturación y brillo de un color lineal medidos en sRGB, como en una herramienta de arte. */
	inline FLinearColor SRGBHSV(const FLinearColor& Linear)
	{
		const FColor C = Linear.ToFColor(true);
		return FLinearColor(C.R / 255.f, C.G / 255.f, C.B / 255.f, 1.f).LinearRGBToHSV();
	}

	/** De tono, saturación y brillo en sRGB a color lineal. */
	inline FLinearColor FromSRGBHSV(const FLinearColor& HSV, float Alpha = 1.f)
	{
		FLinearColor Out = FLinearColor::FromSRGBColor(HSV.HSVToLinearRGB().ToFColor(false));
		Out.A = Alpha;
		return Out;
	}

	inline const UMP_GameInstance* GameInstanceOf(const UObject* Context)
	{
		return Context ? Cast<UMP_GameInstance>(UGameplayStatics::GetGameInstance(Context)) : nullptr;
	}

	/** Color de piel del aspecto (lineal, el de M_TurtleBody); bOutCustom = false con el verde de serie. */
	inline FLinearColor SkinColorOf(const UObject* Context, const FTN_TurtleLook& Look, bool& bOutCustom)
	{
		const UMP_GameInstance* GI = GameInstanceOf(Context);
		const FTN_SkinData* Row = (GI && Look.SkinId != NAME_None) ? GI->FindSkinRow(Look.SkinId, TEXT("RaceFace")) : nullptr;
		bOutCustom = Row != nullptr;
		return Row ? Row->Color : SerieGreen;
	}

	/**
	 * Color de la tortuga para la interfaz (aro de la cara, cinta del nombre): el del caparazón si lleva uno de la
	 * tienda y, si no, el de la piel; con brillo suficiente para leerse sobre el azul marino.
	 */
	inline FLinearColor UIColorOf(const UObject* Context, const FTN_TurtleLook& Look)
	{
		const UMP_GameInstance* GI = GameInstanceOf(Context);
		const FTN_SkinData* ShellRow = (GI && Look.ShellId != NAME_None) ? GI->FindSkinRow(Look.ShellId, TEXT("RaceRing")) : nullptr;
		bool bCustom = false;
		const FLinearColor Base = ShellRow ? ShellRow->Color : SkinColorOf(Context, Look, bCustom);
		FLinearColor HSV = SRGBHSV(Base);
		HSV.G = FMath::Min(HSV.G, 0.85f);
		HSV.B = FMath::Max(HSV.B, 0.62f);
		return FromSRGBHSV(HSV);
	}

	/**
	 * Cambia el verde de la piel de una cara del HUD por Skin: gira el tono y escala saturación y brillo respecto al
	 * verde de serie, así que las luces, sombras, pecas y el filo oscuro se conservan. Lo que no es verde (ojos, boca,
	 * lengua, colorete, sudor, estrellas, borde de pegatina) no se toca.
	 */
	inline void RecolorSkin(TNHUDArt::FPainter& P, const FLinearColor& Skin)
	{
		const FLinearColor Ref = SRGBHSV(SerieGreen);
		const FLinearColor Want = SRGBHSV(Skin);
		const float HueShift = Want.R - Ref.R;
		const float SatK = Want.G / FMath::Max(0.05f, Ref.G);
		const float ValK = FMath::Pow(Want.B / FMath::Max(0.05f, Ref.B), 0.35f);
		for (FLinearColor& Pixel : P.Px)
		{
			if (Pixel.A <= 0.f) { continue; }
			FLinearColor HSV = SRGBHSV(Pixel);
			const bool bSkin = HSV.R >= 60.f && HSV.R <= 170.f && HSV.G > 0.2f && HSV.B > 0.06f;
			if (!bSkin) { continue; }
			HSV.R = FMath::Fmod(HSV.R + HueShift + 360.f, 360.f);
			HSV.G = FMath::Clamp(HSV.G * SatK, 0.f, 1.f);
			HSV.B = FMath::Clamp(HSV.B * ValK, 0.f, 1.f);
			Pixel = FromSRGBHSV(HSV, Pixel.A);
		}
	}

	/** Cara de tortuga del HUD con la piel del aspecto Look (la de serie si no lleva color de la tienda). */
	inline UTexture2D* TurtleFaceFor(const UObject* Context, const FTN_TurtleLook& Look, ETNTurtleFace Face)
	{
		bool bCustom = false;
		const FLinearColor Skin = SkinColorOf(Context, Look, bCustom);
		if (!bCustom) { return TNHUDFaces::TurtleFace(Face); }
		const FColor Key = Skin.ToFColor(true);
		const FString Name = FString::Printf(TEXT("TN_Race_Face%d_%02X%02X%02X"), static_cast<int32>(Face), Key.R, Key.G, Key.B);
		return TNHUDArt::Cached(FName(*Name), [&Name, &Skin, Face]
		{
			TNHUDArt::FPainter P(TNHUDFaces::Size, TNHUDFaces::Size);
			TNHUDFaces::DrawFace(P, Face);
			RecolorSkin(P, Skin);
			return P.ToTexture(*Name);
		});
	}

	/**
	 * Fondo del recuento: el mar azul marino con rayos de luz desde arriba y, abajo (desde el 80 % del alto), la orilla
	 * de arena con su línea de espuma, donde se apoyan las columnas de los jugadores.
	 */
	inline UTexture2D* TallyBackdrop()
	{
		return TNHUDArt::Cached(TEXT("RaceTallyBackdrop"), []
		{
			TNHUDArt::FPainter P(256, 256);
			auto Shore = [](float x) { return 205.f + 3.f * FMath::Sin(x * 0.09f) + 1.5f * FMath::Sin(x * 0.23f + 1.f); };
			P.Layer([](float x, float y) { return TNHUDArt::Box(x, y, 128.f, 128.f, 128.f, 128.f, 0.f); }, [&Shore](float x, float y)
			{
				const float S = Shore(x);
				if (y > S + 6.f)
				{
					return TNHUDArt::Mix(TNHUDArt::SandLight, TNHUDArt::SandC, (y - S) / 40.f);
				}
				if (y > S)
				{
					return TNHUDArt::Mix(TNHUDArt::Foam, TNHUDArt::SandLight, (y - S) / 6.f);
				}
				const float T = y / S;
				FLinearColor C = T < 0.55f ? TNHUDArt::Mix(TNHUDArt::Hex(0x1C6EA4), TNHUDArt::Navy, T / 0.55f)
					: TNHUDArt::Mix(TNHUDArt::Navy, TNHUDArt::Hex(0x0E3B63), (T - 0.55f) / 0.45f);
				// Rayos de luz que bajan en abanico desde arriba y se apagan hacia el fondo.
				const float Ang = FMath::Atan2(x - 150.f, y + 60.f);
				const float Rays = FMath::Pow(FMath::Max(0.f, FMath::Sin(Ang * 13.f + 0.6f)), 6.f);
				C = TNHUDArt::Mix(C, TNHUDArt::SeaLight, 0.16f * Rays * FMath::Max(0.f, 1.f - y / 190.f));
				return C;
			});
			// Ondas de la orilla: una segunda línea de espuma algo más arriba.
			P.Fill([&Shore](float x, float y) { return FMath::Abs(y - (Shore(x) - 7.f + 1.5f * FMath::Sin(x * 0.15f))) - 0.8f; }, TNHUDArt::Hex(0x9BE4F2, 0.55f));
			return P.ToTexture(TEXT("TN_Race_TallyBackdrop"));
		});
	}

	/** Hueco de concha: cuenco de arena húmeda con un aro discontinuo y la silueta de la concha, esperando a llenarse. */
	inline UTexture2D* ShellSocket()
	{
		return TNHUDArt::Cached(TEXT("RaceShellSocket"), []
		{
			TNHUDArt::FPainter P(128, 128);
			P.Fill([](float x, float y) { return TNHUDArt::Circle(x, y, 65.f, 68.f, 56.f); }, TNHUDArt::Hex(0x0A1C38, 0.35f));
			P.Fill([](float x, float y) { return TNHUDArt::Circle(x, y, 64.f, 64.f, 56.f); }, TNHUDArt::Hex(0xC89B5E));
			P.Layer([](float x, float y) { return TNHUDArt::Circle(x, y, 64.f, 64.f, 49.f); },
				[](float, float y) { return TNHUDArt::Mix(TNHUDArt::Hex(0xB8864A), TNHUDArt::Hex(0xF2D49B), (y - 20.f) / 90.f); });
			// Aro discontinuo (doce trazos) y silueta de la concha en el fondo.
			P.Fill([](float x, float y)
			{
				const float A = FMath::Atan2(y - 64.f, x - 64.f);
				const float Dash = FMath::Sin(A * 12.f);
				return FMath::Max(FMath::Abs(TNHUDArt::Circle(x, y, 64.f, 64.f, 41.f)) - 2.f, -Dash * 6.f);
			}, TNHUDArt::Hex(0xFFFBF0, 0.55f));
			auto Fan = [](float x, float y) { return FMath::Min(FMath::Max(TNHUDArt::Circle(x, y, 64.f, 62.f, 26.f), y - 80.f), TNHUDArt::Box(x, y, 64.f, 82.f, 11.f, 5.f, 2.f)); };
			P.Fill(Fan, TNHUDArt::Hex(0x7A5A2E, 0.28f));
			return P.ToTexture(TEXT("TN_Race_ShellSocket"));
		});
	}

	/**
	 * Media concha del recuento: la concha reina de las de puntos (TNHUDArt::ShellIconTier(3), sin su estrella) partida
	 * en diagonal, de abajo a la izquierda a arriba a la derecha, con el corte en zigzag como una concha rota y su borde
	 * de pegatina también por el corte. Sin bSecond, la mitad de arriba a la izquierda; con bSecond, la otra, que encaja
	 * con ella diente con diente: juntas son la concha entera (128 × 128, como el icono).
	 */
	inline UTexture2D* HalfShell(bool bSecond)
	{
		return TNHUDArt::Cached(bSecond ? TEXT("RaceHalfShellB") : TEXT("RaceHalfShellA"), [bSecond]
		{
			TNHUDArt::FPainter P(128, 128);
			const FLinearColor Top = TNHUDArt::Hex(0xFFD6F5);
			const FLinearColor Bottom = TNHUDArt::Hex(0xC04CE0);
			const FLinearColor Ribs = TNHUDArt::Hex(0x7A1F9A, 0.55f);
			const FLinearColor RimColor = TNHUDArt::Hex(0xE8A92E, 0.9f);
			const FLinearColor Gloss = TNHUDArt::Hex(0xFFFFFF, 0.85f);
			const float Side = bSecond ? -1.f : 1.f;
			auto Body = [](float x, float y) { return FMath::Max(TNHUDArt::Circle(x, y, 64.f, 60.f, 44.f), y - 92.f); };
			auto Ears = [](float x, float y) { return TNHUDArt::Box(x, y, 64.f, 96.f, 20.f, 9.f, 4.f); };
			// Corte: U mide hacia abajo a la derecha desde la diagonal que pasa por el centro; los dientes van a lo largo
			// de ella (onda triangular de ±2,4 px cada 13 px).
			auto Cut = [Side](float x, float y)
			{
				const float U = ((x - 64.f) + (y - 64.f)) * 0.70710678f;
				const float V = ((x - 64.f) - (y - 64.f)) * 0.70710678f;
				const float Teeth = 4.8f * (2.f * FMath::Abs(FMath::Frac(V / 13.f) - 0.5f) - 0.5f);
				return Side * (U - Teeth);
			};
			auto Piece = [&](float x, float y) { return FMath::Max(FMath::Min(Body(x, y), Ears(x, y)), Cut(x, y)); };
			P.Sticker(Piece, 6.f);
			P.Layer(Piece, [&Top, &Bottom](float, float y) { return TNHUDArt::Mix(Top, Bottom, (y - 18.f) / 80.f); });
			// Costillas desde la charnela, solo en su mitad.
			for (int32 k = -4; k <= 4; ++k)
			{
				const float A = k * 0.3f;
				const float Ex = 64.f + FMath::Sin(A) * 50.f;
				const float Ey = 96.f - FMath::Cos(A) * 50.f;
				P.Fill([&](float x, float y) { return FMath::Max(TNHUDArt::Segment(x, y, 64.f, 96.f, Ex, Ey, 1.8f), FMath::Max(Body(x, y), Cut(x, y))); }, Ribs);
			}
			// Filo dorado alrededor, también por el corte (se ve roto), y el brillo si le toca.
			P.Fill([&](float x, float y) { return TNHUDArt::Rim(Piece, x, y, 2.f, 1.4f); }, RimColor);
			P.Fill([&](float x, float y) { return FMath::Max(TNHUDArt::Ellipse(x, y, 48.f, 38.f, 9.f, 6.f), Cut(x, y)); }, Gloss);
			return P.ToTexture(bSecond ? TEXT("TN_Race_HalfShellB") : TEXT("TN_Race_HalfShellA"));
		});
	}

	/** Brillo redondo y suave (blanco, para teñirlo): detrás del ganador y en los destellos. */
	inline UTexture2D* SoftGlow()
	{
		return TNHUDArt::Cached(TEXT("RaceSoftGlow"), []
		{
			TNHUDArt::FPainter P(128, 128);
			P.Layer([](float x, float y) { return TNHUDArt::Circle(x, y, 64.f, 64.f, 64.f); }, [](float x, float y)
			{
				const float R = FMath::Sqrt(FMath::Square(x - 64.f) + FMath::Square(y - 64.f)) / 64.f;
				return FLinearColor(1.f, 1.f, 1.f, FMath::Square(FMath::Max(0.f, 1.f - R)));
			});
			return P.ToTexture(TEXT("TN_Race_SoftGlow"));
		});
	}

	/** Destello de cuatro puntas (blanco, para teñirlo). */
	inline UTexture2D* Sparkle()
	{
		return TNHUDArt::Cached(TEXT("RaceSparkle"), []
		{
			TNHUDArt::FPainter P(64, 64);
			P.Layer([](float x, float y) { return TNHUDArt::Circle(x, y, 32.f, 32.f, 30.f); }, [](float x, float y)
			{
				const float R = FMath::Sqrt(FMath::Square(x - 32.f) + FMath::Square(y - 32.f)) / 30.f;
				return FLinearColor(1.f, 1.f, 1.f, 0.35f * FMath::Square(FMath::Max(0.f, 1.f - R)));
			});
			const TArray<FVector2f> Star = TNHUDArt::StarPoints(32.f, 32.f, 29.f, 0.22f, 4);
			P.Fill([&Star](float x, float y) { return TNHUDArt::Polygon(x, y, Star); }, FLinearColor::White);
			return P.ToTexture(TEXT("TN_Race_Sparkle"));
		});
	}

	/** Corona dorada con tres puntas y gemas (la del campeón). */
	inline UTexture2D* Crown()
	{
		return TNHUDArt::Cached(TEXT("RaceCrown"), []
		{
			TNHUDArt::FPainter P(128, 96);
			const TArray<FVector2f> Shape = { { 16.f, 78.f }, { 12.f, 26.f }, { 40.f, 50.f }, { 64.f, 14.f }, { 88.f, 50.f }, { 116.f, 26.f }, { 112.f, 78.f } };
			auto Body = [&Shape](float x, float y) { return FMath::Min(TNHUDArt::Polygon(x, y, Shape), TNHUDArt::Box(x, y, 64.f, 78.f, 50.f, 8.f, 4.f)); };
			auto Tips = [](float x, float y)
			{
				return FMath::Min(FMath::Min(TNHUDArt::Circle(x, y, 12.f, 24.f, 7.f), TNHUDArt::Circle(x, y, 64.f, 12.f, 8.f)), TNHUDArt::Circle(x, y, 116.f, 24.f, 7.f));
			};
			P.Sticker([&](float x, float y) { return FMath::Min(Body(x, y), Tips(x, y)); }, 5.f);
			P.Fill([&](float x, float y) { return FMath::Min(Body(x, y), Tips(x, y)) - 2.5f; }, TNHUDArt::Hex(0x8A5A10));
			P.Layer(Body, [](float, float y) { return TNHUDArt::Mix(TNHUDArt::Hex(0xFFE27A), TNHUDArt::Hex(0xE8A92E), (y - 14.f) / 70.f); });
			P.Fill(Tips, TNHUDArt::Hex(0xFFF1B8));
			P.Fill([&Body](float x, float y) { return FMath::Max(FMath::Abs(y - 70.f) - 1.2f, Body(x, y)); }, TNHUDArt::Hex(0x8A5A10, 0.5f));
			P.Fill([](float x, float y) { return TNHUDArt::Circle(x, y, 64.f, 56.f, 7.f); }, TNHUDArt::CoralC);
			P.Fill([](float x, float y) { return FMath::Min(TNHUDArt::Circle(x, y, 36.f, 64.f, 5.f), TNHUDArt::Circle(x, y, 92.f, 64.f, 5.f)); }, TNHUDArt::Sea);
			P.Fill([](float x, float y) { return TNHUDArt::Circle(x, y, 61.f, 53.f, 2.2f); }, TNHUDArt::Hex(0xFFFFFF, 0.8f));
			return P.ToTexture(TEXT("TN_Race_Crown"));
		});
	}

	/** Cielo de la playa detrás del podio: azul arriba, casi blanco en el horizonte y un poco cálido abajo. */
	inline UTexture2D* SkyGradient()
	{
		return TNHUDArt::Cached(TEXT("RaceSky"), []
		{
			TNHUDArt::FPainter P(8, 256);
			P.Layer([](float x, float y) { return TNHUDArt::Box(x, y, 4.f, 128.f, 4.f, 128.f, 0.f); }, [](float, float y)
			{
				const float T = y / 256.f;
				if (T < 0.62f) { return TNHUDArt::Mix(TNHUDArt::Hex(0x3FA6E6), TNHUDArt::Hex(0x9FDBF5), T / 0.62f); }
				if (T < 0.8f) { return TNHUDArt::Mix(TNHUDArt::Hex(0x9FDBF5), TNHUDArt::Hex(0xE6F7F8), (T - 0.62f) / 0.18f); }
				return TNHUDArt::Mix(TNHUDArt::Hex(0xE6F7F8), TNHUDArt::Hex(0xFFE8C2), (T - 0.8f) / 0.2f);
			});
			return P.ToTexture(TEXT("TN_Race_Sky"));
		});
	}

	/** Sol: disco crema con un halo cálido que se apaga hacia fuera. */
	inline UTexture2D* SunGlow()
	{
		return TNHUDArt::Cached(TEXT("RaceSun"), []
		{
			TNHUDArt::FPainter P(256, 256);
			P.Layer([](float x, float y) { return TNHUDArt::Circle(x, y, 128.f, 128.f, 128.f); }, [](float x, float y)
			{
				const float R = FMath::Sqrt(FMath::Square(x - 128.f) + FMath::Square(y - 128.f)) / 128.f;
				return TNHUDArt::Hex(0xFFE7A0, 0.55f * FMath::Square(FMath::Max(0.f, 1.f - R)));
			});
			P.Fill([](float x, float y) { return TNHUDArt::Circle(x, y, 128.f, 128.f, 46.f); }, TNHUDArt::Hex(0xFFF6D8));
			P.Fill([](float x, float y) { return FMath::Abs(TNHUDArt::Circle(x, y, 128.f, 128.f, 46.f)) - 2.f; }, TNHUDArt::Hex(0xFFD36B, 0.8f));
			return P.ToTexture(TEXT("TN_Race_Sun"));
		});
	}

	/** Nube esponjosa, blanca con la panza azulada. */
	inline UTexture2D* Cloud()
	{
		return TNHUDArt::Cached(TEXT("RaceCloud"), []
		{
			TNHUDArt::FPainter P(256, 128);
			auto Puff = [](float x, float y)
			{
				float D = TNHUDArt::Circle(x, y, 70.f, 76.f, 34.f);
				D = FMath::Min(D, TNHUDArt::Circle(x, y, 118.f, 56.f, 46.f));
				D = FMath::Min(D, TNHUDArt::Circle(x, y, 170.f, 70.f, 38.f));
				D = FMath::Min(D, TNHUDArt::Circle(x, y, 204.f, 88.f, 24.f));
				return FMath::Min(D, TNHUDArt::Box(x, y, 136.f, 98.f, 96.f, 14.f, 13.f));
			};
			P.Layer(Puff, [](float, float y) { return TNHUDArt::Mix(FLinearColor::White, TNHUDArt::Hex(0xCFE6F4), (y - 50.f) / 62.f); });
			return P.ToTexture(TEXT("TN_Race_Cloud"));
		});
	}

	/** Panel lateral del campeón: azul marino que se funde a transparente hacia la derecha (se estira a lo alto). */
	inline UTexture2D* SidePanel()
	{
		return TNHUDArt::Cached(TEXT("RaceSidePanel"), []
		{
			TNHUDArt::FPainter P(256, 8);
			P.Layer([](float x, float y) { return TNHUDArt::Box(x, y, 128.f, 4.f, 128.f, 4.f, 0.f); }, [](float x, float)
			{
				const float T = x / 256.f;
				const float A = T < 0.5f ? FMath::Lerp(0.9f, 0.8f, T / 0.5f) : 0.8f * FMath::Square(1.f - (T - 0.5f) / 0.5f);
				FLinearColor C = TNHUDArt::Mix(TNHUDArt::NavyDeep, TNHUDArt::Navy, T);
				C.A = A;
				return C;
			});
			return P.ToTexture(TEXT("TN_Race_SidePanel"));
		});
	}

	/** Iconos de los botones del campeón, en tinta azul marino sobre la etiqueta de arena. */
	enum class EButtonIcon : uint8 { Replay, Modes, Exit, Lock };

	inline UTexture2D* ButtonIcon(EButtonIcon Icon)
	{
		static const TCHAR* Names[] = { TEXT("RaceIconReplay"), TEXT("RaceIconModes"), TEXT("RaceIconExit"), TEXT("RaceIconLock") };
		const int32 Index = FMath::Clamp(static_cast<int32>(Icon), 0, 3);
		return TNHUDArt::Cached(Names[Index], [Icon, Index]
		{
			TNHUDArt::FPainter P(64, 64);
			const FLinearColor InkC = TNHUDArt::Ink;
			switch (Icon)
			{
			case EButtonIcon::Replay:
			{
				// Flecha que da la vuelta.
				P.Fill([](float x, float y) { return TNHUDArt::Arc(x, y, 32.f, 34.f, 19.f, -2.4f, 2.1f, 4.2f); }, InkC);
				const TArray<FVector2f> Head = { { 12.f, 10.f }, { 30.f, 16.f }, { 16.f, 30.f } };
				P.Fill([&Head](float x, float y) { return TNHUDArt::Polygon(x, y, Head) - 1.f; }, InkC);
				break;
			}
			case EButtonIcon::Modes:
			{
				// Dos flechas que se cruzan (cambiar).
				P.Fill([](float x, float y) { return FMath::Min(TNHUDArt::Segment(x, y, 12.f, 22.f, 46.f, 22.f, 3.6f), TNHUDArt::Segment(x, y, 52.f, 42.f, 18.f, 42.f, 3.6f)); }, InkC);
				const TArray<FVector2f> HeadA = { { 42.f, 12.f }, { 56.f, 22.f }, { 42.f, 32.f } };
				const TArray<FVector2f> HeadB = { { 22.f, 32.f }, { 8.f, 42.f }, { 22.f, 52.f } };
				P.Fill([&](float x, float y) { return FMath::Min(TNHUDArt::Polygon(x, y, HeadA), TNHUDArt::Polygon(x, y, HeadB)) - 1.f; }, InkC);
				break;
			}
			case EButtonIcon::Exit:
			{
				// Puerta entreabierta y flecha hacia fuera.
				P.Fill([](float x, float y) { return FMath::Abs(TNHUDArt::Box(x, y, 22.f, 32.f, 12.f, 22.f, 2.f)) - 2.8f; }, InkC);
				P.Fill([](float x, float y) { return TNHUDArt::Segment(x, y, 26.f, 32.f, 50.f, 32.f, 3.6f); }, InkC);
				const TArray<FVector2f> Head = { { 46.f, 22.f }, { 58.f, 32.f }, { 46.f, 42.f } };
				P.Fill([&Head](float x, float y) { return TNHUDArt::Polygon(x, y, Head) - 1.f; }, InkC);
				break;
			}
			default:
			{
				// Candado.
				P.Fill([](float x, float y) { return FMath::Max(FMath::Abs(TNHUDArt::Circle(x, y, 32.f, 28.f, 12.f)) - 3.5f, y - 30.f); }, InkC);
				P.Fill([](float x, float y) { return TNHUDArt::Box(x, y, 32.f, 42.f, 17.f, 13.f, 3.f); }, InkC);
				P.Fill([](float x, float y) { return FMath::Min(TNHUDArt::Circle(x, y, 32.f, 40.f, 3.5f), TNHUDArt::Box(x, y, 32.f, 46.f, 1.6f, 5.f, 1.f)); }, TNHUDArt::SandC);
				break;
			}
			}
			return P.ToTexture(*FString::Printf(TEXT("TN_Race_%s"), Names[Index]));
		});
	}
}
