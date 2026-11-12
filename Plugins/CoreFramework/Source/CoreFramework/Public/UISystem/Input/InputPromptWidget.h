// CoreFramework UI System - Input
#pragma once

#include "CoreMinimal.h"
#include "CommonInputTypeEnum.h"
#include "Engine/DataTable.h"
#include "InputCoreTypes.h"
#include "UISystem/Foundation/BaseUserWidget.h"
#include "InputSystem/Core/InputFrameTypes.h"
#include "InputPromptWidget.generated.h"

class UCommonTextBlock;
class UImage;
class UInputAction;
class UInputFrameSubsystem;

/**
 * A button prompt that follows the player's device: the button icon (glyph) while a gamepad is used, plain key text
 * (or nothing) with keyboard and mouse. It updates by itself when the device changes and when keys are remapped.
 *
 * Where the key comes from (first match wins):
 *   1. Keyboard Key / Gamepad Key set on the widget (explicit, for fixed prompts),
 *   2. Input Action: the key the Enhanced Input action uses on that device, player remaps included,
 *   3. UI Action: a CommonUI input action row (the Back / Confirm rows of the CommonInput data table).
 * Icons come from the CommonUI controller data (Project Settings > Common Input Settings), for the gamepad family in use.
 *
 * Use it for menu footers (the Back prompt), world interaction prompts ("[A] Pick up" / "E  Pick up") and hints.
 *
 * The Blueprint may contain, by these exact names (all optional):
 *   GlyphImage  (Image)        the icon
 *   KeyText     (Common Text)  the key name, used when there is no icon or with Key Text display
 *   LabelText   (Common Text)  the action label ("Back", "Pick up")
 */
UCLASS(Abstract, Blueprintable)
class COREFRAMEWORK_API UInputPromptWidget : public UBaseUserWidget
{
	GENERATED_BODY()

public:
	UInputPromptWidget(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "Input Prompt")
	void SetInputAction(UInputAction* NewAction);

	UFUNCTION(BlueprintCallable, Category = "Input Prompt")
	void SetUIAction(FDataTableRowHandle NewUIAction);

	/** Explicit keys. Pass an invalid key to fall back to the input action / UI action for that device. */
	UFUNCTION(BlueprintCallable, Category = "Input Prompt")
	void SetKeys(FKey NewKeyboardKey, FKey NewGamepadKey);

	UFUNCTION(BlueprintCallable, Category = "Input Prompt")
	void SetLabel(const FText& NewLabel);

	/** Looks up the key and icon again (done automatically on device and binding changes). */
	UFUNCTION(BlueprintCallable, Category = "Input Prompt")
	void RefreshPrompt();

	/** The key shown now (invalid when none). */
	UFUNCTION(BlueprintPure, Category = "Input Prompt")
	FKey GetDisplayedKey() const { return DisplayedKey; }

	UFUNCTION(BlueprintPure, Category = "Input Prompt")
	bool IsShowingGlyph() const { return bShowingGlyph; }

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** After every refresh: Key is invalid when nothing is shown. Use it for custom visuals. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Input Prompt", meta = (DisplayName = "On Prompt Refreshed"))
	void BP_OnPromptRefreshed(FKey Key, bool bShowsGlyph, bool bGamepad);

	/** Enhanced Input action whose key is shown (player remaps included). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input Prompt")
	TObjectPtr<UInputAction> InputAction;

	/** CommonUI input action row (CommonInputActionDataBase), for UI actions such as Back or Confirm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input Prompt", meta = (RowType = "/Script/CommonUI.CommonInputActionDataBase"))
	FDataTableRowHandle UIAction;

	/** Fixed keyboard / mouse key. Wins over the actions. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input Prompt")
	FKey KeyboardKey;

	/** Fixed gamepad key. Wins over the actions. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input Prompt")
	FKey GamepadKey;

	/** Text of LabelText. Empty: LabelText is collapsed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input Prompt")
	FText Label;

	/** What is shown with keyboard and mouse. With a gamepad the icon is always shown (key text when there is none). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input Prompt")
	EInputPromptKeyDisplay KeyboardDisplay = EInputPromptKeyDisplay::ProjectDefault;

	/** Collapse the whole prompt when the current device has no key for it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input Prompt")
	bool bCollapseWhenNoKey = false;

	/** Size forced on the icon. 0 keeps the size of the controller data brush. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input Prompt")
	FVector2D GlyphSize = FVector2D(32.0, 32.0);

	/** The UMG designer shows the gamepad look (otherwise the keyboard look). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input Prompt")
	bool bPreviewGamepadInDesigner = true;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UImage> GlyphImage;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> KeyText;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> LabelText;

private:
	FKey ResolveKey(bool bGamepad, const UInputFrameSubsystem* Input) const;
	void ApplyPrompt(bool bGamepad, FName GamepadName, const UInputFrameSubsystem* Input);

	UFUNCTION()
	void HandleDeviceChanged(ECommonInputType InputType, FName GamepadName);

	UFUNCTION()
	void HandleBindingsChanged();

	TWeakObjectPtr<UInputFrameSubsystem> BoundInput;
	FKey DisplayedKey;
	bool bShowingGlyph = false;
};
