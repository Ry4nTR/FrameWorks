// CoreFramework UI System - Settings
#include "UISystem/Settings/ToggleSettingRowWidget.h"

#define LOCTEXT_NAMESPACE "CoreFrameworkSettings"

UToggleSettingRowWidget::UToggleSettingRowWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	OffText = LOCTEXT("ToggleOff", "Off");
	OnText = LOCTEXT("ToggleOn", "On");

	// Two values: Left / Right and Accept always switch.
	bWrapAround = true;
	bClickSelectsNext = true;
}

void UToggleSettingRowWidget::RebuildOptions()
{
	const int32 Current = FMath::Clamp(GetSelectedIndex(), 0, 1);
	SetOptions({ OffText, OnText }, Current);
}

void UToggleSettingRowWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	RebuildOptions();
}

void UToggleSettingRowWidget::NativePreConstruct()
{
	RebuildOptions();
	Super::NativePreConstruct();
}

void UToggleSettingRowWidget::SetIsOn(bool bOn, bool bBroadcast)
{
	SetSelectedIndex(bOn ? 1 : 0, bBroadcast);
}

void UToggleSettingRowWidget::Toggle()
{
	SelectNext();
}

void UToggleSettingRowWidget::OnSelectionApplied(bool bUserInitiated)
{
	Super::OnSelectionApplied(bUserInitiated);

	if (bUserInitiated)
	{
		OnToggled.Broadcast(this, IsOn());
	}
}

#undef LOCTEXT_NAMESPACE
