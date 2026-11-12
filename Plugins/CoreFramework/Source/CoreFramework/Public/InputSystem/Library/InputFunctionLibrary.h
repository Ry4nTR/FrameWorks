// CoreFramework Input System - Library
#pragma once

#include "CoreMinimal.h"
#include "CommonInputTypeEnum.h"
#include "InputCoreTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "InputSystem/Core/InputFrameTypes.h"
#include "InputFunctionLibrary.generated.h"

class UInputAction;
class UInputFrameSubsystem;

/**
 * Blueprint shortcuts to the input system that only need a world context (any actor, component or widget).
 * Actors use the first local player of their world, widgets their owning player.
 */
UCLASS()
class COREFRAMEWORK_API UInputFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Input", meta = (WorldContext = "WorldContextObject", DisplayName = "Get Input Frame Subsystem"))
	static UInputFrameSubsystem* GetInputFrameSubsystem(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Input|Device", meta = (WorldContext = "WorldContextObject"))
	static bool IsUsingGamepad(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Input|Device", meta = (WorldContext = "WorldContextObject"))
	static ECommonInputType GetCurrentInputType(const UObject* WorldContextObject);

	/** Key an action uses on the device the player is using now (player remaps included). */
	UFUNCTION(BlueprintPure, Category = "Input|Prompts", meta = (WorldContext = "WorldContextObject"))
	static FKey GetKeyForAction(const UObject* WorldContextObject, const UInputAction* Action);

	/** Short display name of a key ("E", "Esc", "LMB"). */
	UFUNCTION(BlueprintPure, Category = "Input|Prompts")
	static FText GetKeyDisplayText(const FKey& Key);

	UFUNCTION(BlueprintPure, Category = "Input|Preferences", meta = (WorldContext = "WorldContextObject"))
	static FInputPreferences GetInputPreferences(const UObject* WorldContextObject);

	/** Aim assist wanted by the player and a gamepad in use. */
	UFUNCTION(BlueprintPure, Category = "Input|Preferences", meta = (WorldContext = "WorldContextObject"))
	static bool IsAimAssistActive(const UObject* WorldContextObject);
};
