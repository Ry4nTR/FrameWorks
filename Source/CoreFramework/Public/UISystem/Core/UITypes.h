// CoreFramework UI System - Core
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UITypes.generated.h"

COREFRAMEWORK_API DECLARE_LOG_CATEGORY_EXTERN(LogCoreUI, Log, All);

class UBaseActivatableWidget;
class UBaseTabWidget;
class UBaseUserWidget;

/** Delegate used by the input action binding helpers of UBaseUserWidget and UBaseActivatableWidget. */
DECLARE_DYNAMIC_DELEGATE(FBaseUIActionDelegate);

/**
 * How an activatable screen wants input routed while it is the topmost active screen.
 * Mapped onto CommonUI's input configuration (Game / Menu / All).
 */
UENUM(BlueprintType)
enum class EUIInputMode : uint8
{
	/** Do not request an input configuration; whatever is active stays active. */
	Default UMETA(DisplayName = "Keep Current"),

	/** Game receives all input. Use for the gameplay HUD. */
	GameOnly UMETA(DisplayName = "Game Only"),

	/** UI receives all input, cursor visible, game input blocked. Use for menus. */
	UIOnly UMETA(DisplayName = "UI Only"),

	/** UI gets first refusal, unconsumed input still reaches the game. Use for live-gameplay overlays. */
	GameAndUI UMETA(DisplayName = "Game And UI")
};

/**
 * Registry entry describing one screen (menu, HUD, popup...) that the UI subsystem can push by tag.
 * Lives in UUIConfigDataAsset or is registered at runtime through UUIFrameSubsystem::RegisterScreen.
 * The framework ships no screens: every entry belongs to the game project.
 */
USTRUCT(BlueprintType)
struct COREFRAMEWORK_API FUIScreenDefinition
{
	GENERATED_BODY()

	/** Unique id used with PushScreen / PopScreen. Create your own tags (for example UI.Screen.MainMenu) in Project Settings > Gameplay Tags. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Screen")
	FGameplayTag ScreenTag;

	/** Soft reference: nothing is loaded until the screen is requested (or preloaded). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Screen")
	TSoftClassPtr<UBaseActivatableWidget> WidgetClass;

	/** Layer (stack) the screen is pushed onto: one of the CoreFramework.UI.Layer.* tags or a project layer. Empty falls back to the Menu layer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Screen")
	FGameplayTag LayerTag;

	/** When false, pushing a screen that is already open returns the open instance instead of stacking a second one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Screen")
	bool bAllowMultipleInstances = false;

	/** Load at UI start-up and keep the class resident so the first open never hitches. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Screen")
	bool bPreloadAndKeepResident = false;
};

/** One tab of a tabbed menu (Settings, Main Menu...). */
USTRUCT(BlueprintType)
struct COREFRAMEWORK_API FUITabDescriptor
{
	GENERATED_BODY()

	/** Unique id inside one menu. A later descriptor with the same id replaces an earlier one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tab")
	FName TabId = NAME_None;

	/** Text shown on the tab button. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tab")
	FText DisplayName;

	/** Content widget. Async loaded when the menu opens. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tab")
	TSoftClassPtr<UBaseTabWidget> TabWidgetClass;

	/** Lower values appear first. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tab")
	int32 SortOrder = 0;

	/** Set false on an overriding descriptor to remove a tab that another source provides. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tab")
	bool bEnabled = true;
};

/** A widget that is injected into a named slot of the HUD widget (health bar, minimap, quest tracker...). */
USTRUCT(BlueprintType)
struct COREFRAMEWORK_API FUIHUDElementDescriptor
{
	GENERATED_BODY()

	/** Unique id. Registering the same id twice replaces the earlier entry. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD Element")
	FName ElementId = NAME_None;

	/** Slot registered on the HUD widget (UBaseHUDWidget::RegisterElementSlot). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD Element")
	FGameplayTag SlotTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD Element")
	TSoftClassPtr<UBaseUserWidget> WidgetClass;
};
