// CoreFramework UI System - Settings
#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "UISystem/Foundation/BaseButton.h"
#include "KeyBindingButton.generated.h"

class UImage;

/**
 * The box of a key binding: shows the bound key (icon for gamepad buttons, key name for keyboard / mouse) and the
 * "press a key" text while it listens. Clicking it is handled by the row (UKeyBindingRowWidget).
 *
 * The Blueprint may contain, by these exact names:
 *   LabelText  (Common Text, from UBaseButton)  the key name or the listening text
 *   KeyGlyph   (Image)                         the key icon (optional)
 */
UCLASS(Abstract, Blueprintable)
class COREFRAMEWORK_API UKeyBindingButton : public UBaseButton
{
	GENERATED_BODY()

public:
	UKeyBindingButton(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "Key Binding")
	void SetKey(const FKey& InKey);

	UFUNCTION(BlueprintPure, Category = "Key Binding")
	FKey GetKey() const { return Key; }

	/** Shows ListeningText instead of the key while the player chooses a new one. */
	UFUNCTION(BlueprintCallable, Category = "Key Binding")
	void SetListening(bool bInListening, const FText& ListeningText);

	UFUNCTION(BlueprintPure, Category = "Key Binding")
	bool IsListening() const { return bListening; }

protected:
	virtual void NativePreConstruct() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Key Binding", meta = (DisplayName = "On Key Display Changed"))
	void BP_OnKeyDisplayChanged(FKey DisplayedKey, bool bIsListening, bool bShowsGlyph);

	/** Gamepad buttons show their icon (CommonUI controller data); off = always text. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Key Binding")
	bool bGlyphForGamepadKeys = true;

	/** Keyboard keys show an icon when the controller data has one; off = key name (clean text on PC). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Key Binding")
	bool bGlyphForKeyboardKeys = false;

	/** Shown when the action has no key on this device. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Key Binding")
	FText UnboundText;

	/** Size forced on the icon. 0 keeps the controller data size. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Key Binding")
	FVector2D GlyphSize = FVector2D(32.0, 32.0);

	/** Key shown in the designer preview. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Key Binding")
	FKey PreviewKey;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UImage> KeyGlyph;

private:
	void RefreshDisplay();

	FKey Key;
	FText CurrentListeningText;
	bool bListening = false;
};
