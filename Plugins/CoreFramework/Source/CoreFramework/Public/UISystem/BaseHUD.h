// CoreFramework UI System
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "GameplayTagContainer.h"
#include "BaseHUD.generated.h"

/**
 * Bootstrap actor for the UI system. Set it as the HUD class of the GameMode.
 *
 * AHUD exists only for local player controllers (listen-server host and clients, never on a dedicated server),
 * which makes BeginPlay the correct, multiplayer-safe moment to start the UI of that local player.
 * It draws nothing itself; all UI is UMG through UUIFrameSubsystem.
 *
 * Projects that already own an AHUD subclass can skip this class and call
 * UUIFrameSubsystem::InitializeForPlayerController / ReleasePlayerController from their own HUD.
 */
UCLASS()
class COREFRAMEWORK_API ABaseHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	/**
	 * Screens pushed when the UI starts for this map / game mode, in order. Put the HUD first, overlays after.
	 * Main menu map: [Screen.MainMenu]. Gameplay map: [Screen.HUD].
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Base UI", meta = (Categories = "CoreFramework.UI.Screen"))
	TArray<FGameplayTag> StartupScreens;
};
