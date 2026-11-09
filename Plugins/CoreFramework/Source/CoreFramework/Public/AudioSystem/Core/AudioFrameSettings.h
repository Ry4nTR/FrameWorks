// CoreFramework Audio System - Core
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "AudioFrameSettings.generated.h"

class UAudioConfigDataAsset;

/** Project Settings > CoreFramework > CoreFramework Audio. Stored in DefaultGame.ini. */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "CoreFramework Audio"))
class COREFRAMEWORK_API UAudioFrameSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** The project's audio configuration asset (categories, cue library, UI default cues, fade defaults). */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Audio")
	TSoftObjectPtr<UAudioConfigDataAsset> DefaultAudioConfig;

	virtual FName GetCategoryName() const override { return TEXT("CoreFramework"); }

	static const UAudioFrameSettings* Get() { return GetDefault<UAudioFrameSettings>(); }
};
