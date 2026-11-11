// CoreFramework UI System - Foundation
#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "GameplayTagContainer.h"
#include "BaseButton.generated.h"

class UCommonTextBlock;
class UCommonTextStyle;
class UWidget;

/**
 * Parent class for every button of a game project (menu buttons, tab buttons, popup buttons).
 *
 * It solves one beginner trap of CommonUI: giving each button instance its own text.
 *  - Add a Common Text widget named exactly LabelText inside the button Blueprint.
 *  - Type the text in the Button Label property of each placed button (visible in the Designer preview).
 *  - Change it at runtime with SetButtonLabel.
 * Appearance (colors, hover, pressed, selected) comes from the CommonButtonStyle asset the Blueprint
 * references. The framework ships no style and no art.
 *
 * Text style per state (one style asset drives every button):
 *  CommonUI computes a "current text style" from the button style (Normal / Normal Hovered / Selected /
 *  Selected Hovered / Disabled Text Style) but applies it to nothing. With bApplyStyleTextToLabel (default on)
 *  this class applies it to LabelText whenever the state changes, so editing the CommonButtonStyle or the
 *  CommonTextStyle assets restyles every button that uses them, with no Blueprint graph. A slot left empty in the
 *  style keeps the text style already set on LabelText.
 *
 * Highlight (hover, keyboard / gamepad focus, selection):
 *  CommonUI shows the hovered look only under the mouse. With bHighlightOnFocus (default on) a focused button is
 *  "highlighted" too, so keyboard and controller navigation show the same feedback as the mouse: hovered text
 *  style, and the optional widget named exactly SelectionIndicator (a bar, an arrow, a glow) becomes visible.
 *  IsHighlighted and the event On Highlight Changed expose the state to Blueprint (animations, extra effects).
 *
 * Sounds: hover and click play through the audio system (UAudioFrameSubsystem, UI category). Each button can name its
 * own cues; an empty cue uses the project default of the audio config. Leave the Hovered / Clicked sound slots of the
 * CommonUI button style empty, otherwise a second sound plays next to this one.
 */
UCLASS(Abstract, Blueprintable)
class COREFRAMEWORK_API UBaseButton : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Base UI|Button")
	void SetButtonLabel(const FText& InLabel);

	UFUNCTION(BlueprintPure, Category = "Base UI|Button")
	FText GetButtonLabel() const { return ButtonLabel; }

	/** True while the button is hovered, focused (bHighlightOnFocus) or selected, and enabled. */
	UFUNCTION(BlueprintPure, Category = "Base UI|Button")
	bool IsHighlighted() const { return bHighlighted; }

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeOnHovered() override;
	virtual void NativeOnUnhovered() override;
	virtual void NativeOnClicked() override;
	virtual void NativeOnSelected(bool bBroadcast) override;
	virtual void NativeOnDeselected(bool bBroadcast) override;
	virtual void NativeOnEnabled() override;
	virtual void NativeOnDisabled() override;
	virtual void NativeOnCurrentTextStyleChanged() override;
	virtual void HandleFocusReceived() override;
	virtual void HandleFocusLost() override;

	/** Cue played on hover. Empty uses Default Button Hover Cue of the audio config. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base UI|Audio")
	FGameplayTag HoverCue;

	/** Cue played on click. Empty uses Default Button Click Cue of the audio config. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base UI|Audio")
	FGameplayTag ClickCue;

	/** No sounds at all for this button (neither its own cues nor the project defaults). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base UI|Audio")
	bool bSilent = false;

	/** Fired whenever the label text is applied (designer preview, construct, SetButtonLabel). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Base UI|Button", meta = (DisplayName = "On Button Label Changed"))
	void BP_OnButtonLabelChanged(const FText& InLabel);

	/** Fired when the highlight (hover, focus, selection) turns on or off. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Base UI|Button", meta = (DisplayName = "On Highlight Changed"))
	void BP_OnHighlightChanged(bool bIsHighlighted);

	/** Text shown by the button. Editable on every placed instance in the Details panel. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base UI|Button")
	FText ButtonLabel;

	/**
	 * Apply the text style of the current state (from the CommonButtonStyle of this button) to LabelText.
	 * Turn it off only for a button whose Blueprint styles its text by itself.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base UI|Button")
	bool bApplyStyleTextToLabel = true;

	/** Keyboard / gamepad focus highlights the button like the mouse hover does (hovered text style, SelectionIndicator). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base UI|Button")
	bool bHighlightOnFocus = true;

	/** Visibility of SelectionIndicator while the button is highlighted. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base UI|Button")
	ESlateVisibility IndicatorShownVisibility = ESlateVisibility::HitTestInvisible;

	/** Visibility of SelectionIndicator while the button is not highlighted. Hidden keeps the layout, Collapsed removes it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base UI|Button")
	ESlateVisibility IndicatorHiddenVisibility = ESlateVisibility::Hidden;

	/** Optional Common Text widget named exactly LabelText. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> LabelText;

	/** Optional widget named exactly SelectionIndicator. Visible only while the button is highlighted. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> SelectionIndicator;

	/** The text style LabelText should use now (state of the button + highlight). Null = leave LabelText as it is. */
	virtual TSubclassOf<UCommonTextStyle> ResolveLabelTextStyle() const;

private:
	void ApplyLabel();

	/** Recomputes the highlight and applies text style and indicator when anything changed. */
	void RefreshVisualState();

	bool bHasFocus = false;
	bool bHighlighted = false;
};
