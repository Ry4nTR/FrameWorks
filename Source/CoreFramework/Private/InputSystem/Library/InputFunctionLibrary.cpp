// CoreFramework Input System - Library
#include "InputSystem/Library/InputFunctionLibrary.h"

#include "InputSystem/Core/InputFrameSettings.h"
#include "InputSystem/Manager/InputFrameSubsystem.h"

UInputFrameSubsystem* UInputFunctionLibrary::GetInputFrameSubsystem(const UObject* WorldContextObject)
{
	return UInputFrameSubsystem::Get(WorldContextObject);
}

bool UInputFunctionLibrary::IsUsingGamepad(const UObject* WorldContextObject)
{
	const UInputFrameSubsystem* Input = UInputFrameSubsystem::Get(WorldContextObject);
	return Input && Input->IsUsingGamepad();
}

ECommonInputType UInputFunctionLibrary::GetCurrentInputType(const UObject* WorldContextObject)
{
	const UInputFrameSubsystem* Input = UInputFrameSubsystem::Get(WorldContextObject);
	return Input ? Input->GetCurrentInputType() : ECommonInputType::MouseAndKeyboard;
}

FKey UInputFunctionLibrary::GetKeyForAction(const UObject* WorldContextObject, const UInputAction* Action)
{
	const UInputFrameSubsystem* Input = UInputFrameSubsystem::Get(WorldContextObject);
	return Input ? Input->GetKeyForActionOnCurrentDevice(Action) : EKeys::Invalid;
}

FText UInputFunctionLibrary::GetKeyDisplayText(const FKey& Key)
{
	return UInputFrameSubsystem::GetKeyDisplayText(Key);
}

FInputPreferences UInputFunctionLibrary::GetInputPreferences(const UObject* WorldContextObject)
{
	const UInputFrameSubsystem* Input = UInputFrameSubsystem::Get(WorldContextObject);
	return Input ? Input->GetPreferences() : UInputFrameSettings::Get()->DefaultPreferences;
}

bool UInputFunctionLibrary::IsAimAssistActive(const UObject* WorldContextObject)
{
	const UInputFrameSubsystem* Input = UInputFrameSubsystem::Get(WorldContextObject);
	return Input && Input->IsAimAssistActive();
}
