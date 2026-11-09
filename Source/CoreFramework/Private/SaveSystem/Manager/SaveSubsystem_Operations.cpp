// CoreFramework Save System
// USaveSubsystem: asynchronous operations (World slots, profiles), the slot index and versioning.
// The queue, the public entry points and the lifecycle are in SaveSubsystem.cpp; the registry in SaveSubsystem_Registry.cpp.

#include "SaveSystem/Manager/SaveSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "SaveSystem/Core/SaveSystemSettings.h"
#include "SaveSystem/Data/SaveHooks.h"
#include "SaveSystem/Data/SaveMigrationStep.h"
#include "SaveSystem/Data/SaveSlotData.h"

// ---------------------------------------------------------------------------------------------------------------------
// Slot index
// ---------------------------------------------------------------------------------------------------------------------

void USaveSubsystem::EnsureIndexLoaded(TFunction<void()> Then)
{
	if (SlotIndex)
	{
		Then();
		return;
	}

	const int32 UserIndex = USaveSystemSettings::Get()->WorldSaveUserIndex;
	if (!UGameplayStatics::DoesSaveGameExist(IndexFileName(), UserIndex))
	{
		SlotIndex = NewObject<USaveSlotIndex>(this);
		Then();
		return;
	}

	UGameplayStatics::AsyncLoadGameFromSlot(IndexFileName(), UserIndex,
		FAsyncLoadGameFromSlotDelegate::CreateWeakLambda(this, [this, Then](const FString&, const int32, USaveGame* Loaded)
		{
			USaveSlotIndex* LoadedIndex = Cast<USaveSlotIndex>(Loaded);
			if (!LoadedIndex || LoadedIndex->FormatVersion > CoreSave::FormatVersion)
			{
				UE_LOG(LogCoreSave, Warning, TEXT("The slot index could not be read. Starting with an empty list; slots appear again as they are saved."));
				LoadedIndex = NewObject<USaveSlotIndex>(this);
			}
			SlotIndex = LoadedIndex;
			Then();
		}));
}

void USaveSubsystem::SaveIndex(TFunction<void(bool)> Then)
{
	if (!SlotIndex)
	{
		Then(false);
		return;
	}

	UGameplayStatics::AsyncSaveGameToSlot(SlotIndex, IndexFileName(), USaveSystemSettings::Get()->WorldSaveUserIndex,
		FAsyncSaveGameToSlotDelegate::CreateWeakLambda(this, [Then](const FString&, const int32, bool bSuccess)
		{
			Then(bSuccess);
		}));
}

void USaveSubsystem::RefreshCache()
{
	CachedSlots = SlotIndex ? SlotIndex->Slots : TArray<FSaveSlotInfo>();
	CachedSlots.Sort([](const FSaveSlotInfo& A, const FSaveSlotInfo& B) { return A.SavedAt > B.SavedAt; });
	OnSlotListChanged.Broadcast();
}

// ---------------------------------------------------------------------------------------------------------------------
// World slot operations
// ---------------------------------------------------------------------------------------------------------------------

FSaveSlotInfo USaveSubsystem::BuildInfo(const FString& SlotName, const FString& DisplayName, const FSaveSlotInfo* Existing) const
{
	const USaveSystemSettings* Settings = USaveSystemSettings::Get();

	FSaveSlotInfo Info;
	Info.SlotName = SlotName;
	if (!DisplayName.IsEmpty())
	{
		Info.DisplayName = DisplayName;
	}
	else if (Existing && !Existing->DisplayName.IsEmpty())
	{
		Info.DisplayName = Existing->DisplayName;
	}
	else
	{
		Info.DisplayName = SlotName;
	}
	Info.SavedAt = FDateTime::Now();
	Info.PlaytimeSeconds = ActivePlaytime;
	Info.MapName = UGameplayStatics::GetCurrentLevelName(this, /*bRemovePrefixString=*/true);
	Info.GameVersion = Settings->CurrentSaveVersion;
	Info.bIsAutosave = (SlotName == Settings->AutosaveSlotName);
	Info.CustomData = SlotCustomData;

	if (Hooks)
	{
		Info = Hooks->BuildSlotInfo(Info);
		Info.SlotName = SlotName; // The game may relabel a slot, never rename it.
	}
	return Info;
}

