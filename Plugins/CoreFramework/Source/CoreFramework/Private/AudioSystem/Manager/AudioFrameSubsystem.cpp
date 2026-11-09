// CoreFramework Audio System
#include "AudioSystem/Manager/AudioFrameSubsystem.h"

#include "AudioDevice.h"
#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundConcurrency.h"
#include "Sound/SoundMix.h"
#include "AudioSystem/Core/AudioFrameSettings.h"
#include "AudioSystem/Core/AudioGameplayTags.h"
#include "AudioSystem/Data/AudioConfigDataAsset.h"

DEFINE_LOG_CATEGORY(LogCoreAudio);

namespace
{
	/** Section of GameUserSettings.ini that stores the player's volumes. */
	const TCHAR* const GAudioSection = TEXT("CoreFramework.Audio");

	FString VolumeKey(const FGameplayTag& Category) { return FString::Printf(TEXT("Volume.%s"), *Category.ToString()); }
	FString MuteKey(const FGameplayTag& Category) { return FString::Printf(TEXT("Mute.%s"), *Category.ToString()); }

	/** Random factor in [1 - Range, 1 + Range]. */
	float Spread(float Range)
	{
		return Range > 0.0f ? FMath::FRandRange(1.0f - Range, 1.0f + Range) : 1.0f;
	}

	/** Above this many tracked handles, dead entries are removed on the next registration. */
	constexpr int32 HandlePruneThreshold = 128;
}

// ---------------------------------------------------------------------------------------------------------------------
// Lookup and lifecycle
// ---------------------------------------------------------------------------------------------------------------------

UAudioFrameSubsystem* UAudioFrameSubsystem::Get(const UObject* WorldContext)
{
	if (!WorldContext || !GEngine)
	{
		return nullptr;
	}

	const UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull);
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UAudioFrameSubsystem>() : nullptr;
}

void UAudioFrameSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	bAudioEnabled = !IsRunningDedicatedServer() && FApp::CanEverRenderAudio();
	LoadConfig();
}

void UAudioFrameSubsystem::Deinitialize()
{
	SaveAudioSettings();

	for (FAudioLayerState* Layer : { &MusicLayer, &AmbientLayer })
	{
		if (Layer->Current)
		{
			Layer->Current->Stop();
		}
		if (Layer->Previous)
		{
			Layer->Previous->Stop();
		}
		*Layer = FAudioLayerState();
	}

	Handles.Reset();
	PendingHandleIds.Reset();
	CueLoadHandles.Reset();
	PreloadHandle.Reset();
	CategoryClasses.Reset();
	ActiveMix = nullptr;
	Config = nullptr;

	Super::Deinitialize();
}

