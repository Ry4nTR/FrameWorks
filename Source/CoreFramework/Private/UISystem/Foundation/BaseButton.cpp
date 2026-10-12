// CoreFramework UI System - Foundation
#include "UISystem/Foundation/BaseButton.h"

#include "CommonTextBlock.h"

void UBaseButton::NativePreConstruct()
{
	Super::NativePreConstruct();

	// Runs in the editor preview too, so the label is visible while designing.
	ApplyLabel();
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
