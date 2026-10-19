// MyPlayerController.h
// PROJECT TEMPLATE - optional PlayerController of the game module (pause key -> UI framework).
//
// Copy to Source/<YourGame>/Public/Player/ and rename (see Templates/README.md).
// Skip this class entirely if you open the pause menu some other way (the UI guide, section 4.7, shows the
// Blueprint-only way).
//
// WHAT THIS CLASS IS
//   The local player's entry point for input that is not about the body (pause, menu keys).
//   The character keeps the input that is about the body (move, look, jump...): see ABaseCharacter / UInputConfigData.
//   Rule of thumb: input that must still work when there is no pawn belongs here.
//
// WHERE IT EXISTS
//   Server (one per player) and the owning client. Input is only set up on the machine that owns the player.
//
// FRAMEWORK WIRING
//   Pause key -> IA_Pause (Started) -> HandlePauseAction -> TogglePauseScreen
//     -> UUIFrameSubsystem::GetFromController(this) -> PushScreen / PopScreen(PauseScreenTag)
//   The pause screen (a child of UBaseMenuWidget) decides what pausing means: Input Mode, Pause Game While Active,
//   Is Back Handler. This class never calls SetInputMode and never pauses the game itself.
//
// SETUP (Blueprint child of this class, Class Defaults)
//   Pause Action          = IA_Pause (Input Action, Digital bool)
//   Player Mapping Context = IMC_Player (maps IA_Pause to Escape and Gamepad Special Right)
//   Pause Screen Tag      = the tag of your pause screen, for example UI.Screen.PauseMenu
//   Then set the Blueprint as "Player Controller Class" in the gameplay GameMode.
//
// KEY HANDLING
//   While a UI Only screen is open (the pause menu), game input is blocked, so this action does not fire again.
//   The pause screen closes itself through its Back action (Is Back Handler). If your pause screen uses
//   "Game And UI" instead, tick "Trigger When Paused" on IA_Pause so the toggle also works while the game is paused.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "MyPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

UCLASS(Blueprintable)
class MYGAME_API AMyPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	/** Opens the pause screen, or closes it when it is already open. Safe to call from Blueprint (for example a gamepad shortcut). */
	UFUNCTION(BlueprintCallable, Category = "MyGame|UI")
	void TogglePauseScreen();

protected:
	//~ Begin APlayerController Interface
	virtual void SetupInputComponent() override;
	//~ End APlayerController Interface

	/** Bound to PauseAction. */
	void HandlePauseAction(const FInputActionValue& Value);

	/** Mapping context added for the local player. It holds the controller-level actions (pause). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGame|Input")
	TObjectPtr<UInputMappingContext> PlayerMappingContext;

	/** Priority of PlayerMappingContext. The character's own context has its priority in its Input Config asset. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGame|Input")
	int32 MappingPriority = 0;

	/** Digital (bool) action that toggles the pause screen. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGame|Input")
	TObjectPtr<UInputAction> PauseAction;

	/** Screen tag of the pause screen, as registered in the UI config asset (DA_UIConfig > Screens). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGame|UI")
	FGameplayTag PauseScreenTag;

	// PROJECT-SPECIFIC: add the other controller-level actions of your game below (scoreboard, chat, map...).
};