void USaveSubsystem::ExecuteSave(const FOperation& Operation)
{
	if (!IsValidSlotName(Operation.SlotName))
	{
		FinishOperation(Operation, ESaveResult::InvalidSlotName);
		return;
	}
	if (!IsWorldAuthority())
	{
		FinishOperation(Operation, ESaveResult::NotAuthority);
		return;
	}
	if (Hooks && !Hooks->CanSave(Operation.SlotName))
	{
		FinishOperation(Operation, ESaveResult::Blocked);
		return;
	}

	EnsureIndexLoaded([this, Operation]()
	{
		const USaveSystemSettings* Settings = USaveSystemSettings::Get();

		const FSaveSlotInfo* Existing = SlotIndex->Find(Operation.SlotName);
		if (!Existing && Settings->MaxSlots > 0 && SlotIndex->Slots.Num() >= Settings->MaxSlots)
		{
			FinishOperation(Operation, ESaveResult::TooManySlots);
			return;
		}
		const FSaveSlotInfo ExistingCopy = Existing ? *Existing : FSaveSlotInfo();

		if (Hooks)
		{
			Hooks->OnBeforeSave(ESaveScope::World, Operation.SlotName);
		}

		// Everything that is alive is captured now, on the game thread. Objects that are not registered (other
		// levels, destroyed actors) keep the entries the active data already holds.
		USaveSlotData* Data = GetData(ESaveScope::World, 0, /*bCreate=*/true);
		CaptureScope(ESaveScope::World, 0, Data);

		const FSaveSlotInfo Info = BuildInfo(Operation.SlotName, Operation.DisplayName, Existing ? &ExistingCopy : nullptr);
		Data->Info = Info;
		Data->GameVersion = Settings->CurrentSaveVersion;
		Data->FormatVersion = CoreSave::FormatVersion;

		// The engine serializes the object right here; only the file write is asynchronous. Payload first, then the
		// index, so a crash never lists a slot that has no file.
		UGameplayStatics::AsyncSaveGameToSlot(Data, SlotFileName(Operation.SlotName), Settings->WorldSaveUserIndex,
			FAsyncSaveGameToSlotDelegate::CreateWeakLambda(this, [this, Operation, Info](const FString&, const int32, bool bSaved)
			{
				if (!bSaved)
				{
					FinishOperation(Operation, ESaveResult::FileError, Info);
					return;
				}

				SlotIndex->AddOrReplace(Info);
				ActiveSlotName = Info.SlotName;
				bSessionActive = true;

				SaveIndex([this, Operation, Info](bool bIndexSaved)
				{
					RefreshCache();
					FinishOperation(Operation, bIndexSaved ? ESaveResult::Success : ESaveResult::FileError, Info);
				});
			}));
	});
}

void USaveSubsystem::ExecuteLoad(const FOperation& Operation)
{
	if (!IsValidSlotName(Operation.SlotName))
	{
		FinishOperation(Operation, ESaveResult::InvalidSlotName);
		return;
	}
	if (!IsWorldAuthority())
	{
		FinishOperation(Operation, ESaveResult::NotAuthority);
		return;
	}

	const int32 UserIndex = USaveSystemSettings::Get()->WorldSaveUserIndex;
	if (!UGameplayStatics::DoesSaveGameExist(SlotFileName(Operation.SlotName), UserIndex))
	{
		FinishOperation(Operation, ESaveResult::SlotNotFound);
		return;
	}

	UGameplayStatics::AsyncLoadGameFromSlot(SlotFileName(Operation.SlotName), UserIndex,
		FAsyncLoadGameFromSlotDelegate::CreateWeakLambda(this, [this, Operation](const FString&, const int32, USaveGame* Loaded)
		{
			USaveSlotData* Data = Cast<USaveSlotData>(Loaded);
			if (!Data)
			{
				FinishOperation(Operation, ESaveResult::FileError);
				return;
			}

			const ESaveResult Prepared = PrepareLoadedData(Data, ESaveScope::World);
			if (Prepared != ESaveResult::Success)
			{
				// The file on disk is untouched, the running game is untouched.
				FinishOperation(Operation, Prepared, Data->Info);
				return;
			}

			Data->Info.SlotName = Operation.SlotName;
			ActiveWorldData = Data;
			ActiveSlotName = Operation.SlotName;
			ActivePlaytime = Data->Info.PlaytimeSeconds;
			SlotCustomData = Data->Info.CustomData;
			bSessionActive = true;

			// Objects that are alive now are restored in place. Objects that come to life later (a level that is about
			// to open) restore themselves when they register.
			ApplyScope(ESaveScope::World, 0, Data);

			if (Hooks)
			{
				Hooks->OnAfterLoad(ESaveScope::World, Data->Info);
			}
			FinishOperation(Operation, ESaveResult::Success, Data->Info);
		}));
}

