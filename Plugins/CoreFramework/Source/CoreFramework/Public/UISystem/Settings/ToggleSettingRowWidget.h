// CoreFramework UI System - Settings
#pragma once

#include "CoreMinimal.h"
#include "UISystem/Settings/OptionSettingRowWidget.h"
#include "ToggleSettingRowWidget.generated.h"

class UToggleSettingRowWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnToggleSettingChanged, UToggleSettingRowWidget*, Row, bool, bIsOn);

/**
 * Settings line for an On / Off value: "Vibration   <  On  >". An option row with two values (Off, On), so it looks and
 * navigates like every other option: Left / Right or Accept / click switch it.
 * For a switch-style look, use the BP event On Selection Applied (index 1 = On) or Is On.
 *
 * Same widget names as UOptionSettingRowWidget.
 */
UCLASS(Abstract, Blueprintable)
class COREFRAMEWORK_API UToggleSettingRowWidget : public UOptionSettingRowWidget
{
	GENERATED_BODY()

public:
	UToggleSettingRowWidget(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "Setting Row|Toggle")
	void SetIsOn(bool bOn, bool bBroadcast = false);

	UFUNCTION(BlueprintPure, Category = "Setting Row|Toggle")
	bool IsOn() const { return GetSelectedIndex() == 1; }

	/** Player action (broadcasts). */
	UFUNCTION(BlueprintCallable, Category = "Setting Row|Toggle")
	void Toggle();

	/** The player switched it (or SetIsOn with bBroadcast). */
	UPROPERTY(BlueprintAssignable, Category = "Setting Row|Toggle")
	FOnToggleSettingChanged OnToggled;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativePreConstruct() override;
	virtual void OnSelectionApplied(bool bUserInitiated) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setting Row|Toggle")
	FText OffText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setting Row|Toggle")
	FText OnText;

private:
	void RebuildOptions();
};
