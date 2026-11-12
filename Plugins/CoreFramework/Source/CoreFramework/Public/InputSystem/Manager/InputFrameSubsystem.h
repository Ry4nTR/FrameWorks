// CoreFramework Input System - Manager
#pragma once

#include "CoreMinimal.h"
#include "CommonInputTypeEnum.h"
#include "InputCoreTypes.h"
#include "Styling/SlateBrush.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "InputSystem/Core/InputFrameTypes.h"
#include "InputFrameSubsystem.generated.h"

class APlayerController;
class FKeyCaptureInputProcessor;
class UCommonInputSubsystem;
class UEnhancedInputUserSettings;
class UEnhancedPlayerMappableKeyProfile;
class UInputAction;
struct FKeyMappingRow;
struct FPlayerKeyMapping;

/** The player switched device (keyboard / mouse <-> gamepad) or gamepad family (Xbox, PlayStation ...). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInputFrameDeviceChanged, ECommonInputType, InputType, FName, GamepadName);

/** Key bindings changed (remap, reset, a mapping context applied or removed): prompts should look up their keys again. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInputFrameBindingsChanged);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInputFramePreferencesChanged, const FInputPreferences&, Preferences);

/** Result of a key capture. Key is invalid when bCancelled (cancel key, timeout, CancelKeyCapture). */
DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnInputFrameKeyCaptured, FKey, Key, bool, bCancelled);

/** C++ form of FOnInputFrameKeyCaptured. */
using FInputFrameKeyCaptureCallback = TFunction<void(const FKey& /*Key*/, bool /*bCancelled*/)>;

/**
 * The input system of one local player (split-screen safe). One place to ask "what is the player using, which key does
 * that action use, which icon shows it, how does the player like the controls".
 *
 * Device detection
 *   Built on CommonUI's UCommonInputSubsystem (the engine already tracks the last used device per local player).
 *   This class adds a Blueprint event, OnInputDeviceChanged, fired on every switch keyboard / mouse <-> gamepad and on a
 *   gamepad family change. Prompts, the HUD and world widgets bind to it instead of polling.
 *
 * Keys and glyphs
 *   GetKeyForAction finds the key an Enhanced Input action uses on keyboard / mouse or gamepad, player remaps included,
 *   also for mapping contexts that are not applied yet (main menu) when they are listed in Remappable Contexts.
 *   GetKeyGlyph returns the icon of a key from the CommonUI controller data (Project Settings > Common Input Settings),
 *   for the gamepad family in use. UInputPromptWidget is the ready-made widget that combines both.
 *
 * Preferences
 *   Mouse / gamepad sensitivity, invert X / Y, aim assist, vibration (FInputPreferences). Saved in GameUserSettings.ini,
 *   [CoreFramework.Input]. Look values are applied by ABaseCharacter::Input_Look, vibration on the player controller.
 *
 * Key bindings (remapping)
 *   A thin layer over the Enhanced Input user settings (Project Settings > Enhanced Input > Enable User Settings):
 *   GetKeyBindings lists every player-mappable action with its keyboard and gamepad key, RebindKey changes one with the
 *   project's conflict policy, BeginKeyCapture implements "press any key". UKeyBindingsScreenWidget is the ready-made UI.
 */
