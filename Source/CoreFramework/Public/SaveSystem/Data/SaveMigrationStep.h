// CoreFramework Save System - Data
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SaveSystem/Core/SaveTypes.h"
#include "SaveMigrationStep.generated.h"

class USaveSlotData;

/**
 * One step of a save migration: upgrades a loaded save from FromVersion to ToVersion.
 *
 * When to write one
 *   Most game updates need NO step: adding or removing a SaveGame variable is handled automatically, and an object
 *   that only changed the meaning of its own data can fix it in ISaveableInterface::OnAfterLoad(SavedVersion).
 *   Write a step when the structure around the objects changes: a saveable got a new key, two objects merged,
 *   an object was removed and its data must be dropped, a value must be moved.
 *
 * How
 *   Create a Blueprint or C++ child, set From Version and To Version (normally To = From + 1), override Migrate,
 *   and add the class to Project Settings > CoreFramework > CoreFramework Save > Migration Steps.
 *   On load the framework runs the steps in order, starting at the file's version, until it reaches the current
 *   version. A missing link in the chain makes the load fail with MigrationFailed; the file is not touched.
 *   The migrated data is written back only by the next explicit save.
 */
UCLASS(Abstract, Blueprintable)
class COREFRAMEWORK_API USaveMigrationStep : public UObject
{
	GENERATED_BODY()

public:
	/** Version this step upgrades from. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Migration", meta = (ClampMin = "0"))
	int32 FromVersion = 1;

	/** Version the data has after this step. Must be greater than FromVersion. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Migration", meta = (ClampMin = "1"))
	int32 ToVersion = 2;

	/**
	 * Upgrades Data in place. Runs on the game thread, for World slots and Profile files alike.
	 * Return false to abort the load (MigrationFailed).
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Migration")
	bool Migrate(USaveSlotData* Data, ESaveScope Scope);
};
