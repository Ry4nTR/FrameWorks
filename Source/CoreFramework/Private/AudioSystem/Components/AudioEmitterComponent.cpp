// AudioEmitterComponent.cpp
// CoreFramework Audio System - replicated sound events and looping sound state for any actor.

#include "AudioSystem/Components/AudioEmitterComponent.h"

#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"
#include "AudioSystem/Manager/AudioFrameSubsystem.h"

UAudioEmitterComponent::UAudioEmitterComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UAudioEmitterComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopAllLocalLoops();
	Super::EndPlay(EndPlayReason);
}

void UAudioEmitterComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UAudioEmitterComponent, ActiveLoops);
	DOREPLIFETIME(UAudioEmitterComponent, SharedMusic);
}

// ----------------------------------------------------------------------
// One-shots
// ----------------------------------------------------------------------

void UAudioEmitterComponent::PlayCue(FGameplayTag CueTag, EAudioPlacement Placement, FName SocketName)
{
	AActor* Owner = GetOwner();
	if (!Owner || !CueTag.IsValid())
	{
		return;
	}

	if (Owner->HasAuthority())
	{
		const UAudioFrameSubsystem* Subsystem = UAudioFrameSubsystem::Get(this);
		if (!Subsystem || !Subsystem->HasCue(CueTag))
		{
			UE_LOG(LogCoreAudio, Warning, TEXT("PlayCue: cue %s is not in the audio config."), *CueTag.ToString());
			return;
		}
		BroadcastCue(CueTag, Placement, SocketName, /*bSkipOwner=*/false);
		return;
	}

	// Client that owns the actor: play at once, then let the server tell the others.
	PlayCueLocal(CueTag, Placement, SocketName);
	if (Owner->HasLocalNetOwner())
	{
		ServerPlayCue(CueTag, Placement, SocketName);
	}
}

FAudioHandle UAudioEmitterComponent::PlayCueLocal(FGameplayTag CueTag, EAudioPlacement Placement, FName SocketName)
{
	AActor* Owner = GetOwner();
	UAudioFrameSubsystem* Subsystem = UAudioFrameSubsystem::Get(this);
	if (!Owner || !Subsystem)
	{
		return FAudioHandle();
	}

	switch (Placement)
	{
	case EAudioPlacement::Global2D:
		return Subsystem->PlayCue2D(CueTag);

	case EAudioPlacement::Attached:
		{
			FName Socket;
			if (USceneComponent* Target = ResolveAttachTarget(SocketName, Socket))
			{
				return Subsystem->PlayCueAttached(CueTag, Target, Socket);
			}
			break;
		}

	case EAudioPlacement::AtActor:
	default:
		break;
	}

	return Subsystem->PlayCueAtLocation(CueTag, Owner->GetActorLocation(), Owner->GetActorRotation());
}

void UAudioEmitterComponent::BroadcastCue(FGameplayTag CueTag, EAudioPlacement Placement, FName SocketName, bool bSkipOwner)
{
	const UAudioFrameSubsystem* Subsystem = UAudioFrameSubsystem::Get(this);
	const FAudioCueDefinition* Cue = Subsystem ? Subsystem->FindCue(CueTag) : nullptr;

	// The multicast also runs on the server itself (and does nothing there on a dedicated server).
	if (Cue && Cue->bReliable)
	{
		MulticastPlayCueReliable(CueTag, Placement, SocketName, bSkipOwner);
	}
	else
	{
		MulticastPlayCue(CueTag, Placement, SocketName, bSkipOwner);
	}
}

void UAudioEmitterComponent::HandleCueEvent(FGameplayTag CueTag, EAudioPlacement Placement, FName SocketName, bool bSkipOwner)
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// The owning client played the cue when it asked.
	if (bSkipOwner && Owner->HasLocalNetOwner())
	{
		return;
	}

	PlayCueLocal(CueTag, Placement, SocketName);
}

