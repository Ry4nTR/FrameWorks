// CoreFramework Save System
#include "SaveSystem/Manager/SaveSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "SaveSystem/Core/SaveSystemSettings.h"
#include "SaveSystem/Data/SaveHooks.h"
#include "SaveSystem/Data/SaveMigrationStep.h"
#include "SaveSystem/Data/SaveSlotData.h"
#include "SaveSystem/Interface/SaveableInterface.h"
#include "../Core/SaveSerialization.h"

DEFINE_LOG_CATEGORY(LogCoreSave);

namespace
{
	const TCHAR* OperationName(ESaveOperation Operation)
	{
		switch (Operation)
		{
		case ESaveOperation::SaveSlot: return TEXT("Save slot");
		case ESaveOperation::LoadSlot: return TEXT("Load slot");
		case ESaveOperation::DeleteSlot: return TEXT("Delete slot");
		case ESaveOperation::ListSlots: return TEXT("List slots");
		case ESaveOperation::SaveProfile: return TEXT("Save profile");
		case ESaveOperation::LoadProfile: return TEXT("Load profile");
		}
		return TEXT("Operation");
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Lookup and lifecycle
// ---------------------------------------------------------------------------------------------------------------------

USaveSubsystem* USaveSubsystem::Get(const UObject* WorldContext)
{
	if (!WorldContext || !GEngine)
	{
		return nullptr;
	}

	const UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull);
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<USaveSubsystem>() : nullptr;
}

void USaveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const USaveSystemSettings* Settings = USaveSystemSettings::Get();

	if (!Settings->SaveHooksClass.IsNull())
	{
		if (UClass* HooksClass = Settings->SaveHooksClass.LoadSynchronous())
		{
			Hooks = NewObject<USaveHooks>(this, HooksClass);
		}
	}

	for (const TSoftClassPtr<USaveMigrationStep>& StepClass : Settings->MigrationSteps)
	{
		if (UClass* LoadedClass = StepClass.LoadSynchronous())
		{
			MigrationSteps.Add(NewObject<USaveMigrationStep>(this, LoadedClass));
		}
	}

	// The profile is loaded as soon as local player 0 exists (the game instance is created before its players).
	bProfileAutoLoadPending = Settings->bAutoLoadProfile && !IsRunningDedicatedServer();
}

void USaveSubsystem::Deinitialize()
{
	Queue.Reset();
	Registry.Reset();
	ActiveWorldData = nullptr;
	ProfileData.Reset();
	SlotIndex = nullptr;
	Hooks = nullptr;
	MigrationSteps.Reset();

	Super::Deinitialize();
}

bool USaveSubsystem::IsTickable() const
{
	return !IsTemplate(RF_ClassDefaultObject);
}

TStatId USaveSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(USaveSubsystem, STATGROUP_Tickables);
}

UWorld* USaveSubsystem::GetTickableGameObjectWorld() const
{
	return GetWorld();
}

