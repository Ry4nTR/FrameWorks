// CoreFramework UI System - Core
#include "UISystem/Core/UIGameplayTags.h"

#include "NativeGameplayTags.h"

UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_CoreUI_Layer_Game, "CoreFramework.UI.Layer.Game", "Persistent gameplay layer (HUD)");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_CoreUI_Layer_Menu, "CoreFramework.UI.Layer.Menu", "Full screen menus");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_CoreUI_Layer_Modal, "CoreFramework.UI.Layer.Modal", "Popups and dialogs that block the layers below");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_CoreUI_Layer_Overlay, "CoreFramework.UI.Layer.Overlay", "Notifications, toasts, loading indicators");

UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_CoreUI_HUDSlot_Default, "CoreFramework.UI.HUDSlot.Default", "Default HUD element container");

FGameplayTag FUITags::LayerGame() { return TAG_CoreUI_Layer_Game.GetTag(); }
FGameplayTag FUITags::LayerMenu() { return TAG_CoreUI_Layer_Menu.GetTag(); }
FGameplayTag FUITags::LayerModal() { return TAG_CoreUI_Layer_Modal.GetTag(); }
FGameplayTag FUITags::LayerOverlay() { return TAG_CoreUI_Layer_Overlay.GetTag(); }

FGameplayTag FUITags::HUDSlotDefault() { return TAG_CoreUI_HUDSlot_Default.GetTag(); }