UCLASS()
class COREFRAMEWORK_API UInputFrameSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * Finds the subsystem for any object: a widget (its owning local player), a player controller, a pawn (its controller),
	 * a local player, or else the first local player of the object's world (world-space widgets, actors).
	 */
	static UInputFrameSubsystem* Get(const UObject* ContextObject);
	static UInputFrameSubsystem* GetFromLocalPlayer(const ULocalPlayer* LocalPlayer);

	//~ Begin USubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem

	//~ Begin ULocalPlayerSubsystem
	virtual void PlayerControllerChanged(APlayerController* NewPlayerController) override;
	//~ End ULocalPlayerSubsystem

	// ---- Device ---------------------------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Input|Device")
	ECommonInputType GetCurrentInputType() const;

	UFUNCTION(BlueprintPure, Category = "Input|Device")
	bool IsUsingGamepad() const;

	/** Gamepad family CommonUI detected or was told (Generic, XSX, PS5 ... as named in the controller data). */
	UFUNCTION(BlueprintPure, Category = "Input|Device")
	FName GetCurrentGamepadName() const;

	UPROPERTY(BlueprintAssignable, Category = "Input|Device")
	FOnInputFrameDeviceChanged OnInputDeviceChanged;

	// ---- Keys and glyphs -------------------------------------------------------------------------------

	/** The key an action uses on gamepad (bGamepad) or keyboard / mouse, player remaps included. Invalid when none. */
	UFUNCTION(BlueprintPure, Category = "Input|Prompts")
	FKey GetKeyForAction(const UInputAction* Action, bool bGamepad) const;

	/** GetKeyForAction for the device the player is using now. */
	UFUNCTION(BlueprintPure, Category = "Input|Prompts")
	FKey GetKeyForActionOnCurrentDevice(const UInputAction* Action) const;

	/** Icon of a key from the CommonUI controller data, for the gamepad family in use. False when no icon exists. */
	UFUNCTION(BlueprintPure, Category = "Input|Prompts")
	bool GetKeyGlyph(const FKey& Key, FSlateBrush& OutBrush) const;

	/**
	 * Same lookup without a local player (designer previews, tools). Uses GamepadName, or the platform default gamepad
	 * when it is None.
	 */
	static bool FindKeyGlyph(const FKey& Key, FName GamepadName, FSlateBrush& OutBrush);

	/** Short display name of a key ("E", "Esc", "LMB", "Space"). Empty for an invalid key. */
	UFUNCTION(BlueprintPure, Category = "Input|Prompts")
	static FText GetKeyDisplayText(const FKey& Key);

	UPROPERTY(BlueprintAssignable, Category = "Input|Prompts")
	FOnInputFrameBindingsChanged OnInputBindingsChanged;

	// ---- Preferences ------------------------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Input|Preferences")
	FInputPreferences GetPreferences() const { return Preferences; }

	/** Applies new preferences at once (look, vibration) and keeps them in memory. SavePreferences writes them to disk. */
	UFUNCTION(BlueprintCallable, Category = "Input|Preferences")
	void SetPreferences(const FInputPreferences& NewPreferences);

	/** Writes the preferences to GameUserSettings.ini now. Also done when the local player is removed. */
	UFUNCTION(BlueprintCallable, Category = "Input|Preferences")
	void SavePreferences();

	/** Back to Default Preferences of Project Settings > CoreFramework Input (not saved). */
	UFUNCTION(BlueprintCallable, Category = "Input|Preferences")
	void ResetPreferencesToDefaults();

	/** Look input scaled by the sensitivity of the device in use and inverted per the preferences. */
	UFUNCTION(BlueprintPure, Category = "Input|Preferences")
	FVector2D ApplyLookPreferences(FVector2D LookInput) const;

	/** The player wants aim assist and uses a gamepad. The game decides what assistance it gives. */
	UFUNCTION(BlueprintPure, Category = "Input|Preferences")
	bool IsAimAssistActive() const;

	UPROPERTY(BlueprintAssignable, Category = "Input|Preferences")
	FOnInputFramePreferencesChanged OnPreferencesChanged;

	// ---- Key bindings -----------------------------------------------------------------------------------

	/** False when Enhanced Input user settings are disabled (Project Settings > Enhanced Input > Enable User Settings). */
	UFUNCTION(BlueprintPure, Category = "Input|Bindings")
	bool AreKeyBindingsAvailable() const;

	/** Every player-mappable action, in the order of Remappable Contexts (then any other registered context). */
	UFUNCTION(BlueprintCallable, Category = "Input|Bindings")
	TArray<FInputFrameKeyBinding> GetKeyBindings() const;

	UFUNCTION(BlueprintCallable, Category = "Input|Bindings")
	bool GetKeyBinding(FName MappingName, FInputFrameKeyBinding& OutBinding) const;

	/** True when the key may be bound in the gamepad column (bGamepad) or the keyboard / mouse column. */
	UFUNCTION(BlueprintPure, Category = "Input|Bindings")
	bool IsKeyAllowedForBinding(const FKey& Key, bool bGamepad) const;

	/**
	 * Binds NewKey to an action on one device, applying the project's conflict policy. Applies the mappings at once and
	 * saves them when Save Key Bindings On Change is on.
	 * @param OutOtherAction  Display name of the action that had the key (swapped, unbound or the reason of a rejection).
	 */
	UFUNCTION(BlueprintCallable, Category = "Input|Bindings")
	EKeyRebindResult RebindKey(FName MappingName, bool bGamepad, FKey NewKey, FText& OutOtherAction);

	/** Puts one action back to its default key on one device. */
	UFUNCTION(BlueprintCallable, Category = "Input|Bindings")
	void ResetKeyBinding(FName MappingName, bool bGamepad);

	/** Puts every action back to its default keys. */
	UFUNCTION(BlueprintCallable, Category = "Input|Bindings")
	void ResetAllKeyBindings();

	UFUNCTION(BlueprintCallable, Category = "Input|Bindings")
	void SaveKeyBindings();

	// ---- Key capture ("press any key") -------------------------------------------------------------------

	/**
	 * Listens for the next key of one device (gamepad buttons, or keyboard keys and mouse buttons / wheel) and swallows
	 * all input meanwhile, so nothing else reacts. Cancel keys and the timeout end it without a key.
	 * Returns false when a capture is already running.
	 */
	UFUNCTION(BlueprintCallable, Category = "Input|Bindings", meta = (DisplayName = "Begin Key Capture"))
	bool K2_BeginKeyCapture(bool bGamepad, FOnInputFrameKeyCaptured OnCaptured);

	bool BeginKeyCapture(bool bGamepad, FInputFrameKeyCaptureCallback OnCaptured);

	/** Stops listening; the callback receives bCancelled = true. */
	UFUNCTION(BlueprintCallable, Category = "Input|Bindings")
	void CancelKeyCapture();

	UFUNCTION(BlueprintPure, Category = "Input|Bindings")
	bool IsCapturingKey() const;

