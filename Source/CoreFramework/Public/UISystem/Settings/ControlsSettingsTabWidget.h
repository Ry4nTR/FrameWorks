// CoreFramework UI System - Settings
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UISystem/Menu/Tabs/BaseTabWidget.h"
#include "InputSystem/Core/InputFrameTypes.h"
#include "ControlsSettingsTabWidget.generated.h"

class UBaseButton;
class UInputFrameSubsystem;
class USliderSettingRowWidget;
class UToggleSettingRowWidget;

/**
 * Controls page of a settings menu: the player's input preferences of the input system (UInputFrameSubsystem) and the
 * entry to the key bindings screen.
 *
 * Values apply at once (the player feels the new sensitivity right away) and are written to disk by the menu's Apply
 * button (and when the game closes); Revert puts back the values the page had when it opened.
 *
 * Rows are found by name and all optional:
 *   Slider rows (USliderSettingRowWidget):  MouseSensitivityRow, GamepadSensitivityRow
 *     (range and step come from Project Settings > CoreFramework Input)
 *   Toggle rows (UToggleSettingRowWidget):  InvertXRow, InvertYRow, AimAssistRow, VibrationRow
 *   Button (UBaseButton):                   KeyBindingsButton  pushes Key Bindings Screen Tag (hidden when remapping is off)
 */
UCLASS(Abstract, Blueprintable)
class COREFRAMEWORK_API UControlsSettingsTabWidget : public UBaseTabWidget
{
	GENERATED_BODY()

public:
	//~ Begin UBaseTabWidget
	virtual void NotifyTabSelected() override;
	virtual bool HasPendingChanges_Implementation() const override;
	virtual void ApplyChanges_Implementation() override;
	virtual void RevertChanges_Implementation() override;
	//~ End UBaseTabWidget

	/** Back to the project defaults (applied at once, written on Apply). */
	UFUNCTION(BlueprintCallable, Category = "Controls Settings")
	void ResetToDefaults();

	/** Opens the key bindings screen (Key Bindings Screen Tag). */
	UFUNCTION(BlueprintCallable, Category = "Controls Settings")
	void OpenKeyBindings();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** Screen registered in the UI config (a UKeyBindingsScreenWidget) pushed by KeyBindingsButton. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Controls Settings")
	FGameplayTag KeyBindingsScreenTag;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<USliderSettingRowWidget> MouseSensitivityRow;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<USliderSettingRowWidget> GamepadSensitivityRow;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UToggleSettingRowWidget> InvertXRow;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UToggleSettingRowWidget> InvertYRow;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UToggleSettingRowWidget> AimAssistRow;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UToggleSettingRowWidget> VibrationRow;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseButton> KeyBindingsButton;

private:
	UInputFrameSubsystem* GetInput() const;
	void RefreshRows();
	void PushRowsToPreferences();

	UFUNCTION()
	void HandleSliderChanged(USliderSettingRowWidget* Row, float Value);

	UFUNCTION()
	void HandleToggleChanged(UToggleSettingRowWidget* Row, bool bIsOn);

	UFUNCTION()
	void HandlePreferencesChanged(const FInputPreferences& NewPreferences);

	void HandleKeyBindingsClicked();

	TWeakObjectPtr<UInputFrameSubsystem> BoundInput;
	FInputPreferences Snapshot;
	bool bRefreshing = false;
};
