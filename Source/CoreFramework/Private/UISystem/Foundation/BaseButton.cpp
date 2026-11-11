// CoreFramework UI System - Foundation
#include "UISystem/Foundation/BaseButton.h"

#include "CommonTextBlock.h"
#include "Components/Widget.h"
#include "AudioSystem/Core/AudioTypes.h"
#include "AudioSystem/Manager/AudioFrameSubsystem.h"

void UBaseButton::NativePreConstruct()
{
	Super::NativePreConstruct();

	// Runs in the editor preview too, so the label and its style are visible while designing.
	ApplyLabel();
	RefreshVisualState();
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

	RefreshVisualState();
}

void UBaseButton::NativeOnUnhovered()
{
	Super::NativeOnUnhovered();
	RefreshVisualState();
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

void UBaseButton::NativeOnSelected(bool bBroadcast)
{
	Super::NativeOnSelected(bBroadcast);
	RefreshVisualState();
}

void UBaseButton::NativeOnDeselected(bool bBroadcast)
{
	Super::NativeOnDeselected(bBroadcast);
	RefreshVisualState();
}

void UBaseButton::NativeOnEnabled()
{
	Super::NativeOnEnabled();
	RefreshVisualState();
}

void UBaseButton::NativeOnDisabled()
{
	Super::NativeOnDisabled();
	RefreshVisualState();
}

void UBaseButton::NativeOnCurrentTextStyleChanged()
{
	Super::NativeOnCurrentTextStyleChanged();

	// CommonUI changed its idea of the current text style (state or style asset changed): apply ours.
	RefreshVisualState();
}

void UBaseButton::HandleFocusReceived()
{
	Super::HandleFocusReceived();
	bHasFocus = true;
	RefreshVisualState();
}

void UBaseButton::HandleFocusLost()
{
	Super::HandleFocusLost();
	bHasFocus = false;
	RefreshVisualState();
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

TSubclassOf<UCommonTextStyle> UBaseButton::ResolveLabelTextStyle() const
{
	const UCommonButtonStyle* ButtonStyle = GetStyleCDO();
	if (!ButtonStyle)
	{
		return nullptr;
	}

	if (!IsInteractionEnabled())
	{
		return ButtonStyle->DisabledTextStyle;
	}

	// A hovered slot left empty in the style falls back to the non-hovered one of the same state.
	if (GetSelected())
	{
		return (bHighlighted && ButtonStyle->SelectedHoveredTextStyle) ? ButtonStyle->SelectedHoveredTextStyle : ButtonStyle->SelectedTextStyle;
	}
	return (bHighlighted && ButtonStyle->NormalHoveredTextStyle) ? ButtonStyle->NormalHoveredTextStyle : ButtonStyle->NormalTextStyle;
}

void UBaseButton::RefreshVisualState()
{
	const bool bNewHighlighted = IsInteractionEnabled() && (IsHovered() || GetSelected() || (bHighlightOnFocus && bHasFocus));
	const bool bHighlightChanged = (bNewHighlighted != bHighlighted);
	bHighlighted = bNewHighlighted;

	if (bApplyStyleTextToLabel && LabelText)
	{
		// An empty slot in the style leaves the style already set on LabelText (set in the button Blueprint).
		if (const TSubclassOf<UCommonTextStyle> TextStyle = ResolveLabelTextStyle())
		{
			LabelText->SetStyle(TextStyle);
		}
	}

	if (SelectionIndicator)
	{
		SelectionIndicator->SetVisibility(bHighlighted ? IndicatorShownVisibility : IndicatorHiddenVisibility);
	}

	if (bHighlightChanged)
	{
		BP_OnHighlightChanged(bHighlighted);
	}
}
