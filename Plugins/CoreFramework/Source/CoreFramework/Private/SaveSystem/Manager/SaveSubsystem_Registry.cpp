// CoreFramework Save System
// USaveSubsystem: the saveable registry (capture / restore of objects) and the profile value store.

#include "SaveSystem/Manager/SaveSubsystem.h"

#include "Engine/GameInstance.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "SaveSystem/Core/SaveSystemSettings.h"
#include "SaveSystem/Data/SaveSlotData.h"
#include "SaveSystem/Interface/SaveableInterface.h"
#include "../Core/SaveSerialization.h"

// ---------------------------------------------------------------------------------------------------------------------
// Data containers
// ---------------------------------------------------------------------------------------------------------------------

USaveSlotData* USaveSubsystem::NewData() const
{
	USaveSlotData* Data = NewObject<USaveSlotData>(const_cast<USaveSubsystem*>(this));
	Data->GameVersion = USaveSystemSettings::Get()->CurrentSaveVersion;
	return Data;
}

USaveSlotData* USaveSubsystem::GetData(ESaveScope Scope, int32 LocalPlayerIndex, bool bCreate)
{
	if (Scope == ESaveScope::World)
	{
		if (!ActiveWorldData && bCreate)
		{
			ActiveWorldData = NewData();
		}
		return ActiveWorldData;
	}

	if (TObjectPtr<USaveSlotData>* Found = ProfileData.Find(LocalPlayerIndex))
	{
		return Found->Get();
	}
	if (!bCreate)
	{
		return nullptr;
	}

	USaveSlotData* Created = NewData();
	ProfileData.Add(LocalPlayerIndex, Created);
	return Created;
}

// ---------------------------------------------------------------------------------------------------------------------
// Registry
// ---------------------------------------------------------------------------------------------------------------------

FString USaveSubsystem::MakeRegistryKey(const FSaveableDescriptor& Descriptor)
{
	// The profile index is part of the key so that two local players can own an object with the same key.
	const int32 PlayerIndex = Descriptor.Scope == ESaveScope::Profile ? Descriptor.LocalPlayerIndex : 0;
	return FString::Printf(TEXT("%d|%d|%s"), static_cast<int32>(Descriptor.Scope), PlayerIndex, *Descriptor.Key);
}

bool USaveSubsystem::QueryDescriptor(UObject* Object, FSaveableDescriptor& OutDescriptor)
{
	if (!Object || !Object->GetClass()->ImplementsInterface(USaveableInterface::StaticClass()))
	{
		return false;
	}

	OutDescriptor = FSaveableDescriptor();
	ISaveableInterface::Execute_GetSaveDescriptor(Object, OutDescriptor);
	OutDescriptor.Version = FMath::Max(1, OutDescriptor.Version);
	return !OutDescriptor.Key.IsEmpty();
}

bool USaveSubsystem::CanHandleScope(ESaveScope Scope, int32 LocalPlayerIndex) const
{
	if (Scope == ESaveScope::World)
	{
		return IsWorldAuthority();
	}
	return GetLocalPlayerForProfile(LocalPlayerIndex) != nullptr;
}

void USaveSubsystem::RegisterSaveable(UObject* Saveable)
{
	FSaveableDescriptor Descriptor;
	if (!QueryDescriptor(Saveable, Descriptor))
	{
		UE_LOG(LogCoreSave, Warning, TEXT("RegisterSaveable: %s does not implement ISaveableInterface, or its Save Key is empty."), *GetNameSafe(Saveable));
		return;
	}

	const FString RegistryKey = MakeRegistryKey(Descriptor);
	{
		FRegisteredSaveable& Slot = Registry.FindOrAdd(RegistryKey);
		const UObject* Previous = Slot.Object.Get();
		if (Previous && Previous != Saveable)
		{
			UE_LOG(LogCoreSave, Warning, TEXT("Two saveables use the Save Key '%s' (%s and %s). The newest one wins; give them different keys."),
				*Descriptor.Key, *GetNameSafe(Previous), *GetNameSafe(Saveable));
		}
		Slot.Object = Saveable;
		Slot.Descriptor = Descriptor;
	}

	// "Registering = restoring": if the active slot already has data for this key, apply it now.
	if (!CanHandleScope(Descriptor.Scope, Descriptor.LocalPlayerIndex))
	{
		return;
	}
	if (const USaveSlotData* Data = GetData(Descriptor.Scope, Descriptor.LocalPlayerIndex, /*bCreate=*/false))
	{
		if (const FSaveEntry* Entry = Data->Entries.Find(Descriptor.Key))
		{
			const FSaveEntry EntryCopy = *Entry; // Restoring runs game code that may register or capture other objects.
			RestoreObject(Saveable, EntryCopy);
		}
	}
}

