// CoreFramework UI System
#include "UISystem/Menu/Tabs/BaseTabWidget.h"

#include "Components/Widget.h"

UBaseTabWidget::UBaseTabWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UBaseTabWidget::InitializeTab(const FUITabDescriptor& InDescriptor)
{
	Descriptor = InDescriptor;
	BP_OnTabInitialized(Descriptor.TabId, Descriptor.DisplayName);
}

void UBaseTabWidget::NotifyTabSelected()
{
	if (bTabSelected)
	{
		return;
	}
	bTabSelected = true;
	BP_OnTabSelected();
}

void UBaseTabWidget::NotifyTabDeselected()
{
	if (!bTabSelected)
	{
		return;
	}
	bTabSelected = false;
	BP_OnTabDeselected();
}

void UBaseTabWidget::FocusTabContent()
{
	if (DefaultFocusWidget)
	{
		DefaultFocusWidget->SetFocus();
	}
}

bool UBaseTabWidget::HasPendingChanges_Implementation() const
{
	return false;
}

void UBaseTabWidget::ApplyChanges_Implementation()
{
}

void UBaseTabWidget::RevertChanges_Implementation()
{
}
