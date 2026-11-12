// CoreFramework Audio System
#pragma once

#include "CoreMinimal.h"
#include "Components/AudioComponent.h" // EAudioFaderCurve (FadeOutOrStop); without it the header only compiles in unity builds.
#include "GameplayTagContainer.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "AudioSystem/Core/AudioTypes.h"
#include "AudioFrameSubsystem.generated.h"

class UAudioComponent;
class UAudioConfigDataAsset;
class USceneComponent;
class USoundBase;
class USoundClass;
class USoundMix;
class UWorld;
struct FStreamableHandle;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAudioCategoryVolumeChanged, FGameplayTag, Category, float, Volume);

/** State of one crossfaded stream (music or ambience). */
USTRUCT()
struct FAudioLayerState
{
	GENERATED_BODY()

	/** The track that is audible (or fading in). */
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> Current;

	/** The track that is fading out. At most one: a third track in a row cuts the oldest one. */
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> Previous;

	int32 CurrentHandleId = 0;

	/** Incremented by every request, so a late async load of an older request is ignored. */
	uint32 RequestSerial = 0;
};

/**
 * Local audio manager of one game instance (one per machine, one per PIE client).
 *
 * Responsibilities
 *  - Cue library: plays sounds by Gameplay Tag, 2D, at a location or attached to a component. Any USoundBase works
 *    (Sound Wave, Sound Cue, MetaSound Source). Cues are loaded asynchronously from soft references.
 *  - Categories: Master / SFX / Music / Ambient / UI (+ project categories) with volume and mute, applied through
 *    Sound Classes and one Sound Mix, saved per player in GameUserSettings.ini.
 *  - Layers: Music and Ambient with crossfade. They keep playing across map travel.
 *  - Handles: stop, fade and drive (MetaSound parameters) anything that was started.
 *
 * It is purely local. Nothing here touches the network: replication of sound events and looping state is done by
 * UAudioEmitterComponent, which calls into this class on every machine.
 *
 * Dedicated server: the subsystem exists (the cue library is needed to validate client requests) but creates no
 * audio objects; every Play call returns an invalid handle.
 *
 * Why a GameInstance subsystem: audio settings and music outlive maps, one machine has one audio device even in
 * split-screen, and reusable game-instance logic must be a subsystem, never a UGameInstance subclass.
 *
 * Configuration: Project Settings > CoreFramework > CoreFramework Audio, which points at a UAudioConfigDataAsset.
 */
