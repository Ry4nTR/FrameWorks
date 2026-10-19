// MyGameInstance.cpp
// PROJECT TEMPLATE - see Public/Core/MyGameInstance.h for the role of this class.

#include "Core/MyGameInstance.h"

#include "Core/MyGameLog.h"

void UMyGameInstance::Init()
{
	Super::Init();

	// Runs once when the game process starts, before the first map loads.
	// CoreFramework needs nothing from you here: UUIFrameSubsystem is created by the engine together with the
	// local player, and the project settings of the UI (Default UI Config) are read when the first HUD starts.
	UE_LOG(LogMyGame, Verbose, TEXT("GameInstance Init."));

	// PROJECT-SPECIFIC: register delegates, load the profile or settings that every map needs.
}

void UMyGameInstance::OnStart()
{
	Super::OnStart();

	// Runs when the game instance starts playing, right before the first map is entered.
	// PROJECT-SPECIFIC: start-up work that needs a world (for example a first-run decision).
}

void UMyGameInstance::Shutdown()
{
	// PROJECT-SPECIFIC: undo what Init registered (delegates, handles).
	UE_LOG(LogMyGame, Verbose, TEXT("GameInstance Shutdown."));

	Super::Shutdown();
}