void USaveSubsystem::Tick(float DeltaTime)
{
	if (bProfileAutoLoadPending && GetLocalPlayerForProfile(0))
	{
		bProfileAutoLoadPending = false;
		LoadProfile(0);
	}

	if (bSessionActive)
	{
		const UWorld* World = GetWorld();
		if (World && !World->IsPaused() && !World->IsNetMode(NM_DedicatedServer))
		{
			ActivePlaytime += DeltaTime;
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Environment helpers
// ---------------------------------------------------------------------------------------------------------------------

bool USaveSubsystem::IsValidSlotName(const FString& SlotName)
{
	if (SlotName.IsEmpty() || SlotName.Len() > 64)
	{
		return false;
	}
	for (const TCHAR Character : SlotName)
	{
		if (!FChar::IsAlnum(Character) && Character != TEXT('_') && Character != TEXT('-'))
		{
			return false;
		}
	}
	return true;
}

bool USaveSubsystem::IsWorldAuthority() const
{
	// A network client never owns the world. No world yet (main menu start-up) counts as local.
	const UWorld* World = GetWorld();
	return !World || World->GetNetMode() != NM_Client;
}

ULocalPlayer* USaveSubsystem::GetLocalPlayerForProfile(int32 LocalPlayerIndex) const
{
	if (IsRunningDedicatedServer())
	{
		return nullptr;
	}
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetLocalPlayerByIndex(LocalPlayerIndex) : nullptr;
}

int32 USaveSubsystem::GetProfileUserIndex(int32 LocalPlayerIndex) const
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayerForProfile(LocalPlayerIndex);
	return LocalPlayer ? LocalPlayer->GetPlatformUserIndex() : 0;
}

FString USaveSubsystem::SlotFileName(const FString& SlotName)
{
	return FString::Printf(TEXT("CF_Slot_%s"), *SlotName);
}

FString USaveSubsystem::ProfileFileName(int32 LocalPlayerIndex)
{
	return FString::Printf(TEXT("CF_Profile_%d"), LocalPlayerIndex);
}

const FString& USaveSubsystem::IndexFileName()
{
	static const FString Name(TEXT("CF_Index"));
	return Name;
}

// ---------------------------------------------------------------------------------------------------------------------
// Queue
// ---------------------------------------------------------------------------------------------------------------------

FSaveCallback USaveSubsystem::WrapDelegate(FSaveOperationDelegate Delegate)
{
	if (!Delegate.IsBound())
	{
		return nullptr;
	}
	return [Delegate](ESaveResult Result, const FSaveSlotInfo& Info)
	{
		Delegate.ExecuteIfBound(Result, Info);
	};
}

void USaveSubsystem::SetBusy(bool bNewBusy)
{
	if (bBusy != bNewBusy)
	{
		bBusy = bNewBusy;
		OnBusyChanged.Broadcast(bBusy);
	}
}

void USaveSubsystem::Enqueue(FOperation&& Operation)
{
	Queue.Add(MoveTemp(Operation));
	SetBusy(true);
	PumpQueue();
}

void USaveSubsystem::PumpQueue()
{
	// One operation at a time: FinishOperation pumps again when the running one is done.
	if (bRunning || Queue.Num() == 0)
	{
		return;
	}

	bRunning = true;
	CurrentOperation = MoveTemp(Queue[0]);
	Queue.RemoveAt(0);

	// Copy: CurrentOperation is replaced when the next operation starts.
	const FOperation Operation = CurrentOperation;
	Dispatch(Operation);
}

void USaveSubsystem::Dispatch(const FOperation& Operation)
{
	switch (Operation.Type)
	{
	case ESaveOperation::SaveSlot: ExecuteSave(Operation); break;
	case ESaveOperation::LoadSlot: ExecuteLoad(Operation); break;
	case ESaveOperation::DeleteSlot: ExecuteDelete(Operation); break;
	case ESaveOperation::ListSlots: ExecuteList(Operation); break;
	case ESaveOperation::SaveProfile: ExecuteSaveProfile(Operation); break;
	case ESaveOperation::LoadProfile: ExecuteLoadProfile(Operation); break;
	}
}

void USaveSubsystem::FinishOperation(const FOperation& Operation, ESaveResult Result, const FSaveSlotInfo& Info)
{
	if (Result != ESaveResult::Success)
	{
		UE_LOG(LogCoreSave, Warning, TEXT("%s '%s' failed (result %d)."), OperationName(Operation.Type), *Operation.SlotName, static_cast<int32>(Result));
	}

	// Callbacks may queue new operations: they wait because bRunning is still true.
	if (Operation.Callback)
	{
		Operation.Callback(Result, Info);
	}
	OnOperationFinished.Broadcast(Operation.Type, Result, Info);

	bRunning = false;
	if (Queue.Num() > 0)
	{
		PumpQueue();
	}
	else
	{
		SetBusy(false);
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Public operations
// ---------------------------------------------------------------------------------------------------------------------

ESaveResult USaveSubsystem::StartNewGame(const FString& SlotName)
{
	if (!IsValidSlotName(SlotName))
	{
		return ESaveResult::InvalidSlotName;
	}
	if (!IsWorldAuthority())
	{
		return ESaveResult::NotAuthority;
	}

	ActiveWorldData = NewData();
	ActiveSlotName = SlotName;
	ActivePlaytime = 0.0;
	SlotCustomData.Reset();
	bSessionActive = true;
	return ESaveResult::Success;
}

void USaveSubsystem::SaveToSlot(const FString& SlotName, const FString& DisplayName, FSaveOperationDelegate OnComplete)
{
	SaveToSlot(SlotName, DisplayName, WrapDelegate(OnComplete));
}

void USaveSubsystem::SaveToSlot(const FString& SlotName, const FString& DisplayName, FSaveCallback OnComplete)
{
	FOperation Operation;
	Operation.Type = ESaveOperation::SaveSlot;
	Operation.SlotName = SlotName;
	Operation.DisplayName = DisplayName;
	Operation.Callback = MoveTemp(OnComplete);
	Enqueue(MoveTemp(Operation));
}

void USaveSubsystem::QuickSave(FSaveOperationDelegate OnComplete)
{
	QuickSave(WrapDelegate(OnComplete));
}

void USaveSubsystem::QuickSave(FSaveCallback OnComplete)
{
	if (ActiveSlotName.IsEmpty())
	{
		FOperation Operation;
		Operation.Type = ESaveOperation::SaveSlot;
		Operation.Callback = MoveTemp(OnComplete);
		// Not queued: nothing to wait for. Reported like any other failure.
		UE_LOG(LogCoreSave, Warning, TEXT("QuickSave: no active slot. Call StartNewGame or LoadSlot first."));
		if (Operation.Callback)
		{
			Operation.Callback(ESaveResult::SlotNotFound, FSaveSlotInfo());
		}
		OnOperationFinished.Broadcast(ESaveOperation::SaveSlot, ESaveResult::SlotNotFound, FSaveSlotInfo());
		return;
	}
	SaveToSlot(ActiveSlotName, FString(), MoveTemp(OnComplete));
}

void USaveSubsystem::AutoSave(FSaveOperationDelegate OnComplete)
{
	AutoSave(WrapDelegate(OnComplete));
}

void USaveSubsystem::AutoSave(FSaveCallback OnComplete)
{
	FOperation Operation;
	Operation.Type = ESaveOperation::SaveSlot;
	Operation.SlotName = USaveSystemSettings::Get()->AutosaveSlotName;
	Operation.Callback = MoveTemp(OnComplete);
	Enqueue(MoveTemp(Operation));
}

void USaveSubsystem::LoadSlot(const FString& SlotName, FSaveOperationDelegate OnComplete)
{
	LoadSlot(SlotName, WrapDelegate(OnComplete));
}

void USaveSubsystem::LoadSlot(const FString& SlotName, FSaveCallback OnComplete)
{
	FOperation Operation;
	Operation.Type = ESaveOperation::LoadSlot;
	Operation.SlotName = SlotName;
	Operation.Callback = MoveTemp(OnComplete);
	Enqueue(MoveTemp(Operation));
}

void USaveSubsystem::DeleteSlot(const FString& SlotName, FSaveOperationDelegate OnComplete)
{
	DeleteSlot(SlotName, WrapDelegate(OnComplete));
}

void USaveSubsystem::DeleteSlot(const FString& SlotName, FSaveCallback OnComplete)
{
	FOperation Operation;
	Operation.Type = ESaveOperation::DeleteSlot;
	Operation.SlotName = SlotName;
	Operation.Callback = MoveTemp(OnComplete);
	Enqueue(MoveTemp(Operation));
}

void USaveSubsystem::ListSlots(FSaveOperationDelegate OnComplete)
{
	ListSlots(WrapDelegate(OnComplete));
}

void USaveSubsystem::ListSlots(FSaveCallback OnComplete)
{
	FOperation Operation;
	Operation.Type = ESaveOperation::ListSlots;
	Operation.Callback = MoveTemp(OnComplete);
	Enqueue(MoveTemp(Operation));
}

void USaveSubsystem::LoadProfile(int32 LocalPlayerIndex, FSaveOperationDelegate OnComplete)
{
	LoadProfile(LocalPlayerIndex, WrapDelegate(OnComplete));
}

void USaveSubsystem::LoadProfile(int32 LocalPlayerIndex, FSaveCallback OnComplete)
{
	FOperation Operation;
	Operation.Type = ESaveOperation::LoadProfile;
	Operation.ProfileIndex = LocalPlayerIndex;
	Operation.Callback = MoveTemp(OnComplete);
	Enqueue(MoveTemp(Operation));
}

void USaveSubsystem::SaveProfile(int32 LocalPlayerIndex, FSaveOperationDelegate OnComplete)
{
	SaveProfile(LocalPlayerIndex, WrapDelegate(OnComplete));
}

void USaveSubsystem::SaveProfile(int32 LocalPlayerIndex, FSaveCallback OnComplete)
{
	FOperation Operation;
	Operation.Type = ESaveOperation::SaveProfile;
	Operation.ProfileIndex = LocalPlayerIndex;
	Operation.Callback = MoveTemp(OnComplete);
	Enqueue(MoveTemp(Operation));
}

bool USaveSubsystem::DoesSlotExist(const FString& SlotName) const
{
	return CachedSlots.ContainsByPredicate([&SlotName](const FSaveSlotInfo& Info) { return Info.SlotName == SlotName; });
}

void USaveSubsystem::SetSlotCustomData(FName Key, const FString& Value)
{
	if (Value.IsEmpty())
	{
		SlotCustomData.Remove(Key);
	}
	else
	{
		SlotCustomData.Add(Key, Value);
	}
}
