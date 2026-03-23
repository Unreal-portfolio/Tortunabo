#include "UI/HUD/TN_EmoteWheelDataAsset.h"

const FTN_EmoteWheelEntry* UTN_EmoteWheelDataAsset::FindEntryById(uint8 EmoteID) const
{
	for (const FTN_EmoteWheelEntry& Entry : Entries)
	{
		if (Entry.EmoteID == EmoteID)
		{
			return &Entry;
		}
	}

	return nullptr;
}

TArray<FTN_RadialWheelEntryView> UTN_EmoteWheelDataAsset::BuildWheelEntries() const
{
	TArray<FTN_RadialWheelEntryView> Result;
	Result.Reserve(Entries.Num());

	for (const FTN_EmoteWheelEntry& Entry : Entries)
	{
		FTN_RadialWheelEntryView View;
		View.EntryId = Entry.EmoteID;
		View.Label = Entry.Name;
		View.Icon = Entry.Icon;
		View.bEnabled = true;
		Result.Add(View);
	}

	return Result;
}

#if WITH_EDITOR
EDataValidationResult UTN_EmoteWheelDataAsset::IsDataValid(FDataValidationContext& Context) const
{
	TSet<uint8> UsedIds;
	bool bHasErrors = false;

	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		const FTN_EmoteWheelEntry& Entry = Entries[Index];

		if (UsedIds.Contains(Entry.EmoteID))
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("EmoteID duplicado: %d (index %d)"), Entry.EmoteID, Index)));
			bHasErrors = true;
		}
		UsedIds.Add(Entry.EmoteID);

		if (Entry.Name.IsEmpty())
		{
			Context.AddWarning(FText::FromString(FString::Printf(TEXT("EmoteID %d no tiene Name"), Entry.EmoteID)));
		}
	}

	return bHasErrors ? EDataValidationResult::Invalid : EDataValidationResult::Valid;
}
#endif

