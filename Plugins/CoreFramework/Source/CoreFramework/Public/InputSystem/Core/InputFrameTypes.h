// CoreFramework Input System - Core
#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "InputFrameTypes.generated.h"

COREFRAMEWORK_API DECLARE_LOG_CATEGORY_EXTERN(LogCoreInput, Log, All);

/**
 * The player's input preferences (Controls settings page). Applied by UInputFrameSubsystem:
 *  - look sensitivity and inversion through ABaseCharacter::Input_Look (UInputFrameSubsystem::ApplyLookPreferences),
 *  - vibration on the local player's controller,
 *  - aim assist is only stored: the game decides what it means (UInputFrameSubsystem::IsAimAssistActive).
 * Saved per machine in GameUserSettings.ini, section [CoreFramework.Input].
 */
USTRUCT(BlueprintType)
struct COREFRAMEWORK_API FInputPreferences
{
	GENERATED_BODY()

	/** Multiplier of mouse look. 1 = the look speed of the input mapping. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Preferences", meta = (ClampMin = "0.01"))
	float MouseSensitivity = 1.0f;

	/** Multiplier of gamepad stick look. 1 = the look speed of the input mapping. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Preferences", meta = (ClampMin = "0.01"))
	float GamepadSensitivity = 1.0f;

	/** Inverts horizontal look (yaw). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Preferences")
	bool bInvertLookX = false;

	/** Inverts vertical look (pitch). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Preferences")
	bool bInvertLookY = false;

	/** The player wants aim assistance with a gamepad. The game decides what assistance means. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Preferences")
	bool bAimAssist = true;

	/** Controller vibration (force feedback). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Preferences")
	bool bVibration = true;

	bool operator==(const FInputPreferences& Other) const
	{
		return FMath::IsNearlyEqual(MouseSensitivity, Other.MouseSensitivity)
			&& FMath::IsNearlyEqual(GamepadSensitivity, Other.GamepadSensitivity)
			&& bInvertLookX == Other.bInvertLookX
			&& bInvertLookY == Other.bInvertLookY
			&& bAimAssist == Other.bAimAssist
			&& bVibration == Other.bVibration;
	}

	bool operator!=(const FInputPreferences& Other) const { return !(*this == Other); }
};

/** What an input prompt shows while the player uses keyboard and mouse. With a gamepad it always shows the glyph. */
UENUM(BlueprintType)
enum class EInputPromptKeyDisplay : uint8
{
	/** Use Keyboard Prompt Display of Project Settings > CoreFramework > CoreFramework Input. */
	ProjectDefault UMETA(DisplayName = "Project Default"),

	/** No key at all: only the label (clean text buttons on PC). */
	None,

	/** The key name as text ("E", "Esc", "LMB"). */
	KeyText UMETA(DisplayName = "Key Text"),

	/** The key icon of the CommonUI controller data for keyboard, or the key name when there is none. */
	Glyph
};

/** What happens when a key is bound to an action while another action already uses it. */
UENUM(BlueprintType)
enum class EKeyRebindConflictPolicy : uint8
{
	/** The other action receives the key the edited action had before. */
	Swap,

	/** The other action loses the key (it is left without a binding on that device). */
	Unbind,

	/** Both actions keep the key. */
	Allow,

	/** The new binding is refused. */
	Reject
};

/** Result of UInputFrameSubsystem::RebindKey. */
UENUM(BlueprintType)
enum class EKeyRebindResult : uint8
{
	Success,

	/** The key is already bound to that action. */
	Unchanged,

	/** The key is in Reserved Keys (Project Settings > CoreFramework Input). */
	ReservedKey,

	/** A gamepad key for the keyboard column or the opposite. */
	WrongDevice,

	/** Another action uses the key and the conflict policy is Reject. */
	Rejected,

	/** The action has no player-mappable binding for that device. */
	NotRemappable,

	/** Enhanced Input user settings are disabled or the mapping name is unknown. */
	Failed
};

/**
 * One remappable action as the key bindings screen shows it: its name and the key it uses on keyboard / mouse and on
 * gamepad. Built from the Enhanced Input user settings (player-mappable key profile).
 */
USTRUCT(BlueprintType)
struct COREFRAMEWORK_API FInputFrameKeyBinding
{
	GENERATED_BODY()

	/** Player-mappable name (Input Action > Player Mappable Key Settings > Name, or the per-mapping override). */
	UPROPERTY(BlueprintReadOnly, Category = "Input|Bindings")
	FName MappingName;

	UPROPERTY(BlueprintReadOnly, Category = "Input|Bindings")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Input|Bindings")
	FText Category;

	/** The action has a keyboard / mouse binding the player can change. */
	UPROPERTY(BlueprintReadOnly, Category = "Input|Bindings")
	bool bHasKeyboardBinding = false;

	UPROPERTY(BlueprintReadOnly, Category = "Input|Bindings")
	FKey KeyboardKey;

	UPROPERTY(BlueprintReadOnly, Category = "Input|Bindings")
	FKey KeyboardDefaultKey;

	/** The action has a gamepad binding the player can change. */
	UPROPERTY(BlueprintReadOnly, Category = "Input|Bindings")
	bool bHasGamepadBinding = false;

	UPROPERTY(BlueprintReadOnly, Category = "Input|Bindings")
	FKey GamepadKey;

	UPROPERTY(BlueprintReadOnly, Category = "Input|Bindings")
	FKey GamepadDefaultKey;

	bool IsKeyboardCustomized() const { return bHasKeyboardBinding && KeyboardKey != KeyboardDefaultKey; }
	bool IsGamepadCustomized() const { return bHasGamepadBinding && GamepadKey != GamepadDefaultKey; }
};
