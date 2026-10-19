// MyPlayerController.cpp
// PROJECT TEMPLATE - see Public/Player/MyPlayerController.h for the role of this class.

#include "Player/MyPlayerController.h"

#include "Core/MyGameLog.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "UISystem/Manager/UIFrameSubsystem.h"

void AMyPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// Only the machine that owns this player has an input component worth configuring.
	if (!IsLocalPlayerController())
	{
		return;
	}

	if (PlayerMappingContext)
	{
		if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			InputSubsystem->AddMappingContext(PlayerMappingContext, MappingPriority);
		}
	}
	else
	{
		UE_LOG(LogMyGame, Warning, TEXT("%s: Player Mapping Context is not set (Class Defaults > MyGame|Input). The pause key has no mapping."), *GetNameSafe(this));
	}

	if (PauseAction)
	{
		if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent))
		{
			EnhancedInput->BindAction(PauseAction, ETriggerEvent::Started, this, &AMyPlayerController::HandlePauseAction);
		}
		else
		{
			UE_LOG(LogMyGame, Warning, TEXT("%s: the input component is not an Enhanced Input component. Check Project Settings > Input > Default Input Component Class."), *GetNameSafe(this));
		}
	}
	else
	{
		UE_LOG(LogMyGame, Warning, TEXT("%s: Pause Action is not set (Class Defaults > MyGame|Input)."), *GetNameSafe(this));
	}

	// PROJECT-SPECIFIC: bind the other controller-level actions of your game here.
}

void AMyPlayerController::HandlePauseAction(const FInputActionValue& /*Value*/)
{
	TogglePauseScreen();
}

void AMyPlayerController::TogglePauseScreen()
{
	if (!PauseScreenTag.IsValid())
	{
		UE_LOG(LogMyGame, Warning, TEXT("%s: Pause Screen Tag is not set (Class Defaults > MyGame|UI)."), *GetNameSafe(this));
		return;
	}

	// The UI subsystem belongs to the local player. It does not exist on a dedicated server, so this is null there.
	UUIFrameSubsystem* UI = UUIFrameSubsystem::GetFromController(this);
	if (!UI)
	{
		UE_LOG(LogMyGame, Warning, TEXT("%s: no UI subsystem for this controller (no local player)."), *GetNameSafe(this));
		return;
	}

	if (UI->IsScreenOpen(PauseScreenTag))
	{
		UI->PopScreen(PauseScreenTag);
	}
	else
	{
		UI->PushScreen(PauseScreenTag);
	}
}
