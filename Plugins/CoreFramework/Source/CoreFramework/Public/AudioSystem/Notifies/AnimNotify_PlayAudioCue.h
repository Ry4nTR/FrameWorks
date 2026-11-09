// CoreFramework Audio System - Notifies
#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GameplayTagContainer.h"
#include "AnimNotify_PlayAudioCue.generated.h"

/** Which machines hear a PlayAudioCue notify. */
UENUM(BlueprintType)
enum class EAudioNotifyAudience : uint8
{
	/** Every machine that plays the animation. */
	Everyone,

	/** Only the machine of the player that owns the mesh. Use on first-person arm animations. */
	OwnerOnly,

	/** Every machine except the owner's. Use on body animations whose first-person counterpart plays on the arms. */
	OthersOnly
};

/**
 * Animation notify that plays an audio cue at (or attached to) the animated mesh: footsteps, weapon sounds, cloth.
 *
 * It plays locally and sends nothing over the network. Animation already runs on every machine that sees the
 * character (state replicates, montages are forwarded), so each machine triggers its own sound. That is the cheapest
 * and most accurate way to do animation-driven audio in multiplayer.
 *
 * It does nothing in the animation editor preview (there is no game instance there). Test in Play In Editor.
 */
UCLASS(meta = (DisplayName = "Play Audio Cue"))
class COREFRAMEWORK_API UAnimNotify_PlayAudioCue : public UAnimNotify
{
	GENERATED_BODY()

public:
	UAnimNotify_PlayAudioCue();

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

	/** Cue from the audio config. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	FGameplayTag CueTag;

	/** Socket or bone of the mesh the sound comes from. None uses the mesh origin. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	FName SocketName = NAME_None;

	/** Follow the socket while the sound plays. Off for short one-shots such as footsteps (they stay where they happened). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	bool bAttachToMesh = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	EAudioNotifyAudience Audience = EAudioNotifyAudience::Everyone;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio", meta = (ClampMin = "0.0"))
	float VolumeMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio", meta = (ClampMin = "0.01"))
	float PitchMultiplier = 1.0f;
};
