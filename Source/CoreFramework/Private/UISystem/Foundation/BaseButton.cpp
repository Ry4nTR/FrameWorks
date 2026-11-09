// CoreFramework UI System - Foundation
#include "UISystem/Foundation/BaseButton.h"

#include "CommonTextBlock.h"
#include "AudioSystem/Core/AudioTypes.h"
#include "AudioSystem/Manager/AudioFrameSubsystem.h"

void UBaseButton::NativePreConstruct()
{
	Super::NativePreConstruct();

	// Runs in the editor preview too, so the label is visible while designing.
	ApplyLabel();
}

void UBaseButton::NativeOnHovered()
{
	Super::NativeOnHovered();

	// Null in the designer preview (no game instance), so nothing plays while editing.
	if (!bSilent)
	{
		if (UAudioFrameSubsystem* Audio = UAudioFrameSubsystem::Get(this))
		{
			Audio->PlayUIEvent(EAudioUIEvent::ButtonHover, HoverCue);
		}
	}
}

void UBaseButton::NativeOnClicked()
{
	Super::NativeOnClicked();

	if (!bSilent)
	{
		if (UAudioFrameSubsystem* Audio = UAudioFrameSubsystem::Get(this))
		{
			Audio->PlayUIEvent(EAudioUIEvent::ButtonClick, ClickCue);
		}
	}
}

void UBaseButton::SetButtonLabel(const FText& InLabel)
{
	ButtonLabel = InLabel;
	ApplyLabel();
}

void UBaseButton::ApplyLabel()
{
	if (LabelText)
	{
		LabelText->SetText(ButtonLabel);
	}
	BP_OnButtonLabelChanged(ButtonLabel);
}
