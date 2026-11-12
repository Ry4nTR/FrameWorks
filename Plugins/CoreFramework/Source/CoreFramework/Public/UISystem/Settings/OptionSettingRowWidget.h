// CoreFramework UI System - Settings
#pragma once

#include "CoreMinimal.h"
#include "Input/NavigationReply.h"
#include "UISystem/Settings/BaseSettingRowWidget.h"
#include "OptionSettingRowWidget.generated.h"

class UBaseButton;
class UOptionSettingRowWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnOptionSettingChanged, UOptionSettingRowWidget*, Row, int32, SelectedIndex);

/**
 * Settings line that picks one value from a list with left / right arrows: "Shadows   <  High  >".
 *
 * Input
 *  - Keyboard / gamepad: focus is on SelectorButton; Left / Right (arrows, D-pad, stick) change the value, Up / Down
 *    move to the next row. Accept (Enter / A) selects the next value when bClickSelectsNext is on.
 *  - Mouse: click PreviousButton / NextButton (they never take focus), or the value itself.
 *
 * The Blueprint must contain, by this exact name:
 *   SelectorButton  (a UBaseButton Blueprint)  focusable; shows the value in its own LabelText unless ValueText exists
 * Optional:
 *   PreviousButton, NextButton  (UBaseButton Blueprints)  the arrows ("<" ">" or arrow icons)
 *   ValueText                   (Common Text)            the value, when it should not be the button's label
 *   plus LabelText / FocusHighlight of UBaseSettingRowWidget.
 */
UCLASS(Abstract, Blueprintable)
class COREFRAMEWORK_API UOptionSettingRowWidget : public UBaseSettingRowWidget
{
	GENERATED_BODY()

public:
	/** Replaces the list of values. */
	UFUNCTION(BlueprintCallable, Category = "Setting Row|Option")
	void SetOptions(const TArray<FText>& InOptions, int32 InSelectedIndex = 0);

	/** Selects a value. bBroadcast fires On Selection Changed as if the player changed it. */
	UFUNCTION(BlueprintCallable, Category = "Setting Row|Option")
	void SetSelectedIndex(int32 NewIndex, bool bBroadcast = false);

	UFUNCTION(BlueprintPure, Category = "Setting Row|Option")
	int32 GetSelectedIndex() const { return SelectedIndex; }

	UFUNCTION(BlueprintPure, Category = "Setting Row|Option")
	int32 GetNumOptions() const { return Options.Num(); }

	UFUNCTION(BlueprintPure, Category = "Setting Row|Option")
	FText GetSelectedText() const;

	/** Player actions (they broadcast). */
	UFUNCTION(BlueprintCallable, Category = "Setting Row|Option")
	void SelectNext();

	UFUNCTION(BlueprintCallable, Category = "Setting Row|Option")
	void SelectPrevious();

	/** The player changed the value (or SetSelectedIndex with bBroadcast). */
	UPROPERTY(BlueprintAssignable, Category = "Setting Row|Option")
	FOnOptionSettingChanged OnSelectionChanged;

	virtual UWidget* GetFocusTarget() const override;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativePreConstruct() override;
	virtual FNavigationReply NativeOnNavigation(const FGeometry& MyGeometry, const FNavigationEvent& InNavigationEvent, const FNavigationReply& InDefaultReply) override;
	virtual void OnRowEnabledChanged(bool bEnabled) override;

	/** After the selection changed (by the player or by code). Children react here. */
	virtual void OnSelectionApplied(bool bUserInitiated);

	/** For custom visuals (a switch, dots under the value). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Setting Row|Option", meta = (DisplayName = "On Selection Applied"))
	void BP_OnSelectionApplied(int32 Index, bool bUserInitiated);

	/** The values. Settings tabs usually fill them from code. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setting Row|Option")
	TArray<FText> Options;

	/** Index shown before code sets one (designer preview). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setting Row|Option", meta = (ClampMin = "0"))
	int32 SelectedIndex = 0;

	/** Right on the last value goes to the first (and the opposite). Off: the arrow at the end is disabled. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setting Row|Option")
	bool bWrapAround = false;

	/** Accept / click on the value selects the next one. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setting Row|Option")
	bool bClickSelectsNext = true;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget))
	TObjectPtr<UBaseButton> SelectorButton;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseButton> PreviousButton;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseButton> NextButton;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> ValueText;

private:
	void Step(int32 Direction);
	void ApplySelection(bool bUserInitiated);
	TSharedPtr<SWidget> HandleNavigation(EUINavigation Direction);

	void HandleSelectorClicked();
	void HandlePreviousClicked();
	void HandleNextClicked();

	/** Answers Left / Right navigation by changing the value instead of moving focus. */
	FNavigationDelegate NavigationDelegate;
};