UCLASS()
class COREFRAMEWORK_API UAudioFrameSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** Finds the subsystem for any object that has a world (actor, component, widget). Null without a game instance (editor preview). */
	static UAudioFrameSubsystem* Get(const UObject* WorldContext);

	//~ Begin USubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem

	// ---- Categories and volume ---------------------------------------------------------------------

	/** Sets the volume (0..1) of a category and saves it for this player. Master scales every other category. */
	UFUNCTION(BlueprintCallable, Category = "Audio|Volume")
	void SetCategoryVolume(FGameplayTag Category, float Volume);

	UFUNCTION(BlueprintPure, Category = "Audio|Volume")
	float GetCategoryVolume(FGameplayTag Category) const;

	UFUNCTION(BlueprintCallable, Category = "Audio|Volume")
	void SetCategoryMuted(FGameplayTag Category, bool bMuted);

	UFUNCTION(BlueprintPure, Category = "Audio|Volume")
	bool IsCategoryMuted(FGameplayTag Category) const;

	/** Writes the volumes to GameUserSettings.ini now. Also done automatically when the game instance shuts down. */
	UFUNCTION(BlueprintCallable, Category = "Audio|Volume")
	void SaveAudioSettings();

	/** Fires after a volume or mute change, on the machine that changed it. Bind settings UI here. */
	UPROPERTY(BlueprintAssignable, Category = "Audio|Volume")
	FOnAudioCategoryVolumeChanged OnCategoryVolumeChanged;

	// ---- Cue library ---------------------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Audio|Cues")
	bool HasCue(FGameplayTag CueTag) const;

	/** C++ access to a cue definition. Null when the tag is not in the library. Do not keep the pointer. */
	const FAudioCueDefinition* FindCue(FGameplayTag CueTag) const;

	// ---- One-shots and loops by cue ------------------------------------------------------------------

	/** Plays a cue without a position (UI, stings, music-like sounds). Returns at once: a cue that is not loaded yet plays when it has loaded. */
	UFUNCTION(BlueprintCallable, Category = "Audio|Play")
	FAudioHandle PlayCue2D(FGameplayTag CueTag, float VolumeMultiplier = 1.0f, float PitchMultiplier = 1.0f);

	/** Plays a cue in the world. Spatialization comes from the cue's attenuation override or the sound asset's own attenuation. */
	UFUNCTION(BlueprintCallable, Category = "Audio|Play")
	FAudioHandle PlayCueAtLocation(FGameplayTag CueTag, FVector Location, FRotator Rotation = FRotator::ZeroRotator, float VolumeMultiplier = 1.0f, float PitchMultiplier = 1.0f);

	/** Plays a cue attached to a component (and optional socket). It follows the component. */
	UFUNCTION(BlueprintCallable, Category = "Audio|Play")
	FAudioHandle PlayCueAttached(FGameplayTag CueTag, USceneComponent* AttachToComponent, FName SocketName = NAME_None, float VolumeMultiplier = 1.0f, float PitchMultiplier = 1.0f);

	/** 2D cue routed to the UI category unless the cue names its own category. Used by widgets. */
	UFUNCTION(BlueprintCallable, Category = "Audio|Play")
	FAudioHandle PlayUICue(FGameplayTag CueTag, float VolumeMultiplier = 1.0f, float PitchMultiplier = 1.0f);

	/** Plays the cue the widget names, or the project default of that UI event when the tag is empty. Silent when neither exists. */
	UFUNCTION(BlueprintCallable, Category = "Audio|Play")
	FAudioHandle PlayUIEvent(EAudioUIEvent Event, FGameplayTag OverrideCue);

	// ---- One-shots and loops by sound asset (no cue entry needed) --------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Audio|Play")
	FAudioHandle PlaySound2D(USoundBase* Sound, FGameplayTag Category, float VolumeMultiplier = 1.0f, float PitchMultiplier = 1.0f);

	UFUNCTION(BlueprintCallable, Category = "Audio|Play")
	FAudioHandle PlaySoundAtLocation(USoundBase* Sound, FGameplayTag Category, FVector Location, FRotator Rotation = FRotator::ZeroRotator, float VolumeMultiplier = 1.0f, float PitchMultiplier = 1.0f);

	UFUNCTION(BlueprintCallable, Category = "Audio|Play")
	FAudioHandle PlaySoundAttached(USoundBase* Sound, FGameplayTag Category, USceneComponent* AttachToComponent, FName SocketName = NAME_None, float VolumeMultiplier = 1.0f, float PitchMultiplier = 1.0f);

	// ---- Handles ---------------------------------------------------------------------------------------

	/** Stops a sound, optionally fading out. Also cancels a cue that is still loading. Safe on stale handles. */
	UFUNCTION(BlueprintCallable, Category = "Audio|Handle")
	void StopSound(FAudioHandle Handle, float FadeOutTime = 0.0f);

	UFUNCTION(BlueprintPure, Category = "Audio|Handle")
	bool IsSoundPlaying(FAudioHandle Handle) const;

	UFUNCTION(BlueprintCallable, Category = "Audio|Handle")
	void SetHandleVolume(FAudioHandle Handle, float VolumeMultiplier, float FadeTime = 0.0f);

	UFUNCTION(BlueprintCallable, Category = "Audio|Handle")
	void SetHandlePitch(FAudioHandle Handle, float PitchMultiplier);

	/** Drives a MetaSound (or Sound Cue) float input by name. */
	UFUNCTION(BlueprintCallable, Category = "Audio|Handle")
	void SetHandleFloatParameter(FAudioHandle Handle, FName ParameterName, float Value);

	UFUNCTION(BlueprintCallable, Category = "Audio|Handle")
	void SetHandleBoolParameter(FAudioHandle Handle, FName ParameterName, bool bValue);

	UFUNCTION(BlueprintCallable, Category = "Audio|Handle")
	void SetHandleIntParameter(FAudioHandle Handle, FName ParameterName, int32 Value);

	// ---- Music and ambience (crossfaded layers) --------------------------------------------------------

	/**
	 * Starts a music track by cue. The running track fades out while the new one fades in.
	 * @param FadeTime  Seconds. Negative uses the default of the config asset. 0 cuts.
	 * @param bRestartIfSame  When false, asking for the track that already plays does nothing.
	 * The sound asset itself must loop (Looping on the Sound Wave, a looping Sound Cue or MetaSound) to play forever.
	 */
	UFUNCTION(BlueprintCallable, Category = "Audio|Music")
	FAudioHandle PlayMusic(FGameplayTag CueTag, float FadeTime = -1.0f, bool bRestartIfSame = false);

	UFUNCTION(BlueprintCallable, Category = "Audio|Music")
	FAudioHandle PlayMusicSound(USoundBase* Sound, float FadeTime = -1.0f, bool bRestartIfSame = false);

	UFUNCTION(BlueprintCallable, Category = "Audio|Music")
	void StopMusic(float FadeOutTime = -1.0f);

	UFUNCTION(BlueprintCallable, Category = "Audio|Music")
	FAudioHandle PlayAmbient(FGameplayTag CueTag, float FadeTime = -1.0f, bool bRestartIfSame = false);

	UFUNCTION(BlueprintCallable, Category = "Audio|Music")
	FAudioHandle PlayAmbientSound(USoundBase* Sound, float FadeTime = -1.0f, bool bRestartIfSame = false);

	UFUNCTION(BlueprintCallable, Category = "Audio|Music")
	void StopAmbient(float FadeOutTime = -1.0f);

	/** Handle of the track that is audible on a layer, to adjust it or drive MetaSound parameters. Invalid when the layer is silent. */
	UFUNCTION(BlueprintPure, Category = "Audio|Music")
	FAudioHandle GetLayerHandle(EAudioLayer Layer) const;

