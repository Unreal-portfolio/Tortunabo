#pragma once

#include "CoreMinimal.h"
#include "TN_HUDArt.h"

/**
 * Cara de la tortuga fantasma para el HUD (Docs/Fantasma_Espectador.md): una pegatina como las caras de TN_HUDFaces.h
 * (misma cabeza, mismo borde crema y la misma sombra), pero blanco azulada, sin barbilla: la parte de abajo acaba en
 * una colita ondulada que se enrosca hacia un lado, como la de Casper. Ojos grandes y oscuros con brillos, boquita en
 * «o», coloretes y el caparazón fantasma asomando por detrás. La usan el cartel del espectador (UTN_GhostHUDWidget) y
 * la tripulación del HUD (UTN_RunFlowHUDWidget) cuando alguien es fantasma. Se dibuja una vez y queda en la caché de
 * TNHUDArt (el HUD no tenía icono de fantasma que reutilizar).
 */
namespace TNHUDGhostFace
{
	inline UTexture2D* Texture()
	{
		return TNHUDArt::Cached(TEXT("Face_Ghost"), []
		{
			constexpr int32 CanvasSize = 256;
			TNHUDArt::FPainter Painter(CanvasSize, CanvasSize);
			const FLinearColor GhostLine = TNHUDArt::Hex(0x4F7FA8);
			const FLinearColor GhostTop = TNHUDArt::Hex(0xF6FBFF);
			const FLinearColor GhostBottom = TNHUDArt::Hex(0xB6DAF2);
			const FLinearColor ShellFill = TNHUDArt::Hex(0xCFE6F8);
			const FLinearColor ShellSeam = TNHUDArt::Hex(0x93BEE0);
			const FLinearColor EyeInk = TNHUDArt::Hex(0x1C2640);

			// Silueta: cabeza, cuerpo que se estrecha y cola ondulada que se enrosca a la derecha; detrás, el caparazón.
			auto HeadShape = [](float x, float y) { return TNHUDArt::Ellipse(x, y, 124.f, 112.f, 86.f, 70.f); };
			auto BodyShape = [](float x, float y) { return TNHUDArt::Ellipse(x, y, 118.f, 156.f, 64.f, 46.f); };
			auto TailShape = [](float x, float y)
			{
				float D = TNHUDArt::Segment(x, y, 116.f, 176.f, 140.f, 208.f, 30.f);
				D = FMath::Min(D, TNHUDArt::Segment(x, y, 140.f, 208.f, 176.f, 224.f, 20.f));
				D = FMath::Min(D, TNHUDArt::Segment(x, y, 176.f, 224.f, 204.f, 210.f, 12.f));
				D = FMath::Min(D, TNHUDArt::Segment(x, y, 204.f, 210.f, 212.f, 190.f, 7.f));
				return D;
			};
			auto GhostShape = [&](float x, float y)
			{
				// Borde de abajo con ondas (el faldón del fantasma).
				const float Wave = 3.5f * FMath::Sin(x * 0.11f) * FMath::SmoothStep(150.f, 196.f, y);
				return FMath::Min(FMath::Min(HeadShape(x, y), BodyShape(x, y)), TailShape(x, y)) + Wave;
			};
			auto ShellShape = [](float x, float y) { return FMath::Max(TNHUDArt::Ellipse(x, y, 130.f, 64.f, 84.f, 50.f), y - 96.f); };
			Painter.Sticker([&](float x, float y) { return FMath::Min(GhostShape(x, y), ShellShape(x, y)); }, 7.f, 6.f, FVector2f(3.f, 6.f));

			// Caparazón fantasma asomando por detrás de la cabeza, con sus escudos.
			Painter.Fill([&](float x, float y) { return ShellShape(x, y) - 4.f; }, GhostLine);
			Painter.Fill(ShellShape, ShellFill);
			Painter.Fill([&](float x, float y)
			{
				const float Plates = FMath::Min(TNHUDArt::Segment(x, y, 104.f, 20.f, 96.f, 96.f, 2.5f), TNHUDArt::Segment(x, y, 158.f, 20.f, 166.f, 96.f, 2.5f));
				return FMath::Max(FMath::Min(Plates, FMath::Abs(TNHUDArt::Ellipse(x, y, 130.f, 70.f, 60.f, 34.f)) - 2.5f), ShellShape(x, y));
			}, ShellSeam);

			// Cuerpo con filo y degradado.
			Painter.Fill([&](float x, float y) { return GhostShape(x, y) - 4.f; }, GhostLine);
			Painter.Layer(GhostShape, [&](float, float y) { return TNHUDArt::Mix(GhostTop, GhostBottom, (y - 50.f) / 180.f); });
			Painter.Fill([](float x, float y) { return TNHUDArt::Ellipse(x, y, 94.f, 84.f, 30.f, 16.f); }, TNHUDArt::Hex(0xFFFFFF, 0.55f));

			// Coloretes.
			Painter.Fill([](float x, float y) { return FMath::Min(TNHUDArt::Ellipse(x, y, 72.f, 140.f, 15.f, 9.f), TNHUDArt::Ellipse(x, y, 176.f, 140.f, 15.f, 9.f)); },
				TNHUDArt::Hex(0xFFB3C7, 0.45f));

			// Ojos grandes y oscuros con dos brillos.
			for (const float EyeX : { 98.f, 150.f })
			{
				Painter.Fill([EyeX](float x, float y) { return TNHUDArt::Ellipse(x, y, EyeX, 110.f, 17.f, 24.f); }, EyeInk);
				Painter.Fill([EyeX](float x, float y)
				{
					return FMath::Min(TNHUDArt::Circle(x, y, EyeX + 6.f, 99.f, 6.5f), TNHUDArt::Circle(x, y, EyeX - 5.f, 121.f, 3.f));
				}, FLinearColor::White);
			}

			// Boquita en «o».
			Painter.Fill([](float x, float y) { return TNHUDArt::Ellipse(x, y, 124.f, 148.f, 10.f, 12.f); }, EyeInk);
			Painter.Fill([](float x, float y) { return TNHUDArt::Ellipse(x, y, 124.f, 151.f, 6.f, 7.f); }, TNHUDArt::Hex(0x7A3B5E));

			// Chispitas de fantasma alrededor.
			for (const FVector2f& Spark : { FVector2f(46.f, 190.f), FVector2f(222.f, 58.f), FVector2f(36.f, 70.f) })
			{
				const TArray<FVector2f> Star = TNHUDArt::StarPoints(Spark.X, Spark.Y, 10.f, 0.34f, 4);
				Painter.Fill([&Star](float x, float y) { return TNHUDArt::Polygon(x, y, Star) - 2.f; }, GhostLine);
				Painter.Fill([&Star](float x, float y) { return TNHUDArt::Polygon(x, y, Star); }, TNHUDArt::Hex(0xE8F6FF));
			}
			return Painter.ToTexture(TEXT("TN_HUD_Face_Ghost"));
		});
	}
}
