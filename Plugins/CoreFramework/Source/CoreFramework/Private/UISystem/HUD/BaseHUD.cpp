// CoreFramework UI System
#include "UISystem/HUD/BaseHUD.h"

#include "GameFramework/PlayerController.h"
#include "UISystem/Manager/UIFrameSubsystem.h"

void ABaseHUD::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* PC = GetOwningPlayerController();
	if (PC && PC->IsLocalController())
	{
		if (UUIFrameSubsystem* Subsystem = UUIFrameSubsystem::GetFromController(PC))
		{
			Subsystem->InitializeForPlayerController(PC, StartupScreens, this);
		}
	}
}

void ABaseHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (APlayerController* PC = GetOwningPlayerController())
	{
		if (UUIFrameSubsystem* Subsystem = UUIFrameSubsystem::GetFromController(PC))
		{
			// Only releases when this HUD still owns the UI (a replacement HUD may already have taken over).
			Subsystem->ReleasePlayerController(PC, this);
		}
	}

	Super::EndPlay(EndPlayReason);
}
