#include "UI/HUD/TN_RadialWheelWidgetBase.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/SizeBox.h"
#include "Rendering/DrawElements.h"

// ─────────────────────────────────────────────────────────────────────────────
// C++ default implementation — draws labels + icons in RadialSlotsContainer.
// Blueprint children may override this event for full custom visuals; if they
// do, the C++ version is NOT called (BlueprintNativeEvent semantics).
// ─────────────────────────────────────────────────────────────────────────────

void UTN_RadialWheelWidgetBase::BP_OnEntriesSet_Implementation(const TArray<FTN_RadialWheelEntryView>& InEntries)
{
	SlotBorders.Reset();

	if (!RadialSlotsContainer)
	{
		return;
	}

	RadialSlotsContainer->ClearChildren();

	const int32 N = InEntries.Num();
	if (N == 0)
	{
		return;
	}

	const float AngleStepRad  = (2.f * PI) / static_cast<float>(N);
	const float OffsetRad     = FMath::DegreesToRadians(SelectionAngleOffsetDegrees);

	for (int32 i = 0; i < N; ++i)
	{
		const FTN_RadialWheelEntryView& Entry = InEntries[i];

		// ── Compute position (UMG Y grows downward) ──────────────────────────
		const float Angle = OffsetRad + AngleStepRad * static_cast<float>(i);
		const float PosX  = RadialCenter.X + FMath::Cos(Angle) * SlotRadius;
		const float PosY  = RadialCenter.Y - FMath::Sin(Angle) * SlotRadius;

		// ── Build slot: Border > SizeBox > VerticalBox > [Icon?] + Label ─────
		UVerticalBox* SlotBox = NewObject<UVerticalBox>(this);

		if (Entry.Icon)
		{
			UImage* IconWidget = NewObject<UImage>(this);
			IconWidget->SetBrushFromTexture(Entry.Icon, false);
			IconWidget->SetBrushSize(SlotIconSize);
			if (UVerticalBoxSlot* VSlot = SlotBox->AddChildToVerticalBox(IconWidget))
			{
				VSlot->SetHorizontalAlignment(HAlign_Center);
				VSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
			}
		}

		UTextBlock* LabelWidget = NewObject<UTextBlock>(this);
		LabelWidget->SetText(Entry.Label);
		LabelWidget->SetJustification(ETextJustify::Center);
		LabelWidget->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		if (UVerticalBoxSlot* VSlot = SlotBox->AddChildToVerticalBox(LabelWidget))
		{
			VSlot->SetHorizontalAlignment(HAlign_Center);
		}

		// SizeBox — limita el ancho para que el texto no invada el quesito adyacente
		USizeBox* SizeBoxWidget = NewObject<USizeBox>(this);
		SizeBoxWidget->SetWidthOverride(SlotMaxWidth);
		SizeBoxWidget->AddChild(SlotBox);

		// Border — fondo del quesito; actualizado en SetSelectedIndexInternal
		UBorder* BorderWidget = NewObject<UBorder>(this);
		BorderWidget->SetBrushColor(SliceNormalColor);
		BorderWidget->SetPadding(FMargin(6.f, 4.f));
		if (UBorderSlot* BS = Cast<UBorderSlot>(BorderWidget->AddChild(SizeBoxWidget)))
		{
			BS->SetHorizontalAlignment(HAlign_Center);
			BS->SetVerticalAlignment(VAlign_Center);
		}
		SlotBorders.Add(BorderWidget);

		// ── Add to canvas centered on computed position ───────────────────────
		if (UCanvasPanelSlot* CanvasSlot = RadialSlotsContainer->AddChildToCanvas(BorderWidget))
		{
			CanvasSlot->SetAutoSize(true);
			CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			CanvasSlot->SetPosition(FVector2D(PosX, PosY));
		}
	}
}

