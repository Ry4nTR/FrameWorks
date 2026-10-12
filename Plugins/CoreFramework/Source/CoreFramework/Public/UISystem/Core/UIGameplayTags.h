// CoreFramework UI System - Core
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

/**
 * Native tags owned by the framework. They describe infrastructure only:
 * the four standard layers and the default HUD element slot.
 *
 * Screen ids (main menu, pause menu, inventory...) and project specific slots / layers
 * are created by the game project in Project Settings > Gameplay Tags.
 */
struct COREFRAMEWORK_API FUITags
{
	// Layers. Draw order is decided by the root layout Blueprint (later = on top).
	static FGameplayTag LayerGame();
	static FGameplayTag LayerMenu();
	static FGameplayTag LayerModal();
	static FGameplayTag LayerOverlay();

	// HUD element slot bound to the optional widget named DefaultElementSlot.
	static FGameplayTag HUDSlotDefault();
};
