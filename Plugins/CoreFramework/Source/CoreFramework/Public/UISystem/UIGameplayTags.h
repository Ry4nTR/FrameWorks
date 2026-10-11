// CoreFramework UI System
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

/**
 * Native tags owned by the framework. Child projects add their own screen, layer and slot tags
 * (Project Settings > Gameplay Tags); nothing here has to change for that.
 */
struct COREFRAMEWORK_API FUITags
{
	// Layers. Draw order is decided by the root layout Blueprint (later = on top).
	static FGameplayTag LayerGame();
	static FGameplayTag LayerMenu();
	static FGameplayTag LayerModal();
	static FGameplayTag LayerOverlay();

	// Screens shipped as ready-made ids.
	static FGameplayTag ScreenHUD();
	static FGameplayTag ScreenMainMenu();
	static FGameplayTag ScreenPauseMenu();
	static FGameplayTag ScreenSettings();

	// HUD element slots.
	static FGameplayTag HUDSlotDefault();
};
