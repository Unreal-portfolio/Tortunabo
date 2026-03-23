#include "UI/HUD/TN_RadialWheelWidgetBase.h"

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