int32 UTN_RadialWheelWidgetBase::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 N = Entries.Num();
	if (N > 0)
	{
		const FVector2D Center  = RadialCenter;
		const float OffsetRad   = FMath::DegreesToRadians(SelectionAngleOffsetDegrees);
		const float AngleStep   = (2.f * PI) / static_cast<float>(N);

		// ── Círculo exterior ──────────────────────────────────────────────────
		constexpr int32 CircleSegs = 64;
		TArray<FVector2D> CirclePts;
		CirclePts.Reserve(CircleSegs + 1);
		for (int32 k = 0; k <= CircleSegs; ++k)
		{
			const float A = (2.f * PI * k) / CircleSegs;
			CirclePts.Add(Center + FVector2D(FMath::Cos(A) * WheelRadius, -FMath::Sin(A) * WheelRadius));
		}
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId,
			AllottedGeometry.ToPaintGeometry(), CirclePts,
			ESlateDrawEffect::None, DividerColor, true, DividerThickness);

		// ── Líneas divisorias entre quesitos ─────────────────────────────────
		for (int32 i = 0; i < N; ++i)
		{
			// Línea en el límite entre slice i-1 y slice i
			const float DivAngle = OffsetRad + AngleStep * static_cast<float>(i) - AngleStep * 0.5f;
			TArray<FVector2D> DivLine;
			DivLine.Add(Center);
			DivLine.Add(Center + FVector2D(FMath::Cos(DivAngle) * WheelRadius, -FMath::Sin(DivAngle) * WheelRadius));
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId,
				AllottedGeometry.ToPaintGeometry(), DivLine,
				ESlateDrawEffect::None, DividerColor, true, DividerThickness);
		}
	}

	// Dibujar hijos encima
	return Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements,
		LayerId + 1, InWidgetStyle, bParentEnabled);
}

void UTN_RadialWheelWidgetBase::SetEntries(const TArray<FTN_RadialWheelEntryView>& InEntries)
{
	Entries = InEntries;
	SelectedIndex = INDEX_NONE;
	CurrentInputVector = FVector2D::ZeroVector;

	BP_OnEntriesSet(Entries);
	BP_OnSelectionCleared();
	OnSelectionCleared.Broadcast();
}

void UTN_RadialWheelWidgetBase::UpdateInputVector(FVector2D InVector)
{
	CurrentInputVector = InVector.GetClampedToMaxSize(1.f);

	if (Entries.Num() == 0)
	{
		SetSelectedIndexInternal(INDEX_NONE);
		return;
	}

	if (CurrentInputVector.SizeSquared() < FMath::Square(SelectionDeadzone))
	{
		SetSelectedIndexInternal(INDEX_NONE);
		return;
	}

	float Angle = FMath::Atan2(CurrentInputVector.Y, CurrentInputVector.X);
	Angle -= FMath::DegreesToRadians(SelectionAngleOffsetDegrees);

	// Añadir medio paso para que el slice i quede centrado en su sector angular
	// (sin esto, la mitad izquierda del slice 0 seleccionaría el slice N-1)
	const float SliceAngle = (2.f * PI) / static_cast<float>(Entries.Num());
	Angle += SliceAngle * 0.5f;

	while (Angle < 0.f)
	{
		Angle += 2.f * PI;
	}
	while (Angle >= 2.f * PI)
	{
		Angle -= 2.f * PI;
	}

	const int32 NewIndex = FMath::Clamp(FMath::FloorToInt(Angle / SliceAngle), 0, Entries.Num() - 1);
	SetSelectedIndexInternal(NewIndex);
}

void UTN_RadialWheelWidgetBase::ClearSelection()
{
	SetSelectedIndexInternal(INDEX_NONE);
	CurrentInputVector = FVector2D::ZeroVector;
}

bool UTN_RadialWheelWidgetBase::TryConfirmSelection(FTN_RadialWheelEntryView& OutEntry)
{
	if (!HasValidSelection())
	{
		return false;
	}

	OutEntry = Entries[SelectedIndex];
	OnSelectionConfirmed.Broadcast(SelectedIndex, OutEntry.EntryId, OutEntry.Label);
	BP_OnSelectionConfirmed(SelectedIndex, OutEntry.EntryId, OutEntry.Label);
	return true;
}

bool UTN_RadialWheelWidgetBase::HasValidSelection() const
{
	return Entries.IsValidIndex(SelectedIndex) && Entries[SelectedIndex].bEnabled;
}

void UTN_RadialWheelWidgetBase::SetSelectedIndexInternal(int32 NewIndex)
{
	if (SelectedIndex == NewIndex)
	{
		return;
	}

	SelectedIndex = NewIndex;

	// Actualizar color de fondo de todos los borders
	for (int32 Idx = 0; Idx < SlotBorders.Num(); ++Idx)
	{
		if (SlotBorders[Idx])
		{
			SlotBorders[Idx]->SetBrushColor(Idx == SelectedIndex ? SliceSelectedColor : SliceNormalColor);
		}
	}

	if (!Entries.IsValidIndex(SelectedIndex))
	{
		OnSelectionCleared.Broadcast();
		BP_OnSelectionCleared();
		return;
	}

	const FTN_RadialWheelEntryView& Entry = Entries[SelectedIndex];
	OnSelectionChanged.Broadcast(SelectedIndex, Entry.EntryId, Entry.Label);
	BP_OnSelectionChanged(SelectedIndex, Entry.EntryId, Entry.Label);
}