void UAudioFrameSubsystem::LoadConfig()
{
	// The config is a small data-only asset (soft references only), so a synchronous load is cheap and happens once.
	Config = UAudioFrameSettings::Get()->DefaultAudioConfig.LoadSynchronous();
	if (!Config)
	{
		UE_LOG(LogCoreAudio, Warning, TEXT("No audio config assigned (Project Settings > CoreFramework > CoreFramework Audio). "
			"Cue playback and category volumes are unavailable; playing raw sounds still works."));
		return;
	}

	// Cue library. Kept on every machine, the dedicated server included: it validates client requests with it.
	CueMap.Reset();
	for (const FAudioCueDefinition& Cue : Config->Cues)
	{
		if (!Cue.CueTag.IsValid())
		{
			UE_LOG(LogCoreAudio, Warning, TEXT("Audio config contains a cue without a tag. It is ignored."));
			continue;
		}
		if (CueMap.Contains(Cue.CueTag))
		{
			UE_LOG(LogCoreAudio, Warning, TEXT("Audio config contains the cue %s twice. The last entry wins."), *Cue.CueTag.ToString());
		}
		CueMap.Add(Cue.CueTag, Cue);
	}

	// Categories: saved player volumes win over the defaults of the config.
	for (const FAudioCategoryDefinition& Category : Config->Categories)
	{
		if (!Category.Category.IsValid())
		{
			continue;
		}

		CategoryPlayWhenPaused.Add(Category.Category, Category.bPlayWhenPaused);

		float Volume = Category.DefaultVolume;
		GConfig->GetFloat(GAudioSection, *VolumeKey(Category.Category), Volume, GGameUserSettingsIni);
		CategoryVolumes.Add(Category.Category, FMath::Clamp(Volume, 0.0f, 1.0f));

		bool bMuted = false;
		GConfig->GetBool(GAudioSection, *MuteKey(Category.Category), bMuted, GGameUserSettingsIni);
		if (bMuted)
		{
			MutedCategories.Add(Category.Category);
		}
	}

	if (!bAudioEnabled)
	{
		return;
	}

	for (const FAudioCategoryDefinition& Category : Config->Categories)
	{
		if (!Category.Category.IsValid())
		{
			continue;
		}
		if (USoundClass* SoundClass = Category.SoundClass.LoadSynchronous())
		{
			CategoryClasses.Add(Category.Category, SoundClass);
		}
		else
		{
			UE_LOG(LogCoreAudio, Warning, TEXT("Audio category %s has no Sound Class. Its volume cannot be applied."), *Category.Category.ToString());
		}
	}

	ActiveMix = Config->MasterSoundMix.LoadSynchronous();
	if (!ActiveMix)
	{
		ActiveMix = NewObject<USoundMix>(this, TEXT("FrameworkAudioMix"));
	}

	// Preload: the handle keeps the sounds in memory, so the first play of a UI or common sound never waits.
	TArray<FSoftObjectPath> PreloadPaths;
	for (const TPair<FGameplayTag, FAudioCueDefinition>& Pair : CueMap)
	{
		if (Pair.Value.bPreload && !Pair.Value.Sound.IsNull())
		{
			PreloadPaths.AddUnique(Pair.Value.Sound.ToSoftObjectPath());
		}
	}
	if (PreloadPaths.Num() > 0)
	{
		PreloadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(PreloadPaths);
	}
}

bool UAudioFrameSubsystem::CanPlay() const
{
	if (!bAudioEnabled)
	{
		return false;
	}

	const UWorld* World = GetWorld();
	return World && World->bAllowAudioPlayback && !World->IsNetMode(NM_DedicatedServer);
}

// ---------------------------------------------------------------------------------------------------------------------
// Categories and volume
// ---------------------------------------------------------------------------------------------------------------------

void UAudioFrameSubsystem::SetCategoryVolume(FGameplayTag Category, float Volume)
{
	if (!Category.IsValid())
	{
		return;
	}

	Volume = FMath::Clamp(Volume, 0.0f, 1.0f);
	CategoryVolumes.Add(Category, Volume);
	GConfig->SetFloat(GAudioSection, *VolumeKey(Category), Volume, GGameUserSettingsIni);

	if (!EnsureMixApplied())
	{
		ApplyCategoryVolume(Category, Config ? Config->VolumeFadeTime : 0.0f);
	}
	OnCategoryVolumeChanged.Broadcast(Category, Volume);
}

float UAudioFrameSubsystem::GetCategoryVolume(FGameplayTag Category) const
{
	const float* Volume = CategoryVolumes.Find(Category);
	return Volume ? *Volume : 1.0f;
}

void UAudioFrameSubsystem::SetCategoryMuted(FGameplayTag Category, bool bMuted)
{
	if (!Category.IsValid())
	{
		return;
	}

	if (bMuted)
	{
		MutedCategories.Add(Category);
	}
	else
	{
		MutedCategories.Remove(Category);
	}
	GConfig->SetBool(GAudioSection, *MuteKey(Category), bMuted, GGameUserSettingsIni);

	if (!EnsureMixApplied())
	{
		ApplyCategoryVolume(Category, Config ? Config->VolumeFadeTime : 0.0f);
	}
	OnCategoryVolumeChanged.Broadcast(Category, GetCategoryVolume(Category));
}

bool UAudioFrameSubsystem::IsCategoryMuted(FGameplayTag Category) const
{
	return MutedCategories.Contains(Category);
}

void UAudioFrameSubsystem::SaveAudioSettings()
{
	if (GConfig)
	{
		GConfig->Flush(false, GGameUserSettingsIni);
	}
}

