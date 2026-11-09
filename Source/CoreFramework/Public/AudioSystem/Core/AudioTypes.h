// CoreFramework Audio System - Core
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "AudioTypes.generated.h"

COREFRAMEWORK_API DECLARE_LOG_CATEGORY_EXTERN(LogCoreAudio, Log, All);

class USoundAttenuation;
class USoundBase;
class USoundClass;
class USoundConcurrency;

/** The two long-running, crossfaded streams of the audio system. */
UENUM(BlueprintType)
enum class EAudioLayer : uint8
{
	Music,
	Ambient
};

/** Where a replicated or notify-driven cue is heard. */
UENUM(BlueprintType)
enum class EAudioPlacement : uint8
{
	/** At the owning actor's location when the event arrives. A moving source is not followed. */
	AtActor UMETA(DisplayName = "At Actor"),

	/** Attached to the owning actor's root (or the named socket of its first skeletal mesh) and follows it. */
	Attached UMETA(DisplayName = "Attached To Actor"),

	/** Not positioned in the world: every listener hears it at full volume (stings, announcements). */
	Global2D UMETA(DisplayName = "Global 2D")
};

/** Standard UI events that have a project-wide default cue in UAudioConfigDataAsset. */
UENUM(BlueprintType)
enum class EAudioUIEvent : uint8
{
	ButtonHover,
	ButtonClick,
	ScreenOpen,
	ScreenClose
};

/**
 * Reference to a sound that was started through the audio system. Keep it when the sound must be stopped or
 * adjusted later (loops, MetaSound parameters). One-shots can ignore it. Handles are local to one machine.
 */
USTRUCT(BlueprintType)
struct COREFRAMEWORK_API FAudioHandle
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Audio")
	int32 Id = 0;

	bool IsValid() const { return Id > 0; }
	bool operator==(const FAudioHandle& Other) const { return Id == Other.Id; }
};

/**
 * One entry of the cue library. A cue is a named sound plus how it is played; game code only knows the CueTag.
 * Lives in UAudioConfigDataAsset. The framework ships no cues: every entry belongs to the game project.
 */
USTRUCT(BlueprintType)
struct COREFRAMEWORK_API FAudioCueDefinition
{
	GENERATED_BODY()

	/** Unique id used by every Play call. Create your own tags (for example Audio.Cue.UI.Click) in Project Settings > Gameplay Tags. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cue")
	FGameplayTag CueTag;

	/** Any sound type: Sound Wave, Sound Cue or MetaSound Source. Soft reference, loaded on demand or at start-up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cue")
	TSoftObjectPtr<USoundBase> Sound;

	/** Volume category (Sound Class) the sound is routed to. Empty means SFX. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cue", meta = (Categories = "CoreFramework.Audio.Category"))
	FGameplayTag Category;

	/** Base volume multiplier of this cue. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Variation", meta = (ClampMin = "0.0"))
	float VolumeMultiplier = 1.0f;

	/** Random volume spread: each play uses Volume x (1 +/- this). 0 disables it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Variation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float VolumeRandomRange = 0.0f;

	/** Base pitch multiplier of this cue. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Variation", meta = (ClampMin = "0.01"))
	float PitchMultiplier = 1.0f;

	/** Random pitch spread: each play uses Pitch x (1 +/- this). 0 disables it. Useful against repetition (footsteps, impacts). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Variation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PitchRandomRange = 0.0f;

	/** Replaces the attenuation of the sound asset for 3D playback. Empty uses the attenuation of the sound asset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spatialization")
	TObjectPtr<USoundAttenuation> AttenuationOverride;

	/** Replaces the concurrency of the sound asset (voice limits, ducking of repeats). Empty uses the sound asset's own. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spatialization")
	TObjectPtr<USoundConcurrency> ConcurrencyOverride;

	/** Load the sound at start-up so the first play never waits. Turn off for rare or large sounds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loading")
	bool bPreload = true;

	/**
	 * Multiplayer: lets a client ask the server to replicate this cue to everyone (through UAudioEmitterComponent).
	 * Off by default, so clients cannot make arbitrary sounds play on other machines.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Multiplayer")
	bool bAllowClientRequest = false;

	/** Multiplayer: the server ignores client requests for this cue that arrive faster than this (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Multiplayer", meta = (ClampMin = "0.0", EditCondition = "bAllowClientRequest"))
	float ClientRequestMinInterval = 0.05f;

	/** Multiplayer: send server events for this cue reliably. Use only for rare, important cues; one-shots are unreliable by default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Multiplayer")
	bool bReliable = false;
};

/** Maps a category tag to its Sound Class. Lives in UAudioConfigDataAsset. */
USTRUCT(BlueprintType)
struct COREFRAMEWORK_API FAudioCategoryDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Category", meta = (Categories = "CoreFramework.Audio.Category"))
	FGameplayTag Category;

	/** Sound Class of this category. Child classes of Master inherit its volume automatically. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Category")
	TSoftObjectPtr<USoundClass> SoundClass;

	/** Volume (0..1) used until the player changes it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Category", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DefaultVolume = 1.0f;

	/** Keep playing while the game is paused. Always on for the UI category. Tick it for Music, and for Ambient if you want it to continue in pause menus. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Category")
	bool bPlayWhenPaused = false;
};
