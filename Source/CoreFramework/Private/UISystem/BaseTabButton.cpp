// CoreFramework UI System
#include "UISystem/BaseTabButton.h"

#include "CommonTextBlock.h"

void UBaseTabButton::SetTabLabel(const FText& InLabel)
{
	if (TabLabelText)
	{
		TabLabelText->SetText(InLabel);
	}
	BP_OnTabLabelChanged(InLabel);
}
