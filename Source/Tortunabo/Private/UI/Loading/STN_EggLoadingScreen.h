#pragma once

#include "CoreMinimal.h"
#include "Brushes/SlateColorBrush.h"
#include "Styling/SlateBrush.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SLeafWidget.h"

class UTexture2D;

/**
 * Arte de la pantalla de carga del huevo, dibujado en código con las funciones de distancia de TNHUDArt (sin
 * texturas externas). Cada imagen se genera una vez (hilo de juego) y queda en la raíz del recolector: así también
 * la puede pintar el hilo de carga de MoviePlayer durante un LoadMap bloqueante.
 */
namespace TNEggLoadingArt
{
	/** Mitad de arriba y de abajo del huevo (mismo lienzo de 360 x 480: cerradas encajan por el zigzag). */
	UTexture2D* EggTop();
	UTexture2D* EggBottom();
	/** Tortuga de perfil andando hacia la derecha: 4 colores de caparazón y 2 fotogramas de patas. */
	UTexture2D* Turtle(int32 ColorIndex, int32 Frame);
	UTexture2D* Sky();
	UTexture2D* Sand();
	UTexture2D* Dot();
	UTexture2D* Shard();
	UTexture2D* Burst();

	/** Genera todo de una vez (hilo de juego). */
	void Warm();

	/** Número de tortugas (y de colores). */
	constexpr int32 NumTurtles = 4;
}

/** Tiempos compartidos entre la pantalla y su pintor (segundos de FPlatformTime). */
struct FTNEggTimeline
{
	double ShowTime = 0.0;
	double BreakTime = -1.0;
	bool bStartClosed = false;

	/** Segundos desde que empezó a romperse (negativo si no se está rompiendo). */
	float BreakElapsed(double Now) const { return BreakTime < 0.0 ? -1.f : static_cast<float>(Now - BreakTime); }

	/** Momentos de la rotura: tiembla y se agrieta, «¡pum!» y fundido hasta el final. */
	static constexpr float PopAt = 0.75f;
	static constexpr float FadeStart = 0.95f;
	static constexpr float BreakEnd = 1.5f;

	/** Opacidad general: entra en 0,2 s y se funde al final de la rotura. */
	float Opacity(double Now) const
	{
		const float In = FMath::Clamp(static_cast<float>(Now - ShowTime) / 0.2f, 0.f, 1.f);
		const float B = BreakElapsed(Now);
		const float Out = B < FadeStart ? 1.f : 1.f - FMath::Clamp((B - FadeStart) / (BreakEnd - FadeStart), 0.f, 1.f);
		return (bStartClosed ? 1.f : In) * Out;
	}
};

/**
 * Pintor de la escena: cielo de noche, estrellas, arena con cuatro tortugas andando, el huevo (se cierra desde arriba
 * y desde abajo, se balancea mientras carga y, al estar listo el mapa, tiembla, se agrieta y revienta con «¡pum!").
 * Solo dibuja; el estado vive en FTNEggTimeline.
 */
class STN_EggPainter : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(STN_EggPainter) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FTNEggTimeline>& InTimeline);

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override { return FVector2D(64.0, 64.0); }

private:
	TSharedPtr<FTNEggTimeline> Timeline;
	FSlateBrush SkyBrush;
	FSlateBrush SandBrush;
	FSlateBrush DotBrush;
	FSlateBrush EggTopBrush;
	FSlateBrush EggBottomBrush;
	FSlateBrush ShardBrush;
	FSlateBrush BurstBrush;
	FSlateBrush TurtleBrushes[TNEggLoadingArt::NumTurtles][2];
	FSlateColorBrush FlashBrush = FSlateColorBrush(FLinearColor::White);
};

/**
 * Pantalla de carga de Tortunavy: la escena del huevo (STN_EggPainter) con el nombre del juego arriba, el estado de
 * la carga y un consejo que va cambiando. La crea UTN_LoadingScreenSubsystem (en el viewport y, fuera de PIE, en
 * MoviePlayer durante los LoadMap bloqueantes).
 */
class STN_EggLoadingScreen : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(STN_EggLoadingScreen) : _StartClosed(false) {}
		/** Empieza con el huevo ya cerrado (sin la entrada de las dos mitades). */
		SLATE_ARGUMENT(bool, StartClosed)
		SLATE_ARGUMENT(FText, Status)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void SetStatus(const FText& InStatus) { Status = InStatus; }

	/** Empieza la rotura (tiembla, se agrieta y revienta); al acabar, IsBreakFinished. */
	void StartBreak();
	bool IsBreaking() const { return Timeline->BreakTime >= 0.0; }
	float GetBreakElapsed() const { return Timeline->BreakElapsed(FPlatformTime::Seconds()); }
	bool IsBreakFinished() const { return IsBreaking() && GetBreakElapsed() >= FTNEggTimeline::BreakEnd; }

private:
	FText GetStatusText() const;
	FText GetTipText() const;
	FSlateColor GetTitleColor() const;
	FSlateColor GetStatusColor() const;
	FSlateColor GetTipColor() const;

	TSharedRef<FTNEggTimeline> Timeline = MakeShared<FTNEggTimeline>();
	FText Status;
};
