// CoreFramework UI System
#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Engine/DataTable.h"
#include "Input/UIActionBindingHandle.h"
#include "UISystem/Binding/BaseUIBinderInterface.h"
#include "UISystem/Core/UITypes.h"
#include "BaseUserWidget.generated.h"

class APawn;
class APlayerController;
class APlayerState;
class UUIFrameSubsystem;
class UWidgetAnimation;

/**
 * Base class for every non-activatable widget in the framework (HUD elements, tab content, list entries...).
 *
 * Provides:
 *  - Show / Hide with optional UMG animations (Anim_Show / Anim_Hide) and lifecycle events.
 *  - Automatic registration with the local player's binding hub (IBaseUIBinderInterface).
 *  - Input action binding helpers on top of CommonUI.
 *  - Convenience access to the UI subsystem and the bound controller / pawn / player state.
 */
UCLASS(Abstract, Blueprintable)
class COREFRAMEWORK_API UBaseUserWidget : public UCommonUserWidget, public IBaseUIBinderInterface
{
	GENERATED_BODY()

public:
	UBaseUserWidget(const FObjectInitializer& ObjectInitializer);

	// ---- Show / Hide with transition hook -------------------------------------------------------

	/** Makes the widget visible (ShownVisibility) and plays Anim_Show when bAnimate is true. */
	UFUNCTION(BlueprintCallable, Category = "Base UI|Lifecycle")
	void ShowWidget(bool bAnimate = true);

	/** Plays Anim_Hide (when bAnimate) and then collapses the widget (HiddenVisibility). */
	UFUNCTION(BlueprintCallable, Category = "Base UI|Lifecycle")
	void HideWidget(bool bAnimate = true);

	UFUNCTION(BlueprintPure, Category = "Base UI|Lifecycle")
	bool IsWidgetShown() const { return bIsShown; }

	// ---- Access ----------------------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Base UI")
	UUIFrameSubsystem* GetUIFrameSubsystem() const;

	UFUNCTION(BlueprintPure, Category = "Base UI|Binding")
	APlayerController* GetBoundController() const;

	UFUNCTION(BlueprintPure, Category = "Base UI|Binding")
	APawn* GetBoundPawn() const;

	UFUNCTION(BlueprintPure, Category = "Base UI|Binding")
	APlayerState* GetBoundPlayerState() const;

	/** Typed convenience for C++: GetBoundPawnAs<AMyCharacter>(). */
	template <typename PawnType>
	PawnType* GetBoundPawnAs() const
	{
		return Cast<PawnType>(GetBoundPawn());
	}

	/** True while the last used input device on this local player is a gamepad. */
	UFUNCTION(BlueprintPure, Category = "Base UI|Input")
	bool IsUsingGamepad() const;

	// ---- Input binding helpers -------------------------------------------------------------------

	/**
	 * Binds a CommonUI input action (row of a CommonInputActionDataBase table, resolves to
	 * keyboard / gamepad / Enhanced Input keys per platform). Automatically unbound when the widget is destroyed.
	 */
	UFUNCTION(BlueprintCallable, Category = "Base UI|Input")
	void BindUIAction(const FDataTableRowHandle& ActionRow, bool bShowInActionBar, FBaseUIActionDelegate OnExecuted);

	/** C++ overload with a native delegate. */
	FUIActionBindingHandle BindUIActionNative(const FDataTableRowHandle& ActionRow, bool bShowInActionBar, const FSimpleDelegate& OnExecuted);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** Fired after ShowWidget made the widget visible. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Base UI|Lifecycle", meta = (DisplayName = "On Widget Shown"))
	void BP_OnWidgetShown();

	/** Fired after HideWidget finished (animation included) and the widget is collapsed. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Base UI|Lifecycle", meta = (DisplayName = "On Widget Hidden"))
	void BP_OnWidgetHidden();

	/** Register with the binding hub on construct. Disable for widgets that never react to the local player's pawn. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base UI|Binding")
	bool bAutoRegisterAsBinder = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base UI|Lifecycle")
	ESlateVisibility ShownVisibility = ESlateVisibility::SelfHitTestInvisible;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base UI|Lifecycle")
	ESlateVisibility HiddenVisibility = ESlateVisibility::Collapsed;

	/** Optional UMG animation named exactly Anim_Show. Played by ShowWidget. */
	UPROPERTY(Transient, BlueprintReadOnly, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_Show;

	/** Optional UMG animation named exactly Anim_Hide. Played by HideWidget before collapsing. */
	UPROPERTY(Transient, BlueprintReadOnly, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_Hide;

private:
	UFUNCTION()
	void HandleHideAnimationFinished();

	void FinalizeHide();

	bool bIsShown = true;
};
