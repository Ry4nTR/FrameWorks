// CoreFramework Audio System - Library
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AudioSystem/Core/AudioTypes.h"
#include "AudioFunctionLibrary.generated.h"

class UAudioFrameSubsystem;
class USceneComponent;
class USoundBase;

/**
 * Blueprint (and C++) shortcuts to the audio subsystem. They need only a world context object, so no subsystem node
 * has to be fetched first. Everything here is local to the machine that calls it. For sounds that every player must
 * hear, use UAudioEmitterComponent::PlayCue instead.
 */
UCLASS()
class COREFRAMEWORK_API UAudioFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** The audio subsystem of the game instance. Null for objects without a game instance (editor preview). */
	UFUNCTION(BlueprintPure, Category = "Audio", meta = (WorldContext = "WorldContextObject", DisplayName = "Get Audio Subsystem"))
	static UAudioFrameSubsystem* GetAudioSubsystem(const UObject* WorldContextObject);

	// ---- Cues ----------------------------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Audio|Play", meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "VolumeMultiplier,PitchMultiplier"))
	static FAudioHandle PlayCue2D(const UObject* WorldContextObject, FGameplayTag CueTag, float VolumeMultiplier = 1.0f, float PitchMultiplier = 1.0f);

	UFUNCTION(BlueprintCallable, Category = "Audio|Play", meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "Rotation,VolumeMultiplier,PitchMultiplier"))
	static FAudioHandle PlayCueAtLocation(const UObject* WorldContextObject, FGameplayTag CueTag, FVector Location, FRotator Rotation = FRotator::ZeroRotator, float VolumeMultiplier = 1.0f, float PitchMultiplier = 1.0f);

	UFUNCTION(BlueprintCallable, Category = "Audio|Play", meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "SocketName,VolumeMultiplier,PitchMultiplier"))
	static FAudioHandle PlayCueAttached(const UObject* WorldContextObject, FGameplayTag CueTag, USceneComponent* AttachToComponent, FName SocketName = NAME_None, float VolumeMultiplier = 1.0f, float PitchMultiplier = 1.0f);

	/** 2D cue routed to the UI category. For custom widgets and anything that is interface feedback. */
	UFUNCTION(BlueprintCallable, Category = "Audio|Play", meta = (WorldContext = "WorldContextObject"))
	static FAudioHandle PlayUICue(const UObject* WorldContextObject, FGameplayTag CueTag);

	// ---- Sound assets (no cue entry needed) --------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Audio|Play", meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "VolumeMultiplier,PitchMultiplier"))
	static FAudioHandle PlaySound2D(const UObject* WorldContextObject, USoundBase* Sound, FGameplayTag Category, float VolumeMultiplier = 1.0f, float PitchMultiplier = 1.0f);

	UFUNCTION(BlueprintCallable, Category = "Audio|Play", meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "Rotation,VolumeMultiplier,PitchMultiplier"))
	static FAudioHandle PlaySoundAtLocation(const UObject* WorldContextObject, USoundBase* Sound, FGameplayTag Category, FVector Location, FRotator Rotation = FRotator::ZeroRotator, float VolumeMultiplier = 1.0f, float PitchMultiplier = 1.0f);

	// ---- Handles -------------------------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Audio|Handle", meta = (WorldContext = "WorldContextObject"))
	static void StopSound(const UObject* WorldContextObject, FAudioHandle Handle, float FadeOutTime = 0.0f);

	UFUNCTION(BlueprintCallable, Category = "Audio|Handle", meta = (WorldContext = "WorldContextObject"))
	static void SetHandleFloatParameter(const UObject* WorldContextObject, FAudioHandle Handle, FName ParameterName, float Value);

	UFUNCTION(BlueprintPure, Category = "Audio|Handle")
	static bool IsAudioHandleValid(FAudioHandle Handle) { return Handle.IsValid(); }

	// ---- Music and ambience --------------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Audio|Music", meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "FadeTime,bRestartIfSame"))
	static FAudioHandle PlayMusic(const UObject* WorldContextObject, FGameplayTag CueTag, float FadeTime = -1.0f, bool bRestartIfSame = false);

	UFUNCTION(BlueprintCallable, Category = "Audio|Music", meta = (WorldContext = "WorldContextObject"))
	static void StopMusic(const UObject* WorldContextObject, float FadeOutTime = -1.0f);

	UFUNCTION(BlueprintCallable, Category = "Audio|Music", meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "FadeTime,bRestartIfSame"))
	static FAudioHandle PlayAmbient(const UObject* WorldContextObject, FGameplayTag CueTag, float FadeTime = -1.0f, bool bRestartIfSame = false);

	UFUNCTION(BlueprintCallable, Category = "Audio|Music", meta = (WorldContext = "WorldContextObject"))
	static void StopAmbient(const UObject* WorldContextObject, float FadeOutTime = -1.0f);

	// ---- Volume --------------------------------------------------------------------------------------

	/** Volume 0..1 of a category (CoreFramework.Audio.Category.*). Saved for the player. */
	UFUNCTION(BlueprintCallable, Category = "Audio|Volume", meta = (WorldContext = "WorldContextObject"))
	static void SetCategoryVolume(const UObject* WorldContextObject, FGameplayTag Category, float Volume);

	UFUNCTION(BlueprintPure, Category = "Audio|Volume", meta = (WorldContext = "WorldContextObject"))
	static float GetCategoryVolume(const UObject* WorldContextObject, FGameplayTag Category);

	UFUNCTION(BlueprintCallable, Category = "Audio|Volume", meta = (WorldContext = "WorldContextObject"))
	static void SetCategoryMuted(const UObject* WorldContextObject, FGameplayTag Category, bool bMuted);

	// ---- Standard categories as pure nodes ------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Audio|Volume", meta = (DisplayName = "Category: Master"))
	static FGameplayTag GetCategoryMaster();

	UFUNCTION(BlueprintPure, Category = "Audio|Volume", meta = (DisplayName = "Category: SFX"))
	static FGameplayTag GetCategorySFX();

	UFUNCTION(BlueprintPure, Category = "Audio|Volume", meta = (DisplayName = "Category: Music"))
	static FGameplayTag GetCategoryMusic();

	UFUNCTION(BlueprintPure, Category = "Audio|Volume", meta = (DisplayName = "Category: Ambient"))
	static FGameplayTag GetCategoryAmbient();

	UFUNCTION(BlueprintPure, Category = "Audio|Volume", meta = (DisplayName = "Category: UI"))
	static FGameplayTag GetCategoryUI();
};