bool UAudioFrameSubsystem::EnsureMixApplied()
{
	UWorld* World = GetWorld();
	if (!ActiveMix || !World || !CanPlay() || MixWorld.Get() == World)
	{
		return false;
	}

	// A new world (first map, map travel, another PIE session) may have another audio device: apply everything again.
	MixWorld = World;
	UGameplayStatics::SetBaseSoundMix(World, ActiveMix);
	for (const TPair<FGameplayTag, float>& Pair : CategoryVolumes)
	{
		ApplyCategoryVolume(Pair.Key, 0.0f);
	}
	return true;
}

void UAudioFrameSubsystem::ApplyCategoryVolume(FGameplayTag Category, float FadeTime)
{
	UWorld* World = GetWorld();
	const TObjectPtr<USoundClass>* SoundClass = CategoryClasses.Find(Category);
	if (!ActiveMix || !World || !SoundClass || !SoundClass->Get() || !CanPlay())
	{
		return;
	}

	const float Volume = MutedCategories.Contains(Category) ? 0.0f : GetCategoryVolume(Category);

	// bApplyToChildren stays false on purpose: the Sound Class hierarchy already multiplies a parent's volume into
	// its children, so applying it to the children as well would count Master twice.
	UGameplayStatics::SetSoundMixClassOverride(World, ActiveMix, SoundClass->Get(), Volume, 1.0f, FadeTime, /*bApplyToChildren=*/false);
}

FGameplayTag UAudioFrameSubsystem::ResolveCategory(const FAudioCueDefinition* Cue, FGameplayTag Fallback) const
{
	if (Cue && Cue->Category.IsValid())
	{
		return Cue->Category;
	}
	return Fallback.IsValid() ? Fallback : FAudioTags::CategorySFX();
}

bool UAudioFrameSubsystem::IsPlayWhenPaused(FGameplayTag Category) const
{
	if (Category == FAudioTags::CategoryUI())
	{
		return true;
	}
	const bool* bPlay = CategoryPlayWhenPaused.Find(Category);
	return bPlay && *bPlay;
}

// ---------------------------------------------------------------------------------------------------------------------
// Cue library
// ---------------------------------------------------------------------------------------------------------------------

bool UAudioFrameSubsystem::HasCue(FGameplayTag CueTag) const
{
	return CueMap.Contains(CueTag);
}

const FAudioCueDefinition* UAudioFrameSubsystem::FindCue(FGameplayTag CueTag) const
{
	return CueMap.Find(CueTag);
}

// ---------------------------------------------------------------------------------------------------------------------
// Playing by cue
// ---------------------------------------------------------------------------------------------------------------------

FAudioHandle UAudioFrameSubsystem::PlayCue2D(FGameplayTag CueTag, float VolumeMultiplier, float PitchMultiplier)
{
	FSpawnRequest Request;
	Request.Kind = ESpawnKind::Flat;
	Request.Volume = VolumeMultiplier;
	Request.Pitch = PitchMultiplier;
	return PlayCueInternal(CueTag, Request);
}

FAudioHandle UAudioFrameSubsystem::PlayCueAtLocation(FGameplayTag CueTag, FVector Location, FRotator Rotation, float VolumeMultiplier, float PitchMultiplier)
{
	FSpawnRequest Request;
	Request.Kind = ESpawnKind::Location;
	Request.Location = Location;
	Request.Rotation = Rotation;
	Request.Volume = VolumeMultiplier;
	Request.Pitch = PitchMultiplier;
	return PlayCueInternal(CueTag, Request);
}

FAudioHandle UAudioFrameSubsystem::PlayCueAttached(FGameplayTag CueTag, USceneComponent* AttachToComponent, FName SocketName, float VolumeMultiplier, float PitchMultiplier)
{
	if (!AttachToComponent)
	{
		UE_LOG(LogCoreAudio, Warning, TEXT("PlayCueAttached(%s): no component to attach to."), *CueTag.ToString());
		return FAudioHandle();
	}

	FSpawnRequest Request;
	Request.Kind = ESpawnKind::Attached;
	Request.AttachTo = AttachToComponent;
	Request.Socket = SocketName;
	Request.Volume = VolumeMultiplier;
	Request.Pitch = PitchMultiplier;
	return PlayCueInternal(CueTag, Request);
}

