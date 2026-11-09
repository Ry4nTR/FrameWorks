// CoreFramework Save System - Interface
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SaveSystem/Core/SaveTypes.h"
#include "SaveableInterface.generated.h"

UINTERFACE(BlueprintType, MinimalAPI)
class USaveableInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Contract of any object that wants its state saved: an actor, a component, a subsystem, a PlayerState, a plain UObject.
 *
 * Minimum to make an object saveable
 *   1. Implement this interface and answer GetSaveDescriptor (a unique, stable Key).
 *   2. Mark the variables to keep as SaveGame (UPROPERTY(SaveGame) in C++, the "Save Game" checkbox of a Blueprint variable).
 *   3. Call USaveSubsystem::RegisterSaveable(this) when the object comes to life and UnregisterSaveable(this)
 *      when it goes away.
 * Actors skip all of this: add a USaveableComponent instead.
 *
 * What happens
 *   Save: OnBeforeSave, then the SaveGame variables are written to bytes under the object's Key.
 *   Load: when the object is registered (or when a slot finishes loading while the object is alive) the variables
 *         are restored, then OnAfterLoad(SavedVersion) is called so the object can refresh anything derived from them.
 *
 * Fully custom data (C++ only): override SerializeSaveData and write or read with the archive. Return true to say
 * the data was handled; the SaveGame variables are then not written.
 */
class COREFRAMEWORK_API ISaveableInterface
{
	GENERATED_BODY()

public:
	/** Identity of this object. Leave Scope, Version and LocalPlayerIndex at their defaults unless needed. */
	UFUNCTION(BlueprintNativeEvent, Category = "Save")
	void GetSaveDescriptor(FSaveableDescriptor& OutDescriptor) const;

	/** Called right before the object's state is captured. Update anything that is not kept up to date live. */
	UFUNCTION(BlueprintNativeEvent, Category = "Save")
	void OnBeforeSave();

	/**
	 * Called after the object's state was restored.
	 * @param SavedVersion  Version the data was saved with (the Version of the descriptor at that time).
	 *                      Compare it with the current Version to upgrade data whose meaning changed.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Save")
	void OnAfterLoad(int32 SavedVersion);

	/**
	 * Custom serialization (C++ only, not visible to Blueprint). The same function reads and writes: check
	 * Ar.IsSaving() / Ar.IsLoading(). Return false (default) to use the SaveGame variables.
	 */
	virtual bool SerializeSaveData(FArchive& Ar) { return false; }
};
