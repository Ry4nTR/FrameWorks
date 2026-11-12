// CoreFramework Input System - Manager (private)
#include "KeyCaptureInputProcessor.h"

#include "Containers/Ticker.h"
#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"

namespace
{
	bool IsStickDirectionKey(const FKey& Key)
	{
		return Key == EKeys::Gamepad_LeftStick_Up || Key == EKeys::Gamepad_LeftStick_Down
			|| Key == EKeys::Gamepad_LeftStick_Left || Key == EKeys::Gamepad_LeftStick_Right
			|| Key == EKeys::Gamepad_RightStick_Up || Key == EKeys::Gamepad_RightStick_Down
			|| Key == EKeys::Gamepad_RightStick_Left || Key == EKeys::Gamepad_RightStick_Right;
	}
}

FKeyCaptureInputProcessor::FKeyCaptureInputProcessor(bool bInCaptureGamepad, float InTimeout, const TArray<FKey>& InCancelKeys, bool bInAllowStickDirections, FOnCaptureFinished InOnFinished)
	: OnFinished(MoveTemp(InOnFinished))
	, CancelKeys(InCancelKeys)
	, Timeout(InTimeout)
	, bCaptureGamepad(bInCaptureGamepad)
	, bAllowStickDirections(bInAllowStickDirections)
{
}

void FKeyCaptureInputProcessor::StopCapture()
{
	if (bCapturing)
	{
		bCapturing = false;
		OnFinished.Unbind();
	}
	QueueUnregisterIfClean();
}

void FKeyCaptureInputProcessor::Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor)
{
	if (bCapturing)
	{
		Elapsed += DeltaTime;
		if (Timeout > 0.0f && Elapsed >= Timeout)
		{
			Finish(EKeys::Invalid, true);
		}
	}
	else
	{
		QueueUnregisterIfClean();
	}
}

bool FKeyCaptureInputProcessor::IsAcceptedKey(const FKey& Key) const
{
	if (!Key.IsValid() || Key.IsAnalog() || Key.IsTouch() || Key.IsGesture())
	{
		return false;
	}
	if (Key.IsGamepadKey() != bCaptureGamepad)
	{
		return false;
	}
	return bAllowStickDirections || !IsStickDirectionKey(Key);
}

void FKeyCaptureInputProcessor::HandlePressedKey(const FKey& Key)
{
	if (!bCapturing)
	{
		return;
	}
	if (CancelKeys.Contains(Key))
	{
		Finish(EKeys::Invalid, true);
		return;
	}
	if (IsAcceptedKey(Key))
	{
		Finish(Key, false);
	}
	// Any other key: swallowed, keep listening.
}

void FKeyCaptureInputProcessor::Finish(const FKey& Key, bool bCancelled)
{
	if (!bCapturing)
	{
		return;
	}
	bCapturing = false;

	// Copy first: the callback may start a new capture (which replaces this processor).
	FOnCaptureFinished Callback = OnFinished;
	OnFinished.Unbind();
	Callback.ExecuteIfBound(Key, bCancelled);
}

void FKeyCaptureInputProcessor::QueueUnregisterIfClean()
{
	if (bCapturing || bUnregisterQueued || SwallowedKeys.Num() > 0)
	{
		return;
	}
	bUnregisterQueued = true;

	// Never unregister from inside Slate's pre-processor loop: do it on the next core tick.
	TWeakPtr<FKeyCaptureInputProcessor> WeakThis = AsShared();
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis](float)
	{
		if (TSharedPtr<FKeyCaptureInputProcessor> Pinned = WeakThis.Pin())
		{
			if (FSlateApplication::IsInitialized())
			{
				FSlateApplication::Get().UnregisterInputPreProcessor(Pinned);
			}
		}
		return false;
	}));
}

bool FKeyCaptureInputProcessor::HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (!bCapturing)
	{
		return false;
	}

	SwallowedKeys.Add(Key);
	if (!InKeyEvent.IsRepeat())
	{
		HandlePressedKey(Key);
	}
	return true;
}

bool FKeyCaptureInputProcessor::HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	// Only the releases of presses this processor swallowed. The release of the key that STARTED the capture (gamepad A,
	// Enter) must reach CommonUI, otherwise that button would stay pressed.
	const bool bWasSwallowed = SwallowedKeys.Remove(InKeyEvent.GetKey()) > 0;
	if (!bCapturing)
	{
		QueueUnregisterIfClean();
	}
	return bWasSwallowed;
}

bool FKeyCaptureInputProcessor::HandleAnalogInputEvent(FSlateApplication& SlateApp, const FAnalogInputEvent& InAnalogInputEvent)
{
	// Sticks and trigger axes must not navigate the menu while listening. Trigger presses also arrive as key-downs.
	return bCapturing;
}

bool FKeyCaptureInputProcessor::HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	if (!bCapturing)
	{
		return false;
	}

	const FKey Button = MouseEvent.GetEffectingButton();
	SwallowedKeys.Add(Button);
	if (bCaptureGamepad)
	{
		// A mouse click while choosing a gamepad button: the player gave up.
		Finish(EKeys::Invalid, true);
	}
	else
	{
		HandlePressedKey(Button);
	}
	return true;
}

bool FKeyCaptureInputProcessor::HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	const bool bWasSwallowed = SwallowedKeys.Remove(MouseEvent.GetEffectingButton()) > 0;
	if (!bCapturing)
	{
		QueueUnregisterIfClean();
	}
	return bWasSwallowed;
}

bool FKeyCaptureInputProcessor::HandleMouseButtonDoubleClickEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	return HandleMouseButtonDownEvent(SlateApp, MouseEvent);
}

bool FKeyCaptureInputProcessor::HandleMouseWheelOrGestureEvent(FSlateApplication& SlateApp, const FPointerEvent& InWheelEvent, const FPointerEvent* InGestureEvent)
{
	if (!bCapturing)
	{
		return false;
	}

	// The wheel has no release event: nothing to remember.
	if (!bCaptureGamepad && InGestureEvent == nullptr && InWheelEvent.GetWheelDelta() != 0.0f)
	{
		HandlePressedKey(InWheelEvent.GetWheelDelta() > 0.0f ? EKeys::MouseScrollUp : EKeys::MouseScrollDown);
	}
	return true;
}
