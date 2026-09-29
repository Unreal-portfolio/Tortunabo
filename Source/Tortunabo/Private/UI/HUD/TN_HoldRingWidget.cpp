#include "UI/HUD/TN_HoldRingWidget.h"
#include "TN_HUDArt.h"
#include "Rendering/DrawElements.h"

void UTN_HoldRingWidget::SetProgress(float InProgress)
{
	const float Clamped = FMath::Clamp(InProgress, 0.f, 1.f);
	if (FMath::IsNearlyEqual(Clamped, Progress, 0.001f))
	{
		return;
	}
	Progress = Clamped;
	Invalidate(EInvalidateWidgetReason::Paint);
}

void UTN_HoldRingWidget::SetRingSize(float InDiameter, float InThickness)
{
	RingDiameter = FMath::Max(8.f, InDiameter);
	RingThickness = FMath::Clamp(InThickness, 1.f, RingDiameter * 0.25f);
	SetMinimumDesiredSize(FVector2D(RingDiameter, RingDiameter));
	Invalidate(EInvalidateWidgetReason::Paint);
}

int32 UTN_HoldRingWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	int32 Layer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	const FVector2f LocalSize = FVector2f(AllottedGeometry.GetLocalSize());
	const FVector2f Mid = LocalSize * 0.5f;
	// Radio del eje del aro: el filo crema (el trazo más grueso) cabe entero en el tamaño asignado.
	const float Rim = RingThickness + 4.f;
	const float AxisRadius = FMath::Max(2.f, FMath::Min(LocalSize.X, LocalSize.Y) * 0.5f - Rim * 0.5f);
	const FLinearColor Tint = InWidgetStyle.GetColorAndOpacityTint();

	// Puntos de un arco sobre el eje, de arriba (-90°) en el sentido de las agujas del reloj (y hacia abajo en Slate).
	auto ArcPoints = [&](float Fraction)
	{
		constexpr int32 FullSegments = 56;
		const int32 Segments = FMath::Max(2, FMath::CeilToInt(FullSegments * Fraction));
		TArray<FVector2f> Points;
		Points.Reserve(Segments + 1);
		for (int32 i = 0; i <= Segments; ++i)
		{
			const float Angle = -HALF_PI + 2.f * PI * Fraction * static_cast<float>(i) / Segments;
			Points.Add(Mid + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * AxisRadius);
		}
		return Points;
	};

	const FPaintGeometry PaintGeometry = AllottedGeometry.ToPaintGeometry();
	const TArray<FVector2f> Full = ArcPoints(1.f);
	// Filo crema, pista azul marino translúcida encima y el relleno dorado.
	FSlateDrawElement::MakeLines(OutDrawElements, ++Layer, PaintGeometry, Full, ESlateDrawEffect::None,
		TNHUDArt::Cream * Tint, true, Rim);
	FSlateDrawElement::MakeLines(OutDrawElements, ++Layer, PaintGeometry, Full, ESlateDrawEffect::None,
		TNHUDArt::Hex(0x0A1C38, 0.85f) * Tint, true, RingThickness);
	if (Progress > 0.001f)
	{
		FSlateDrawElement::MakeLines(OutDrawElements, ++Layer, PaintGeometry, ArcPoints(Progress), ESlateDrawEffect::None,
			TNHUDArt::Gold * Tint, true, RingThickness);
	}
	return Layer;
}
