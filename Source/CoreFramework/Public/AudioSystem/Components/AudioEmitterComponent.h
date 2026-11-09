// AudioEmitterComponent.h
// CoreFramework Audio System - replicated sound events and looping sound state for any actor.
//
// ROLE
//   The network layer of the audio system. UAudioFrameSubsystem plays sounds on the local machine only; this
//   component makes the same sound happen on every machine that should hear it. Add it to a character, a door, a
//   weapon, a vehicle or the GameState. ABaseCharacter already has one (AudioEmitter).
//
// PRINCIPLE
//   Audio is presentation. Events and state are replicated, never audio data: a one-shot is one Gameplay Tag, a loop is
//   one small replicated entry. Every machine then plays through its own UAudioFrameSubsystem.
//   Sounds that can be derived from state that is already replicated (footsteps from animation, UI, ambience) need
//   no emitter at all: play them locally (UAnimNotify_PlayAudioCue, UAudioFunctionLibrary).
//
// NETWORK MODEL
//   One-shots      PlayCue. Server: plays locally and multicasts (unreliable, or reliable when the cue says so).
//                  Owning client: plays at once (no latency), asks the server, and the server multicasts to everyone
//                  except the owner. The server only accepts it when the cue has "Allow Client Request" and the
//                  request is not faster than the cue's minimum interval. Relevancy and net cull distance of the
//                  actor decide who is sent the event.
//                  A client that does not own the actor cannot replicate: its call plays locally only.
//   Loops          StartLoop / StopLoop (server only). Replicated list with a RepNotify, so players that join later or
//                  for whom the actor becomes relevant later start the right loops.
//   Shared music   SetSharedMusic (server only). One replicated cue that every client plays as music, late join correct.
//                  Use it on a single actor per world, normally the GameState. Games with per-player music never call it.
//   Dedicated server: no audio objects are created; the component only relays.
//
// REQUIREMENTS
//   The owning actor must replicate (bReplicates) for events to reach other machines. Cues must be in the audio config.
//
// PORTING: copy this folder, keep COREFRAMEWORK_API (or rename it to your module macro).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "AudioSystem/Core/AudioTypes.h"
#include "AudioEmitterComponent.generated.h"

class USceneComponent;

/** One replicated looping sound: an id chosen by gameplay (Engine, Alarm ...) and the cue that plays. */
USTRUCT()
struct FAudioLoopEntry
{
	GENERATED_BODY()

	UPROPERTY()
	FName LoopId = NAME_None;

	UPROPERTY()
	FGameplayTag CueTag;
};

/** Replicated music request. Sequence changes on every request so a repeated request is never lost. */
USTRUCT()
struct FAudioSharedMusic
{
	GENERATED_BODY()

	UPROPERTY()
	FGameplayTag CueTag;

	UPROPERTY()
	float FadeTime = -1.0f;

	UPROPERTY()
	uint8 Sequence = 0;
};

UCLASS(ClassGroup = (Frameworks), meta = (BlueprintSpawnableComponent))
class COREFRAMEWORK_API UAudioEmitterComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAudioEmitterComponent();

	//~ Begin UActorComponent Interface
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~ End UActorComponent Interface

	// ------------------------------------------------------------------
	// Configuration
	// ------------------------------------------------------------------

	/** Seconds a loop takes to fade out when it is stopped. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Emitter", meta = (ClampMin = "0.0", Units = "s"))
	float LoopFadeOutTime = 0.25f;

	/** Socket of a skeletal mesh of the owner that loops attach to. None attaches them to the root component. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Emitter")
	FName LoopAttachSocket = NAME_None;

	// ------------------------------------------------------------------
	// One-shots
	// ------------------------------------------------------------------

	/**
	 * Plays a cue on every machine that should hear it.
	 * Call it on the server, or on the client that owns the actor (the cue must then allow client requests).
	 * @param Placement   At the actor, attached to it (follows it), or global 2D.
	 * @param SocketName  Attached only: socket of one of the owner's skeletal meshes. None attaches to the root.
	 */
	UFUNCTION(BlueprintCallable, Category = "Audio|Emitter")
	void PlayCue(FGameplayTag CueTag, EAudioPlacement Placement = EAudioPlacement::AtActor, FName SocketName = NAME_None);

	/** Plays a cue on this machine only. No network traffic. */
	UFUNCTION(BlueprintCallable, Category = "Audio|Emitter")
	FAudioHandle PlayCueLocal(FGameplayTag CueTag, EAudioPlacement Placement = EAudioPlacement::AtActor, FName SocketName = NAME_None);

	// ------------------------------------------------------------------
	// Loops (server only)
	// ------------------------------------------------------------------

	/**
	 * Starts a looping sound attached to the owner on every machine. The sound asset of the cue must loop.
	 * Starting an id that runs with another cue replaces it. Server only.
	 */
	UFUNCTION(BlueprintCallable, Category = "Audio|Emitter")
	void StartLoop(FName LoopId, FGameplayTag CueTag);

	/** Stops a loop everywhere, fading out over LoopFadeOutTime. Server only. */
	UFUNCTION(BlueprintCallable, Category = "Audio|Emitter")
	void StopLoop(FName LoopId);

	UFUNCTION(BlueprintPure, Category = "Audio|Emitter")
	bool IsLoopActive(FName LoopId) const;

	// ------------------------------------------------------------------
	// Shared music (server only)
	// ------------------------------------------------------------------

	/** Every client plays this cue as music. Late joiners get it too. Server only. FadeTime below zero uses the config default. */
	UFUNCTION(BlueprintCallable, Category = "Audio|Emitter")
	void SetSharedMusic(FGameplayTag CueTag, float FadeTime = -1.0f);

	/** Stops the shared music on every client (default fade). Server only. */
	UFUNCTION(BlueprintCallable, Category = "Audio|Emitter")
	void ClearSharedMusic();

private:
	UFUNCTION(Server, Unreliable)
	void ServerPlayCue(FGameplayTag CueTag, EAudioPlacement Placement, FName SocketName);

	/** bSkipOwner is true when the owning client already played the cue when it asked. */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayCue(FGameplayTag CueTag, EAudioPlacement Placement, FName SocketName, bool bSkipOwner);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayCueReliable(FGameplayTag CueTag, EAudioPlacement Placement, FName SocketName, bool bSkipOwner);

	UFUNCTION()
	void OnRep_ActiveLoops();

	UFUNCTION()
	void OnRep_SharedMusic();

	/** Server side: multicasts with the reliability the cue asks for. */
	void BroadcastCue(FGameplayTag CueTag, EAudioPlacement Placement, FName SocketName, bool bSkipOwner);
	void HandleCueEvent(FGameplayTag CueTag, EAudioPlacement Placement, FName SocketName, bool bSkipOwner);

	/** Brings the loops that play on this machine in line with ActiveLoops. */
	void RefreshLoops();
	void StopAllLocalLoops();
	void ApplySharedMusic();

	USceneComponent* ResolveAttachTarget(FName SocketName, FName& OutSocket) const;

	UPROPERTY(ReplicatedUsing = OnRep_ActiveLoops)
	TArray<FAudioLoopEntry> ActiveLoops;

	UPROPERTY(ReplicatedUsing = OnRep_SharedMusic)
	FAudioSharedMusic SharedMusic;

	/** Loops that currently play on this machine. */
	TMap<FName, FAudioHandle> LocalLoopHandles;
	TMap<FName, FGameplayTag> LocalLoopCues;

	/** Server side throttle of client requests. */
	TMap<FGameplayTag, double> LastClientRequestTime;
};
