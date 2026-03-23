#include "UI/HUD/TN_QuickChatWheelDataAsset.h"

const FTN_QuickChatWheelEntry* UTN_QuickChatWheelDataAsset::FindEntryById(uint8 MessageID) const
{
	for (const FTN_QuickChatWheelEntry& Entry : Entries)
	{
		if (Entry.MessageID == MessageID)
		{
			return &Entry;
		}
	}

	return nullptr;
}

TArray<FTN_RadialWheelEntryView> UTN_QuickChatWheelDataAsset::BuildWheelEntries() const
{
	TArray<FTN_RadialWheelEntryView> Result;
	Result.Reserve(Entries.Num());

	for (const FTN_QuickChatWheelEntry& Entry : Entries)
	{
		FTN_RadialWheelEntryView View;
		View.EntryId = Entry.MessageID;
		View.Label = Entry.Text;
		View.Icon = Entry.Icon;
		View.bEnabled = true;
		Result.Add(View);
	}

	return Result;
}

#if WITH_EDITOR
EDataValidationResult UTN_QuickChatWheelDataAsset::IsDataValid(FDataValidationContext& Context) const
{
	TSet<uint8> UsedIds;
	bool bHasErrors = false;

	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		const FTN_QuickChatWheelEntry& Entry = Entries[Index];

		if (UsedIds.Contains(Entry.MessageID))
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("MessageID duplicado: %d (index %d)"), Entry.MessageID, Index)));
			bHasErrors = true;
		}
		UsedIds.Add(Entry.MessageID);

		if (Entry.Text.IsEmpty())
		{
			Context.AddWarning(FText::FromString(FString::Printf(TEXT("MessageID %d no tiene Text"), Entry.MessageID)));
		}
	}

	return bHasErrors ? EDataValidationResult::Invalid : EDataValidationResult::Valid;
}
#endif

