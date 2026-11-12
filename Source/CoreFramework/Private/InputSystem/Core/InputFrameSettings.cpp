// CoreFramework Input System - Core
#include "InputSystem/Core/InputFrameSettings.h"

#include "InputMappingContext.h"

UInputFrameSettings::UInputFrameSettings()
{
	// Escape and the gamepad Menu button open the pause menu and close screens: never bindable.
	ReservedKeys = { EKeys::Escape, EKeys::Gamepad_Special_Right };

	// Escape (keyboard) and the gamepad View / Back button stop listening without binding anything.
	CancelCaptureKeys = { EKeys::Escape, EKeys::Gamepad_Special_Left };
}
