// MyGameMode.h
// PROJECT TEMPLATE - GameMode skeleton of the game module.
//
// Copy to Source/<YourGame>/Public/Core/ and rename (see Templates/README.md).
//
// WHAT THIS CLASS IS
//   The rules of one kind of map in THIS game. It derives from AGameModeBase directly: CoreFramework has no
//   GameMode base class on purpose, because match rules belong to the game.
//   Need match states (warm-up, in progress, ended)? Change the parent to AGameMode and the GameState to
//   AGameState. Nothing else in this file changes.
//
// WHERE IT EXISTS
//   Server only (dedicated server, listen-server host, standalone). Never on a client.
//   UI code and client code must never read it. Data that clients need goes into the GameState or the PlayerState.
//
// FRAMEWORK WIRING (done in a Blueprint child, Class Defaults > Classes; no C++ needed)
//   HUD Class                = BP_Startup_* (child of ABaseHUD, UI framework). The HUD actor starts the UI of each
//                              local player and its "Startup Screens" list says which screens open on this map type.
//   Default Pawn Class       = your character Blueprint (child of ABaseCharacter, Character framework).
//                              Set it to None on a menu map. The Camera, Movement, StateMachine and Animation
//                              frameworks come with the character; nothing to wire here.
//   Player Controller Class  = your controller Blueprint (child of AMyPlayerController) on gameplay maps.
//
// MAP TRAVEL
//   A new GameMode exists on every map. Anything that must survive a map change lives in UMyGameInstance
//   or in your own subsystem, never here.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MyGameMode.generated.h"

UCLASS(Blueprintable)
class MYGAME_API AMyGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AMyGameMode();

	//~ Begin AGameModeBase Interface
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	//~ End AGameModeBase Interface

	// PROJECT-SPECIFIC: add the match rules of your game below (spawn rules, win and lose conditions, round flow).
	// Keep them out of CoreFramework: they differ from game to game.
};
