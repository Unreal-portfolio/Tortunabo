#pragma once

#include "CoreMinimal.h"
#include "../HUD/TN_HUDArt.h"

/**
 * Iconos de las salas (Docs/Salas.md) pintados en código con el pintor del HUD (TNHUDArt), como los del menú de pausa:
 * el «⋮» de las opciones de un jugador, el candado de una sala cerrada o abierta y el grupo de tortugas del botón «Sala»
 * de la portada del menú de pausa (tinta azul marino sobre la etiqueta de arena). Cada uno se dibuja una vez y queda en la
 * caché de TNHUDArt.
 */
namespace TNRoomArt
{
	/** «⋮»: tres puntos dorados con borde crema (opciones de un jugador). */
	inline UTexture2D* MoreIcon()
	{
		return TNHUDArt::Cached(TEXT("RoomMore"), []
		{
			TNHUDArt::FPainter P(64, 64);
			auto Dots = [](float x, float y)
			{
				return FMath::Min(TNHUDArt::Circle(x, y, 32.f, 14.f, 6.5f),
					FMath::Min(TNHUDArt::Circle(x, y, 32.f, 32.f, 6.5f), TNHUDArt::Circle(x, y, 32.f, 50.f, 6.5f)));
			};
			P.Sticker(Dots, 3.f, 3.f, FVector2f(1.f, 2.f));
			P.Fill(Dots, TNHUDArt::Gold);
			return P.ToTexture(TEXT("TN_Room_More"));
		});
	}

	/** Candado: cerrado (coral) o abierto (verde), con borde crema. */
	inline UTexture2D* LockIcon(bool bLocked)
	{
		return TNHUDArt::Cached(bLocked ? TEXT("RoomLockClosed") : TEXT("RoomLockOpen"), [bLocked]
		{
			TNHUDArt::FPainter P(64, 64);
			// El arco sube y se separa del cuerpo al abrirse (solo la pata izquierda sigue dentro).
			const float ArcY = bLocked ? 27.f : 19.f;
			auto Shackle = [ArcY, bLocked](float x, float y)
			{
				float D = TNHUDArt::Arc(x, y, 32.f, ArcY, 10.f, -PI, 0.f, 3.2f);
				D = FMath::Min(D, TNHUDArt::Segment(x, y, 22.f, ArcY, 22.f, 33.f, 3.2f));
				D = FMath::Min(D, TNHUDArt::Segment(x, y, 42.f, ArcY, 42.f, bLocked ? 33.f : ArcY + 4.f, 3.2f));
				return D;
			};
			auto Body = [](float x, float y) { return TNHUDArt::Box(x, y, 32.f, 42.f, 16.f, 12.f, 4.f); };
			auto All = [&](float x, float y) { return FMath::Min(Shackle(x, y), Body(x, y)); };
			P.Sticker(All, 3.f, 3.f, FVector2f(1.f, 2.f));
			P.Fill(Shackle, TNHUDArt::Hex(0x8FA3B3));
			P.Fill(Body, bLocked ? TNHUDArt::CoralC : TNHUDArt::ShellGreen);
			// Ojo de la cerradura.
			P.Fill([](float x, float y)
			{
				return FMath::Min(TNHUDArt::Circle(x, y, 32.f, 40.f, 3.2f), TNHUDArt::Segment(x, y, 32.f, 40.f, 32.f, 47.f, 1.6f));
			}, TNHUDArt::NavyDeep);
			return P.ToTexture(bLocked ? TEXT("TN_Room_LockClosed") : TEXT("TN_Room_LockOpen"));
		});
	}

	/** Grupo de tres tortugas (el botón «Sala» de la portada del menú de pausa): tinta sobre la etiqueta de arena. */
	inline UTexture2D* RoomMenuIcon()
	{
		return TNHUDArt::Cached(TEXT("RoomMenuIcon"), []
		{
			TNHUDArt::FPainter P(64, 64);
			// Una figura: cabeza redonda y caparazón (media elipse con la base plana).
			auto Person = [](float x, float y, float Cx, float Cy, float S)
			{
				const float Head = TNHUDArt::Circle(x, y, Cx, Cy - 10.f * S, 6.5f * S);
				const float Shell = FMath::Max(TNHUDArt::Ellipse(x, y, Cx, Cy + 10.f * S, 12.f * S, 11.f * S), y - (Cy + 12.f * S));
				return FMath::Min(Head, Shell);
			};
			auto Sides = [&Person](float x, float y) { return FMath::Min(Person(x, y, 17.f, 36.f, 0.78f), Person(x, y, 47.f, 36.f, 0.78f)); };
			auto Middle = [&Person](float x, float y) { return Person(x, y, 32.f, 31.f, 1.f); };
			P.Fill(Sides, TNHUDArt::Ink);
			// Un hueco de arena alrededor de la del medio, para que se lea delante de las otras dos.
			P.Fill([&Middle](float x, float y) { return Middle(x, y) - 2.5f; }, TNHUDArt::SandLight);
			P.Fill(Middle, TNHUDArt::Ink);
			return P.ToTexture(TEXT("TN_Room_MenuIcon"));
		});
	}
}