void UAudioEmitterComponent::ServerPlayCue_Implementation(FGameplayTag CueTag, EAudioPlacement Placement, FName SocketName)
{
	const UAudioFrameSubsystem* Subsystem = UAudioFrameSubsystem::Get(this);
	const FAudioCueDefinition* Cue = Subsystem ? Subsystem->FindCue(CueTag) : nullptr;

	// Rejected quietly: a client must not be able to play arbitrary sounds on other machines.
	if (!Cue || !Cue->bAllowClientRequest)
	{
		UE_LOG(LogCoreAudio, Verbose, TEXT("Client request for cue %s ignored (unknown cue, or 'Allow Client Request' is off)."), *CueTag.ToString());
		return;
	}

	const double Now = FPlatformTime::Seconds();
	double& LastTime = LastClientRequestTime.FindOrAdd(CueTag);
	if (Now - LastTime < Cue->ClientRequestMinInterval)
	{
		return;
	}
	LastTime = Now;

	BroadcastCue(CueTag, Placement, SocketName, /*bSkipOwner=*/true);
}

void UAudioEmitterComponent::MulticastPlayCue_Implementation(FGameplayTag CueTag, EAudioPlacement Placement, FName SocketName, bool bSkipOwner)
{
	HandleCueEvent(CueTag, Placement, SocketName, bSkipOwner);
}

void UAudioEmitterComponent::MulticastPlayCueReliable_Implementation(FGameplayTag CueTag, EAudioPlacement Placement, FName SocketName, bool bSkipOwner)
{
	HandleCueEvent(CueTag, Placement, SocketName, bSkipOwner);
}

// ----------------------------------------------------------------------
// Loops
// ----------------------------------------------------------------------

void UAudioEmitterComponent::StartLoop(FName LoopId, FGameplayTag CueTag)
{
	const AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		UE_LOG(LogCoreAudio, Warning, TEXT("StartLoop(%s) ignored: loops are started on the server."), *LoopId.ToString());
		return;
	}

	const UAudioFrameSubsystem* Subsystem = UAudioFrameSubsystem::Get(this);
	if (LoopId.IsNone() || !Subsystem || !Subsystem->HasCue(CueTag))
	{
		UE_LOG(LogCoreAudio, Warning, TEXT("StartLoop(%s): needs a loop id and a cue that is in the audio config."), *LoopId.ToString());
		return;
	}

	if (FAudioLoopEntry* Existing = ActiveLoops.FindByPredicate([LoopId](const FAudioLoopEntry& Entry) { return Entry.LoopId == LoopId; }))
	{
		if (Existing->CueTag == CueTag)
		{
			return;
		}
		Existing->CueTag = CueTag;
	}
	else
	{
		FAudioLoopEntry NewEntry;
		NewEntry.LoopId = LoopId;
		NewEntry.CueTag = CueTag;
		ActiveLoops.Add(NewEntry);
	}

	RefreshLoops();
}

void UAudioEmitterComponent::StopLoop(FName LoopId)
{
	const AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		UE_LOG(LogCoreAudio, Warning, TEXT("StopLoop(%s) ignored: loops are stopped on the server."), *LoopId.ToString());
		return;
	}

	if (ActiveLoops.RemoveAll([LoopId](const FAudioLoopEntry& Entry) { return Entry.LoopId == LoopId; }) > 0)
	{
		RefreshLoops();
	}
}

bool UAudioEmitterComponent::IsLoopActive(FName LoopId) const
{
	return ActiveLoops.ContainsByPredicate([LoopId](const FAudioLoopEntry& Entry) { return Entry.LoopId == LoopId; });
}

void UAudioEmitterComponent::OnRep_ActiveLoops()
{
	RefreshLoops();
}

