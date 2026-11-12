// CoreFramework UI System - Settings
#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "UISystem/Foundation/BaseActivatableWidget.h"
#include "InputSystem/Core/InputFrameTypes.h"
#include "KeyBindingsScreenWidget.generated.h"

class UBaseButton;
class UBaseSettingRowWidget;
class UCommonTextBlock;
class UInputFrameSubsystem;
class UKeyBindingRowWidget;
class UPanelWidget;

/**
 * Screen that lists the player's key bindings and lets them change one: actions on the left, the keyboard / mouse key
 * and the gamepad button on the right. Open it from the Controls page (UControlsSettingsTabWidget > Key Bindings
 * Screen Tag) or from anywhere with PushScreen.
 *
 * Rebinding
 *   Clicking a box starts listening ("Press a key"): the next key of that device becomes the binding, a cancel key
 *   (Escape, gamepad View) or the timeout stops without changes. While listening no other input reaches the menu.
 *   Conflicts follow Project Settings > CoreFramework Input > Conflict Policy (Swap by default) and the result is written
 *   in StatusText. Bindings are saved by the Enhanced Input user settings (per player, its own save file).
 *
 * Data
 *   The rows come from UInputFrameSubsystem::GetKeyBindings: every player-mappable action of the Remappable Contexts.
 *   Name and category come from each Input Action's Player Mappable Key Settings (Display Name, Display Category).
 *
 * The Blueprint must contain, by this exact name:
 *   RowsContainer  (a panel: Vertical Box, Scroll Box...)  receives one row per action (Row Class)
 * Optional:
 *   ResetAllButton (UBaseButton)   puts every binding back to its default
 *   StatusText     (Common Text)   feedback ("Swapped with Jump", "Escape cannot be bound")
 */
UCLASS(Abstract, Blueprintable)
class COREFRAMEWORK_API UKeyBindingsScreenWidget : public UBaseActivatableWidget
{
	GENERATED_BODY()

public:
	UKeyBindingsScreenWidget(const FObjectInitializer& ObjectInitializer);

	/** Recreates every row from the current bindings. */
	UFUNCTION(BlueprintCallable, Category = "Key Bindings")
	void RebuildRows();

	/** Updates the keys shown by the existing rows (keeps focus). */
	UFUNCTION(BlueprintCallable, Category = "Key Bindings")
	void RefreshRows();

	UFUNCTION(BlueprintCallable, Category = "Key Bindings")
	void ResetAllBindings();

	/** Starts listening for a new key of one device for one row (what a click on a box does). */
	UFUNCTION(BlueprintCallable, Category = "Key Bindings")
	void BeginRebind(UKeyBindingRowWidget* Row, bool bGamepad);

	UFUNCTION(BlueprintPure, Category = "Key Bindings")
	bool IsListening() const { return ListeningRow.IsValid(); }

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual void NativeDestruct() override;
	virtual UWidget* ResolveDefaultFocusWidget() const override;

	/** After the rows were created. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Key Bindings", meta = (DisplayName = "On Rows Built"))
	void BP_OnRowsBuilt();

	/** After a rebind attempt (also cancelled ones). Message is what StatusText shows. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Key Bindings", meta = (DisplayName = "On Rebind Finished"))
	void BP_OnRebindFinished(EKeyRebindResult Result, const FText& Message);

	/** Widget Blueprint of one action (child of UKeyBindingRowWidget). Required. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Key Bindings")
	TSubclassOf<UKeyBindingRowWidget> RowClass;

	/** Optional header placed before each category (a setting row Blueprint whose Label shows the category). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Key Bindings")
	TSubclassOf<UBaseSettingRowWidget> CategoryHeaderClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Key Bindings")
	bool bShowKeyboardColumn = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Key Bindings")
	bool bShowGamepadColumn = true;

	// ---- Texts (Class Defaults) ------------------------------------------------------------------------
	// Placeholders: {Key} = the key name, {Action} = the other action's name.

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Key Bindings|Texts")
	FText ListeningKeyboardText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Key Bindings|Texts")
	FText ListeningGamepadText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Key Bindings|Texts")
	FText SwappedMessage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Key Bindings|Texts")
	FText UnboundMessage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Key Bindings|Texts")
	FText RejectedMessage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Key Bindings|Texts")
	FText ReservedKeyMessage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Key Bindings|Texts")
	FText ResetMessage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Key Bindings|Texts")
	FText UnavailableMessage;

	// ---- Widgets ---------------------------------------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget))
	TObjectPtr<UPanelWidget> RowsContainer;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseButton> ResetAllButton;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> StatusText;

private:
	UInputFrameSubsystem* GetInput() const;
	void SetStatus(const FText& Message);
	void StopListening();
	void FinishRebind(const FKey& Key, bool bCancelled);

	UFUNCTION()
	void HandleBindingClicked(UKeyBindingRowWidget* Row, bool bGamepad);

	UFUNCTION()
	void HandleBindingsChanged();

	void HandleResetAllClicked();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UKeyBindingRowWidget>> Rows;

	TWeakObjectPtr<UKeyBindingRowWidget> ListeningRow;
	TWeakObjectPtr<UInputFrameSubsystem> BoundInput;
	bool bListeningGamepad = false;
};
