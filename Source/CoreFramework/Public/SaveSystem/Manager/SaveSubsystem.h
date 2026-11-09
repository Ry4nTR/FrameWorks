// CoreFramework Save System
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "SaveSystem/Core/SaveTypes.h"
#include "SaveSubsystem.generated.h"

class ULocalPlayer;
class USaveHooks;
class USaveMigrationStep;
class USaveSlotData;
class USaveSlotIndex;
class USaveSystemSettings;

/** C++ completion callback of an operation. */
using FSaveCallback = TFunction<void(ESaveResult, const FSaveSlotInfo&)>;

/**
 * The save system of one game instance. It is created by the engine for whatever UGameInstance class the project
 * uses: no game instance code, no base class and no registration is needed.
 *
 * Two scopes
 *   World    One slot per playthrough ("Slot1", "Autosave"). Written and read by the authority only (standalone, listen
 *            host, dedicated server). A network client gets ESaveResult::NotAuthority.
 *   Profile  One file per local player (settings, unlocks). Works on every machine that has that local player.
 *
 * Saveable objects register themselves (RegisterSaveable, or a USaveableComponent). "Registering = restoring": an object
 * that registers while the active slot holds data for its key is restored at once, so loading a slot and then opening
 * its map needs no ordering code. An object that unregisters hands its current state back to the active slot, so
 * progress survives level changes before the next save.
 *
 * Every operation is asynchronous and runs through one queue (one at a time), so two operations never race on the files.
 * Results come back through the completion delegate and through OnOperationFinished.
 *
 * Files (written through the engine's async slot API, safe on every platform)
 *   CF_Index            list of World slots (desktop platforms cannot list saves themselves)
 *   CF_Slot_<name>      one World slot
 *   CF_Profile_<n>      profile of local player n
 *
 * Versioning: see USaveMigrationStep and Project Settings > CoreFramework > CoreFramework Save.
 * Game specific behaviour: see USaveHooks.
 */
