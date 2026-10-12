// CoreFramework UI System - Foundation
#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "BaseButton.generated.h"

class UCommonTextBlock;

/**
 * Parent class for every button of a game project (menu buttons, tab buttons, popup buttons).
 *
 * It solves one beginner trap of CommonUI: giving each button instance its own text.
 *  - Add a Common Text widget named exactly LabelText inside the button Blueprint.
 *  - Type the text in the Button Label property of each placed button (visible in the Designer preview).
 *  - Change it at runtime with SetButtonLabel.
 * Appearance (colors, hover, pressed, selected) comes from the CommonButtonStyle asset the Blueprint
 * references. The framework ships no style and no art.
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

protected:
	virtual void NativePreConstruct() override;

	/** Fired whenever the label text is applied (designer preview, construct, SetButtonLabel). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Base UI|Button", meta = (DisplayName = "On Button Label Changed"))
	void BP_OnButtonLabelChanged(const FText& InLabel);

	/** Text shown by the button. Editable on every placed instance in the Details panel. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base UI|Button")
	FText ButtonLabel;

	/** Optional Common Text widget named exactly LabelText. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> LabelText;

private:
	void ApplyLabel();
};