FAudioHandle UAudioFrameSubsystem::PlayUICue(FGameplayTag CueTag, float VolumeMultiplier, float PitchMultiplier)
{
	FSpawnRequest Request;
	Request.Kind = ESpawnKind::Flat;
	Request.Volume = VolumeMultiplier;
	Request.Pitch = PitchMultiplier;
	Request.FallbackCategory = FAudioTags::CategoryUI();
	return PlayCueInternal(CueTag, Request);
}

FAudioHandle UAudioFrameSubsystem::PlayUIEvent(EAudioUIEvent Event, FGameplayTag OverrideCue)
{
	FGameplayTag Cue = OverrideCue;
	if (!Cue.IsValid() && Config)
	{
		switch (Event)
		{
		case EAudioUIEvent::ButtonHover:
			Cue = Config->DefaultButtonHoverCue;
			break;
		case EAudioUIEvent::ButtonClick:
			Cue = Config->DefaultButtonClickCue;
			break;
		case EAudioUIEvent::ScreenOpen:
			Cue = Config->DefaultScreenOpenCue;
			break;
		case EAudioUIEvent::ScreenClose:
			Cue = Config->DefaultScreenCloseCue;
			break;
		}
	}

	return Cue.IsValid() ? PlayUICue(Cue) : FAudioHandle();
}

FAudioHandle UAudioFrameSubsystem::PlayCueInternal(FGameplayTag CueTag, FSpawnRequest Request)
{
	if (!CanPlay())
	{
		return FAudioHandle();
	}

	const FAudioCueDefinition* Cue = FindCue(CueTag);
	if (!Cue)
	{
		UE_LOG(LogCoreAudio, Warning, TEXT("Cue %s is not in the audio config."), *CueTag.ToString());
		return FAudioHandle();
	}
	if (Cue->Sound.IsNull())
	{
		UE_LOG(LogCoreAudio, Warning, TEXT("Cue %s has no sound assigned."), *CueTag.ToString());
		return FAudioHandle();
	}

	// The handle is reserved now, so the caller can stop the sound even while it still loads.
	Request.HandleId = NextHandleId++;
	Request.RequestTime = FPlatformTime::Seconds();

	if (Cue->Sound.Get())
	{
		SpawnForCue(CueTag, Request);
		return FAudioHandle{ Request.HandleId };
	}

	PendingHandleIds.Add(Request.HandleId);
	const float MaxLate = Config ? Config->MaxLateCueSeconds : 1.0f;
	TSharedPtr<FStreamableHandle> LoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		Cue->Sound.ToSoftObjectPath(),
		FStreamableDelegate::CreateWeakLambda(this, [this, CueTag, Request, MaxLate]()
		{
			// Removed from the pending set by StopSound: the caller cancelled it.
			if (PendingHandleIds.Remove(Request.HandleId) == 0)
			{
				return;
			}
			if (FPlatformTime::Seconds() - Request.RequestTime > MaxLate)
			{
				UE_LOG(LogCoreAudio, Verbose, TEXT("Cue %s finished loading too late and was dropped."), *CueTag.ToString());
				return;
			}
			SpawnForCue(CueTag, Request);
		}));
	if (LoadHandle.IsValid())
	{
		CueLoadHandles.Add(CueTag, LoadHandle);
	}

	return FAudioHandle{ Request.HandleId };
}

void UAudioFrameSubsystem::SpawnForCue(FGameplayTag CueTag, const FSpawnRequest& Request)
{
	const FAudioCueDefinition* Cue = FindCue(CueTag);
	USoundBase* Sound = Cue ? Cue->Sound.Get() : nullptr;
	if (!Sound)
	{
		return;
	}

	UAudioComponent* Component = SpawnComponent(Sound, Cue, ResolveCategory(Cue, Request.FallbackCategory), Request);
	RegisterHandle(Request.HandleId, Component);
}

// ---------------------------------------------------------------------------------------------------------------------
// Playing by sound asset
// ---------------------------------------------------------------------------------------------------------------------

FAudioHandle UAudioFrameSubsystem::PlaySound2D(USoundBase* Sound, FGameplayTag Category, float VolumeMultiplier, float PitchMultiplier)
{
	FSpawnRequest Request;
	Request.Kind = ESpawnKind::Flat;
	Request.Volume = VolumeMultiplier;
	Request.Pitch = PitchMultiplier;
	return PlaySoundInternal(Sound, Category, Request);
}

