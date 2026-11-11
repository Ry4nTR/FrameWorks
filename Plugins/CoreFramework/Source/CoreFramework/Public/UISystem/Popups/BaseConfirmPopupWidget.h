// CoreFramework UI System - Popups
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UISystem/Foundation/BaseActivatableWidget.h"
#include "BaseConfirmPopupWidget.generated.h"

class UBaseButton;
class UCommonTextBlock;
class UUIFrameSubsystem;

/** The answer of a confirmation popup. */
UENUM(BlueprintType)
enum class EUIConfirmResult : uint8
{
	Confirmed,
	/** Cancel button, Back action (Escape / gamepad B), or the popup was closed by something else. */
	Canceled
};

/** What a confirmation popup shows. Every text left empty keeps what the popup Blueprint already displays. */
USTRUCT(BlueprintType)
struct COREFRAMEWORK_API FUIConfirmRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Confirm")
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Confirm", meta = (MultiLine = "true"))
	FText Message;

	/** Label of the confirm button. Empty keeps the button's own Button Label. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Confirm")
	FText ConfirmLabel;

	/** Label of the cancel button. Empty keeps the button's own Button Label. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Confirm")
	FText CancelLabel;

	/**
	 * Which button has keyboard / gamepad focus when the popup opens. Leave false for destructive actions
	 * (delete, overwrite): a quick Enter / A press then cancels instead of destroying data.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Confirm")
	bool bFocusConfirmButton = false;
};

/** Blueprint answer of a confirmation popup. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUIConfirmResult, EUIConfirmResult, Result);

/** C++ answer of a confirmation popup. */
using FUIConfirmCallback = TFunction<void(EUIConfirmResult)>;

/**
 * Parent of every yes / no popup of a game project (WBP_Popup_Confirm).
 *
 * It is an ordinary screen: register it in DA_UIConfig > Screens on a modal layer (CoreFramework.UI.Layer.Modal) under a
 * tag of the game (for example UI.Screen.ConfirmPopup). The game never creates it by hand; it asks:
 *
 *   C++        UBaseConfirmPopupWidget::OpenConfirmation(UIFrameSubsystem, ScreenTag, Request, [](EUIConfirmResult R){ ... });
 *   Blueprint  Push Screen (tag), then on the widget: Setup Confirm (Request) and bind On Result.
 *
 * Widgets (all optional, auto-wired by name)
 *   TitleText       Common Text   receives Request.Title
 *   MessageText     Common Text   receives Request.Message
 *   ConfirmButton   UBaseButton   answers Confirmed
 *   CancelButton    UBaseButton   answers Canceled
 *
 * Behaviour
 *   * Modal and Back Handler by default: the layers below receive no input, Escape / gamepad B answers Canceled.
 *   * Exactly one answer per request. Closing the popup any other way (layer cleared, map change) answers Canceled.
 *     A new request while one is pending cancels the pending one first.
 *   * Focus lands on CancelButton unless the request asks for ConfirmButton (bFocusConfirmButton).
 *   * The popup closes itself before the answer is delivered, so the caller may push another screen from the callback.
 */
UCLASS(Abstract, Blueprintable)
class COREFRAMEWORK_API UBaseConfirmPopupWidget : public UBaseActivatableWidget
{
	GENERATED_BODY()

public:
	UBaseConfirmPopupWidget(const FObjectInitializer& ObjectInitializer);

	/**
	 * Pushes the popup screen registered under ScreenTag, fills it with Request and calls OnResult once with the answer.
	 * If the screen cannot be opened (tag not registered, class is not a confirm popup) OnResult receives Canceled and a
	 * warning is logged, so the caller never waits forever.
	 */
	static void OpenConfirmation(UUIFrameSubsystem* UIFrame, FGameplayTag ScreenTag, const FUIConfirmRequest& Request, FUIConfirmCallback OnResult);

	/** Fills the popup. Call right after pushing it. A pending request is canceled first. */
	UFUNCTION(BlueprintCallable, Category = "Base UI|Confirm")
	void SetupConfirm(const FUIConfirmRequest& Request);

	/** C++ version with a callback (also broadcasts OnResult). */
	void SetupConfirmNative(const FUIConfirmRequest& Request, FUIConfirmCallback OnResult);

	/** Answers Confirmed and closes the popup. What ConfirmButton does. */
	UFUNCTION(BlueprintCallable, Category = "Base UI|Confirm")
	void Confirm();

	/** Answers Canceled and closes the popup. What CancelButton and the Back action do. */
	UFUNCTION(BlueprintCallable, Category = "Base UI|Confirm")
	void Cancel();

	/** True between SetupConfirm and the answer. */
	UFUNCTION(BlueprintPure, Category = "Base UI|Confirm")
	bool IsWaitingForAnswer() const { return bPending; }

	UFUNCTION(BlueprintPure, Category = "Base UI|Confirm")
	const FUIConfirmRequest& GetRequest() const { return CurrentRequest; }

	/** The answer of the current request. Fires once per request. */
	UPROPERTY(BlueprintAssignable, Category = "Base UI|Confirm")
	FOnUIConfirmResult OnResult;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnDeactivated() override;
	virtual bool NativeOnHandleBackAction() override;
	virtual UWidget* ResolveDefaultFocusWidget() const override;

	/** A request was applied to the widgets. Use it for extra visuals (an icon for destructive requests, an animation). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Base UI|Confirm", meta = (DisplayName = "On Request Set"))
	void BP_OnRequestSet(const FUIConfirmRequest& Request);

	/** Optional Common Text named exactly TitleText. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> TitleText;

	/** Optional Common Text named exactly MessageText. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> MessageText;

	/** Optional button named exactly ConfirmButton. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseButton> ConfirmButton;

	/** Optional button named exactly CancelButton. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseButton> CancelButton;

private:
	void HandleConfirmClicked();
	void HandleCancelClicked();

	/** Delivers the answer once: clears the pending state, closes the popup (unless already closing), calls back. */
	void Finish(EUIConfirmResult Result, bool bCloseScreen);

	FUIConfirmRequest CurrentRequest;
	FUIConfirmCallback PendingCallback;
	bool bPending = false;
};
