// CoreFramework UI System - Settings
#include "UISystem/Settings/OptionSettingRowWidget.h"

#include "CommonTextBlock.h"
#include "UISystem/Foundation/BaseButton.h"

void UOptionSettingRowWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	NavigationDelegate.BindUObject(this, &UOptionSettingRowWidget::HandleNavigation);

	// BindWidget: the Blueprint does not compile without SelectorButton.
	SelectorButton->OnClicked().AddUObject(this, &UOptionSettingRowWidget::HandleSelectorClicked);

	// The arrows are for the mouse only: keyboard / gamepad change the value with Left / Right on the selector.
	if (PreviousButton)
	{
		PreviousButton->SetIsFocusable(false);
		PreviousButton->OnClicked().AddUObject(this, &UOptionSettingRowWidget::HandlePreviousClicked);
	}
	if (NextButton)
	{
		NextButton->SetIsFocusable(false);
		NextButton->OnClicked().AddUObject(this, &UOptionSettingRowWidget::HandleNextClicked);
	}
}

void UOptionSettingRowWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	ApplySelection(false);
}

UWidget* UOptionSettingRowWidget::GetFocusTarget() const
{
	return SelectorButton;
}

void UOptionSettingRowWidget::OnRowEnabledChanged(bool bEnabled)
{
	ApplySelection(false);
}

FNavigationReply UOptionSettingRowWidget::NativeOnNavigation(const FGeometry& MyGeometry, const FNavigationEvent& InNavigationEvent, const FNavigationReply& InDefaultReply)
{
	// Navigation bubbles up from the focused SelectorButton: Left / Right change the value, Up / Down leave the row.
	const EUINavigation Direction = InNavigationEvent.GetNavigationType();
	if ((Direction == EUINavigation::Left || Direction == EUINavigation::Right) && GetIsEnabled() && Options.Num() > 0)
	{
		return FNavigationReply::Custom(NavigationDelegate);
	}
	return Super::NativeOnNavigation(MyGeometry, InNavigationEvent, InDefaultReply);
}

TSharedPtr<SWidget> UOptionSettingRowWidget::HandleNavigation(EUINavigation Direction)
{
	Step(Direction == EUINavigation::Left ? -1 : 1);
	return nullptr; // Focus stays on the selector.
}

void UOptionSettingRowWidget::HandleSelectorClicked()
{
	if (bClickSelectsNext)
	{
		Step(1);
	}
}

void UOptionSettingRowWidget::HandlePreviousClicked()
{
	Step(-1);
}

void UOptionSettingRowWidget::HandleNextClicked()
{
	Step(1);
}

void UOptionSettingRowWidget::SelectNext()
{
	Step(1);
}

void UOptionSettingRowWidget::SelectPrevious()
{
	Step(-1);
}

void UOptionSettingRowWidget::Step(int32 Direction)
{
	const int32 Count = Options.Num();
	if (Count == 0 || !GetIsEnabled())
	{
		return;
	}

	int32 NewIndex = SelectedIndex + Direction;
	if (bWrapAround)
	{
		NewIndex = (NewIndex % Count + Count) % Count;
	}
	else
	{
		NewIndex = FMath::Clamp(NewIndex, 0, Count - 1);
	}

	if (NewIndex == SelectedIndex)
	{
		return;
	}

	SelectedIndex = NewIndex;
	ApplySelection(true);
	OnSelectionChanged.Broadcast(this, SelectedIndex);
}

void UOptionSettingRowWidget::SetOptions(const TArray<FText>& InOptions, int32 InSelectedIndex)
{
	Options = InOptions;
	SelectedIndex = Options.Num() > 0 ? FMath::Clamp(InSelectedIndex, 0, Options.Num() - 1) : 0;
	ApplySelection(false);
}

void UOptionSettingRowWidget::SetSelectedIndex(int32 NewIndex, bool bBroadcast)
{
	if (Options.Num() == 0)
	{
		return;
	}

	SelectedIndex = FMath::Clamp(NewIndex, 0, Options.Num() - 1);
	ApplySelection(bBroadcast);
	if (bBroadcast)
	{
		OnSelectionChanged.Broadcast(this, SelectedIndex);
	}
}

FText UOptionSettingRowWidget::GetSelectedText() const
{
	return Options.IsValidIndex(SelectedIndex) ? Options[SelectedIndex] : FText::GetEmpty();
}

void UOptionSettingRowWidget::ApplySelection(bool bUserInitiated)
{
	const FText Text = GetSelectedText();
	if (ValueText)
	{
		ValueText->SetText(Text);
	}
	else if (SelectorButton)
	{
		SelectorButton->SetButtonLabel(Text);
	}

	// Without wrap-around the arrow at the end of the list is disabled, so the player sees the limit.
	const bool bEnabled = GetIsEnabled() && Options.Num() > 1;
	if (PreviousButton)
	{
		PreviousButton->SetIsEnabled(bEnabled && (bWrapAround || SelectedIndex > 0));
	}
	if (NextButton)
	{
		NextButton->SetIsEnabled(bEnabled && (bWrapAround || SelectedIndex < Options.Num() - 1));
	}

	OnSelectionApplied(bUserInitiated);
	BP_OnSelectionApplied(SelectedIndex, bUserInitiated);
}

void UOptionSettingRowWidget::OnSelectionApplied(bool bUserInitiated)
{
}
