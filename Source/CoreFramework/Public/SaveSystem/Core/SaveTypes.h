// CoreFramework Save System - Core
#pragma once

#include "CoreMinimal.h"
#include "Misc/DateTime.h"
#include "SaveTypes.generated.h"

COREFRAMEWORK_API DECLARE_LOG_CATEGORY_EXTERN(LogCoreSave, Log, All);

namespace CoreSave
{
	/**
	 * Version of the on-disk container written by the framework itself (not the game's data).
	 * Raised only when the container layout changes. A file with a higher number is refused (it was written by a
	 * newer framework); a lower number is upgraded by the framework.
	 */
	constexpr int32 FormatVersion = 1;
}

/**
 * Which kind of save a saveable object belongs to.
 *  World    State of the game world / campaign. One slot per playthrough. Saved and loaded by the authority only.
 *  Profile  State of one local player that is not tied to a playthrough: settings, unlocks, statistics.
 *           One file per local player. Never touches the server.
 */
UENUM(BlueprintType)
enum class ESaveScope : uint8
{
	World,
	Profile
};

UENUM(BlueprintType)
enum class ESaveOperation : uint8
{
	SaveSlot,
	LoadSlot,
	DeleteSlot,
	ListSlots,
	SaveProfile,
	LoadProfile
};

UENUM(BlueprintType)
enum class ESaveResult : uint8
{
	Success,
	/** Slot name is empty or contains characters other than letters, digits, '_' and '-'. */
	InvalidSlotName,
	/** A World operation was requested on a machine that is a network client. */
	NotAuthority,
	/** A Profile operation was requested for a local player that does not exist (or on a dedicated server). */
	NoLocalPlayer,
	SlotNotFound,
	/** Reading or writing the file failed. */
	FileError,
	/** The file was written by a newer version of the game or of the framework. It is left untouched. */
	VersionTooNew,
	/** The file is older and no chain of migration steps leads to the current version. It is left untouched. */
	MigrationFailed,
	TooManySlots,
	/** The game's Save Hooks refused the save (CanSave returned false). */
	Blocked
};

/** Identity of a saveable object, filled by ISaveableInterface::GetSaveDescriptor. */
USTRUCT(BlueprintType)
struct COREFRAMEWORK_API FSaveableDescriptor
{
	GENERATED_BODY()

	/**
	 * Unique id of the object inside its scope. Any string: "Player.12345", "Door_12", "Inventory".
	 * Must stay the same between runs. Two objects with the same key overwrite each other.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Save")
	FString Key;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Save")
	ESaveScope Scope = ESaveScope::World;

	/** Version of this object's own data. Handed back as SavedVersion on load, so the object can upgrade old data. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Save", meta = (ClampMin = "1"))
	int32 Version = 1;

	/** Profile scope only: index of the local player whose profile stores this object (0 = first player). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Save", meta = (ClampMin = "0"))
	int32 LocalPlayerIndex = 0;
};

/**
 * Metadata of one save slot. Stored in the slot file and in the index, so a slot list can be shown
 * without loading any slot.
 */
USTRUCT(BlueprintType)
struct COREFRAMEWORK_API FSaveSlotInfo
{
	GENERATED_BODY()

	/** Technical name, unique. Letters, digits, '_' and '-'. */
	UPROPERTY(BlueprintReadOnly, Category = "Save")
	FString SlotName;

	/** Name shown to the player. */
	UPROPERTY(BlueprintReadOnly, Category = "Save")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Save")
	FDateTime SavedAt;

	/** Time played in this playthrough, seconds (game paused time not counted). */
	UPROPERTY(BlueprintReadOnly, Category = "Save")
	double PlaytimeSeconds = 0.0;

	/** Short name of the level the game was in when saved. */
	UPROPERTY(BlueprintReadOnly, Category = "Save")
	FString MapName;

	/** Game save version (Project Settings > CoreFramework Save > Current Save Version) the slot was written with. */
	UPROPERTY(BlueprintReadOnly, Category = "Save")
	int32 GameVersion = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Save")
	bool bIsAutosave = false;

	/** Free labels added by the game (chapter, difficulty, character level ...). See USaveHooks::BuildSlotInfo. */
	UPROPERTY(BlueprintReadOnly, Category = "Save")
	TMap<FName, FString> CustomData;

	bool IsValid() const { return !SlotName.IsEmpty(); }
};

/** The saved state of one object: its own version and its bytes. */
USTRUCT()
struct FSaveEntry
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Version = 1;

	UPROPERTY()
	TArray<uint8> Bytes;
};

/** Called when a queued operation finishes. */
DECLARE_DYNAMIC_DELEGATE_TwoParams(FSaveOperationDelegate, ESaveResult, Result, const FSaveSlotInfo&, Info);

/** Fires for every finished operation, whoever requested it. UI binds here. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSaveOperationFinished, ESaveOperation, Operation, ESaveResult, Result, const FSaveSlotInfo&, Info);

/** The list of slots changed (save, delete, list). Read it with GetCachedSlots. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSaveSlotListChanged);

/** An operation started or the queue became empty. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSaveBusyChanged, bool, bBusy);

/** A profile value was set. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSaveProfileValueChanged, FName, Key);
