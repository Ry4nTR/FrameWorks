// CoreFramework Audio System - Data
#pragma once

#include "CoreMinimal.h"
#include "Components/AudioComponent.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "AudioSystem/Core/AudioTypes.h"
#include "AudioConfigDataAsset.generated.h"

class USoundMix;

/**
 * The single data-driven entry point of the audio system. One asset per game project, assigned in
 * Project Settings > CoreFramework > CoreFramework Audio. Everything is soft-referenced.
 * The framework ships no content for it: the game fills every list.
 */
UCLASS(BlueprintType)
class COREFRAMEWORK_API UAudioConfigDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** One entry per volume category (Master, SFX, Music, Ambient, UI + project categories). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Categories", meta = (TitleProperty = "Category"))
	TArray<FAudioCategoryDefinition> Categories;

	/**
	 * Optional. Sound Mix that carries the category volumes. Leave empty: the framework then uses an internal mix.
	 * Assign one only if you want to add your own passive effects (EQ, ducking) to it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Categories")
	TSoftObjectPtr<USoundMix> MasterSoundMix;

	/** Seconds a volume change takes to apply. A short fade avoids clicks when a slider is dragged. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Categories", meta = (ClampMin = "0.0", Units = "s"))
	float VolumeFadeTime = 0.05f;

	/** Every named sound of the game. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cues", meta = (TitleProperty = "CueTag"))
	TArray<FAudioCueDefinition> Cues;

	/** A cue that finished loading later than this after it was requested is dropped instead of played (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cues", meta = (ClampMin = "0.0", Units = "s"))
	float MaxLateCueSeconds = 1.0f;

	// ---- Music and ambience ----------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layers", meta = (ClampMin = "0.0", Units = "s"))
	float DefaultMusicFadeTime = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layers", meta = (ClampMin = "0.0", Units = "s"))
	float DefaultAmbientFadeTime = 3.0f;

	/** Fade curve of crossfades. Equal Power keeps the loudness constant while two tracks overlap. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layers")
	EAudioFaderCurve CrossfadeCurve = EAudioFaderCurve::Sin;

	// ---- Default UI cues --------------------------------------------------------------------------

	/** Played by every UBaseButton on hover unless the button names its own cue. Empty = silent. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI Defaults")
	FGameplayTag DefaultButtonHoverCue;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI Defaults")
	FGameplayTag DefaultButtonClickCue;

	/** Played when a screen that has "Use Default Screen Cues" ticked opens. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI Defaults")
	FGameplayTag DefaultScreenOpenCue;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI Defaults")
	FGameplayTag DefaultScreenCloseCue;
};
