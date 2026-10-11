// CoreFramework UI System
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UIFrameSettings.generated.h"

class UUIConfigDataAsset;

/** Project Settings > CoreFramework > CoreFramework UI. Stored in DefaultGame.ini. */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "CoreFramework UI"))
class COREFRAMEWORK_API UUIFrameSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** The project's UI configuration asset (screens, HUD elements, tab sets, root layout). */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "UI")
	TSoftObjectPtr<UUIConfigDataAsset> DefaultUIConfig;

	/** Z-order of the root layout in the player screen. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "UI", meta = (ClampMin = "0"))
	int32 RootLayoutZOrder = 0;

	/** Seconds between safety-net checks for pawn / player state changes. 0 disables the safety net. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Binding", meta = (ClampMin = "0.0"))
	float BindingPollIntervalSeconds = 0.2f;

	virtual FName GetCategoryName() const override { return TEXT("CoreFramework"); }

	static const UUIFrameSettings* Get() { return GetDefault<UUIFrameSettings>(); }
};