void UAudioEmitterComponent::RefreshLoops()
{
	UAudioFrameSubsystem* Subsystem = UAudioFrameSubsystem::Get(this);
	if (!Subsystem)
	{
		return;
	}

	// Stop loops that were removed or whose cue changed.
	for (auto It = LocalLoopHandles.CreateIterator(); It; ++It)
	{
		const FName LoopId = It.Key();
		const FAudioLoopEntry* Entry = ActiveLoops.FindByPredicate([LoopId](const FAudioLoopEntry& E) { return E.LoopId == LoopId; });
		const FGameplayTag* PlayingCue = LocalLoopCues.Find(LoopId);

		if (!Entry || !PlayingCue || Entry->CueTag != *PlayingCue)
		{
			Subsystem->StopSound(It.Value(), LoopFadeOutTime);
			LocalLoopCues.Remove(LoopId);
			It.RemoveCurrent();
		}
	}

	// Start loops that are new.
	for (const FAudioLoopEntry& Entry : ActiveLoops)
	{
		if (LocalLoopHandles.Contains(Entry.LoopId))
		{
			continue;
		}

		FName Socket;
		USceneComponent* Target = ResolveAttachTarget(LoopAttachSocket, Socket);
		const FAudioHandle Handle = Target ? Subsystem->PlayCueAttached(Entry.CueTag, Target, Socket) : FAudioHandle();
		if (Handle.IsValid())
		{
			LocalLoopHandles.Add(Entry.LoopId, Handle);
			LocalLoopCues.Add(Entry.LoopId, Entry.CueTag);
		}
	}
}

void UAudioEmitterComponent::StopAllLocalLoops()
{
	if (LocalLoopHandles.Num() == 0)
	{
		return;
	}

	if (UAudioFrameSubsystem* Subsystem = UAudioFrameSubsystem::Get(this))
	{
		for (const TPair<FName, FAudioHandle>& Pair : LocalLoopHandles)
		{
			Subsystem->StopSound(Pair.Value, 0.0f);
		}
	}
	LocalLoopHandles.Reset();
	LocalLoopCues.Reset();
}

// ----------------------------------------------------------------------
// Shared music
// ----------------------------------------------------------------------

void UAudioEmitterComponent::SetSharedMusic(FGameplayTag CueTag, float FadeTime)
{
	const AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		UE_LOG(LogCoreAudio, Warning, TEXT("SetSharedMusic ignored: shared music is set on the server."));
		return;
	}

	SharedMusic.CueTag = CueTag;
	SharedMusic.FadeTime = FadeTime;
	++SharedMusic.Sequence;
	ApplySharedMusic();
}

void UAudioEmitterComponent::ClearSharedMusic()
{
	const AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		UE_LOG(LogCoreAudio, Warning, TEXT("ClearSharedMusic ignored: shared music is set on the server."));
		return;
	}

	// Back to the default value: clients that play it stop it, and players who join later receive nothing at all.
	SharedMusic = FAudioSharedMusic();
	ApplySharedMusic();
}

void UAudioEmitterComponent::OnRep_SharedMusic()
{
	ApplySharedMusic();
}

void UAudioEmitterComponent::ApplySharedMusic()
{
	UAudioFrameSubsystem* Subsystem = UAudioFrameSubsystem::Get(this);
	if (!Subsystem)
	{
		return;
	}

	if (SharedMusic.CueTag.IsValid())
	{
		Subsystem->PlayMusic(SharedMusic.CueTag, SharedMusic.FadeTime, /*bRestartIfSame=*/false);
	}
	else
	{
		Subsystem->StopMusic(SharedMusic.FadeTime);
	}
}

// ----------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------

USceneComponent* UAudioEmitterComponent::ResolveAttachTarget(FName SocketName, FName& OutSocket) const
{
	OutSocket = NAME_None;

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	if (!SocketName.IsNone())
	{
		TInlineComponentArray<USkeletalMeshComponent*> Meshes(Owner);
		for (USkeletalMeshComponent* Mesh : Meshes)
		{
			if (Mesh && Mesh->DoesSocketExist(SocketName))
			{
				OutSocket = SocketName;
				return Mesh;
			}
		}
	}

	return Owner->GetRootComponent();
}