UCLASS()
class COREFRAMEWORK_API USaveSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	/** Finds the subsystem from any object with a world. Null without a game instance (editor preview). */
	static USaveSubsystem* Get(const UObject* WorldContext);

	//~ Begin USubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem

	//~ Begin FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;
	virtual UWorld* GetTickableGameObjectWorld() const override;
	//~ End FTickableGameObject

	// ---- Saveable objects --------------------------------------------------------------------------

	/**
	 * Registers an object that implements ISaveableInterface. If the active slot (or the loaded profile) already holds
	 * data for its key, the object is restored immediately. Call it when the object comes to life.
	 */
	UFUNCTION(BlueprintCallable, Category = "Save|Objects")
	void RegisterSaveable(UObject* Saveable);

	/**
	 * Removes an object from the registry. With bCaptureState (default) its current state is first copied into the active
	 * data, so it is not lost when the object is destroyed before the next save. Pass false when the object is going
	 * away because the game is quitting.
	 */
	UFUNCTION(BlueprintCallable, Category = "Save|Objects")
	void UnregisterSaveable(UObject* Saveable, bool bCaptureState = true);

	// ---- World slots ----------------------------------------------------------------------------------

	/**
	 * Starts a new playthrough: forgets the active data, makes SlotName the active slot and resets the playtime.
	 * Nothing is written until the first save. Returns InvalidSlotName or NotAuthority when it cannot start.
	 */
	UFUNCTION(BlueprintCallable, Category = "Save|Slots")
	ESaveResult StartNewGame(const FString& SlotName);

	/** Saves the world into SlotName (created if new, overwritten if it exists). DisplayName empty keeps the old name or uses the slot name. */
	UFUNCTION(BlueprintCallable, Category = "Save|Slots")
	void SaveToSlot(const FString& SlotName, const FString& DisplayName, FSaveOperationDelegate OnComplete);
	void SaveToSlot(const FString& SlotName, const FString& DisplayName, FSaveCallback OnComplete = nullptr);

	/** Saves into the active slot. Fails with SlotNotFound when there is no active slot (call StartNewGame or LoadSlot first). */
	UFUNCTION(BlueprintCallable, Category = "Save|Slots")
	void QuickSave(FSaveOperationDelegate OnComplete);
	void QuickSave(FSaveCallback OnComplete = nullptr);

	/** Saves into the autosave slot of the settings. The active slot does not change. */
	UFUNCTION(BlueprintCallable, Category = "Save|Slots")
	void AutoSave(FSaveOperationDelegate OnComplete);
	void AutoSave(FSaveCallback OnComplete = nullptr);

	/**
	 * Loads a slot: reads the file, checks versions, runs migration steps, makes it the active slot and restores every
	 * registered saveable. The result carries the slot info (MapName: the level to open).
	 */
	UFUNCTION(BlueprintCallable, Category = "Save|Slots")
	void LoadSlot(const FString& SlotName, FSaveOperationDelegate OnComplete);
	void LoadSlot(const FString& SlotName, FSaveCallback OnComplete = nullptr);

	/** Deletes a slot file and its index entry. */
	UFUNCTION(BlueprintCallable, Category = "Save|Slots")
	void DeleteSlot(const FString& SlotName, FSaveOperationDelegate OnComplete);
	void DeleteSlot(const FString& SlotName, FSaveCallback OnComplete = nullptr);

	/** Reads the slot index (slots whose file vanished are dropped). The result is then available from GetCachedSlots. */
	UFUNCTION(BlueprintCallable, Category = "Save|Slots")
	void ListSlots(FSaveOperationDelegate OnComplete);
	void ListSlots(FSaveCallback OnComplete = nullptr);

	/** The slots of the last list or save, newest first. Call ListSlots first. */
	UFUNCTION(BlueprintPure, Category = "Save|Slots")
	TArray<FSaveSlotInfo> GetCachedSlots() const { return CachedSlots; }

	UFUNCTION(BlueprintPure, Category = "Save|Slots")
	bool DoesSlotExist(const FString& SlotName) const;

	UFUNCTION(BlueprintPure, Category = "Save|Slots")
	FString GetActiveSlotName() const { return ActiveSlotName; }

	UFUNCTION(BlueprintPure, Category = "Save|Slots")
	bool HasActiveSlot() const { return !ActiveSlotName.IsEmpty(); }

	/** Playtime of the active playthrough, seconds. Counts while a playthrough is active and the game is not paused. */
	UFUNCTION(BlueprintPure, Category = "Save|Slots")
	double GetPlaytimeSeconds() const { return ActivePlaytime; }

	/** A label stored in the header of every following save (shown in the slot list). Empty value removes it. */
	UFUNCTION(BlueprintCallable, Category = "Save|Slots")
	void SetSlotCustomData(FName Key, const FString& Value);

	// ---- Profiles --------------------------------------------------------------------------------------

	/** Loads the profile of a local player and restores its profile saveables. A missing file is not an error: the profile starts empty. */
	UFUNCTION(BlueprintCallable, Category = "Save|Profile")
	void LoadProfile(int32 LocalPlayerIndex, FSaveOperationDelegate OnComplete);
	void LoadProfile(int32 LocalPlayerIndex, FSaveCallback OnComplete = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Save|Profile")
	void SaveProfile(int32 LocalPlayerIndex, FSaveOperationDelegate OnComplete);
	void SaveProfile(int32 LocalPlayerIndex, FSaveCallback OnComplete = nullptr);

	UFUNCTION(BlueprintPure, Category = "Save|Profile")
	bool IsProfileLoaded(int32 LocalPlayerIndex) const { return LoadedProfiles.Contains(LocalPlayerIndex); }

	// ---- Profile values (for settings screens: no saveable object needed) ---------------------------------

	UFUNCTION(BlueprintCallable, Category = "Save|Profile")
	void SetProfileFloat(FName Key, float Value, int32 LocalPlayerIndex = 0);

	UFUNCTION(BlueprintCallable, Category = "Save|Profile")
	void SetProfileInt(FName Key, int32 Value, int32 LocalPlayerIndex = 0);

	UFUNCTION(BlueprintCallable, Category = "Save|Profile")
	void SetProfileBool(FName Key, bool bValue, int32 LocalPlayerIndex = 0);

	UFUNCTION(BlueprintCallable, Category = "Save|Profile")
	void SetProfileString(FName Key, const FString& Value, int32 LocalPlayerIndex = 0);

	UFUNCTION(BlueprintPure, Category = "Save|Profile")
	float GetProfileFloat(FName Key, float Default, int32 LocalPlayerIndex = 0) const;

	UFUNCTION(BlueprintPure, Category = "Save|Profile")
	int32 GetProfileInt(FName Key, int32 Default, int32 LocalPlayerIndex = 0) const;

	UFUNCTION(BlueprintPure, Category = "Save|Profile")
	bool GetProfileBool(FName Key, bool bDefault, int32 LocalPlayerIndex = 0) const;

	UFUNCTION(BlueprintPure, Category = "Save|Profile")
	FString GetProfileString(FName Key, const FString& Default, int32 LocalPlayerIndex = 0) const;

	UFUNCTION(BlueprintPure, Category = "Save|Profile")
	bool HasProfileValue(FName Key, int32 LocalPlayerIndex = 0) const;

	// ---- State and events (for UI) -----------------------------------------------------------------------

	/** True while an operation runs or waits in the queue. Show a spinner and disable buttons. */
	UFUNCTION(BlueprintPure, Category = "Save|State")
	bool IsBusy() const { return bBusy; }

	/** Fires for every finished operation, whoever requested it. */
	UPROPERTY(BlueprintAssignable, Category = "Save|State")
	FOnSaveOperationFinished OnOperationFinished;

	UPROPERTY(BlueprintAssignable, Category = "Save|State")
	FOnSaveSlotListChanged OnSlotListChanged;

	UPROPERTY(BlueprintAssignable, Category = "Save|State")
	FOnSaveBusyChanged OnBusyChanged;

	UPROPERTY(BlueprintAssignable, Category = "Save|State")
	FOnSaveProfileValueChanged OnProfileValueChanged;

	/** True if a name is acceptable as a slot name: not empty, only letters, digits, '_' and '-'. */
	static bool IsValidSlotName(const FString& SlotName);

private:
	struct FOperation
	{
		ESaveOperation Type = ESaveOperation::ListSlots;
		FString SlotName;
		FString DisplayName;
		int32 ProfileIndex = 0;
		FSaveCallback Callback;
	};

	struct FRegisteredSaveable
	{
		TWeakObjectPtr<UObject> Object;
		FSaveableDescriptor Descriptor;
	};

	// Queue
	void Enqueue(FOperation&& Operation);
	void PumpQueue();
	void Dispatch(const FOperation& Operation);
	void FinishOperation(const FOperation& Operation, ESaveResult Result, const FSaveSlotInfo& Info = FSaveSlotInfo());
	void SetBusy(bool bNewBusy);
	static FSaveCallback WrapDelegate(FSaveOperationDelegate Delegate);

	// Operations (each ends with FinishOperation)
	void ExecuteSave(const FOperation& Operation);
	void ExecuteLoad(const FOperation& Operation);
	void ExecuteDelete(const FOperation& Operation);
	void ExecuteList(const FOperation& Operation);
	void ExecuteSaveProfile(const FOperation& Operation);
	void ExecuteLoadProfile(const FOperation& Operation);

	void EnsureIndexLoaded(TFunction<void()> Then);
	void SaveIndex(TFunction<void(bool)> Then);
	void RefreshCache();

	// Versioning
	ESaveResult PrepareLoadedData(USaveSlotData* Data, ESaveScope Scope);
	bool RunMigrations(USaveSlotData* Data, ESaveScope Scope);

	// Data and registry
	USaveSlotData* GetData(ESaveScope Scope, int32 LocalPlayerIndex, bool bCreate);
	USaveSlotData* NewData() const;
	FSaveSlotInfo BuildInfo(const FString& SlotName, const FString& DisplayName, const FSaveSlotInfo* Existing) const;
	static FString MakeRegistryKey(const FSaveableDescriptor& Descriptor);
	static bool QueryDescriptor(UObject* Object, FSaveableDescriptor& OutDescriptor);
	bool CanHandleScope(ESaveScope Scope, int32 LocalPlayerIndex) const;
	void CaptureObject(UObject* Object, const FSaveableDescriptor& Descriptor, USaveSlotData* Into);
	void RestoreObject(UObject* Object, const FSaveEntry& Entry);
	void CaptureScope(ESaveScope Scope, int32 LocalPlayerIndex, USaveSlotData* Into);
	void ApplyScope(ESaveScope Scope, int32 LocalPlayerIndex, USaveSlotData* From);

	// Environment
	bool IsWorldAuthority() const;
	ULocalPlayer* GetLocalPlayerForProfile(int32 LocalPlayerIndex) const;
	int32 GetProfileUserIndex(int32 LocalPlayerIndex) const;

	static FString SlotFileName(const FString& SlotName);
	static FString ProfileFileName(int32 LocalPlayerIndex);
	static const FString& IndexFileName();

	void SetProfileValueInternal(FName Key, const FString& Value, int32 LocalPlayerIndex);
	const FString* FindProfileValue(FName Key, int32 LocalPlayerIndex) const;

	UPROPERTY(Transient)
	TObjectPtr<USaveHooks> Hooks;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USaveMigrationStep>> MigrationSteps;

	UPROPERTY(Transient)
	TObjectPtr<USaveSlotIndex> SlotIndex;

	/** The data of the running playthrough: what the last load read, plus what unloaded objects handed back. */
	UPROPERTY(Transient)
	TObjectPtr<USaveSlotData> ActiveWorldData;

	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<USaveSlotData>> ProfileData;

	TMap<FString, FRegisteredSaveable> Registry;
	TArray<FSaveSlotInfo> CachedSlots;
	TMap<FName, FString> SlotCustomData;
	TSet<int32> LoadedProfiles;

	/** Profiles whose file could not be read (too new, migration failed). They are never overwritten. */
	TSet<int32> LockedProfiles;

	TArray<FOperation> Queue;
	FOperation CurrentOperation;
	bool bRunning = false;
	bool bBusy = false;

	FString ActiveSlotName;
	double ActivePlaytime = 0.0;
	bool bSessionActive = false;
	bool bProfileAutoLoadPending = false;
};