void USaveSubsystem::ExecuteDelete(const FOperation& Operation)
{
	if (!IsValidSlotName(Operation.SlotName))
	{
		FinishOperation(Operation, ESaveResult::InvalidSlotName);
		return;
	}
	if (!IsWorldAuthority())
	{
		FinishOperation(Operation, ESaveResult::NotAuthority);
		return;
	}

	EnsureIndexLoaded([this, Operation]()
	{
		const int32 UserIndex = USaveSystemSettings::Get()->WorldSaveUserIndex;
		const FString File = SlotFileName(Operation.SlotName);

		// Deleting a small file is synchronous in the engine API.
		const bool bFileExisted = UGameplayStatics::DoesSaveGameExist(File, UserIndex);
		if (bFileExisted && !UGameplayStatics::DeleteGameInSlot(File, UserIndex))
		{
			FinishOperation(Operation, ESaveResult::FileError);
			return;
		}

		const bool bWasListed = SlotIndex->Remove(Operation.SlotName);
		if (!bFileExisted && !bWasListed)
		{
			FinishOperation(Operation, ESaveResult::SlotNotFound);
			return;
		}

		if (ActiveSlotName == Operation.SlotName)
		{
			ActiveSlotName.Reset(); // The playthrough continues in memory but has no slot until the next save.
		}

		SaveIndex([this, Operation](bool bIndexSaved)
		{
			RefreshCache();
			FSaveSlotInfo Info;
			Info.SlotName = Operation.SlotName;
			FinishOperation(Operation, bIndexSaved ? ESaveResult::Success : ESaveResult::FileError, Info);
		});
	});
}

void USaveSubsystem::ExecuteList(const FOperation& Operation)
{
	if (!IsWorldAuthority())
	{
		FinishOperation(Operation, ESaveResult::NotAuthority);
		return;
	}

	EnsureIndexLoaded([this, Operation]()
	{
		const int32 UserIndex = USaveSystemSettings::Get()->WorldSaveUserIndex;

		// Drop entries whose file vanished (deleted by hand, restored backup ...).
		const int32 Removed = SlotIndex->Slots.RemoveAll([UserIndex](const FSaveSlotInfo& Info)
		{
			return !UGameplayStatics::DoesSaveGameExist(SlotFileName(Info.SlotName), UserIndex);
		});

		RefreshCache();

		if (Removed > 0)
		{
			SaveIndex([](bool) {});
		}
		FinishOperation(Operation, ESaveResult::Success);
	});
}

// ---------------------------------------------------------------------------------------------------------------------
// Profile operations
// ---------------------------------------------------------------------------------------------------------------------

void USaveSubsystem::ExecuteLoadProfile(const FOperation& Operation)
{
	const int32 Index = Operation.ProfileIndex;
	if (!GetLocalPlayerForProfile(Index))
	{
		FinishOperation(Operation, ESaveResult::NoLocalPlayer);
		return;
	}

	FSaveSlotInfo ProfileInfo;
	ProfileInfo.SlotName = ProfileFileName(Index);

	const int32 UserIndex = GetProfileUserIndex(Index);
	if (!UGameplayStatics::DoesSaveGameExist(ProfileFileName(Index), UserIndex))
	{
		// First run: the profile starts empty. Not an error.
		USaveSlotData* Data = GetData(ESaveScope::Profile, Index, /*bCreate=*/true);
		LoadedProfiles.Add(Index);
		ApplyScope(ESaveScope::Profile, Index, Data);
		FinishOperation(Operation, ESaveResult::Success, ProfileInfo);
		return;
	}

	UGameplayStatics::AsyncLoadGameFromSlot(ProfileFileName(Index), UserIndex,
		FAsyncLoadGameFromSlotDelegate::CreateWeakLambda(this, [this, Operation, Index](const FString&, const int32, USaveGame* Loaded)
		{
			USaveSlotData* Data = Cast<USaveSlotData>(Loaded);
			const ESaveResult Result = Data ? PrepareLoadedData(Data, ESaveScope::Profile) : ESaveResult::FileError;

			if (Result != ESaveResult::Success)
			{
				// Keep the game running with an empty profile, but never overwrite the file we could not read.
				ProfileData.Add(Index, NewData());
				LoadedProfiles.Add(Index);
				LockedProfiles.Add(Index);
				FinishOperation(Operation, Result);
				return;
			}

			ProfileData.Add(Index, Data);
			LoadedProfiles.Add(Index);
			LockedProfiles.Remove(Index);
			ApplyScope(ESaveScope::Profile, Index, Data);

			if (Hooks)
			{
				Hooks->OnAfterLoad(ESaveScope::Profile, Data->Info);
			}
			FinishOperation(Operation, ESaveResult::Success, Data->Info);
		}));
}

