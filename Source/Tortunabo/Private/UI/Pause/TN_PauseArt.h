#pragma once

#include "CoreMinimal.h"
#include "../HUD/TN_HUDArt.h"

/**
 * Iconos del menú de pausa pintados en código con el pintor del HUD (TNHUDArt): los de los botones de la portada (tinta
 * azul marino sobre la etiqueta de arena, como los del campeón de la carrera) y los de voz de la lista de jugadores
 * (pegatinas con borde crema: altavoz para los compañeros, micrófono para ti, tachados si están silenciados) y la corona
 * del anfitrión. Cada uno se dibuja una vez y queda en la caché de TNHUDArt.
 */
namespace TNPauseArt
{
	/** Iconos de los botones de la portada. */
	enum class EMenuIcon : uint8 { Resume, Settings, Controls, Lobby, Menu, Quit };

	inline UTexture2D* MenuIcon(EMenuIcon Icon)
	{
		static const TCHAR* Names[] = { TEXT("PauseIconResume"), TEXT("PauseIconSettings"), TEXT("PauseIconControls"), TEXT("PauseIconLobby"),
			TEXT("PauseIconMenu"), TEXT("PauseIconQuit") };
		const int32 Index = FMath::Clamp(static_cast<int32>(Icon), 0, 5);
		return TNHUDArt::Cached(Names[Index], [Icon, Index]
		{
			TNHUDArt::FPainter P(64, 64);
			const FLinearColor InkC = TNHUDArt::Ink;
			switch (Icon)
			{
			case EMenuIcon::Resume:
			{
				// Triángulo de «seguir».
				const TArray<FVector2f> Tri = { { 22.f, 13.f }, { 51.f, 32.f }, { 22.f, 51.f } };
				P.Fill([&Tri](float x, float y) { return TNHUDArt::Polygon(x, y, Tri) - 2.f; }, InkC);
				break;
			}
			case EMenuIcon::Settings:
			{
				// Rueda dentada: anillo con ocho dientes redondos y el agujero del centro.
				auto Gear = [](float x, float y)
				{
					float D = TNHUDArt::Circle(x, y, 32.f, 32.f, 16.f);
					for (int32 k = 0; k < 8; ++k)
					{
						const float A = k * PI / 4.f;
						D = FMath::Min(D, TNHUDArt::Circle(x, y, 32.f + 18.f * FMath::Cos(A), 32.f + 18.f * FMath::Sin(A), 5.5f));
					}
					return FMath::Max(D, -TNHUDArt::Circle(x, y, 32.f, 32.f, 7.f));
				};
				P.Fill(Gear, InkC);
				break;
			}
			case EMenuIcon::Controls:
			{
				// Mando: cuerpo con dos asas, cruceta y dos botones en hueco.
				auto Body = [](float x, float y)
				{
					return FMath::Min(TNHUDArt::Box(x, y, 32.f, 30.f, 22.f, 11.f, 10.f),
						FMath::Min(TNHUDArt::Circle(x, y, 17.f, 40.f, 9.f), TNHUDArt::Circle(x, y, 47.f, 40.f, 9.f)));
				};
				P.Fill(Body, InkC);
				const FLinearColor Hole = TNHUDArt::SandLight;
				P.Fill([](float x, float y) { return FMath::Min(TNHUDArt::Box(x, y, 20.f, 31.f, 6.f, 2.f, 1.f), TNHUDArt::Box(x, y, 20.f, 31.f, 2.f, 6.f, 1.f)); }, Hole);
				P.Fill([](float x, float y) { return FMath::Min(TNHUDArt::Circle(x, y, 42.f, 28.f, 3.f), TNHUDArt::Circle(x, y, 47.f, 34.f, 3.f)); }, Hole);
				break;
			}
			case EMenuIcon::Lobby:
			{
				// Banderín del castillo: mástil y bandera.
				P.Fill([](float x, float y) { return TNHUDArt::Segment(x, y, 20.f, 11.f, 20.f, 54.f, 3.2f); }, InkC);
				const TArray<FVector2f> Flag = { { 23.f, 11.f }, { 51.f, 20.f }, { 23.f, 31.f } };
				P.Fill([&Flag](float x, float y) { return TNHUDArt::Polygon(x, y, Flag) - 1.f; }, InkC);
				P.Fill([](float x, float y) { return TNHUDArt::Segment(x, y, 12.f, 54.f, 30.f, 54.f, 3.2f); }, InkC);
				break;
			}
			case EMenuIcon::Menu:
			{
				// Tres renglones (el menú principal).
				P.Fill([](float x, float y)
				{
					return FMath::Min(TNHUDArt::Segment(x, y, 16.f, 20.f, 48.f, 20.f, 3.6f),
						FMath::Min(TNHUDArt::Segment(x, y, 16.f, 32.f, 48.f, 32.f, 3.6f), TNHUDArt::Segment(x, y, 16.f, 44.f, 48.f, 44.f, 3.6f)));
				}, InkC);
				break;
			}
			default:
			{
				// Puerta entreabierta y flecha hacia fuera (salir al escritorio).
				P.Fill([](float x, float y) { return FMath::Abs(TNHUDArt::Box(x, y, 22.f, 32.f, 12.f, 22.f, 2.f)) - 2.8f; }, InkC);
				P.Fill([](float x, float y) { return TNHUDArt::Segment(x, y, 26.f, 32.f, 50.f, 32.f, 3.6f); }, InkC);
				const TArray<FVector2f> Head = { { 46.f, 22.f }, { 58.f, 32.f }, { 46.f, 42.f } };
				P.Fill([&Head](float x, float y) { return TNHUDArt::Polygon(x, y, Head) - 1.f; }, InkC);
				break;
			}
			}
			return P.ToTexture(*FString::Printf(TEXT("TN_%s"), Names[Index]));
		});
	}

