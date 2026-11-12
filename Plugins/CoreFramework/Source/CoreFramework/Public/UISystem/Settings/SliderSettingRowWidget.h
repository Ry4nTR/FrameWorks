// CoreFramework UI System - Settings
#pragma once

#include "CoreMinimal.h"
#include "UISystem/Settings/BaseSettingRowWidget.h"
#include "SliderSettingRowWidget.generated.h"

class USlider;
class USliderSettingRowWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSliderSettingChanged, USliderSettingRowWidget*, Row, float, Value);

/** How the value text of a slider row is written. */
UENUM(BlueprintType)
enum class ESettingValueFormat : uint8
{
	/** Value x 100 with a % sign (0.8 -> "80%"). */
	Percent,

	/** The value with Decimals digits ("1.5"). */
	Number,

	/** The value rounded ("12"). */
	Integer,

	/** The value with Decimals digits and an x ("1.5x"). */
	Multiplier
};

/**
 * Settings line with a slider and its value: "Mouse Sensitivity   ----o----   1.2x".
 * The value snaps to Step Size; Left / Right (keys, D-pad, stick) move it by one step without locking first.
 * Set Min / Max / Step in the Details panel of each placed row, or from code with SetRange.
 *
 * The Blueprint must contain, by this exact name:
 *   ValueSlider  (Slider)          the control (its own min / max / step are overwritten by this class)
 * Optional:
 *   ValueText    (Common Text)     the formatted value
 *   plus LabelText / FocusHighlight of UBaseSettingRowWidget.
 */
UCLASS(Abstract, Blueprintable)
class COREFRAMEWORK_API USliderSettingRowWidget : public UBaseSettingRowWidget
{
	GENERATED_BODY()

public:
	/** Sets the value (snapped and clamped). bBroadcast fires On Value Changed as if the player moved it. */
	UFUNCTION(BlueprintCallable, Category = "Setting Row|Slider")
	void SetValue(float NewValue, bool bBroadcast = false);

	UFUNCTION(BlueprintPure, Category = "Setting Row|Slider")
	float GetValue() const { return Value; }

	UFUNCTION(BlueprintCallable, Category = "Setting Row|Slider")
	void SetRange(float InMinValue, float InMaxValue, float InStepSize);

	/** The player moved the slider (or SetValue with bBroadcast). */
	UPROPERTY(BlueprintAssignable, Category = "Setting Row|Slider")
	FOnSliderSettingChanged OnValueChanged;

	virtual UWidget* GetFocusTarget() const override;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativePreConstruct() override;
	virtual void OnRowEnabledChanged(bool bEnabled) override;

	/** The value text. Override for a custom format. */
	virtual FText FormatValue(float InValue) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setting Row|Slider")
	float MinValue = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setting Row|Slider")
	float MaxValue = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setting Row|Slider", meta = (ClampMin = "0.0001"))
	float StepSize = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setting Row|Slider")
	ESettingValueFormat ValueFormat = ESettingValueFormat::Percent;

	/** Digits after the decimal point for Number and Multiplier. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setting Row|Slider", meta = (ClampMin = "0", ClampMax = "4"))
	int32 Decimals = 1;

	/** Value shown in the designer preview. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setting Row|Slider")
	float PreviewValue = 0.5f;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget))
	TObjectPtr<USlider> ValueSlider;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> ValueText;

private:
	UFUNCTION()
	void HandleSliderValueChanged(float SliderValue);

	float SnapValue(float InValue) const;
	void ApplyRangeToSlider();
	void ShowValue();

	float Value = 0.0f;
	bool bSyncingSlider = false;
};
