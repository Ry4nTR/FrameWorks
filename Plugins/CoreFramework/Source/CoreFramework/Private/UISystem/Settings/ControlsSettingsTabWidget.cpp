// CoreFramework UI System - Settings
#include "UISystem/Settings/ControlsSettingsTabWidget.h"

#include "InputSystem/Core/InputFrameSettings.h"
#include "InputSystem/Manager/InputFrameSubsystem.h"
#include "UISystem/Foundation/BaseButton.h"
#include "UISystem/Manager/UIFrameSubsystem.h"
#include "UISystem/Settings/SliderSettingRowWidget.h"
#include "UISystem/Settings/ToggleSettingRowWidget.h"

UInputFrameSubsystem* UControlsSettingsTabWidget::GetInput() const
{
	return BoundInput.IsValid() ? BoundInput.Get() : UInputFrameSubsystem::Get(this);
}

void UControlsSettingsTabWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	const UInputFrameSettings* Settings = UInputFrameSettings::Get();
	for (USliderSettingRowWidget* Row : { MouseSensitivityRow.Get(), GamepadSensitivityRow.Get() })
	{
		if (Row)
		{
			Row->SetRange(Settings->MinSensitivity, Settings->MaxSensitivity, Settings->SensitivityStep);
			Row->OnValueChanged.AddUniqueDynamic(this, &UControlsSettingsTabWidget::HandleSliderChanged);
		}
	}
	for (UToggleSettingRowWidget* Row : { InvertXRow.Get(), InvertYRow.Get(), AimAssistRow.Get(), VibrationRow.Get() })
	{
		if (Row)
		{
			Row->OnToggled.AddUniqueDynamic(this, &UControlsSettingsTabWidget::HandleToggleChanged);
		}
	}
	if (KeyBindingsButton)
	{
		KeyBindingsButton->OnClicked().AddUObject(this, &UControlsSettingsTabWidget::HandleKeyBindingsClicked);
	}
}

void UControlsSettingsTabWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UInputFrameSubsystem* Input = UInputFrameSubsystem::Get(this))
	{
		BoundInput = Input;
		Input->OnPreferencesChanged.AddUniqueDynamic(this, &UControlsSettingsTabWidget::HandlePreferencesChanged);
		Snapshot = Input->GetPreferences();

		if (KeyBindingsButton)
		{
			KeyBindingsButton->SetVisibility(Input->AreKeyBindingsAvailable() && KeyBindingsScreenTag.IsValid()
				? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		}
	}
	RefreshRows();
}

void UControlsSettingsTabWidget::NativeDestruct()
{
	if (UInputFrameSubsystem* Input = BoundInput.Get())
	{
		Input->OnPreferencesChanged.RemoveDynamic(this, &UControlsSettingsTabWidget::HandlePreferencesChanged);
	}
	BoundInput.Reset();

	Super::NativeDestruct();
}

void UControlsSettingsTabWidget::NotifyTabSelected()
{
	Super::NotifyTabSelected();

	if (const UInputFrameSubsystem* Input = GetInput())
	{
		if (!HasPendingChanges())
		{
			Snapshot = Input->GetPreferences();
		}
	}
	RefreshRows();
}

void UControlsSettingsTabWidget::RefreshRows()
{
	const UInputFrameSubsystem* Input = GetInput();
	if (!Input)
	{
		return;
	}

	TGuardValue<bool> Guard(bRefreshing, true);
	const FInputPreferences Preferences = Input->GetPreferences();
	if (MouseSensitivityRow)   { MouseSensitivityRow->SetValue(Preferences.MouseSensitivity); }
	if (GamepadSensitivityRow) { GamepadSensitivityRow->SetValue(Preferences.GamepadSensitivity); }
	if (InvertXRow)            { InvertXRow->SetIsOn(Preferences.bInvertLookX); }
	if (InvertYRow)            { InvertYRow->SetIsOn(Preferences.bInvertLookY); }
	if (AimAssistRow)          { AimAssistRow->SetIsOn(Preferences.bAimAssist); }
	if (VibrationRow)          { VibrationRow->SetIsOn(Preferences.bVibration); }
}

void UControlsSettingsTabWidget::PushRowsToPreferences()
{
	UInputFrameSubsystem* Input = GetInput();
	if (!Input || bRefreshing)
	{
		return;
	}

	// Rows that are not in the Blueprint keep the current value.
	FInputPreferences Preferences = Input->GetPreferences();
	if (MouseSensitivityRow)   { Preferences.MouseSensitivity = MouseSensitivityRow->GetValue(); }
	if (GamepadSensitivityRow) { Preferences.GamepadSensitivity = GamepadSensitivityRow->GetValue(); }
	if (InvertXRow)            { Preferences.bInvertLookX = InvertXRow->IsOn(); }
	if (InvertYRow)            { Preferences.bInvertLookY = InvertYRow->IsOn(); }
	if (AimAssistRow)          { Preferences.bAimAssist = AimAssistRow->IsOn(); }
	if (VibrationRow)          { Preferences.bVibration = VibrationRow->IsOn(); }
	Input->SetPreferences(Preferences);
}

void UControlsSettingsTabWidget::HandleSliderChanged(USliderSettingRowWidget* Row, float Value)
{
	PushRowsToPreferences();
}

void UControlsSettingsTabWidget::HandleToggleChanged(UToggleSettingRowWidget* Row, bool bIsOn)
{
	PushRowsToPreferences();
}

void UControlsSettingsTabWidget::HandlePreferencesChanged(const FInputPreferences& NewPreferences)
{
	// Changed elsewhere (another page, game code): show it.
	if (!bRefreshing)
	{
		RefreshRows();
	}
}

void UControlsSettingsTabWidget::ResetToDefaults()
{
	if (UInputFrameSubsystem* Input = GetInput())
	{
		Input->ResetPreferencesToDefaults();
		RefreshRows();
	}
}

void UControlsSettingsTabWidget::OpenKeyBindings()
{
	if (!KeyBindingsScreenTag.IsValid())
	{
		UE_LOG(LogCoreInput, Warning, TEXT("%s: Key Bindings Screen Tag is not set."), *GetName());
		return;
	}
	if (UUIFrameSubsystem* UI = GetUIFrameSubsystem())
	{
		UI->PushScreen(KeyBindingsScreenTag);
	}
}

void UControlsSettingsTabWidget::HandleKeyBindingsClicked()
{
	OpenKeyBindings();
}

bool UControlsSettingsTabWidget::HasPendingChanges_Implementation() const
{
	const UInputFrameSubsystem* Input = BoundInput.Get();
	return Input && Input->GetPreferences() != Snapshot;
}

void UControlsSettingsTabWidget::ApplyChanges_Implementation()
{
	if (UInputFrameSubsystem* Input = GetInput())
	{
		Input->SavePreferences();
		Snapshot = Input->GetPreferences();
	}
}

void UControlsSettingsTabWidget::RevertChanges_Implementation()
{
	if (UInputFrameSubsystem* Input = GetInput())
	{
		Input->SetPreferences(Snapshot);
		RefreshRows();
	}
}
