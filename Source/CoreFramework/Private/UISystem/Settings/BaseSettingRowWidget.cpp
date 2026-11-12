// CoreFramework UI System - Settings
#include "UISystem/Settings/BaseSettingRowWidget.h"

#include "CommonTextBlock.h"
#include "Components/Widget.h"

UBaseSettingRowWidget::UBaseSettingRowWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// A settings row never reacts to the pawn; the row itself is not focusable, its control is.
	bAutoRegisterAsBinder = false;
	SetIsFocusable(false);
}

void UBaseSettingRowWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (LabelText)
	{
		LabelText->SetText(Label);
	}
	if (FocusHighlight)
	{
		FocusHighlight->SetVisibility(HighlightHiddenVisibility);
	}
}

void UBaseSettingRowWidget::SetLabel(const FText& InLabel)
{
	Label = InLabel;
	if (LabelText)
	{
		LabelText->SetText(Label);
	}
}

void UBaseSettingRowWidget::SetRowEnabled(bool bEnabled)
{
	SetIsEnabled(bEnabled);
	OnRowEnabledChanged(bEnabled);
}

UWidget* UBaseSettingRowWidget::GetFocusTarget() const
{
	return nullptr;
}

void UBaseSettingRowWidget::FocusRow()
{
	if (UWidget* Target = GetFocusTarget())
	{
		Target->SetFocus();
	}
}

void UBaseSettingRowWidget::NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnAddedToFocusPath(InFocusEvent);
	bHasFocus = true;
	RefreshHighlight();
}

void UBaseSettingRowWidget::NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnRemovedFromFocusPath(InFocusEvent);
	bHasFocus = false;
	RefreshHighlight();
}

void UBaseSettingRowWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	bHovered = true;
	RefreshHighlight();
}

void UBaseSettingRowWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	bHovered = false;
	RefreshHighlight();
}

void UBaseSettingRowWidget::RefreshHighlight()
{
	const bool bNewHighlighted = (bHasFocus || bHovered) && GetIsEnabled();
	if (bNewHighlighted == bHighlighted)
	{
		return;
	}

	bHighlighted = bNewHighlighted;
	if (FocusHighlight)
	{
		FocusHighlight->SetVisibility(bHighlighted ? HighlightShownVisibility : HighlightHiddenVisibility);
	}
	BP_OnRowHighlightChanged(bHighlighted);
}
