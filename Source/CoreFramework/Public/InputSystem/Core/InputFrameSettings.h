// CoreFramework Input System - Core
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "InputCoreTypes.h"
#include "InputSystem/Core/InputFrameTypes.h"
#include "InputFrameSettings.generated.h"

class UInputMappingContext;

/**
 * Project Settings > CoreFramework > CoreFramework Input. Stored in DefaultGame.ini.
 *
 * Glyphs (button icons) are NOT configured here: they are the CommonUI controller data assets listed in
 * Project Settings > Common Input Settings > Platform Input > <platform> > Controller Data. The input system reads them,
 * so CommonUI's own action widgets and the framework's prompts always show the same icons.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "CoreFramework Input"))
class COREFRAMEWORK_API UInputFrameSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UInputFrameSettings();

	virtual FName GetCategoryName() const override { return TEXT("CoreFramework"); }

	static const UInputFrameSettings* Get() { return GetDefault<UInputFrameSettings>(); }

	// ---- Preferences --------------------------------------------------------------------------------

	/** Values used until the player changes them in the Controls page. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Preferences")
	FInputPreferences DefaultPreferences;

	/** Lowest sensitivity multiplier the Controls page offers (mouse and gamepad). */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Preferences", meta = (ClampMin = "0.01"))
	float MinSensitivity = 0.1f;

	/** Highest sensitivity multiplier the Controls page offers (mouse and gamepad). */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Preferences", meta = (ClampMin = "0.01"))
	float MaxSensitivity = 3.0f;

	/** Slider step of the sensitivity rows. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Preferences", meta = (ClampMin = "0.01"))
	float SensitivityStep = 0.05f;

	// ---- Prompts ------------------------------------------------------------------------------------

	/** What input prompts show for keyboard / mouse when the prompt itself says Project Default. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Prompts")
	EInputPromptKeyDisplay KeyboardPromptDisplay = EInputPromptKeyDisplay::KeyText;

	// ---- Key bindings -------------------------------------------------------------------------------

	/**
	 * Mapping contexts whose player-mappable keys appear in the key bindings screen. They are registered with the
	 * Enhanced Input user settings when the local player starts, so the screen works in the main menu too, where the
	 * gameplay contexts are not applied yet. Their order (and the order of their mappings) is the order of the list.
	 * Requires Project Settings > Engine > Enhanced Input > User Settings > Enable User Settings.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Key Bindings")
	TArray<TSoftObjectPtr<UInputMappingContext>> RemappableContexts;

	/** Keys the player can never bind (they open the pause menu or close screens). */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Key Bindings")
	TArray<FKey> ReservedKeys;

	/** Keys that cancel "press any key" listening. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Key Bindings")
	TArray<FKey> CancelCaptureKeys;

	/** Listening stops by itself after this many seconds. 0 = never. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Key Bindings", meta = (ClampMin = "0.0", Units = "s"))
	float KeyCaptureTimeout = 8.0f;

	/**
	 * What happens when the new key is already used by another action on the same device. Actions that share a key by
	 * default (the same key in the mapping context, for example two context-dependent uses of the left mouse button)
	 * are never treated as a conflict.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Key Bindings")
	EKeyRebindConflictPolicy ConflictPolicy = EKeyRebindConflictPolicy::Swap;

	/** Allow the stick directions (Left Stick Up ...) as bindings. Off: a slightly drifting stick cannot steal a binding. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Key Bindings")
	bool bAllowStickDirectionKeys = false;

	/** Write the key bindings to disk after every change. Off: call SaveKeyBindings yourself (for example on Apply). */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Key Bindings")
	bool bSaveKeyBindingsOnChange = true;
};