void USaveSubsystem::UnregisterSaveable(UObject* Saveable, bool bCaptureState)
{
	FSaveableDescriptor Descriptor;
	if (!QueryDescriptor(Saveable, Descriptor))
	{
		return;
	}

	const FString RegistryKey = MakeRegistryKey(Descriptor);
	const FRegisteredSaveable* Slot = Registry.Find(RegistryKey);
	if (!Slot || Slot->Object.Get() != Saveable)
	{
		return; // Replaced by a newer object with the same key, or never registered.
	}

	// Hand the state back to the active data: it must survive the object (level change, destroy) until the next save.
	if (bCaptureState && CanHandleScope(Descriptor.Scope, Descriptor.LocalPlayerIndex))
	{
		const bool bCanStore = Descriptor.Scope == ESaveScope::World || LoadedProfiles.Contains(Descriptor.LocalPlayerIndex);
		if (bCanStore)
		{
			if (USaveSlotData* Data = GetData(Descriptor.Scope, Descriptor.LocalPlayerIndex, /*bCreate=*/true))
			{
				CaptureObject(Saveable, Descriptor, Data);
			}
		}
	}

	Registry.Remove(RegistryKey);
}

// ---------------------------------------------------------------------------------------------------------------------
// Capture / restore
// ---------------------------------------------------------------------------------------------------------------------

void USaveSubsystem::CaptureObject(UObject* Object, const FSaveableDescriptor& Descriptor, USaveSlotData* Into)
{
	ISaveableInterface::Execute_OnBeforeSave(Object);

	FSaveEntry Entry;
	Entry.Version = Descriptor.Version;

	bool bHandled = false;
	if (ISaveableInterface* Native = Cast<ISaveableInterface>(Object))
	{
		FMemoryWriter Writer(Entry.Bytes, /*bIsPersistent=*/true);
		bHandled = Native->SerializeSaveData(Writer);
		if (!bHandled)
		{
			Entry.Bytes.Reset();
		}
	}
	if (!bHandled)
	{
		SaveSerialization::WriteObject(Object, Entry.Bytes);
	}

	Into->Entries.Add(Descriptor.Key, MoveTemp(Entry));
}

void USaveSubsystem::RestoreObject(UObject* Object, const FSaveEntry& Entry)
{
	bool bHandled = false;
	if (ISaveableInterface* Native = Cast<ISaveableInterface>(Object))
	{
		if (Entry.Bytes.Num() > 0)
		{
			FMemoryReader Reader(Entry.Bytes, /*bIsPersistent=*/true);
			bHandled = Native->SerializeSaveData(Reader);
		}
	}
	if (!bHandled)
	{
		SaveSerialization::ReadObject(Object, Entry.Bytes);
	}

	ISaveableInterface::Execute_OnAfterLoad(Object, Entry.Version);
}

void USaveSubsystem::CaptureScope(ESaveScope Scope, int32 LocalPlayerIndex, USaveSlotData* Into)
{
	TArray<FString> Stale;
	for (const TPair<FString, FRegisteredSaveable>& Pair : Registry)
	{
		const FRegisteredSaveable& Saveable = Pair.Value;
		UObject* Object = Saveable.Object.Get();
		if (!Object)
		{
			Stale.Add(Pair.Key);
			continue;
		}
		if (Saveable.Descriptor.Scope != Scope)
		{
			continue;
		}
		if (Scope == ESaveScope::Profile && Saveable.Descriptor.LocalPlayerIndex != LocalPlayerIndex)
		{
			continue;
		}
		CaptureObject(Object, Saveable.Descriptor, Into);
	}

	for (const FString& Key : Stale)
	{
		Registry.Remove(Key);
	}
}

