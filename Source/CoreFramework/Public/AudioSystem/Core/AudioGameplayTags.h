// CoreFramework Audio System - Core
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

/**
 * Native tags owned by the framework. They describe infrastructure only: the standard volume categories.
 *
 * Cue ids (Audio.Cue.UI.Click, Audio.Cue.Door.Open ...) and project specific categories are created by the game
 * project in Project Settings > Gameplay Tags.
 */
struct COREFRAMEWORK_API FAudioTags
{
	/** Root of the hierarchy. Its Sound Class is the parent of all the others. */
	static FGameplayTag CategoryMaster();
	static FGameplayTag CategorySFX();
	static FGameplayTag CategoryMusic();
	static FGameplayTag CategoryAmbient();
	static FGameplayTag CategoryUI();
};
