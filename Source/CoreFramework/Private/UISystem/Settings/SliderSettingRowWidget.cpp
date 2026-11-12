// CoreFramework UI System - Settings
#include "UISystem/Settings/SliderSettingRowWidget.h"

#include "CommonTextBlock.h"
#include "Components/Slider.h"

#define LOCTEXT_NAMESPACE "CoreFrameworkSettings"

void USliderSettingRowWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// BindWidget: the Blueprint does not compile without ValueSlider.
	// Left / Right change the value at once (no "press Accept to lock" step for keyboard and gamepad players).
	ValueSlider->RequiresControllerLock = false;
	ApplyRangeToSlider();
	ValueSlider->OnValueChanged.AddUniqueDynamic(this, &USliderSettingRowWidget::HandleSliderValueChanged);
}

void USliderSettingRowWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (IsDesignTime() && ValueSlider)
	{
		ApplyRangeToSlider();
		Value = SnapValue(PreviewValue);
		ShowValue();
	}
}

void USliderSettingRowWidget::OnRowEnabledChanged(bool bEnabled)
{
	if (ValueSlider)
	{
		ValueSlider->SetLocked(!bEnabled);
	}
}

UWidget* USliderSettingRowWidget::GetFocusTarget() const
{
	return ValueSlider;
}

void USliderSettingRowWidget::ApplyRangeToSlider()
{
	if (!ValueSlider)
	{
		return;
	}
	if (MaxValue < MinValue)
	{
		Swap(MinValue, MaxValue);
	}

	TGuardValue<bool> Guard(bSyncingSlider, true);
	ValueSlider->SetMinValue(MinValue);
	ValueSlider->SetMaxValue(MaxValue);
	ValueSlider->SetStepSize(StepSize);
}

void USliderSettingRowWidget::SetRange(float InMinValue, float InMaxValue, float InStepSize)
{
	MinValue = InMinValue;
	MaxValue = InMaxValue;
	StepSize = FMath::Max(InStepSize, 0.0001f);
	ApplyRangeToSlider();
	SetValue(Value, false);
}

float USliderSettingRowWidget::SnapValue(float InValue) const
{
	const float Clamped = FMath::Clamp(InValue, MinValue, MaxValue);
	if (StepSize <= 0.0f)
	{
		return Clamped;
	}
	const float Steps = FMath::RoundToFloat((Clamped - MinValue) / StepSize);
	return FMath::Clamp(MinValue + Steps * StepSize, MinValue, MaxValue);
}

void USliderSettingRowWidget::SetValue(float NewValue, bool bBroadcast)
{
	Value = SnapValue(NewValue);
	ShowValue();

	if (bBroadcast)
	{
		OnValueChanged.Broadcast(this, Value);
	}
}

void USliderSettingRowWidget::HandleSliderValueChanged(float SliderValue)
{
	if (bSyncingSlider)
	{
		return;
	}

	const float Snapped = SnapValue(SliderValue);
	if (FMath::IsNearlyEqual(Snapped, Value))
	{
		// Dragging inside one step: keep the handle on the step.
		ShowValue();
		return;
	}

	Value = Snapped;
	ShowValue();
	OnValueChanged.Broadcast(this, Value);
}

void USliderSettingRowWidget::ShowValue()
{
	if (ValueSlider)
	{
		TGuardValue<bool> Guard(bSyncingSlider, true);
		ValueSlider->SetValue(Value);
	}
	if (ValueText)
	{
		ValueText->SetText(FormatValue(Value));
	}
}

FText USliderSettingRowWidget::FormatValue(float InValue) const
{
	FNumberFormattingOptions Options;
	Options.MinimumFractionalDigits = Decimals;
	Options.MaximumFractionalDigits = Decimals;

	switch (ValueFormat)
	{
	case ESettingValueFormat::Percent:
		return FText::Format(LOCTEXT("SettingPercent", "{0}%"), FText::AsNumber(FMath::RoundToInt(InValue * 100.0f)));
	case ESettingValueFormat::Integer:
		return FText::AsNumber(FMath::RoundToInt(InValue));
	case ESettingValueFormat::Multiplier:
		return FText::Format(LOCTEXT("SettingMultiplier", "{0}x"), FText::AsNumber(InValue, &Options));
	case ESettingValueFormat::Number:
	default:
		return FText::AsNumber(InValue, &Options);
	}
}

#undef LOCTEXT_NAMESPACE
