// MyGameMode.cpp
// PROJECT TEMPLATE - see Public/Core/MyGameMode.h for the role of this class.

#include "Core/MyGameMode.h"

#include "Core/MyGameLog.h"
#include "GameFramework/PlayerController.h"

AMyGameMode::AMyGameMode()
{
	// PROJECT-SPECIFIC: set classes here only when they are C++ classes. Blueprint classes (HUD, pawn, controller)
	// are set in the Blueprint child instead, so designers can swap them without touching code.
	//
	// PlayerControllerClass = AMyPlayerController::StaticClass();   // uncomment if you always want the C++ controller

	// PROJECT-SPECIFIC: seamless travel keeps player controllers alive between maps. It needs a transition map in
	// Project Settings > Maps & Modes. Leave it off until your game needs it.
	// bUseSeamlessTravel = true;
}

void AMyGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	// Server only. Runs before any actor of the map begins play.
	UE_LOG(LogMyGame, Verbose, TEXT("InitGame: map '%s', options '%s'."), *MapName, *Options);

	// PROJECT-SPECIFIC: read game options from the travel URL here (for example UGameplayStatics::ParseOption).
}

void AMyGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	// Server only. A player controller has joined this map.
	// The UI is NOT started here: the HUD actor of that player (GameMode HUD Class) starts it on the machine
	// that owns the player, through UUIFrameSubsystem::InitializeForPlayerController.
	UE_LOG(LogMyGame, Verbose, TEXT("PostLogin: %s."), *GetNameSafe(NewPlayer));

	// PROJECT-SPECIFIC: per-player setup that only the server may do (team assignment, spawn point choice...).
}

void AMyGameMode::Logout(AController* Exiting)
{
	// PROJECT-SPECIFIC: server-side cleanup for a leaving player, before the engine removes the controller.
	UE_LOG(LogMyGame, Verbose, TEXT("Logout: %s."), *GetNameSafe(Exiting));

	Super::Logout(Exiting);
}
