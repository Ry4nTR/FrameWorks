// CoreFramework Save System - Library
#include "SaveSystem/Library/SaveFunctionLibrary.h"

#include "SaveSystem/Manager/SaveSubsystem.h"

#define LOCTEXT_NAMESPACE "CoreFrameworkSave"

USaveSubsystem* USaveFunctionLibrary::GetSaveSubsystem(const UObject* WorldContextObject)
{
	return USaveSubsystem::Get(WorldContextObject);
}

ESaveResult USaveFunctionLibrary::StartNewGame(const UObject* WorldContextObject, const FString& SlotName)
{
	USaveSubsystem* Subsystem = USaveSubsystem::Get(WorldContextObject);
	return Subsystem ? Subsystem->StartNewGame(SlotName) : ESaveResult::FileError;
}

void USaveFunctionLibrary::SaveToSlot(const UObject* WorldContextObject, const FString& SlotName, FSaveOperationDelegate OnComplete, const FString& DisplayName)
{
	if (USaveSubsystem* Subsystem = USaveSubsystem::Get(WorldContextObject))
	{
		Subsystem->SaveToSlot(SlotName, DisplayName, OnComplete);
	}
}

void USaveFunctionLibrary::QuickSave(const UObject* WorldContextObject, FSaveOperationDelegate OnComplete)
{
	if (USaveSubsystem* Subsystem = USaveSubsystem::Get(WorldContextObject))
	{
		Subsystem->QuickSave(OnComplete);
	}
}

void USaveFunctionLibrary::AutoSave(const UObject* WorldContextObject, FSaveOperationDelegate OnComplete)
{
	if (USaveSubsystem* Subsystem = USaveSubsystem::Get(WorldContextObject))
	{
		Subsystem->AutoSave(OnComplete);
	}
}

void USaveFunctionLibrary::LoadSlot(const UObject* WorldContextObject, const FString& SlotName, FSaveOperationDelegate OnComplete)
{
	if (USaveSubsystem* Subsystem = USaveSubsystem::Get(WorldContextObject))
	{
		Subsystem->LoadSlot(SlotName, OnComplete);
	}
}

void USaveFunctionLibrary::DeleteSlot(const UObject* WorldContextObject, const FString& SlotName, FSaveOperationDelegate OnComplete)
{
	if (USaveSubsystem* Subsystem = USaveSubsystem::Get(WorldContextObject))
	{
		Subsystem->DeleteSlot(SlotName, OnComplete);
	}
}

void USaveFunctionLibrary::ListSlots(const UObject* WorldContextObject, FSaveOperationDelegate OnComplete)
{
	if (USaveSubsystem* Subsystem = USaveSubsystem::Get(WorldContextObject))
	{
		Subsystem->ListSlots(OnComplete);
	}
}

FString USaveFunctionLibrary::MakeUniqueSlotName(const UObject* WorldContextObject, const FString& Prefix)
{
	const USaveSubsystem* Subsystem = USaveSubsystem::Get(WorldContextObject);
	const FString SafePrefix = USaveSubsystem::IsValidSlotName(Prefix) ? Prefix : FString(TEXT("Slot"));

	for (int32 Number = 1; Number < 10000; ++Number)
	{
		const FString Candidate = FString::Printf(TEXT("%s%d"), *SafePrefix, Number);
		if (!Subsystem || !Subsystem->DoesSlotExist(Candidate))
		{
			return Candidate;
		}
	}
	return SafePrefix;
}

bool USaveFunctionLibrary::IsValidSlotName(const FString& SlotName)
{
	return USaveSubsystem::IsValidSlotName(SlotName);
}

void USaveFunctionLibrary::SaveProfile(const UObject* WorldContextObject, FSaveOperationDelegate OnComplete, int32 LocalPlayerIndex)
{
	if (USaveSubsystem* Subsystem = USaveSubsystem::Get(WorldContextObject))
	{
		Subsystem->SaveProfile(LocalPlayerIndex, OnComplete);
	}
}

void USaveFunctionLibrary::LoadProfile(const UObject* WorldContextObject, FSaveOperationDelegate OnComplete, int32 LocalPlayerIndex)
{
	if (USaveSubsystem* Subsystem = USaveSubsystem::Get(WorldContextObject))
	{
		Subsystem->LoadProfile(LocalPlayerIndex, OnComplete);
	}
}

void USaveFunctionLibrary::SetProfileFloat(const UObject* WorldContextObject, FName Key, float Value, int32 LocalPlayerIndex)
{
	if (USaveSubsystem* Subsystem = USaveSubsystem::Get(WorldContextObject))
	{
		Subsystem->SetProfileFloat(Key, Value, LocalPlayerIndex);
	}
}

