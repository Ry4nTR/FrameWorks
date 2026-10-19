// MyGameInstance.h
// PROJECT TEMPLATE - GameInstance skeleton of the game module.
//
// Copy to Source/<YourGame>/Public/Core/ and rename (see Templates/README.md).
//
// WHAT THIS CLASS IS
//   The one object that lives for the whole run of the game, on every machine (server and each client, each with
//   its own copy, never replicated). It survives every map change.
//   A project has exactly ONE GameInstance class (Project Settings > Maps & Modes > Game Instance Class).
//   That is why CoreFramework never ships a GameInstance base class: it would block every other framework.
//
// RULES FOR FRAMEWORKS
//   A framework that needs session-long state ships a UGameInstanceSubsystem. It needs no change here and it
//   can be combined with any other framework. CoreFramework needs none today.
//
// WHAT BELONGS HERE (PROJECT-SPECIFIC)
//   Data that must outlive a map: loaded profile or settings, online session handles, a pending travel request.
//
// WHAT DOES NOT BELONG HERE
//   - UI. The UI lives in UUIFrameSubsystem, a subsystem of the local player (one per split-screen player).
//   - Match rules. They belong in AMyGameMode.
//   - Data that other players must see. That is GameState / PlayerState data.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "MyGameInstance.generated.h"

UCLASS(Blueprintable)
class MYGAME_API UMyGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	//~ Begin UGameInstance Interface
	virtual void Init() override;
	virtual void OnStart() override;
	virtual void Shutdown() override;
	//~ End UGameInstance Interface

	// PROJECT-SPECIFIC: add the session-long data and functions of your game below.
};
