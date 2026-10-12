// CoreFramework UI System
#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Engine/DataTable.h"
#include "Engine/GameViewportClient.h"
#include "GameplayTagContainer.h"
#include "Input/UIActionBindingHandle.h"
#include "UISystem/Binding/BaseUIBinderInterface.h"
#include "UISystem/Core/UITypes.h"
#include "BaseActivatableWidget.generated.h"

class APawn;
class APlayerController;
class APlayerState;
class UUIFrameSubsystem;
class UWidget;
class UWidgetAnimation;

/**
 * Base class for every screen that lives on a layer stack (menus, popups, HUD, notifications).
 * Built on CommonUI's UCommonActivatableWidget so activation, back handling, input routing and
 * gamepad / keyboard focus are handled by the engine.
 *
 * Adds on top of CommonUI:
 *  - Declarative input mode (GameOnly / UIOnly / GameAndUI) + mouse capture, applied automatically.
 *  - Optional look / move input suppression and game pause while the screen is active.
 *  - Default focus target fallback (DefaultFocusWidget) for controller / keyboard navigation.
 *  - Activate / deactivate animation hook (Anim_Activate / Anim_Deactivate).
 *  - Binding hub registration (IBaseUIBinderInterface), screen / layer identity, CloseScreen().
 *
 * Back action: tick "Is Back Handler" in the Blueprint class defaults of screens that Escape / gamepad B should close.
 * Modal screens: tick "Is Modal" in the class defaults.
 */
UCLASS(Abstract, Blueprintable)
class COREFRAMEWORK_API UBaseActivatableWidget : public UCommonActivatableWidget, public IBaseUIBinderInterface
{
	GENERATED_BODY()

public:
	UBaseActivatableWidget(const FObjectInitializer& ObjectInitializer);

	//~ Begin UCommonActivatableWidget
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
	//~ End UCommonActivatableWidget

	/** Called by the UI subsystem before activation. */
	void SetScreenContext(FGameplayTag InScreenTag, FGameplayTag InLayerTag);

	UFUNCTION(BlueprintPure, Category = "Base UI|Screen")
	FGameplayTag GetScreenTag() const { return ScreenTag; }

	UFUNCTION(BlueprintPure, Category = "Base UI|Screen")
	FGameplayTag GetLayerTag() const { return LayerTag; }

	/** True from the moment the subsystem pushed this screen until it deactivates. */
	UFUNCTION(BlueprintPure, Category = "Base UI|Screen")
	bool IsScreenOpen() const { return bScreenOpen; }

	/** Removes this screen from its layer through the UI subsystem. Safe to call from Blueprint buttons. */
	UFUNCTION(BlueprintCallable, Category = "Base UI|Screen")
	void CloseScreen();

	UFUNCTION(BlueprintPure, Category = "Base UI")
	UUIFrameSubsystem* GetUIFrameSubsystem() const;

	UFUNCTION(BlueprintPure, Category = "Base UI|Binding")
	APlayerController* GetBoundController() const;

	UFUNCTION(BlueprintPure, Category = "Base UI|Binding")
	APawn* GetBoundPawn() const;

	UFUNCTION(BlueprintPure, Category = "Base UI|Binding")
	APlayerState* GetBoundPlayerState() const;

	template <typename PawnType>
	PawnType* GetBoundPawnAs() const
	{
		return Cast<PawnType>(GetBoundPawn());
	}

	UFUNCTION(BlueprintPure, Category = "Base UI|Input")
	bool IsUsingGamepad() const;

	/** Moves keyboard / gamepad focus to the default focus widget now. */
	UFUNCTION(BlueprintCallable, Category = "Base UI|Input")
	void RequestDefaultFocus();

	/** Binds a CommonUI input action row. Automatically unbound when the widget is destroyed. */
	UFUNCTION(BlueprintCallable, Category = "Base UI|Input")
	void BindUIAction(const FDataTableRowHandle& ActionRow, bool bShowInActionBar, FBaseUIActionDelegate OnExecuted);

	FUIActionBindingHandle BindUIActionNative(const FDataTableRowHandle& ActionRow, bool bShowInActionBar, const FSimpleDelegate& OnExecuted);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;

	/** Widget that receives focus when the screen opens. Falls back to the Blueprint "Get Desired Focus Target" if implemented. */
	virtual UWidget* ResolveDefaultFocusWidget() const;

	// ---- Input -----------------------------------------------------------------------------------

	/** Input routing requested while this screen is the topmost active screen. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base UI|Input")
	EUIInputMode InputMode = EUIInputMode::UIOnly;

	/** Mouse capture used when the game receives input (GameOnly / GameAndUI). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base UI|Input")
	EMouseCaptureMode GameMouseCaptureMode = EMouseCaptureMode::CapturePermanently;

	/** Stops camera look input while active. Use with GameAndUI so the mouse does not turn the camera. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base UI|Input")
	bool bIgnoreLookInputWhileActive = false;

	/** Stops movement input while active. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base UI|Input")
	bool bIgnoreMoveInputWhileActive = false;

	/** Pauses the game while active (single player / listen server host). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base UI|Input")
	bool bPauseGameWhileActive = false;

	/** Optional widget named exactly DefaultFocusWidget. Receives focus on activation. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> DefaultFocusWidget;

	// ---- Binding ---------------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base UI|Binding")
	bool bAutoRegisterAsBinder = true;

	// ---- Transitions -----------------------------------------------------------------------------

	/** Optional UMG animation named exactly Anim_Activate. Played on activation. */
	UPROPERTY(Transient, BlueprintReadOnly, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_Activate;

	/** Optional UMG animation named exactly Anim_Deactivate. Played on deactivation. */
	UPROPERTY(Transient, BlueprintReadOnly, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_Deactivate;

	void ScheduleDefaultFocus();

private:
	void ApplyDefaultFocus();
	void ApplyActiveSideEffects();
	void ReleaseActiveSideEffects();

	FGameplayTag ScreenTag;
	FGameplayTag LayerTag;
	bool bScreenOpen = false;

	TWeakObjectPtr<APlayerController> SuppressedController;
	bool bLookSuppressed = false;
	bool bMoveSuppressed = false;
	bool bPausedByThisWidget = false;
};
