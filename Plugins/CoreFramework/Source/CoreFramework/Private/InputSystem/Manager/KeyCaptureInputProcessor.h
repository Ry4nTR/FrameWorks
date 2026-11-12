// CoreFramework Input System - Manager (private)
#pragma once

#include "CoreMinimal.h"
#include "Framework/Application/IInputProcessor.h"
#include "InputCoreTypes.h"

/**
 * "Press any key" listener used by UInputFrameSubsystem::BeginKeyCapture.
 *
 * Registered as the FIRST Slate input pre-processor while the player is choosing a key, so it sees every key before
 * widgets, CommonUI actions (Back / Accept) and the game. While capturing it swallows all keys, mouse buttons, the wheel
 * and analog sticks: nothing navigates, clicks or closes the menu.
 *
 *  - A key of the right device (gamepad keys for the gamepad column, keyboard / mouse for the other) ends the capture.
 *  - A cancel key ends it without a key. Keys of the other device and analog input are ignored.
 *  - The key-up of every swallowed key-down is swallowed too, so no widget ever receives a release without its press.
 *    The processor therefore stays registered after the capture until those keys are released, then removes itself.
 */
class FKeyCaptureInputProcessor : public IInputProcessor, public TSharedFromThis<FKeyCaptureInputProcessor>
{
public:
	DECLARE_DELEGATE_TwoParams(FOnCaptureFinished, const FKey& /*Key*/, bool /*bCancelled*/);

	FKeyCaptureInputProcessor(bool bInCaptureGamepad, float InTimeout, const TArray<FKey>& InCancelKeys, bool bInAllowStickDirections, FOnCaptureFinished InOnFinished);

	/** Ends the capture without calling the delegate (keeps swallowing releases until clean, then unregisters). */
	void StopCapture();

	bool IsCapturing() const { return bCapturing; }

	//~ Begin IInputProcessor
	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override;
	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override;
	virtual bool HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override;
	virtual bool HandleAnalogInputEvent(FSlateApplication& SlateApp, const FAnalogInputEvent& InAnalogInputEvent) override;
	virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;
	virtual bool HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;
	virtual bool HandleMouseButtonDoubleClickEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;
	virtual bool HandleMouseWheelOrGestureEvent(FSlateApplication& SlateApp, const FPointerEvent& InWheelEvent, const FPointerEvent* InGestureEvent) override;
	virtual const TCHAR* GetDebugName() const override { return TEXT("CoreFrameworkKeyCapture"); }
	//~ End IInputProcessor

private:
	/** A key press while capturing. Always swallowed. */
	void HandlePressedKey(const FKey& Key);

	bool IsAcceptedKey(const FKey& Key) const;
	void Finish(const FKey& Key, bool bCancelled);
	void QueueUnregisterIfClean();

	FOnCaptureFinished OnFinished;
	TArray<FKey> CancelKeys;
	TSet<FKey> SwallowedKeys;
	float Timeout = 0.0f;
	float Elapsed = 0.0f;
	bool bCaptureGamepad = false;
	bool bAllowStickDirections = false;
	bool bCapturing = true;
	bool bUnregisterQueued = false;
};
