// CoreFramework Save System - Data
#include "SaveSystem/Data/SaveSlotData.h"

bool USaveSlotData::RenameEntry(const FString& OldKey, const FString& NewKey)
{
	if (OldKey == NewKey || !Entries.Contains(OldKey) || Entries.Contains(NewKey))
	{
		return false;
	}

	FSaveEntry Moved;
	Entries.RemoveAndCopyValue(OldKey, Moved);
	Entries.Add(NewKey, MoveTemp(Moved));
	return true;
}

int32 USaveSlotData::GetEntryVersion(const FString& Key) const
{
	const FSaveEntry* Entry = Entries.Find(Key);
	return Entry ? Entry->Version : 0;
}

TArray<FString> USaveSlotData::GetEntryKeys() const
{
	TArray<FString> Keys;
	Entries.GetKeys(Keys);
	return Keys;
}

FString USaveSlotData::GetValue(FName Key, const FString& Default) const
{
	const FString* Found = Values.Find(Key);
	return Found ? *Found : Default;
}

FSaveSlotInfo* USaveSlotIndex::Find(const FString& SlotName)
{
	return Slots.FindByPredicate([&SlotName](const FSaveSlotInfo& Info) { return Info.SlotName == SlotName; });
}

void USaveSlotIndex::AddOrReplace(const FSaveSlotInfo& Info)
{
	if (FSaveSlotInfo* Existing = Find(Info.SlotName))
	{
		*Existing = Info;
	}
	else
	{
		Slots.Add(Info);
	}
}

bool USaveSlotIndex::Remove(const FString& SlotName)
{
	return Slots.RemoveAll([&SlotName](const FSaveSlotInfo& Info) { return Info.SlotName == SlotName; }) > 0;
}