FAudioHandle UAudioFrameSubsystem::PlaySoundAtLocation(USoundBase* Sound, FGameplayTag Category, FVector Location, FRotator Rotation, float VolumeMultiplier, float PitchMultiplier)
{
	FSpawnRequest Request;
	Request.Kind = ESpawnKind::Location;
	Request.Location = Location;
	Request.Rotation = Rotation;
	Request.Volume = VolumeMultiplier;
	Request.Pitch = PitchMultiplier;
	return PlaySoundInternal(Sound, Category, Request);
}

FAudioHandle UAudioFrameSubsystem::PlaySoundAttached(USoundBase* Sound, FGameplayTag Category, USceneComponent* AttachToComponent, FName SocketName, float VolumeMultiplier, float PitchMultiplier)
{
	if (!AttachToComponent)
	{
		UE_LOG(LogCoreAudio, Warning, TEXT("PlaySoundAttached: no component to attach to."));
		return FAudioHandle();
	}

	FSpawnRequest Request;
	Request.Kind = ESpawnKind::Attached;
	Request.AttachTo = AttachToComponent;
	Request.Socket = SocketName;
	Request.Volume = VolumeMultiplier;
	Request.Pitch = PitchMultiplier;
	return PlaySoundInternal(Sound, Category, Request);
}

FAudioHandle UAudioFrameSubsystem::PlaySoundInternal(USoundBase* Sound, FGameplayTag Category, FSpawnRequest Request)
{
	if (!Sound || !CanPlay())
	{
		return FAudioHandle();
	}

	Request.HandleId = NextHandleId++;
	UAudioComponent* Component = SpawnComponent(Sound, nullptr, ResolveCategory(nullptr, Category), Request);
	RegisterHandle(Request.HandleId, Component);
	return Component ? FAudioHandle{ Request.HandleId } : FAudioHandle();
}

// ---------------------------------------------------------------------------------------------------------------------
// Component creation
// ---------------------------------------------------------------------------------------------------------------------

UAudioComponent* UAudioFrameSubsystem::SpawnComponent(USoundBase* Sound, const FAudioCueDefinition* Cue, FGameplayTag Category, const FSpawnRequest& Request)
{
	UWorld* World = GetWorld();
	if (!Sound || !World)
	{
		return nullptr;
	}

	USceneComponent* AttachTo = Request.AttachTo.Get();
	if (Request.Kind == ESpawnKind::Attached && !AttachTo)
	{
		return nullptr; // The component was destroyed while the cue loaded.
	}

	EnsureMixApplied();

	// Same creation path as UGameplayStatics::SpawnSound*, but not started yet: the Sound Class must be set before Play.
	FAudioDevice::FCreateComponentParams Params(World, AttachTo ? AttachTo->GetOwner() : nullptr);
	if (Cue)
	{
		Params.AttenuationSettings = Cue->AttenuationOverride.Get();
		if (Cue->ConcurrencyOverride)
		{
			Params.ConcurrencySet.Add(Cue->ConcurrencyOverride.Get());
		}
	}
	if (Request.Kind == ESpawnKind::Location)
	{
		Params.SetLocation(Request.Location);
	}
	else if (Request.Kind == ESpawnKind::Attached)
	{
		Params.SetLocation(AttachTo->GetSocketLocation(Request.Socket));
	}

	UAudioComponent* Component = FAudioDevice::CreateComponent(Sound, Params);
	if (!Component)
	{
		return nullptr; // Not audible from the listener's position, or no audio device.
	}

	const float Volume = Request.Volume * (Cue ? Cue->VolumeMultiplier * Spread(Cue->VolumeRandomRange) : 1.0f);
	const float Pitch = Request.Pitch * (Cue ? Cue->PitchMultiplier * Spread(Cue->PitchRandomRange) : 1.0f);

	Component->SetVolumeMultiplier(Volume);
	Component->SetPitchMultiplier(Pitch);
	Component->bAutoDestroy = true;
	Component->SubtitlePriority = Sound->GetSubtitlePriority();
	Component->bIsUISound = IsPlayWhenPaused(Category);

	if (const TObjectPtr<USoundClass>* SoundClass = CategoryClasses.Find(Category))
	{
		Component->SoundClassOverride = SoundClass->Get();
	}

	switch (Request.Kind)
	{
	case ESpawnKind::Flat:
		Component->bAllowSpatialization = false;
		break;

	case ESpawnKind::Location:
		Component->SetWorldLocationAndRotation(Request.Location, Request.Rotation);
		Component->bAllowSpatialization = Params.ShouldUseAttenuation();
		break;

	case ESpawnKind::Attached:
		Component->AttachToComponent(AttachTo, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Request.Socket);
		Component->bAllowSpatialization = Params.ShouldUseAttenuation();
		break;
	}

	Component->Play();
	return Component;
}

