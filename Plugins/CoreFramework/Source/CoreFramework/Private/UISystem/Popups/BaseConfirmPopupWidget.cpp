// CoreFramework UI System - Popups
#include "UISystem/Popups/BaseConfirmPopupWidget.h"

#include "CommonTextBlock.h"
#include "UISystem/Foundation/BaseButton.h"
#include "UISystem/Manager/UIFrameSubsystem.h"

UBaseConfirmPopupWidget::UBaseConfirmPopupWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// A question blocks everything below it and Escape / gamepad B means "no".
	bIsModal = true;
	bIsBackHandler = true;
	InputMode = EUIInputMode::UIOnly;
	bUseDefaultScreenCues = true;
}

void UBaseConfirmPopupWidget::OpenConfirmation(UUIFrameSubsystem* UIFrame, FGameplayTag ScreenTag, const FUIConfirmRequest& Request, FUIConfirmCallback OnResult)
{
	if (!UIFrame || !ScreenTag.IsValid())
	{
		UE_LOG(LogCoreUI, Warning, TEXT("OpenConfirmation: no UI subsystem or no screen tag; answering Canceled."));
		if (OnResult)
		{
			OnResult(EUIConfirmResult::Canceled);
		}
		return;
	}

	UIFrame->PushScreenAsync(ScreenTag, [Request, OnResult, ScreenTag](UBaseActivatableWidget* Widget)
	{
		UBaseConfirmPopupWidget* Popup = Cast<UBaseConfirmPopupWidget>(Widget);
		if (!Popup)
		{
			UE_LOG(LogCoreUI, Warning, TEXT("OpenConfirmation: screen '%s' did not open a UBaseConfirmPopupWidget; answering Canceled."), *ScreenTag.ToString());
			if (Widget)
			{
				Widget->CloseScreen();
			}
			if (OnResult)
			{
				OnResult(EUIConfirmResult::Canceled);
			}
			return;
		}
		Popup->SetupConfirmNative(Request, OnResult);
	});
}

void UBaseConfirmPopupWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (ConfirmButton)
	{
		ConfirmButton->OnClicked().AddUObject(this, &UBaseConfirmPopupWidget::HandleConfirmClicked);
	}
	if (CancelButton)
	{
		CancelButton->OnClicked().AddUObject(this, &UBaseConfirmPopupWidget::HandleCancelClicked);
	}
}

void UBaseConfirmPopupWidget::SetupConfirm(const FUIConfirmRequest& Request)
{
	SetupConfirmNative(Request, nullptr);
}

void UBaseConfirmPopupWidget::SetupConfirmNative(const FUIConfirmRequest& Request, FUIConfirmCallback OnResultCallback)
{
	// The popup screen is single instance: a second request reuses it, so the first one gets its answer now.
	if (bPending)
	{
		Finish(EUIConfirmResult::Canceled, /*bCloseScreen*/ false);
	}

	CurrentRequest = Request;
	PendingCallback = MoveTemp(OnResultCallback);
	bPending = true;

	if (TitleText && !Request.Title.IsEmpty())
	{
		TitleText->SetText(Request.Title);
	}
	if (MessageText && !Request.Message.IsEmpty())
	{
		MessageText->SetText(Request.Message);
	}
	if (ConfirmButton && !Request.ConfirmLabel.IsEmpty())
	{
		ConfirmButton->SetButtonLabel(Request.ConfirmLabel);
	}
	if (CancelButton && !Request.CancelLabel.IsEmpty())
	{
		CancelButton->SetButtonLabel(Request.CancelLabel);
	}

	BP_OnRequestSet(CurrentRequest);

	// The popup may already be active (activation focused the default target before the request arrived).
	if (IsActivated())
	{
		RequestDefaultFocus();
	}
}

void UBaseConfirmPopupWidget::Confirm()
{
	Finish(EUIConfirmResult::Confirmed, /*bCloseScreen*/ true);
}

void UBaseConfirmPopupWidget::Cancel()
{
	Finish(EUIConfirmResult::Canceled, /*bCloseScreen*/ true);
}

void UBaseConfirmPopupWidget::HandleConfirmClicked()
{
	Confirm();
}

void UBaseConfirmPopupWidget::HandleCancelClicked()
{
	Cancel();
}

bool UBaseConfirmPopupWidget::NativeOnHandleBackAction()
{
	Cancel();
	return true;
}

void UBaseConfirmPopupWidget::NativeOnDeactivated()
{
	// Closed by something other than an answer (layer cleared, map change): the caller still gets one.
	if (bPending)
	{
		Finish(EUIConfirmResult::Canceled, /*bCloseScreen*/ false);
	}

	Super::NativeOnDeactivated();
}

UWidget* UBaseConfirmPopupWidget::ResolveDefaultFocusWidget() const
{
	// A DefaultFocusWidget placed in the Blueprint wins; otherwise the safe button of the request.
	if (UWidget* Explicit = Super::ResolveDefaultFocusWidget())
	{
		return Explicit;
	}

	UBaseButton* SafeButton = CancelButton ? CancelButton.Get() : ConfirmButton.Get();
	if (CurrentRequest.bFocusConfirmButton && ConfirmButton)
	{
		SafeButton = ConfirmButton.Get();
	}
	return SafeButton;
}

void UBaseConfirmPopupWidget::Finish(EUIConfirmResult Result, bool bCloseScreen)
{
	if (!bPending)
	{
		return;
	}

	bPending = false;
	FUIConfirmCallback Callback = MoveTemp(PendingCallback);
	PendingCallback = nullptr;

	// Close first, so the callback can open another screen (or the same popup again) safely.
	if (bCloseScreen && IsActivated())
	{
		CloseScreen();
	}

	if (Callback)
	{
		Callback(Result);
	}
	OnResult.Broadcast(Result);
}
