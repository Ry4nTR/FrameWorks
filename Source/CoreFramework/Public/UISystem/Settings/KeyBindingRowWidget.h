// CoreFramework UI System - Settings
#pragma once

#include "CoreMinimal.h"
#include "UISystem/Settings/BaseSettingRowWidget.h"
#include "InputSystem/Core/InputFrameTypes.h"
#include "KeyBindingRowWidget.generated.h"

class UKeyBindingButton;
class UKeyBindingRowWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnKeyBindingClicked, UKeyBindingRowWidget*, Row, bool, bGamepad);

/**
 * One action of the key bindings screen: its name on the left, its keyboard / mouse key and its gamepad button on the
 * right. Clicking a box asks the screen (UKeyBindingsScreenWidget) to listen for a new key of that device.
 * Created by the screen from data; the Blueprint is layout only.
 *
 * The Blueprint may contain, by these exact names (optional):
 *   LabelText        (Common Text)                    the action name
 *   KeyboardBinding  (UKeyBindingButton Blueprint)     the keyboard / mouse box
 *   GamepadBinding   (UKeyBindingButton Blueprint)     the gamepad box
 *   FocusHighlight   (any widget)                     shown while the row is focused / hovered
 */
UCLASS(Abstract, Blueprintable)
class COREFRAMEWORK_API UKeyBindingRowWidget : public UBaseSettingRowWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Key Binding")
	void SetBinding(const FInputFrameKeyBinding& InBinding);

	UFUNCTION(BlueprintPure, Category = "Key Binding")
	FInputFrameKeyBinding GetBinding() const { return Binding; }

	UFUNCTION(BlueprintPure, Category = "Key Binding")
	FName GetMappingName() const { return Binding.MappingName; }

	UFUNCTION(BlueprintCallable, Category = "Key Binding")
	void SetListening(bool bGamepad, bool bListening, const FText& ListeningText);

	/** Hides a column (for example the gamepad column in a keyboard-only game). */
	UFUNCTION(BlueprintCallable, Category = "Key Binding")
	void SetColumnsVisible(bool bKeyboard, bool bGamepad);

	UFUNCTION(BlueprintPure, Category = "Key Binding")
	UKeyBindingButton* GetBindingButton(bool bGamepad) const;

	/** The box of the device in use (keyboard box with mouse / keys, gamepad box with a gamepad). */
	virtual UWidget* GetFocusTarget() const override;

	/** A box was clicked. */
	UPROPERTY(BlueprintAssignable, Category = "Key Binding")
	FOnKeyBindingClicked OnBindingClicked;

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UKeyBindingButton> KeyboardBinding;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UKeyBindingButton> GamepadBinding;

private:
	void HandleKeyboardClicked();
	void HandleGamepadClicked();

	FInputFrameKeyBinding Binding;
	bool bKeyboardColumnVisible = true;
	bool bGamepadColumnVisible = true;
};