// ---------------------------------------------------------------------------------------------------------------------
// Handles
// ---------------------------------------------------------------------------------------------------------------------

void UAudioFrameSubsystem::RegisterHandle(int32 HandleId, UAudioComponent* Component)
{
	if (!Component || HandleId <= 0)
	{
		return;
	}

	if (Handles.Num() >= HandlePruneThreshold)
	{
		for (auto It = Handles.CreateIterator(); It; ++It)
		{
			if (!It.Value().IsValid())
			{
				It.RemoveCurrent();
			}
		}
	}
	Handles.Add(HandleId, Component);
}

UAudioComponent* UAudioFrameSubsystem::ResolveHandle(FAudioHandle Handle) const
{
	if (!Handle.IsValid())
	{
		return nullptr;
	}
	const TWeakObjectPtr<UAudioComponent>* Found = Handles.Find(Handle.Id);
	return Found ? Found->Get() : nullptr;
}

void UAudioFrameSubsystem::FadeOutOrStop(UAudioComponent* Component, float FadeTime, EAudioFaderCurve Curve)
{
	if (!Component)
	{
		return;
	}
	if (FadeTime > 0.0f && Component->IsPlaying())
	{
		Component->FadeOut(FadeTime, 0.0f, Curve);
	}
	else
	{
		Component->Stop();
	}
}

void UAudioFrameSubsystem::StopSound(FAudioHandle Handle, float FadeOutTime)
{
	if (!Handle.IsValid())
	{
		return;
	}

	// Still loading: cancel the pending play.
	if (PendingHandleIds.Remove(Handle.Id) > 0)
	{
		return;
	}

	FadeOutOrStop(ResolveHandle(Handle), FadeOutTime, EAudioFaderCurve::Linear);
	Handles.Remove(Handle.Id);
}

bool UAudioFrameSubsystem::IsSoundPlaying(FAudioHandle Handle) const
{
	if (PendingHandleIds.Contains(Handle.Id))
	{
		return true;
	}
	const UAudioComponent* Component = ResolveHandle(Handle);
	return Component && Component->IsPlaying();
}

void UAudioFrameSubsystem::SetHandleVolume(FAudioHandle Handle, float VolumeMultiplier, float FadeTime)
{
	if (UAudioComponent* Component = ResolveHandle(Handle))
	{
		if (FadeTime > 0.0f)
		{
			Component->AdjustVolume(FadeTime, VolumeMultiplier);
		}
		else
		{
			Component->SetVolumeMultiplier(VolumeMultiplier);
		}
	}
}

void UAudioFrameSubsystem::SetHandlePitch(FAudioHandle Handle, float PitchMultiplier)
{
	if (UAudioComponent* Component = ResolveHandle(Handle))
	{
		Component->SetPitchMultiplier(PitchMultiplier);
	}
}

void UAudioFrameSubsystem::SetHandleFloatParameter(FAudioHandle Handle, FName ParameterName, float Value)
{
	if (UAudioComponent* Component = ResolveHandle(Handle))
	{
		Component->SetFloatParameter(ParameterName, Value);
	}
}

void UAudioFrameSubsystem::SetHandleBoolParameter(FAudioHandle Handle, FName ParameterName, bool bValue)
{
	if (UAudioComponent* Component = ResolveHandle(Handle))
	{
		Component->SetBoolParameter(ParameterName, bValue);
	}
}

