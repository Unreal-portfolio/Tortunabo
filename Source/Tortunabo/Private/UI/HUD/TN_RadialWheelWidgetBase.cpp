#include "UI/HUD/TN_RadialWheelWidgetBase.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

// ─────────────────────────────────────────────────────────────────────────────
// C++ default implementation — draws labels + icons in RadialSlotsContainer.
// Blueprint children may override this event for full custom visuals; if they
// do, the C++ version is NOT called (BlueprintNativeEvent semantics).
// ─────────────────────────────────────────────────────────────────────────────

void UTN_RadialWheelWidgetBase::BP_OnEntriesSet_Implementation(const TArray<FTN_RadialWheelEntryView>& InEntries)
{
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

		// ── Build slot widget: [Icon?] + Label ───────────────────────────────
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

		// ── Add to canvas centered on computed position ───────────────────────
		if (UCanvasPanelSlot* CanvasSlot = RadialSlotsContainer->AddChildToCanvas(SlotBox))
		{
			CanvasSlot->SetAutoSize(true);
			CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			CanvasSlot->SetPosition(FVector2D(PosX, PosY));
		}
	}
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
	while (Angle < 0.f)
	{
		Angle += 2.f * PI;
	}
	while (Angle >= 2.f * PI)
	{
		Angle -= 2.f * PI;
	}

	const float SliceAngle = (2.f * PI) / static_cast<float>(Entries.Num());
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