private:
	UCommonInputSubsystem* GetCommonInput() const;
	UEnhancedInputUserSettings* GetUserSettings() const;
	UEnhancedPlayerMappableKeyProfile* GetKeyProfile() const;

	/** Binds to the Enhanced Input user settings and registers Remappable Contexts once they exist. */
	void ConnectEnhancedInput();

	void HandleInputMethodChanged(ECommonInputType NewInputType);

	UFUNCTION()
	void HandleUserSettingsChanged(UEnhancedInputUserSettings* Settings);

	UFUNCTION()
	void HandleControlMappingsRebuilt();

	void LoadPreferences();
	void ApplyVibration() const;
	FString GetConfigSection() const;

	void FinishKeyCapture(const FKey& Key, bool bCancelled);

	/** Applies the mappings to the player and saves them (per settings). */
	void CommitKeyBindings();

	TArray<FName> GetOrderedMappingNames(const UEnhancedPlayerMappableKeyProfile& Profile) const;
	static const FPlayerKeyMapping* FindDeviceMapping(const FKeyMappingRow& Row, bool bGamepad);
	static void FillBinding(FName MappingName, const FKeyMappingRow& Row, FInputFrameKeyBinding& OutBinding);

	FInputPreferences Preferences;

	TWeakObjectPtr<UEnhancedInputUserSettings> BoundUserSettings;
	TSharedPtr<FKeyCaptureInputProcessor> CaptureProcessor;
	FInputFrameKeyCaptureCallback CaptureCallback;

	ECommonInputType LastInputType = ECommonInputType::MouseAndKeyboard;
	FName LastGamepadName;
	bool bContextsRegistered = false;
	bool bMappingsRebuiltBound = false;
};