void UAudioFrameSubsystem::SetHandleIntParameter(FAudioHandle Handle, FName ParameterName, int32 Value)
{
	if (UAudioComponent* Component = ResolveHandle(Handle))
	{
		Component->SetIntParameter(ParameterName, Value);
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Music and ambience
// ---------------------------------------------------------------------------------------------------------------------

FAudioLayerState& UAudioFrameSubsystem::GetLayer(EAudioLayer Layer)
{
	return Layer == EAudioLayer::Music ? MusicLayer : AmbientLayer;
}

const FAudioLayerState& UAudioFrameSubsystem::GetLayer(EAudioLayer Layer) const
{
	return Layer == EAudioLayer::Music ? MusicLayer : AmbientLayer;
}

float UAudioFrameSubsystem::GetLayerDefaultFade(EAudioLayer Layer) const
{
	if (!Config)
	{
		return 1.0f;
	}
	return Layer == EAudioLayer::Music ? Config->DefaultMusicFadeTime : Config->DefaultAmbientFadeTime;
}

FAudioHandle UAudioFrameSubsystem::PlayMusic(FGameplayTag CueTag, float FadeTime, bool bRestartIfSame)
{
	return PlayLayerCue(EAudioLayer::Music, CueTag, FadeTime, bRestartIfSame);
}

FAudioHandle UAudioFrameSubsystem::PlayMusicSound(USoundBase* Sound, float FadeTime, bool bRestartIfSame)
{
	return PlayLayerSound(EAudioLayer::Music, Sound, FadeTime, bRestartIfSame);
}

void UAudioFrameSubsystem::StopMusic(float FadeOutTime)
{
	StopLayer(EAudioLayer::Music, FadeOutTime);
}

FAudioHandle UAudioFrameSubsystem::PlayAmbient(FGameplayTag CueTag, float FadeTime, bool bRestartIfSame)
{
	return PlayLayerCue(EAudioLayer::Ambient, CueTag, FadeTime, bRestartIfSame);
}

FAudioHandle UAudioFrameSubsystem::PlayAmbientSound(USoundBase* Sound, float FadeTime, bool bRestartIfSame)
{
	return PlayLayerSound(EAudioLayer::Ambient, Sound, FadeTime, bRestartIfSame);
}

void UAudioFrameSubsystem::StopAmbient(float FadeOutTime)
{
	StopLayer(EAudioLayer::Ambient, FadeOutTime);
}

FAudioHandle UAudioFrameSubsystem::GetLayerHandle(EAudioLayer Layer) const
{
	const FAudioLayerState& State = GetLayer(Layer);
	return (State.Current && State.Current->IsPlaying()) ? FAudioHandle{ State.CurrentHandleId } : FAudioHandle();
}

FAudioHandle UAudioFrameSubsystem::PlayLayerSound(EAudioLayer Layer, USoundBase* Sound, float FadeTime, bool bRestartIfSame)
{
	if (!Sound || !CanPlay())
	{
		return FAudioHandle();
	}

	FAudioLayerState& State = GetLayer(Layer);
	++State.RequestSerial; // Supersedes a cue of this layer that is still loading.
	return FAudioHandle{ StartLayerTrack(Layer, Sound, nullptr, FadeTime, bRestartIfSame, NextHandleId++) };
}

FAudioHandle UAudioFrameSubsystem::PlayLayerCue(EAudioLayer Layer, FGameplayTag CueTag, float FadeTime, bool bRestartIfSame)
{
	if (!CanPlay())
	{
		return FAudioHandle();
	}

	const FAudioCueDefinition* Cue = FindCue(CueTag);
	if (!Cue || Cue->Sound.IsNull())
	{
		UE_LOG(LogCoreAudio, Warning, TEXT("Cue %s is not in the audio config or has no sound."), *CueTag.ToString());
		return FAudioHandle();
	}

	FAudioLayerState& State = GetLayer(Layer);
	const uint32 Serial = ++State.RequestSerial;
	const int32 HandleId = NextHandleId++;

	if (USoundBase* Loaded = Cue->Sound.Get())
	{
		return FAudioHandle{ StartLayerTrack(Layer, Loaded, Cue, FadeTime, bRestartIfSame, HandleId) };
	}

	// Not loaded yet: start when it has. A newer request on the same layer, or StopSound, cancels this one.
	PendingHandleIds.Add(HandleId);
	TSharedPtr<FStreamableHandle> LoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		Cue->Sound.ToSoftObjectPath(),
		FStreamableDelegate::CreateWeakLambda(this, [this, Layer, CueTag, FadeTime, bRestartIfSame, HandleId, Serial]()
		{
			if (PendingHandleIds.Remove(HandleId) == 0 || GetLayer(Layer).RequestSerial != Serial)
			{
				return;
			}

			const FAudioCueDefinition* LoadedCue = FindCue(CueTag);
			USoundBase* Sound = LoadedCue ? LoadedCue->Sound.Get() : nullptr;
			if (Sound)
			{
				StartLayerTrack(Layer, Sound, LoadedCue, FadeTime, bRestartIfSame, HandleId);
			}
		}));
	if (LoadHandle.IsValid())
	{
		CueLoadHandles.Add(CueTag, LoadHandle);
	}

	return FAudioHandle{ HandleId };
}

int32 UAudioFrameSubsystem::StartLayerTrack(EAudioLayer Layer, USoundBase* Sound, const FAudioCueDefinition* Cue, float FadeTime, bool bRestartIfSame, int32 HandleId)
{
	UWorld* World = GetWorld();
	if (!Sound || !World || !CanPlay())
	{
		return 0;
	}

	FAudioLayerState& State = GetLayer(Layer);

	// Same track already audible: nothing to do.
	if (!bRestartIfSame && State.Current && State.Current->IsPlaying() && State.Current->Sound == Sound)
	{
		return State.CurrentHandleId;
	}

	EnsureMixApplied();

	const float Fade = FadeTime >= 0.0f ? FadeTime : GetLayerDefaultFade(Layer);
	const EAudioFaderCurve Curve = Config ? Config->CrossfadeCurve : EAudioFaderCurve::Linear;

	// Crossfade: the audible track becomes the fading one. A track that was still fading from before is cut.
	if (State.Previous)
	{
		State.Previous->Stop();
		State.Previous = nullptr;
	}
	if (State.Current)
	{
		FadeOutOrStop(State.Current, Fade, Curve);
		State.Previous = State.Current;
	}

	// Created against the audio device, not the world, so the track survives map travel.
	FAudioDevice::FCreateComponentParams Params(World->GetAudioDeviceRaw());
	UAudioComponent* Component = FAudioDevice::CreateComponent(Sound, Params);
	if (!Component)
	{
		State.Current = nullptr;
		State.CurrentHandleId = 0;
		return 0;
	}

	const FGameplayTag Category = ResolveCategory(Cue, Layer == EAudioLayer::Music ? FAudioTags::CategoryMusic() : FAudioTags::CategoryAmbient());

	Component->SetVolumeMultiplier(Cue ? Cue->VolumeMultiplier * Spread(Cue->VolumeRandomRange) : 1.0f);
	Component->SetPitchMultiplier(Cue ? Cue->PitchMultiplier * Spread(Cue->PitchRandomRange) : 1.0f);
	Component->bAllowSpatialization = false;
	Component->bAutoDestroy = true;
	Component->bStopWhenOwnerDestroyed = false;
	Component->bIgnoreForFlushing = true;
	Component->bIsUISound = IsPlayWhenPaused(Category);
	Component->SubtitlePriority = Sound->GetSubtitlePriority();
	if (const TObjectPtr<USoundClass>* SoundClass = CategoryClasses.Find(Category))
	{
		Component->SoundClassOverride = SoundClass->Get();
	}

	if (Fade > 0.0f)
	{
		Component->FadeIn(Fade, 1.0f, 0.0f, Curve);
	}
	else
	{
		Component->Play();
	}

	State.Current = Component;
	State.CurrentHandleId = HandleId;
	RegisterHandle(HandleId, Component);
	return HandleId;
}

void UAudioFrameSubsystem::StopLayer(EAudioLayer Layer, float FadeOutTime)
{
	FAudioLayerState& State = GetLayer(Layer);
	++State.RequestSerial; // Cancels a track that is still loading.

	const float Fade = FadeOutTime >= 0.0f ? FadeOutTime : GetLayerDefaultFade(Layer);
	const EAudioFaderCurve Curve = Config ? Config->CrossfadeCurve : EAudioFaderCurve::Linear;

	if (State.Previous)
	{
		State.Previous->Stop();
		State.Previous = nullptr;
	}
	if (State.Current)
	{
		FadeOutOrStop(State.Current, Fade, Curve);
		State.Previous = State.Current;
		State.Current = nullptr;
		State.CurrentHandleId = 0;
	}
}
