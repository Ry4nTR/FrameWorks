// CoreFramework Audio System - Notifies
#include "AudioSystem/Notifies/AnimNotify_PlayAudioCue.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "AudioSystem/Manager/AudioFrameSubsystem.h"

UAnimNotify_PlayAudioCue::UAnimNotify_PlayAudioCue()
{
#if WITH_EDITORONLY_DATA
	NotifyColor = FColor(120, 200, 255);
#endif
}

void UAnimNotify_PlayAudioCue::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp || !CueTag.IsValid())
	{
		return;
	}

	// Null in animation editor previews: they have no game instance.
	UAudioFrameSubsystem* Subsystem = UAudioFrameSubsystem::Get(MeshComp);
	if (!Subsystem)
	{
		return;
	}

	if (Audience != EAudioNotifyAudience::Everyone)
	{
		const AActor* Owner = MeshComp->GetOwner();
		const bool bOwnerMachine = Owner && Owner->HasLocalNetOwner();
		if ((Audience == EAudioNotifyAudience::OwnerOnly) != bOwnerMachine)
		{
			return;
		}
	}

	if (bAttachToMesh)
	{
		Subsystem->PlayCueAttached(CueTag, MeshComp, SocketName, VolumeMultiplier, PitchMultiplier);
		return;
	}

	const FTransform Transform = SocketName.IsNone() ? MeshComp->GetComponentTransform() : MeshComp->GetSocketTransform(SocketName);
	Subsystem->PlayCueAtLocation(CueTag, Transform.GetLocation(), Transform.Rotator(), VolumeMultiplier, PitchMultiplier);
}
