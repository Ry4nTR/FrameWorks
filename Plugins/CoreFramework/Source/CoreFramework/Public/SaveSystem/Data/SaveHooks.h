// CoreFramework Save System - Data
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SaveSystem/Core/SaveTypes.h"
#include "SaveHooks.generated.h"

/**
 * The game's extension point of the save system. Create a C++ or Blueprint child, override only what the game
 * needs and assign it in Project Settings > CoreFramework > CoreFramework Save > Save Hooks Class.
 * Every function does nothing by default, so a game that needs none of them does not create a child at all.
 *
 * (The subsystem itself is not meant to be subclassed: the engine creates subsystem subclasses by its own rules.
 * Game specific behaviour belongs here.)
 */
UCLASS(Blueprintable)
class COREFRAMEWORK_API USaveHooks : public UObject
{
	GENERATED_BODY()

public:
	/** Return false to refuse a World save (in a cutscene, in combat, in a tutorial ...). Autosaves and manual saves both ask. */
	UFUNCTION(BlueprintNativeEvent, Category = "Save")
	bool CanSave(const FString& SlotName);

	/**
	 * Called while the header of a slot is built. Change anything: add labels to CustomData (Chapter, Difficulty,
	 * "Level 12 Mage"), adjust DisplayName. The slot list shows what you return.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Save")
	FSaveSlotInfo BuildSlotInfo(const FSaveSlotInfo& DefaultInfo);

	/** Runs before the saveable objects are captured for a slot or profile save. Last chance to update live data. */
	UFUNCTION(BlueprintNativeEvent, Category = "Save")
	void OnBeforeSave(ESaveScope Scope, const FString& SlotName);

	/** Runs after a slot or profile was loaded and every saveable that was alive has been restored. */
	UFUNCTION(BlueprintNativeEvent, Category = "Save")
	void OnAfterLoad(ESaveScope Scope, const FSaveSlotInfo& Info);
};
