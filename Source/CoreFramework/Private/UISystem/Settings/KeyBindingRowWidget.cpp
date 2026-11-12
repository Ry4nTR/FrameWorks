// CoreFramework UI System - Settings
#include "UISystem/Settings/KeyBindingRowWidget.h"

#include "InputSystem/Manager/InputFrameSubsystem.h"
#include "UISystem/Settings/KeyBindingButton.h"

void UKeyBindingRowWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (KeyboardBinding)
	{
		KeyboardBinding->OnClicked().AddUObject(this, &UKeyBindingRowWidget::HandleKeyboardClicked);
	}
	if (GamepadBinding)
	{
		GamepadBinding->OnClicked().AddUObject(this, &UKeyBindingRowWidget::HandleGamepadClicked);
	}
}

void UKeyBindingRowWidget::SetBinding(const FInputFrameKeyBinding& InBinding)
{
	Binding = InBinding;
	SetLabel(Binding.DisplayName);

	if (KeyboardBinding)
	{
		KeyboardBinding->SetKey(Binding.KeyboardKey);
		KeyboardBinding->SetIsEnabled(Binding.bHasKeyboardBinding);
	}
	if (GamepadBinding)
	{
		GamepadBinding->SetKey(Binding.GamepadKey);
		GamepadBinding->SetIsEnabled(Binding.bHasGamepadBinding);
	}
}

void UKeyBindingRowWidget::SetListening(bool bGamepad, bool bListening, const FText& ListeningText)
{
	if (UKeyBindingButton* Button = GetBindingButton(bGamepad))
	{
		Button->SetListening(bListening, ListeningText);
	}
}

void UKeyBindingRowWidget::SetColumnsVisible(bool bKeyboard, bool bGamepad)
{
	bKeyboardColumnVisible = bKeyboard;
	bGamepadColumnVisible = bGamepad;
	if (KeyboardBinding)
	{
		KeyboardBinding->SetVisibility(bKeyboard ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (GamepadBinding)
	{
		GamepadBinding->SetVisibility(bGamepad ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

UKeyBindingButton* UKeyBindingRowWidget::GetBindingButton(bool bGamepad) const
{
	return bGamepad ? GamepadBinding : KeyboardBinding;
}

UWidget* UKeyBindingRowWidget::GetFocusTarget() const
{
	const UInputFrameSubsystem* Input = UInputFrameSubsystem::Get(this);
	const bool bPreferGamepad = Input && Input->IsUsingGamepad();

	UKeyBindingButton* Preferred = bPreferGamepad ? GamepadBinding : KeyboardBinding;
	const bool bPreferredUsable = Preferred && (bPreferGamepad ? bGamepadColumnVisible : bKeyboardColumnVisible) && Preferred->GetIsEnabled();
	if (bPreferredUsable)
	{
		return Preferred;
	}

	UKeyBindingButton* Other = bPreferGamepad ? KeyboardBinding : GamepadBinding;
	return Other ? Other : Preferred;
}

void UKeyBindingRowWidget::HandleKeyboardClicked()
{
	OnBindingClicked.Broadcast(this, false);
}

void UKeyBindingRowWidget::HandleGamepadClicked()
{
	OnBindingClicked.Broadcast(this, true);
}
