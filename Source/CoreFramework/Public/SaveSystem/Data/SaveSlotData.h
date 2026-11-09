// CoreFramework Save System - Data
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "SaveSystem/Core/SaveTypes.h"
#include "SaveSlotData.generated.h"

/**
 * The one on-disk container of the save system. A World slot and a Profile file are both this class.
 *
 *   Header   FormatVersion (framework layout), GameVersion (the game's data version), Info (metadata).
 *   Entries  One entry per saveable object: key -> { version, bytes }. The bytes are the object's SaveGame
 *            properties written with the reflection system, so adding or removing a variable never breaks a file.
 *   Values   Profile only: the key/value store used by settings screens.
 *
 * Game code does not use this class directly. Migration steps edit it (rename, move or remove entries).
 */
UCLASS()
class COREFRAMEWORK_API USaveSlotData : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int32 FormatVersion = CoreSave::FormatVersion;

	UPROPERTY()
	int32 GameVersion = 0;

	UPROPERTY()
	FSaveSlotInfo Info;

	UPROPERTY()
	TMap<FString, FSaveEntry> Entries;

	UPROPERTY()
	TMap<FName, FString> Values;

	// ---- Helpers for migration steps ---------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Save|Migration")
	bool HasEntry(const FString& Key) const { return Entries.Contains(Key); }

	/** Removes an entry. Returns false when it did not exist. */
	UFUNCTION(BlueprintCallable, Category = "Save|Migration")
	bool RemoveEntry(const FString& Key) { return Entries.Remove(Key) > 0; }

	/** Gives an entry a new key (the object was renamed). Fails when the old key is missing or the new key is taken. */
	UFUNCTION(BlueprintCallable, Category = "Save|Migration")
	bool RenameEntry(const FString& OldKey, const FString& NewKey);

	UFUNCTION(BlueprintPure, Category = "Save|Migration")
	int32 GetEntryVersion(const FString& Key) const;

	UFUNCTION(BlueprintCallable, Category = "Save|Migration")
	TArray<FString> GetEntryKeys() const;

	UFUNCTION(BlueprintPure, Category = "Save|Migration")
	FString GetValue(FName Key, const FString& Default) const;

	UFUNCTION(BlueprintCallable, Category = "Save|Migration")
	void SetValue(FName Key, const FString& Value) { Values.Add(Key, Value); }

	UFUNCTION(BlueprintCallable, Category = "Save|Migration")
	bool RemoveValue(FName Key) { return Values.Remove(Key) > 0; }
};

/** Index file: the list of all World slots with their metadata. Needed because desktop platforms cannot list saves. */
UCLASS()
class COREFRAMEWORK_API USaveSlotIndex : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int32 FormatVersion = CoreSave::FormatVersion;

	UPROPERTY()
	TArray<FSaveSlotInfo> Slots;

	FSaveSlotInfo* Find(const FString& SlotName);
	void AddOrReplace(const FSaveSlotInfo& Info);
	bool Remove(const FString& SlotName);
};