void USaveSubsystem::ApplyScope(ESaveScope Scope, int32 LocalPlayerIndex, USaveSlotData* From)
{
	if (!From || !CanHandleScope(Scope, LocalPlayerIndex))
	{
		return;
	}

	// Snapshot: restoring runs game code that may register or unregister objects.
	TArray<FRegisteredSaveable> Snapshot;
	Registry.GenerateValueArray(Snapshot);

	for (const FRegisteredSaveable& Saveable : Snapshot)
	{
		UObject* Object = Saveable.Object.Get();
		if (!Object || Saveable.Descriptor.Scope != Scope)
		{
			continue;
		}
		if (Scope == ESaveScope::Profile && Saveable.Descriptor.LocalPlayerIndex != LocalPlayerIndex)
		{
			continue;
		}

		if (const FSaveEntry* Entry = From->Entries.Find(Saveable.Descriptor.Key))
		{
			const FSaveEntry EntryCopy = *Entry;
			RestoreObject(Object, EntryCopy);
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Profile values
// ---------------------------------------------------------------------------------------------------------------------

void USaveSubsystem::SetProfileValueInternal(FName Key, const FString& Value, int32 LocalPlayerIndex)
{
	if (Key.IsNone())
	{
		return;
	}

	if (USaveSlotData* Data = GetData(ESaveScope::Profile, LocalPlayerIndex, /*bCreate=*/true))
	{
		Data->SetValue(Key, Value);
		OnProfileValueChanged.Broadcast(Key);
	}
}

const FString* USaveSubsystem::FindProfileValue(FName Key, int32 LocalPlayerIndex) const
{
	const TObjectPtr<USaveSlotData>* Data = ProfileData.Find(LocalPlayerIndex);
	return (Data && *Data) ? (*Data)->Values.Find(Key) : nullptr;
}

void USaveSubsystem::SetProfileFloat(FName Key, float Value, int32 LocalPlayerIndex)
{
	SetProfileValueInternal(Key, FString::SanitizeFloat(Value), LocalPlayerIndex);
}

void USaveSubsystem::SetProfileInt(FName Key, int32 Value, int32 LocalPlayerIndex)
{
	SetProfileValueInternal(Key, FString::FromInt(Value), LocalPlayerIndex);
}

void USaveSubsystem::SetProfileBool(FName Key, bool bValue, int32 LocalPlayerIndex)
{
	SetProfileValueInternal(Key, bValue ? TEXT("1") : TEXT("0"), LocalPlayerIndex);
}

void USaveSubsystem::SetProfileString(FName Key, const FString& Value, int32 LocalPlayerIndex)
{
	SetProfileValueInternal(Key, Value, LocalPlayerIndex);
}

float USaveSubsystem::GetProfileFloat(FName Key, float Default, int32 LocalPlayerIndex) const
{
	const FString* Value = FindProfileValue(Key, LocalPlayerIndex);
	return Value ? FCString::Atof(**Value) : Default;
}

int32 USaveSubsystem::GetProfileInt(FName Key, int32 Default, int32 LocalPlayerIndex) const
{
	const FString* Value = FindProfileValue(Key, LocalPlayerIndex);
	return Value ? FCString::Atoi(**Value) : Default;
}

bool USaveSubsystem::GetProfileBool(FName Key, bool bDefault, int32 LocalPlayerIndex) const
{
	const FString* Value = FindProfileValue(Key, LocalPlayerIndex);
	return Value ? Value->ToBool() : bDefault;
}

FString USaveSubsystem::GetProfileString(FName Key, const FString& Default, int32 LocalPlayerIndex) const
{
	const FString* Value = FindProfileValue(Key, LocalPlayerIndex);
	return Value ? *Value : Default;
}

bool USaveSubsystem::HasProfileValue(FName Key, int32 LocalPlayerIndex) const
{
	return FindProfileValue(Key, LocalPlayerIndex) != nullptr;
}
