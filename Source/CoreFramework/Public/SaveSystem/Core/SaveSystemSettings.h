// CoreFramework Save System - Core
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "SaveSystemSettings.generated.h"

class USaveHooks;
class USaveMigrationStep;

/** Project Settings > CoreFramework > CoreFramework Save. Stored in DefaultGame.ini of the game project. */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "CoreFramework Save"))
class COREFRAMEWORK_API USaveSystemSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/**
	 * Version of the game's save data. Start at 1. Raise it by one every time you ship a change that old saves
	 * cannot simply load (see USaveMigrationStep). Saves of a higher version than this are refused.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Versioning", meta = (ClampMin = "1"))
	int32 CurrentSaveVersion = 1;

	/** Steps that upgrade older saves. Order does not matter: they are chained by From Version. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Versioning")
	TArray<TSoftClassPtr<USaveMigrationStep>> MigrationSteps;

	/** Optional game extension (labels in the slot list, save restrictions). See USaveHooks. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Hooks")
	TSoftClassPtr<USaveHooks> SaveHooksClass;

	/** Maximum number of World slots. 0 = unlimited. Overwriting an existing slot is always allowed. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Slots", meta = (ClampMin = "0"))
	int32 MaxSlots = 0;

	/** Name of the slot written by AutoSave. It appears in the slot list with the autosave flag. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Slots")
	FString AutosaveSlotName = TEXT("Autosave");

	/** Load the profile of local player 0 as soon as it exists. Profile saveables are then restored without any game code. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Profile")
	bool bAutoLoadProfile = true;

	/** Platform user index used for World slots (consoles keep saves per user). 0 on desktop. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Slots", meta = (ClampMin = "0"))
	int32 WorldSaveUserIndex = 0;

	virtual FName GetCategoryName() const override { return TEXT("CoreFramework"); }

	static const USaveSystemSettings* Get() { return GetDefault<USaveSystemSettings>(); }
};
