// CoreFramework Audio System - Core
#include "AudioSystem/Core/AudioGameplayTags.h"

#include "NativeGameplayTags.h"

UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_CoreAudio_Category_Master, "CoreFramework.Audio.Category.Master", "Master volume: parent of every other category");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_CoreAudio_Category_SFX, "CoreFramework.Audio.Category.SFX", "Sound effects in the game world");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_CoreAudio_Category_Music, "CoreFramework.Audio.Category.Music", "Background music");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_CoreAudio_Category_Ambient, "CoreFramework.Audio.Category.Ambient", "Ambient beds and environment loops");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_CoreAudio_Category_UI, "CoreFramework.Audio.Category.UI", "Menu clicks, hovers, popups");

FGameplayTag FAudioTags::CategoryMaster() { return TAG_CoreAudio_Category_Master.GetTag(); }
FGameplayTag FAudioTags::CategorySFX() { return TAG_CoreAudio_Category_SFX.GetTag(); }
FGameplayTag FAudioTags::CategoryMusic() { return TAG_CoreAudio_Category_Music.GetTag(); }
FGameplayTag FAudioTags::CategoryAmbient() { return TAG_CoreAudio_Category_Ambient.GetTag(); }
FGameplayTag FAudioTags::CategoryUI() { return TAG_CoreAudio_Category_UI.GetTag(); }