private:
	enum class ESpawnKind : uint8
	{
		Flat,
		Location,
		Attached
	};

	/** Everything needed to start a sound later, when an asynchronous load has finished. */
	struct FSpawnRequest
	{
		ESpawnKind Kind = ESpawnKind::Flat;
		FVector Location = FVector::ZeroVector;
		FRotator Rotation = FRotator::ZeroRotator;
		TWeakObjectPtr<USceneComponent> AttachTo;
		FName Socket = NAME_None;
		float Volume = 1.0f;
		float Pitch = 1.0f;
		/** Category used when the cue names none (UI cues use the UI category). */
		FGameplayTag FallbackCategory;
		int32 HandleId = 0;
		double RequestTime = 0.0;
	};

	void LoadConfig();
	bool CanPlay() const;

	/** Makes sure the volume mix is active on the audio device of the current world. Returns true if it applied every category. */
	bool EnsureMixApplied();
	void ApplyCategoryVolume(FGameplayTag Category, float FadeTime);
	FGameplayTag ResolveCategory(const FAudioCueDefinition* Cue, FGameplayTag Fallback) const;
	bool IsPlayWhenPaused(FGameplayTag Category) const;

	FAudioHandle PlayCueInternal(FGameplayTag CueTag, FSpawnRequest Request);
	FAudioHandle PlaySoundInternal(USoundBase* Sound, FGameplayTag Category, FSpawnRequest Request);
	void SpawnForCue(FGameplayTag CueTag, const FSpawnRequest& Request);
	UAudioComponent* SpawnComponent(USoundBase* Sound, const FAudioCueDefinition* Cue, FGameplayTag Category, const FSpawnRequest& Request);

	FAudioHandle PlayLayerCue(EAudioLayer Layer, FGameplayTag CueTag, float FadeTime, bool bRestartIfSame);
	FAudioHandle PlayLayerSound(EAudioLayer Layer, USoundBase* Sound, float FadeTime, bool bRestartIfSame);
	void StopLayer(EAudioLayer Layer, float FadeOutTime);
	int32 StartLayerTrack(EAudioLayer Layer, USoundBase* Sound, const FAudioCueDefinition* Cue, float FadeTime, bool bRestartIfSame, int32 HandleId);
	FAudioLayerState& GetLayer(EAudioLayer Layer);
	const FAudioLayerState& GetLayer(EAudioLayer Layer) const;
	float GetLayerDefaultFade(EAudioLayer Layer) const;

	void RegisterHandle(int32 HandleId, UAudioComponent* Component);
	UAudioComponent* ResolveHandle(FAudioHandle Handle) const;
	static void FadeOutOrStop(UAudioComponent* Component, float FadeTime, EAudioFaderCurve Curve);

	UPROPERTY(Transient)
	TObjectPtr<UAudioConfigDataAsset> Config;

	/** Carries the category volumes. The config's mix when it names one, otherwise an internal empty mix. */
	UPROPERTY(Transient)
	TObjectPtr<USoundMix> ActiveMix;

	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<USoundClass>> CategoryClasses;

	UPROPERTY(Transient)
	FAudioLayerState MusicLayer;

	UPROPERTY(Transient)
	FAudioLayerState AmbientLayer;

	TMap<FGameplayTag, FAudioCueDefinition> CueMap;
	TMap<FGameplayTag, bool> CategoryPlayWhenPaused;
	TMap<FGameplayTag, float> CategoryVolumes;
	TSet<FGameplayTag> MutedCategories;

	TMap<int32, TWeakObjectPtr<UAudioComponent>> Handles;
	TSet<int32> PendingHandleIds;
	int32 NextHandleId = 1;

	/** Keeps preloaded and already requested cue sounds resident. */
	TSharedPtr<FStreamableHandle> PreloadHandle;
	TMap<FGameplayTag, TSharedPtr<FStreamableHandle>> CueLoadHandles;

	TWeakObjectPtr<UWorld> MixWorld;
	bool bAudioEnabled = false;
};
