// CoreFramework UI System - Settings
#pragma once

#include "CoreMinimal.h"
#include "UISystem/Foundation/BaseUserWidget.h"
#include "BaseSettingRowWidget.generated.h"

class UCommonTextBlock;
class UWidget;

/**
 * Base class of one line of a settings page: a label on the left, a control on the right (slider, option selector,
 * toggle, key binding). The framework ships the logic; the game's Widget Blueprints give the look, so one row Blueprint
 * per kind restyles every settings page.
 *
 * Shared behavior
 *  - Label: type it in the Label property of each placed row (visible in the designer), shown by LabelText.
 *  - Highlight: the optional widget FocusHighlight is visible while the row has keyboard / gamepad focus or the mouse,
 *    so the player always sees which line Left / Right will change. BP event On Row Highlight Changed for extra effects.
 *  - Focus: GetFocusTarget names the control that receives focus; FocusRow moves focus there.
 *
 * The Blueprint may contain, by these exact names (optional):
 *   LabelText       (Common Text)  the label
 *   FocusHighlight  (any widget)   a background bar or a marker shown while highlighted
 */
UCLASS(Abstract, Blueprintable)
class COREFRAMEWORK_API UBaseSettingRowWidget : public UBaseUserWidget
{
	GENERATED_BODY()

public:
	UBaseSettingRowWidget(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "Setting Row")
	void SetLabel(const FText& InLabel);

	UFUNCTION(BlueprintPure, Category = "Setting Row")
	FText GetLabel() const { return Label; }

	UFUNCTION(BlueprintPure, Category = "Setting Row")
	FText GetDescription() const { return Description; }

	/** Greys the row out and blocks its control (for example Resolution while the window mode is Borderless). */
	UFUNCTION(BlueprintCallable, Category = "Setting Row")
	void SetRowEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "Setting Row")
	bool IsRowHighlighted() const { return bHighlighted; }

	/** The control that receives keyboard / gamepad focus for this row. */
	UFUNCTION(BlueprintPure, Category = "Setting Row")
	virtual UWidget* GetFocusTarget() const;

	/** Moves keyboard / gamepad focus to the row's control. */
	UFUNCTION(BlueprintCallable, Category = "Setting Row")
	void FocusRow();

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent) override;
	virtual void NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent) override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

	/** Children enable / disable their own control here. */
	virtual void OnRowEnabledChanged(bool bEnabled) {}

	UFUNCTION(BlueprintImplementableEvent, Category = "Setting Row", meta = (DisplayName = "On Row Highlight Changed"))
	void BP_OnRowHighlightChanged(bool bIsHighlighted);

	/** Text of LabelText. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setting Row")
	FText Label;

	/** Optional longer explanation (tooltip, side panel). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setting Row", meta = (MultiLine = true))
	FText Description;

	/** Visibility of FocusHighlight while the row is highlighted. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setting Row")
	ESlateVisibility HighlightShownVisibility = ESlateVisibility::HitTestInvisible;

	/** Visibility of FocusHighlight otherwise. Hidden keeps the layout, Collapsed removes it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setting Row")
	ESlateVisibility HighlightHiddenVisibility = ESlateVisibility::Hidden;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> LabelText;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> FocusHighlight;

private:
	void RefreshHighlight();

	bool bHasFocus = false;
	bool bHovered = false;
	bool bHighlighted = false;
};