void USaveSubsystem::ExecuteSaveProfile(const FOperation& Operation)
{
	const int32 Index = Operation.ProfileIndex;
	if (!GetLocalPlayerForProfile(Index))
	{
		FinishOperation(Operation, ESaveResult::NoLocalPlayer);
		return;
	}
	if (LockedProfiles.Contains(Index))
	{
		FinishOperation(Operation, ESaveResult::VersionTooNew);
		return;
	}

	const USaveSystemSettings* Settings = USaveSystemSettings::Get();

	if (Hooks)
	{
		Hooks->OnBeforeSave(ESaveScope::Profile, ProfileFileName(Index));
	}

	USaveSlotData* Data = GetData(ESaveScope::Profile, Index, /*bCreate=*/true);
	CaptureScope(ESaveScope::Profile, Index, Data);

	FSaveSlotInfo Info;
	Info.SlotName = ProfileFileName(Index);
	Info.DisplayName = Info.SlotName;
	Info.SavedAt = FDateTime::Now();
	Info.GameVersion = Settings->CurrentSaveVersion;
	Data->Info = Info;
	Data->GameVersion = Settings->CurrentSaveVersion;
	Data->FormatVersion = CoreSave::FormatVersion;

	UGameplayStatics::AsyncSaveGameToSlot(Data, ProfileFileName(Index), GetProfileUserIndex(Index),
		FAsyncSaveGameToSlotDelegate::CreateWeakLambda(this, [this, Operation, Info](const FString&, const int32, bool bSaved)
		{
			FinishOperation(Operation, bSaved ? ESaveResult::Success : ESaveResult::FileError, Info);
		}));
}

// ---------------------------------------------------------------------------------------------------------------------
// Versioning
// ---------------------------------------------------------------------------------------------------------------------

ESaveResult USaveSubsystem::PrepareLoadedData(USaveSlotData* Data, ESaveScope Scope)
{
	// Written by a newer framework or a newer game: do not touch it, a save here would destroy data we do not understand.
	if (Data->FormatVersion > CoreSave::FormatVersion)
	{
		return ESaveResult::VersionTooNew;
	}

	const int32 Current = USaveSystemSettings::Get()->CurrentSaveVersion;
	if (Data->GameVersion > Current)
	{
		return ESaveResult::VersionTooNew;
	}

	if (Data->GameVersion < Current && !RunMigrations(Data, Scope))
	{
		return ESaveResult::MigrationFailed;
	}

	// (Framework container upgrades go here when CoreSave::FormatVersion is raised.)
	Data->FormatVersion = CoreSave::FormatVersion;
	return ESaveResult::Success;
}

bool USaveSubsystem::RunMigrations(USaveSlotData* Data, ESaveScope Scope)
{
	const int32 Target = USaveSystemSettings::Get()->CurrentSaveVersion;
	int32 Version = Data->GameVersion;

	while (Version < Target)
	{
		USaveMigrationStep* Step = nullptr;
		for (USaveMigrationStep* Candidate : MigrationSteps)
		{
			if (Candidate && Candidate->FromVersion == Version && Candidate->ToVersion > Version)
			{
				Step = Candidate;
				break;
			}
		}

		if (!Step)
		{
			UE_LOG(LogCoreSave, Warning, TEXT("No migration step from save version %d (current is %d). Add one in Project Settings > CoreFramework Save."), Version, Target);
			return false;
		}
		if (!Step->Migrate(Data, Scope))
		{
			UE_LOG(LogCoreSave, Warning, TEXT("Migration step %s (version %d to %d) failed."), *Step->GetClass()->GetName(), Step->FromVersion, Step->ToVersion);
			return false;
		}

		Version = Step->ToVersion;
		Data->GameVersion = Version;
	}
	return true;
}