float USaveFunctionLibrary::GetProfileFloat(const UObject* WorldContextObject, FName Key, float Default, int32 LocalPlayerIndex)
{
	const USaveSubsystem* Subsystem = USaveSubsystem::Get(WorldContextObject);
	return Subsystem ? Subsystem->GetProfileFloat(Key, Default, LocalPlayerIndex) : Default;
}

void USaveFunctionLibrary::SetProfileInt(const UObject* WorldContextObject, FName Key, int32 Value, int32 LocalPlayerIndex)
{
	if (USaveSubsystem* Subsystem = USaveSubsystem::Get(WorldContextObject))
	{
		Subsystem->SetProfileInt(Key, Value, LocalPlayerIndex);
	}
}

int32 USaveFunctionLibrary::GetProfileInt(const UObject* WorldContextObject, FName Key, int32 Default, int32 LocalPlayerIndex)
{
	const USaveSubsystem* Subsystem = USaveSubsystem::Get(WorldContextObject);
	return Subsystem ? Subsystem->GetProfileInt(Key, Default, LocalPlayerIndex) : Default;
}

void USaveFunctionLibrary::SetProfileBool(const UObject* WorldContextObject, FName Key, bool bValue, int32 LocalPlayerIndex)
{
	if (USaveSubsystem* Subsystem = USaveSubsystem::Get(WorldContextObject))
	{
		Subsystem->SetProfileBool(Key, bValue, LocalPlayerIndex);
	}
}

bool USaveFunctionLibrary::GetProfileBool(const UObject* WorldContextObject, FName Key, bool bDefault, int32 LocalPlayerIndex)
{
	const USaveSubsystem* Subsystem = USaveSubsystem::Get(WorldContextObject);
	return Subsystem ? Subsystem->GetProfileBool(Key, bDefault, LocalPlayerIndex) : bDefault;
}

void USaveFunctionLibrary::RegisterSaveable(const UObject* WorldContextObject, UObject* Saveable)
{
	if (USaveSubsystem* Subsystem = USaveSubsystem::Get(WorldContextObject))
	{
		Subsystem->RegisterSaveable(Saveable);
	}
}

void USaveFunctionLibrary::UnregisterSaveable(const UObject* WorldContextObject, UObject* Saveable, bool bCaptureState)
{
	if (USaveSubsystem* Subsystem = USaveSubsystem::Get(WorldContextObject))
	{
		Subsystem->UnregisterSaveable(Saveable, bCaptureState);
	}
}

FText USaveFunctionLibrary::ResultToText(ESaveResult Result)
{
	switch (Result)
	{
	case ESaveResult::Success:
		return FText::GetEmpty();
	case ESaveResult::InvalidSlotName:
		return LOCTEXT("InvalidSlotName", "The slot name is not valid.");
	case ESaveResult::NotAuthority:
		return LOCTEXT("NotAuthority", "Only the host can save or load the game.");
	case ESaveResult::NoLocalPlayer:
		return LOCTEXT("NoLocalPlayer", "There is no local player to save for.");
	case ESaveResult::SlotNotFound:
		return LOCTEXT("SlotNotFound", "That save does not exist.");
	case ESaveResult::FileError:
		return LOCTEXT("FileError", "The save file could not be read or written.");
	case ESaveResult::VersionTooNew:
		return LOCTEXT("VersionTooNew", "This save was made with a newer version of the game.");
	case ESaveResult::MigrationFailed:
		return LOCTEXT("MigrationFailed", "This save is from an old version and could not be upgraded.");
	case ESaveResult::TooManySlots:
		return LOCTEXT("TooManySlots", "There are too many saves. Delete one first.");
	case ESaveResult::Blocked:
		return LOCTEXT("Blocked", "The game cannot be saved right now.");
	}
	return FText::GetEmpty();
}

FText USaveFunctionLibrary::FormatPlaytime(double Seconds)
{
	const int64 Total = FMath::Max<int64>(0, static_cast<int64>(Seconds));
	const int64 Hours = Total / 3600;
	const int64 Minutes = (Total / 60) % 60;
	const int64 Secs = Total % 60;
	return FText::FromString(FString::Printf(TEXT("%lld:%02lld:%02lld"), Hours, Minutes, Secs));
}

FText USaveFunctionLibrary::FormatSavedAt(const FDateTime& SavedAt)
{
	return FText::AsDateTime(SavedAt);
}

#undef LOCTEXT_NAMESPACE
