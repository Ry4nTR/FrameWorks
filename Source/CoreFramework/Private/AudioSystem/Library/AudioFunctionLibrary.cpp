// CoreFramework Audio System - Library
#include "AudioSystem/Library/AudioFunctionLibrary.h"

#include "AudioSystem/Core/AudioGameplayTags.h"
#include "AudioSystem/Manager/AudioFrameSubsystem.h"

UAudioFrameSubsystem* UAudioFunctionLibrary::GetAudioSubsystem(const UObject* WorldContextObject)
{
	return UAudioFrameSubsystem::Get(WorldContextObject);
}

FAudioHandle UAudioFunctionLibrary::PlayCue2D(const UObject* WorldContextObject, FGameplayTag CueTag, float VolumeMultiplier, float PitchMultiplier)
{
	UAudioFrameSubsystem* Subsystem = UAudioFrameSubsystem::Get(WorldContextObject);
	return Subsystem ? Subsystem->PlayCue2D(CueTag, VolumeMultiplier, PitchMultiplier) : FAudioHandle();
}

FAudioHandle UAudioFunctionLibrary::PlayCueAtLocation(const UObject* WorldContextObject, FGameplayTag CueTag, FVector Location, FRotator Rotation, float VolumeMultiplier, float PitchMultiplier)
{
	UAudioFrameSubsystem* Subsystem = UAudioFrameSubsystem::Get(WorldContextObject);
	return Subsystem ? Subsystem->PlayCueAtLocation(CueTag, Location, Rotation, VolumeMultiplier, PitchMultiplier) : FAudioHandle();
}

FAudioHandle UAudioFunctionLibrary::PlayCueAttached(const UObject* WorldContextObject, FGameplayTag CueTag, USceneComponent* AttachToComponent, FName SocketName, float VolumeMultiplier, float PitchMultiplier)
{
	UAudioFrameSubsystem* Subsystem = UAudioFrameSubsystem::Get(WorldContextObject);
	return Subsystem ? Subsystem->PlayCueAttached(CueTag, AttachToComponent, SocketName, VolumeMultiplier, PitchMultiplier) : FAudioHandle();
}

FAudioHandle UAudioFunctionLibrary::PlayUICue(const UObject* WorldContextObject, FGameplayTag CueTag)
{
	UAudioFrameSubsystem* Subsystem = UAudioFrameSubsystem::Get(WorldContextObject);
	return Subsystem ? Subsystem->PlayUICue(CueTag) : FAudioHandle();
}

FAudioHandle UAudioFunctionLibrary::PlaySound2D(const UObject* WorldContextObject, USoundBase* Sound, FGameplayTag Category, float VolumeMultiplier, float PitchMultiplier)
{
	UAudioFrameSubsystem* Subsystem = UAudioFrameSubsystem::Get(WorldContextObject);
	return Subsystem ? Subsystem->PlaySound2D(Sound, Category, VolumeMultiplier, PitchMultiplier) : FAudioHandle();
}

FAudioHandle UAudioFunctionLibrary::PlaySoundAtLocation(const UObject* WorldContextObject, USoundBase* Sound, FGameplayTag Category, FVector Location, FRotator Rotation, float VolumeMultiplier, float PitchMultiplier)
{
	UAudioFrameSubsystem* Subsystem = UAudioFrameSubsystem::Get(WorldContextObject);
	return Subsystem ? Subsystem->PlaySoundAtLocation(Sound, Category, Location, Rotation, VolumeMultiplier, PitchMultiplier) : FAudioHandle();
}

void UAudioFunctionLibrary::StopSound(const UObject* WorldContextObject, FAudioHandle Handle, float FadeOutTime)
{
	if (UAudioFrameSubsystem* Subsystem = UAudioFrameSubsystem::Get(WorldContextObject))
	{
		Subsystem->StopSound(Handle, FadeOutTime);
	}
}

void UAudioFunctionLibrary::SetHandleFloatParameter(const UObject* WorldContextObject, FAudioHandle Handle, FName ParameterName, float Value)
{
	if (UAudioFrameSubsystem* Subsystem = UAudioFrameSubsystem::Get(WorldContextObject))
	{
		Subsystem->SetHandleFloatParameter(Handle, ParameterName, Value);
	}
}

FAudioHandle UAudioFunctionLibrary::PlayMusic(const UObject* WorldContextObject, FGameplayTag CueTag, float FadeTime, bool bRestartIfSame)
{
	UAudioFrameSubsystem* Subsystem = UAudioFrameSubsystem::Get(WorldContextObject);
	return Subsystem ? Subsystem->PlayMusic(CueTag, FadeTime, bRestartIfSame) : FAudioHandle();
}

void UAudioFunctionLibrary::StopMusic(const UObject* WorldContextObject, float FadeOutTime)
{
	if (UAudioFrameSubsystem* Subsystem = UAudioFrameSubsystem::Get(WorldContextObject))
	{
		Subsystem->StopMusic(FadeOutTime);
	}
}

FAudioHandle UAudioFunctionLibrary::PlayAmbient(const UObject* WorldContextObject, FGameplayTag CueTag, float FadeTime, bool bRestartIfSame)
{
	UAudioFrameSubsystem* Subsystem = UAudioFrameSubsystem::Get(WorldContextObject);
	return Subsystem ? Subsystem->PlayAmbient(CueTag, FadeTime, bRestartIfSame) : FAudioHandle();
}

void UAudioFunctionLibrary::StopAmbient(const UObject* WorldContextObject, float FadeOutTime)
{
	if (UAudioFrameSubsystem* Subsystem = UAudioFrameSubsystem::Get(WorldContextObject))
	{
		Subsystem->StopAmbient(FadeOutTime);
	}
}

void UAudioFunctionLibrary::SetCategoryVolume(const UObject* WorldContextObject, FGameplayTag Category, float Volume)
{
	if (UAudioFrameSubsystem* Subsystem = UAudioFrameSubsystem::Get(WorldContextObject))
	{
		Subsystem->SetCategoryVolume(Category, Volume);
	}
}

float UAudioFunctionLibrary::GetCategoryVolume(const UObject* WorldContextObject, FGameplayTag Category)
{
	const UAudioFrameSubsystem* Subsystem = UAudioFrameSubsystem::Get(WorldContextObject);
	return Subsystem ? Subsystem->GetCategoryVolume(Category) : 1.0f;
}

void UAudioFunctionLibrary::SetCategoryMuted(const UObject* WorldContextObject, FGameplayTag Category, bool bMuted)
{
	if (UAudioFrameSubsystem* Subsystem = UAudioFrameSubsystem::Get(WorldContextObject))
	{
		Subsystem->SetCategoryMuted(Category, bMuted);
	}
}

FGameplayTag UAudioFunctionLibrary::GetCategoryMaster() { return FAudioTags::CategoryMaster(); }
FGameplayTag UAudioFunctionLibrary::GetCategorySFX() { return FAudioTags::CategorySFX(); }
FGameplayTag UAudioFunctionLibrary::GetCategoryMusic() { return FAudioTags::CategoryMusic(); }
FGameplayTag UAudioFunctionLibrary::GetCategoryAmbient() { return FAudioTags::CategoryAmbient(); }
FGameplayTag UAudioFunctionLibrary::GetCategoryUI() { return FAudioTags::CategoryUI(); }