	/** Raya coral de «silenciado» encima de un icono de voz (con su borde crema). */
	inline void DrawMuteSlash(TNHUDArt::FPainter& P)
	{
		auto Slash = [](float x, float y) { return TNHUDArt::Segment(x, y, 13.f, 13.f, 51.f, 51.f, 3.6f); };
		P.Fill([&Slash](float x, float y) { return Slash(x, y) - 3.f; }, TNHUDArt::Cream);
		P.Fill(Slash, TNHUDArt::CoralDeep);
	}

	/** Altavoz con dos ondas (voz de un compañero); bMuted, tachado. Pegatina clara para el panel azul marino. */
	inline UTexture2D* SpeakerIcon(bool bMuted)
	{
		return TNHUDArt::Cached(bMuted ? TEXT("PauseSpeakerMuted") : TEXT("PauseSpeaker"), [bMuted]
		{
			TNHUDArt::FPainter P(64, 64);
			const TArray<FVector2f> Cone = { { 21.f, 24.f }, { 34.f, 13.f }, { 34.f, 51.f }, { 21.f, 40.f } };
			auto Body = [&Cone](float x, float y) { return FMath::Min(TNHUDArt::Box(x, y, 16.f, 32.f, 7.f, 9.f, 2.f), TNHUDArt::Polygon(x, y, Cone)); };
			auto Waves = [](float x, float y)
			{
				return FMath::Min(TNHUDArt::Arc(x, y, 34.f, 32.f, 10.f, -0.9f, 0.9f, 2.4f), TNHUDArt::Arc(x, y, 34.f, 32.f, 18.f, -0.85f, 0.85f, 2.4f));
			};
			auto All = [&](float x, float y) { return bMuted ? Body(x, y) : FMath::Min(Body(x, y), Waves(x, y)); };
			P.Sticker(All, 3.f, 3.f, FVector2f(1.f, 2.f));
			P.Fill(All, bMuted ? TNHUDArt::Hex(0x8FA3B3) : TNHUDArt::SeaLight);
			if (bMuted) { DrawMuteSlash(P); }
			return P.ToTexture(bMuted ? TEXT("TN_PauseSpeakerMuted") : TEXT("TN_PauseSpeaker"));
		});
	}

	/** Micrófono (tu voz); bMuted, tachado. */
	inline UTexture2D* MicIcon(bool bMuted)
	{
		return TNHUDArt::Cached(bMuted ? TEXT("PauseMicMuted") : TEXT("PauseMic"), [bMuted]
		{
			TNHUDArt::FPainter P(64, 64);
			auto Mic = [](float x, float y)
			{
				float D = TNHUDArt::Box(x, y, 32.f, 23.f, 8.f, 14.f, 8.f);
				D = FMath::Min(D, TNHUDArt::Arc(x, y, 32.f, 26.f, 15.f, 0.25f, PI - 0.25f, 2.6f));
				D = FMath::Min(D, TNHUDArt::Segment(x, y, 32.f, 41.f, 32.f, 51.f, 2.6f));
				return FMath::Min(D, TNHUDArt::Segment(x, y, 23.f, 52.f, 41.f, 52.f, 2.6f));
			};
			P.Sticker(Mic, 3.f, 3.f, FVector2f(1.f, 2.f));
			P.Fill(Mic, bMuted ? TNHUDArt::Hex(0x8FA3B3) : TNHUDArt::Gold);
			if (bMuted) { DrawMuteSlash(P); }
			return P.ToTexture(bMuted ? TEXT("TN_PauseMicMuted") : TEXT("TN_PauseMic"));
		});
	}

	/** Corona pequeña del anfitrión. */
	inline UTexture2D* HostCrown()
	{
		return TNHUDArt::Cached(TEXT("PauseHostCrown"), []
		{
			TNHUDArt::FPainter P(64, 64);
			const TArray<FVector2f> Crown = { { 10.f, 44.f }, { 13.f, 18.f }, { 23.f, 31.f }, { 32.f, 12.f }, { 41.f, 31.f }, { 51.f, 18.f }, { 54.f, 44.f } };
			auto Shape = [&Crown](float x, float y) { return FMath::Min(TNHUDArt::Polygon(x, y, Crown), TNHUDArt::Box(x, y, 32.f, 47.f, 22.f, 5.f, 2.f)); };
			P.Sticker(Shape, 3.f, 3.f, FVector2f(1.f, 2.f));
			P.Fill(Shape, TNHUDArt::Gold);
			P.Fill([](float x, float y) { return TNHUDArt::Circle(x, y, 32.f, 36.f, 3.5f); }, TNHUDArt::CoralC);
			return P.ToTexture(TEXT("TN_PauseHostCrown"));
		});
	}
}
