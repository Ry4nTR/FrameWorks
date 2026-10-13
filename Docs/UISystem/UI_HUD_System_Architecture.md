# CoreFramework UI & HUD System — Architecture & Technical Specification (v2)

**Document:** `UI_HUD_System_Architecture.md`
**Module:** `CoreFramework` (plugin, export macro `COREFRAMEWORK_API`)
**Engine target:** Unreal Engine 5.5 – 5.8
**Companion:** `UI_HUD_Setup_Guide.md` (beginner friendly, step by step)

> **What v2 changes (v3 of the setup guide moves the shared CommonUI assets into the plugin; no C++ changed).** The framework is now strictly **infrastructure**. It ships C++ base classes and three shared **assets** that are identical in every game: the root layout with its four empty layer stacks, the CommonUI UI-actions table and the CommonUI input data Blueprint. It ships no menus, no HUD layout, no styles, no art. Every screen of a game is built **in the game project** from the base parents. The C++ is organised in responsibility folders. Section 10 lists the audit results and every change.
>
> **Verification status.** Written against the CommonUI / Enhanced Input / GameplayTags APIs of UE 5.5–5.8 and checked by careful review, but **not compiled** by its author (no engine available there). Appendix A lists every engine API the system touches with the one-line fix if a signature differs in your engine version.

---

## 1. Goals and key decisions

| Requirement | Decision | Why |
|---|---|---|
| **Clean infrastructure only** | The plugin contains input mode handling, CommonUI routing, layer stacks, async loading, player binding, data assets, base classes. Nothing a player can see except the empty root layout. | The framework must work for any genre; visuals are a game decision. |
| **Zero pre-baked layouts** | No Main Menu, Pause Menu, Settings or HUD Blueprint exists in the plugin. No screen ids are shipped either (only layer ids and one default HUD slot id). | A shipped menu would be deleted or fought against in every game. |
| **Reusable parents** | Game widgets use the C++ classes **directly as Blueprint parents**. No empty wrapper Blueprints. | A wrapper adds an extra asset to maintain with no behavior; defaults live in C++ constructors. |
| Stack / layer management, back handling, input routing, gamepad focus | **Build on CommonUI** (`UCommonActivatableWidget`, `UCommonActivatableWidgetStack`, `UCommonTabListWidgetBase`, the CommonUI action router). | CommonUI solves modal input blocking, back action and device switching. The framework adds a declarative layer on top instead of competing with it. |
| Async UI loading | Every screen, HUD element and tab page is a **soft class reference** streamed through the asset manager's streamable manager. Optional preload-and-keep-resident. | No hitch when opening a menu. |
| Multiplayer, split screen | One `UUIFrameSubsystem` per **`ULocalPlayer`**; bootstrap from `AHUD` (only exists for local controllers). | No UI objects on dedicated servers. |
| Extensible without editing framework code | Layers, screens and slots are **gameplay tags** owned by the game; screens, HUD elements and tabs live in **data assets** and **runtime registries**. | A game adds content by creating assets. |
| Safe possess / unpossess / respawn / late replication | **Observer hub** (`UUIBindingHub`) with a strict Bound/Unbound pairing guarantee. | Widgets never poll and never leak subscriptions to a dead pawn. |
| Organised project | C++ split into responsibility folders; game Content split into responsibility folders (Section 9). | Findability. |

---

## 2. Architecture overview

### 2.1 The separation rule

```
┌───────────────────────── CoreFramework plugin ──────────────────────────┐   ┌──────────────────────── Game project ────────────────────────┐
│ INFRASTRUCTURE ONLY                                                     │   │ ALL CONTENT AND DESIGN                                        │
│                                                                         │   │                                                               │
│ C++ base classes (parents)       UBaseUserWidget  UBaseActivatableWidget│   │ WBP_HUD            parent  UBaseHUDWidget                     │
│                                  UBaseButton      UBaseHUDWidget        │   │ WBP_MainMenu       parent  UBaseMenuWidget                    │
│                                  UBaseMenuWidget  UBaseTabWidget        │   │ WBP_PauseMenu      parent  UBaseMenuWidget                    │
│                                  UBaseRootLayoutWidget  ABaseHUD        │   │ WBP_Settings       parent  UBaseMenuWidget                    │
│ Manager / binding                UUIFrameSubsystem  UUIBindingHub       │   │ WBP_Tab_*          parent  UBaseTabWidget                     │
│                                  IBaseUIBinderInterface                 │   │ WBP_Button_*       parent  UBaseButton                        │
│ Data asset CLASSES               UUIConfigDataAsset  UUITabSetDataAsset │   │ WBP_Popup_*        parent  UBaseActivatableWidget             │
│ Native ids                       CoreFramework.UI.Layer.*  HUDSlot.Default│  │ BP_Startup_*       parent  ABaseHUD                           │
│                                                                         │   │ DA_UIConfig, DA_Settings_Tabs   (instances of the data assets)│
│ Content: WBP_BaseRootLayout, DT_UI_Actions, BP_CommonInputData          │   │ Gameplay tags, styles, fonts, textures, animations            │
└─────────────────────────────────────────────────────────────────────────┘   └───────────────────────────────────────────────────────────────┘
        no art, no menus, no layout                  ──── inherits / references ────►        everything the player sees
```

The dependency only goes one way: the game depends on the plugin; the plugin never references a game asset or class.

### 2.2 Components

| Component | Class | Folder | Responsibility |
|---|---|---|---|
| Manager | `UUIFrameSubsystem` (`ULocalPlayerSubsystem`) | `Manager/` | Owns the root layout, pushes/pops screens by tag, async loads, holds the registries, owns the binding hub. |
| Startup actor | `ABaseHUD` (`AHUD`) | `HUD/` | Starts/stops the subsystem for its local controller. Draws nothing. |
| Root | `UBaseRootLayoutWidget` | `Layout/` | One per local player. Holds one `UCommonActivatableWidgetStack` per layer. |
| Screen base | `UBaseActivatableWidget` | `Foundation/` | Anything pushed on a layer. Declares input mode, focus, side effects. |
| Plain widget base | `UBaseUserWidget` | `Foundation/` | HUD elements, list entries. Show/Hide hook, binding, input helpers. |
| Button base | `UBaseButton` | `Foundation/` | Parent of every button: label text support on top of CommonUI buttons. |
| HUD container | `UBaseHUDWidget` | `HUD/` | Game layer container with named element slots. |
| Menu | `UBaseMenuWidget` | `Menu/` | Menu screen with an optional data-driven tab bar. |
| Tab page | `UBaseTabWidget` | `Menu/Tabs/` | Content of one tab; optional Apply / Revert contract. |
| Binding | `IBaseUIBinderInterface`, `UUIBindingHub` | `Binding/` | Controller / PlayerState / Pawn observer. |
| Data | `UUIConfigDataAsset`, `UUITabSetDataAsset` | `Data/` | Data asset classes (the game creates the instances). |
| Settings, ids, types | `UUIFrameSettings`, `FUITags`, `UITypes.h` | `Core/` | Project Settings page, native tags, enums and descriptors. |

### 2.3 Who owns what

| Thing | Owner | Where |
|---|---|---|
| Behavior (stacks, loading, input config, focus, binding, tab building) | Framework | C++ in the plugin |
| Layer structure (four stacks) | Framework | `WBP_BaseRootLayout` (plugin Content) |
| UI actions (Confirm / Back / Next Tab / Previous Tab) and the input data CommonUI reads | Framework | `DT_UI_Actions`, `BP_CommonInputData` (plugin Content) |
| Which screens a map opens when it starts | Game | Startup actors `BP_Startup_*` (parent `ABaseHUD`) |
| Which screen exists, which class, which layer | Game | `DA_UIConfig` |
| Screen ids, project layers, project slots | Game | Gameplay tags in the game project |
| Layout, art, fonts, styles, animations, texts | Game | Game Content |
| Game data shown on the HUD (health, ammo…) | Game | Game C++ components + widgets (Step 5 of the guide) |

---

## 3. Diagrams and data flow

### 3.1 Source folder layout (framework)

```
Plugins/CoreFramework/Source/CoreFramework/UISystem/
├─ Core/         UITypes.h  UIGameplayTags.h/.cpp  UIFrameSettings.h
│                  enums, descriptors, log category, native tags, Project Settings page
├─ Data/         UIConfigDataAsset.h
│                  UUIConfigDataAsset, UUITabSetDataAsset
├─ Manager/      UIFrameSubsystem.h/.cpp
│                  per-local-player manager, registries, async push
├─ Binding/      BaseUIBinderInterface.h  UIBindingHub.h/.cpp
│                  observer contract + hub
├─ Layout/       BaseRootLayoutWidget.h/.cpp
│                  layer stacks container
├─ Foundation/   BaseUserWidget.h/.cpp  BaseActivatableWidget.h/.cpp  BaseButton.h/.cpp
│                  parents of everything the game builds
├─ HUD/          BaseHUD.h/.cpp  BaseHUDWidget.h/.cpp
│                  bootstrap actor + HUD container widget
└─ Menu/         BaseMenuWidget.h/.cpp
   └─ Tabs/      BaseTabWidget.h/.cpp
                   menus and their tab pages
```

Include style: `#include "UISystem/<Folder>/<File>.h"` (works with the flat module layout used by CoreFramework).

### 3.2 Inheritance

```
UCommonUserWidget
 ├─ UBaseUserWidget  ········ [IBaseUIBinderInterface]  Show/Hide hook, binding, input helpers
 │   └─ UBaseTabWidget ······ tab page (HasPendingChanges / ApplyChanges / RevertChanges)
 │        └─ WBP_Tab_*            (GAME)
 └─ UBaseRootLayoutWidget ·· layer stacks
      └─ WBP_BaseRootLayout       (PLUGIN Content: shared asset)

UCommonActivatableWidget
 └─ UBaseActivatableWidget · [IBaseUIBinderInterface]  input mode, focus, pause/suppress, anim hook
     ├─ UBaseHUDWidget ····· Game layer container, named element slots
     │    └─ WBP_HUD              (GAME)
     ├─ UBaseMenuWidget ···· menu with optional tab bar
     │    └─ WBP_MainMenu, WBP_PauseMenu, WBP_Settings   (GAME)
     └─ WBP_Popup_*                                      (GAME, directly on this class)

UCommonButtonBase
 └─ UBaseButton ············ label text support
      └─ WBP_Button_*, WBP_Button_Tab                    (GAME)

UPrimaryDataAsset ─ UUIConfigDataAsset, UUITabSetDataAsset  ─►  DA_UIConfig, DA_Settings_Tabs   (GAME instances)
UDeveloperSettings ─ UUIFrameSettings
AHUD               ─ ABaseHUD ─► BP_Startup_Gameplay, BP_Startup_MainMenu                           (GAME)
ULocalPlayerSubsystem ─ UUIFrameSubsystem
UObject            ─ UUIBindingHub   (owned by the subsystem)
```

### 3.3 Content ownership (what exists where on disk)

```
Plugins/CoreFramework/Content/UI/
├─ Layout/
│  └─ WBP_BaseRootLayout            ← 4 named, empty stacks
└─ Input/
   ├─ DT_UI_Actions                 ← Confirm / Back / NextTab / PreviousTab
   └─ BP_CommonInputData            ← "back" and "click" actions for CommonUI

<GameProject>/Content/UI/           ← everything below is the game's
├─ Config/       DA_UIConfig, DA_Settings_Tabs
├─ Startup/      BP_Startup_MainMenu, BP_Startup_Gameplay (parent: ABaseHUD; which screens a map opens)
├─ HUD/          WBP_HUD                                  (parent: UBaseHUDWidget)
│  └─ Elements/  WBP_HealthBar, WBP_ManaBar …             (parent: UBaseUserWidget / game C++ child)
├─ Menus/
│  ├─ MainMenu/  WBP_MainMenu                             (parent: UBaseMenuWidget)
│  ├─ PauseMenu/ WBP_PauseMenu                            (parent: UBaseMenuWidget)
│  └─ Settings/  WBP_Settings                             (parent: UBaseMenuWidget)
│     └─ Tabs/   WBP_Tab_Graphics, WBP_Tab_Audio …        (parent: UBaseTabWidget)
├─ Popups/       WBP_Popup_Confirm …                      (parent: UBaseActivatableWidget)
└─ Components/
   ├─ Buttons/   WBP_Button_Primary, WBP_Button_Tab       (parent: UBaseButton)
   │  └─ Styles/ BP_ButtonStyle_*                         (CommonButtonStyle)
   └─ Text/      BP_TextStyle_*                           (CommonTextStyle)
```

### 3.4 Runtime ownership and lifetime

```
ULocalPlayer  (one per split-screen player, lives for the session)
 └─ UUIFrameSubsystem ................................ lifetime = local player
     ├─ UUIBindingHub ................................ Controller → PlayerState / Pawn observer
     ├─ Registries (plain maps)
     │    ├─ ScreenRegistry     FGameplayTag → FUIScreenDefinition      (from DA_UIConfig + RegisterScreen)
     │    ├─ HUDElementRegistry FName        → FUIHUDElementDescriptor  (from DA_UIConfig + RegisterHUDElement)
     │    └─ MenuTabRegistry    FGameplayTag → [FUITabDescriptor]       (RegisterMenuTab, runtime only)
     ├─ ResidentHandles ............................... keep "preload" classes in memory
     └─ UBaseRootLayoutWidget (WBP_BaseRootLayout) .... lifetime = HUD actor of the controller
         ├─ [Layer.Game]    Stack ─ game's HUD screen ─┬─ DefaultElementSlot ─ HUD element widgets
         │                                             └─ custom slots (RegisterElementSlot)
         ├─ [Layer.Menu]    Stack ─ game's menus ──────┬─ TabList (UBaseButton × N)
         │                                             └─ TabSwitcher ─ UBaseTabWidget × N
         ├─ [Layer.Modal]   Stack ─ game's popups (Is Modal)
         └─ [Layer.Overlay] Stack ─ notifications

APlayerController
 └─ ABaseHUD child ...... BeginPlay → InitializeForPlayerController(PC, StartupScreens, this)
                         EndPlay   → ReleasePlayerController(PC, this)
```

### 3.5 Start-up sequence

```
GameMode.HUDClass = BP_Startup_Gameplay  (child of ABaseHUD, StartupScreens = [UI.Screen.HUD])
        │
 local PlayerController spawns its HUD
        │
 ABaseHUD::BeginPlay
        │  UUIFrameSubsystem::InitializeForPlayerController
        │     ├─ EnsureConfigLoaded      DA_UIConfig (tiny, soft refs only) → fills Screen + HUD element registries,
        │     │                           preloads screens flagged "Preload And Keep Resident"
        │     ├─ UIBindingHub::Start(PC) → Controller bound, Refresh() → PlayerState / Pawn bound if present
        │     ├─ PushScreenAsync(each StartupScreen)   (queued: root layout does not exist yet)
        │     └─ BeginRootLoad           RequestAsyncLoad(DA_UIConfig.RootLayoutClass = WBP_BaseRootLayout)
        ▼
 root class streamed in (async)
        │  CreateWidget<UBaseRootLayoutWidget>(PC) → AddToPlayerScreen(ZOrder)
        │  NativeOnInitialized registers Game / Menu / Modal / Overlay stacks
        │  FlushDeferredPushes → PushScreenAsync for each queued screen, in order
        ▼
 the game's HUD widget on the Game layer → activated → CommonUI applies its input config ("Game")
        │  NativeConstruct: registers as binder (receives current Controller/PlayerState/Pawn),
        │                   registers DefaultElementSlot, spawns registered HUD elements (async)
```

### 3.6 Pushing a screen (async data flow)

```
Blueprint / C++:  UIFrameSubsystem->PushScreen(UI.Screen.Settings)        (a tag the GAME created)
        │
        ▼
 ScreenRegistry lookup ──(missing)──► warning + callback(null)
        │
        ├─ root layout not ready ─────► queued, replayed after root creation
        ├─ already open, not multi ───► returns the open instance (no duplicate)
        ├─ class already in memory ───► FinishPush immediately (synchronous)
        └─ otherwise
              PendingPushes[Tag] (joins an in-flight request if one exists)
              UAssetManager StreamableManager.RequestAsyncLoad(SoftClass)      ← no hitch
                    │ (game thread, next frames)
                    ▼
              FinishPush
                 Layer = RootLayout.FindLayer(Definition.LayerTag or Layer.Menu)
                 Layer->AddWidget<UBaseActivatableWidget>(Class, InitFunc)
                    InitFunc: SetScreenContext(ScreenTag, LayerTag)   (before activation)
                    CommonUI: construct → activate
                       UBaseActivatableWidget::NativeOnActivated
                          ├─ look/move suppression, optional pause
                          ├─ Anim_Activate
                          └─ default focus fallback (next tick)
                       CommonUI router: applies GetDesiredInputConfig() of the topmost screen
                       CommonUI router: applies the screen's desired focus target
              OnScreenPushed(Tag, Widget)  → all queued callbacks
```

### 3.7 Binding flow (possess, respawn, late replication)

```
 UIBindingHub (per local player)
   Triggers:  AController::OnPossessedPawnChanged   (instant)
              timer, default 0.2 s, pointer compare  (safety net: client replication order)
        │
   Refresh():  PlayerState changed?  ──► Unbound(old) → Bound(new)   to every binder
               Pawn changed?         ──► Unbound(old) → Bound(new)   to every binder
        │
   Binders = widgets implementing IBaseUIBinderInterface (registered in NativeConstruct,
             unregistered in NativeDestruct, optional per widget)

 Guarantees
   • Late join:    a widget that registers after the pawn exists immediately receives Controller/PlayerState/Pawn Bound.
   • Pairing:      every Bound is followed by exactly one Unbound (change, widget destroyed, hub stopped).
   • Respawn:      Unbound(oldPawn) then Bound(newPawn); old pawn may already be destroyed → argument can be null,
                   so implementers cache what they subscribed to.
```

---

## 4. Core C++ declarations

All headers below are the **actual source files** of the package.

### 4.1 `Core/UITypes.h` — enums, screen / tab / HUD element descriptors

```cpp
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
```

### 4.2 `Data/UIConfigDataAsset.h` — the data-driven entry point (instances live in the game)

```cpp
// CoreFramework UI System - Data
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "UISystem/Core/UITypes.h"
#include "UIConfigDataAsset.generated.h"

class UBaseRootLayoutWidget;

/** A reusable list of tabs. Create one per tabbed menu in the game project's Content Browser. */
UCLASS(BlueprintType)
class COREFRAMEWORK_API UUITabSetDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tabs", meta = (TitleProperty = "TabId"))
	TArray<FUITabDescriptor> Tabs;
};

/**
 * The single data-driven entry point of the UI system. One asset per game project, assigned in
 * Project Settings > CoreFramework > CoreFramework UI. Everything is soft-referenced.
 * The framework ships no content for it: the game fills every list.
 */
UCLASS(BlueprintType)
class COREFRAMEWORK_API UUIConfigDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Root layout widget Blueprint (child of UBaseRootLayoutWidget) holding the layer stacks. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Root")
	TSoftClassPtr<UBaseRootLayoutWidget> RootLayoutClass;

	/** Every screen the project can push by tag. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Screens", meta = (TitleProperty = "ScreenTag"))
	TArray<FUIScreenDefinition> Screens;

	/** Widgets injected into HUD slots. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HUD", meta = (TitleProperty = "ElementId"))
	TArray<FUIHUDElementDescriptor> HUDElements;
};
```

### 4.3 `Manager/UIFrameSubsystem.h` — the manager

```cpp
// CoreFramework UI System
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "UISystem/Core/UITypes.h"
#include "UIFrameSubsystem.generated.h"

class APawn;
class APlayerController;
class APlayerState;
class UBaseActivatableWidget;
class UBaseRootLayoutWidget;
class UCommonActivatableWidgetContainerBase;
class UUIBindingHub;
class UUIConfigDataAsset;
class UUserWidget;
struct FStreamableHandle;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnUIScreenPushed, FGameplayTag, ScreenTag, UBaseActivatableWidget*, Widget);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnUIHUDElementRegistered, const FUIHUDElementDescriptor&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnUIHUDElementUnregistered, FName);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnUIMenuTabsChanged, FGameplayTag);

/** Completion callback of an async screen push. Receives null when the push failed. */
using FUIScreenPushedCallback = TFunction<void(UBaseActivatableWidget*)>;

/**
 * Central manager of the UI of one local player (split-screen safe: one instance per ULocalPlayer).
 *
 * Responsibilities
 *  - Owns the root layout widget and its layers (Game, Menu, Modal, Overlay + project layers).
 *  - Pushes / pops screens by gameplay tag; loads their widget classes asynchronously from soft references.
 *  - Keeps the registries that make the system extensible without touching framework code:
 *    screens, HUD elements, menu tabs. Filled from UUIConfigDataAsset and from runtime Register* calls.
 *  - Owns the binding hub that connects widgets to the local controller / player state / pawn.
 *
 * Input mode and focus are not set here: every screen declares them (UBaseActivatableWidget) and CommonUI applies
 * the configuration of the topmost active screen.
 *
 * Bootstrap: ABaseHUD (or any project HUD) calls InitializeForPlayerController in BeginPlay.
 */
UCLASS()
class COREFRAMEWORK_API UUIFrameSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	/** Finds the subsystem of the local player that owns the widget. Null for widgets without an owning local player. */
	static UUIFrameSubsystem* GetFromWidget(const UUserWidget* Widget);
	static UUIFrameSubsystem* GetFromController(const APlayerController* Controller);

	//~ Begin USubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem

	// ---- Bootstrap -------------------------------------------------------------------------------

	/**
	 * Starts the UI for a local controller: loads the config, creates the root layout, starts the binding hub
	 * and pushes the startup screens in order. Safe to call repeatedly for the same controller.
	 * @param Owner  Object that owns the UI lifetime (the HUD). Only that owner can release it again.
	 */
	void InitializeForPlayerController(APlayerController* Controller, const TArray<FGameplayTag>& StartupScreens, const UObject* Owner);

	/** Tears the UI down if the controller / owner pair matches the active one. */
	void ReleasePlayerController(APlayerController* Controller, const UObject* Owner);

	// ---- Screens ---------------------------------------------------------------------------------

	/** Async push by tag. Fire and forget; listen to OnScreenPushed if the widget is needed. */
	UFUNCTION(BlueprintCallable, Category = "Base UI|Screens")
	void PushScreen(FGameplayTag ScreenTag);

	/**
	 * Async push by tag with a completion callback. If the root layout is not ready yet the request is queued.
	 * If the class is already loaded the push completes synchronously.
	 */
	void PushScreenAsync(FGameplayTag ScreenTag, FUIScreenPushedCallback OnComplete = nullptr);

	/** Closes the open instance of a screen. Returns false when it was not open. */
	UFUNCTION(BlueprintCallable, Category = "Base UI|Screens")
	bool PopScreen(FGameplayTag ScreenTag);

	/** Closes one specific screen instance. */
	void PopWidget(UBaseActivatableWidget* Widget);

	/** Closes the topmost screen of a layer. */
	UFUNCTION(BlueprintCallable, Category = "Base UI|Screens")
	void PopTopOfLayer(FGameplayTag LayerTag);

	UFUNCTION(BlueprintCallable, Category = "Base UI|Screens")
	void ClearLayer(FGameplayTag LayerTag);

	UFUNCTION(BlueprintPure, Category = "Base UI|Screens")
	bool IsScreenOpen(FGameplayTag ScreenTag) const;

	UFUNCTION(BlueprintPure, Category = "Base UI|Screens")
	UBaseActivatableWidget* GetScreenWidget(FGameplayTag ScreenTag) const;

	/** Loads a screen class in the background and keeps it resident, so the first PushScreen cannot hitch. */
	UFUNCTION(BlueprintCallable, Category = "Base UI|Screens")
	void PreloadScreen(FGameplayTag ScreenTag);

	UPROPERTY(BlueprintAssignable, Category = "Base UI|Screens")
	FOnUIScreenPushed OnScreenPushed;

	// ---- Extension registries (runtime) ---------------------------------------------------------

	/** Add or replace a screen definition. */
	UFUNCTION(BlueprintCallable, Category = "Base UI|Extensions")
	void RegisterScreen(const FUIScreenDefinition& Definition);

	UFUNCTION(BlueprintCallable, Category = "Base UI|Extensions")
	void UnregisterScreen(FGameplayTag ScreenTag);

	/** Add or replace a HUD element. Live HUDs spawn it immediately. */
	UFUNCTION(BlueprintCallable, Category = "Base UI|Extensions")
	void RegisterHUDElement(const FUIHUDElementDescriptor& Descriptor);

	UFUNCTION(BlueprintCallable, Category = "Base UI|Extensions")
	void UnregisterHUDElement(FName ElementId);

	UFUNCTION(BlueprintPure, Category = "Base UI|Extensions")
	bool IsHUDElementRegistered(FName ElementId) const;

	TArray<FUIHUDElementDescriptor> GetHUDElements() const;

	/** Add or replace a tab of the menu identified by MenuTag. Open menus rebuild their tabs. */
	UFUNCTION(BlueprintCallable, Category = "Base UI|Extensions")
	void RegisterMenuTab(FGameplayTag MenuTag, const FUITabDescriptor& Tab);

	UFUNCTION(BlueprintCallable, Category = "Base UI|Extensions")
	void UnregisterMenuTab(FGameplayTag MenuTag, FName TabId);

	TArray<FUITabDescriptor> GetMenuTabs(FGameplayTag MenuTag) const;

	FOnUIHUDElementRegistered OnHUDElementRegistered;
	FOnUIHUDElementUnregistered OnHUDElementUnregistered;
	FOnUIMenuTabsChanged OnMenuTabsChanged;

	// ---- Binding ---------------------------------------------------------------------------------

	/** Registers an object implementing IBaseUIBinderInterface. Done automatically by the base widgets. */
	void RegisterBinder(UObject* Binder);
	void UnregisterBinder(UObject* Binder);

	UFUNCTION(BlueprintPure, Category = "Base UI|Binding")
	APlayerController* GetBoundController() const;

	UFUNCTION(BlueprintPure, Category = "Base UI|Binding")
	APawn* GetBoundPawn() const;

	UFUNCTION(BlueprintPure, Category = "Base UI|Binding")
	APlayerState* GetBoundPlayerState() const;

	// ---- Input -----------------------------------------------------------------------------------

	/** True while the last used input device of this local player is a gamepad. */
	UFUNCTION(BlueprintPure, Category = "Base UI|Input")
	bool IsUsingGamepad() const;

private:
	struct FPendingPush
	{
		TArray<FUIScreenPushedCallback> Callbacks;
	};

	struct FDeferredPush
	{
		FGameplayTag ScreenTag;
		FUIScreenPushedCallback Callback;
	};

	void EnsureConfigLoaded();
	void BeginRootLoad(APlayerController* Controller);
	void OnRootClassLoaded(APlayerController* Controller);
	void FlushDeferredPushes();
	void TeardownRoot();

	UBaseActivatableWidget* FinishPush(const FUIScreenDefinition& Definition);

	UPROPERTY(Transient)
	TObjectPtr<UUIBindingHub> BindingHub;

	UPROPERTY(Transient)
	TObjectPtr<UBaseRootLayoutWidget> RootLayout;

	UPROPERTY(Transient)
	TObjectPtr<UUIConfigDataAsset> Config;

	TWeakObjectPtr<APlayerController> ActiveController;
	TWeakObjectPtr<const UObject> RootOwner;

	TMap<FGameplayTag, FUIScreenDefinition> ScreenRegistry;
	TMap<FName, FUIHUDElementDescriptor> HUDElementRegistry;
	TMap<FGameplayTag, TArray<FUITabDescriptor>> MenuTabRegistry;

	TMap<FGameplayTag, TWeakObjectPtr<UBaseActivatableWidget>> OpenScreens;
	TMap<FGameplayTag, FPendingPush> PendingPushes;
	TMap<FGameplayTag, TSharedPtr<FStreamableHandle>> ResidentHandles;
	TArray<FDeferredPush> DeferredPushes;
	TSharedPtr<FStreamableHandle> RootClassHandle;

	/** Incremented on teardown so late async completions of an old session are ignored. */
	uint32 SessionId = 0;
	bool bRootLoading = false;
};
```

### 4.4 `Binding/BaseUIBinderInterface.h` — observer contract

```cpp
// CoreFramework UI System
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "BaseUIBinderInterface.generated.h"

class APawn;
class APlayerController;
class APlayerState;

UINTERFACE(BlueprintType, MinimalAPI)
class UBaseUIBinderInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Observer contract between the local player's gameplay objects and any UI object.
 *
 * Implemented by UBaseUserWidget and UBaseActivatableWidget. Registered binders receive:
 *  - a "Bound" call for every object that is currently valid at registration time (late join is safe),
 *  - an "Unbound" then "Bound" pair whenever the pawn or player state changes (respawn, possess, unpossess),
 *  - "Unbound" calls when they unregister or when the local player context is torn down.
 *
 * Every Bound call is paired with exactly one Unbound call. Implementers should drop every
 * subscription they made in Bound inside the matching Unbound. The Unbound argument can be null
 * when the object was destroyed before the call; drop weak or cached subscriptions in that case.
 *
 * The interface carries no gameplay data. Game code defines its own components and delegates
 * and the widget subscribes to them inside OnUIPawnBound.
 */
class COREFRAMEWORK_API IBaseUIBinderInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, Category = "Base UI|Binding")
	void OnUIControllerBound(APlayerController* Controller);

	UFUNCTION(BlueprintNativeEvent, Category = "Base UI|Binding")
	void OnUIControllerUnbound(APlayerController* Controller);

	UFUNCTION(BlueprintNativeEvent, Category = "Base UI|Binding")
	void OnUIPlayerStateBound(APlayerState* PlayerState);

	UFUNCTION(BlueprintNativeEvent, Category = "Base UI|Binding")
	void OnUIPlayerStateUnbound(APlayerState* PlayerState);

	UFUNCTION(BlueprintNativeEvent, Category = "Base UI|Binding")
	void OnUIPawnBound(APawn* Pawn);

	UFUNCTION(BlueprintNativeEvent, Category = "Base UI|Binding")
	void OnUIPawnUnbound(APawn* Pawn);
};
```

### 4.5 `Foundation/BaseUserWidget.h` — plain widget base

```cpp
// CoreFramework UI System
#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Engine/DataTable.h"
#include "Input/UIActionBindingHandle.h"
#include "UISystem/Binding/BaseUIBinderInterface.h"
#include "UISystem/Core/UITypes.h"
#include "BaseUserWidget.generated.h"

class APawn;
class APlayerController;
class APlayerState;
class UUIFrameSubsystem;
class UWidgetAnimation;

/**
 * Base class for every non-activatable widget in the framework (HUD elements, tab content, list entries...).
 *
 * Provides:
 *  - Show / Hide with optional UMG animations (Anim_Show / Anim_Hide) and lifecycle events.
 *  - Automatic registration with the local player's binding hub (IBaseUIBinderInterface).
 *  - Input action binding helpers on top of CommonUI.
 *  - Convenience access to the UI subsystem and the bound controller / pawn / player state.
 */
UCLASS(Abstract, Blueprintable)
class COREFRAMEWORK_API UBaseUserWidget : public UCommonUserWidget, public IBaseUIBinderInterface
{
	GENERATED_BODY()

public:
	UBaseUserWidget(const FObjectInitializer& ObjectInitializer);

	// ---- Show / Hide with transition hook -------------------------------------------------------

	/** Makes the widget visible (ShownVisibility) and plays Anim_Show when bAnimate is true. */
	UFUNCTION(BlueprintCallable, Category = "Base UI|Lifecycle")
	void ShowWidget(bool bAnimate = true);

	/** Plays Anim_Hide (when bAnimate) and then collapses the widget (HiddenVisibility). */
	UFUNCTION(BlueprintCallable, Category = "Base UI|Lifecycle")
	void HideWidget(bool bAnimate = true);

	UFUNCTION(BlueprintPure, Category = "Base UI|Lifecycle")
	bool IsWidgetShown() const { return bIsShown; }

	// ---- Access ----------------------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Base UI")
	UUIFrameSubsystem* GetUIFrameSubsystem() const;

	UFUNCTION(BlueprintPure, Category = "Base UI|Binding")
	APlayerController* GetBoundController() const;

	UFUNCTION(BlueprintPure, Category = "Base UI|Binding")
	APawn* GetBoundPawn() const;

	UFUNCTION(BlueprintPure, Category = "Base UI|Binding")
	APlayerState* GetBoundPlayerState() const;

	/** Typed convenience for C++: GetBoundPawnAs<AMyCharacter>(). */
	template <typename PawnType>
	PawnType* GetBoundPawnAs() const
	{
		return Cast<PawnType>(GetBoundPawn());
	}

	/** True while the last used input device on this local player is a gamepad. */
	UFUNCTION(BlueprintPure, Category = "Base UI|Input")
	bool IsUsingGamepad() const;

	// ---- Input binding helpers -------------------------------------------------------------------

	/**
	 * Binds a CommonUI input action (row of a CommonInputActionDataBase table, resolves to
	 * keyboard / gamepad / Enhanced Input keys per platform). Automatically unbound when the widget is destroyed.
	 */
	UFUNCTION(BlueprintCallable, Category = "Base UI|Input")
	void BindUIAction(const FDataTableRowHandle& ActionRow, bool bShowInActionBar, FBaseUIActionDelegate OnExecuted);

	/** C++ overload with a native delegate. */
	FUIActionBindingHandle BindUIActionNative(const FDataTableRowHandle& ActionRow, bool bShowInActionBar, const FSimpleDelegate& OnExecuted);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** Fired after ShowWidget made the widget visible. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Base UI|Lifecycle", meta = (DisplayName = "On Widget Shown"))
	void BP_OnWidgetShown();

	/** Fired after HideWidget finished (animation included) and the widget is collapsed. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Base UI|Lifecycle", meta = (DisplayName = "On Widget Hidden"))
	void BP_OnWidgetHidden();

	/** Register with the binding hub on construct. Disable for widgets that never react to the local player's pawn. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base UI|Binding")
	bool bAutoRegisterAsBinder = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base UI|Lifecycle")
	ESlateVisibility ShownVisibility = ESlateVisibility::SelfHitTestInvisible;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base UI|Lifecycle")
	ESlateVisibility HiddenVisibility = ESlateVisibility::Collapsed;

	/** Optional UMG animation named exactly Anim_Show. Played by ShowWidget. */
	UPROPERTY(Transient, BlueprintReadOnly, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_Show;

	/** Optional UMG animation named exactly Anim_Hide. Played by HideWidget before collapsing. */
	UPROPERTY(Transient, BlueprintReadOnly, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_Hide;

private:
	UFUNCTION()
	void HandleHideAnimationFinished();

	void FinalizeHide();

	bool bIsShown = true;
};
```

### 4.6 `Foundation/BaseActivatableWidget.h` — screen base (CommonUI)

```cpp
// CoreFramework UI System
#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Engine/DataTable.h"
#include "Engine/GameViewportClient.h"
#include "GameplayTagContainer.h"
#include "Input/UIActionBindingHandle.h"
#include "UISystem/Binding/BaseUIBinderInterface.h"
#include "UISystem/Core/UITypes.h"
#include "BaseActivatableWidget.generated.h"

class APawn;
class APlayerController;
class APlayerState;
class UUIFrameSubsystem;
class UWidget;
class UWidgetAnimation;

/**
 * Base class for every screen that lives on a layer stack (menus, popups, HUD, notifications).
 * Built on CommonUI's UCommonActivatableWidget so activation, back handling, input routing and
 * gamepad / keyboard focus are handled by the engine.
 *
 * Adds on top of CommonUI:
 *  - Declarative input mode (GameOnly / UIOnly / GameAndUI) + mouse capture, applied automatically.
 *  - Optional look / move input suppression and game pause while the screen is active.
 *  - Default focus target fallback (DefaultFocusWidget) for controller / keyboard navigation.
 *  - Activate / deactivate animation hook (Anim_Activate / Anim_Deactivate).
 *  - Binding hub registration (IBaseUIBinderInterface), screen / layer identity, CloseScreen().
 *
 * Back action: tick "Is Back Handler" in the Blueprint class defaults of screens that Escape / gamepad B should close.
 * Modal screens: tick "Is Modal" in the class defaults.
 */
UCLASS(Abstract, Blueprintable)
class COREFRAMEWORK_API UBaseActivatableWidget : public UCommonActivatableWidget, public IBaseUIBinderInterface
{
	GENERATED_BODY()

public:
	UBaseActivatableWidget(const FObjectInitializer& ObjectInitializer);

	//~ Begin UCommonActivatableWidget
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
	//~ End UCommonActivatableWidget

	/** Called by the UI subsystem before activation. */
	void SetScreenContext(FGameplayTag InScreenTag, FGameplayTag InLayerTag);

	UFUNCTION(BlueprintPure, Category = "Base UI|Screen")
	FGameplayTag GetScreenTag() const { return ScreenTag; }

	UFUNCTION(BlueprintPure, Category = "Base UI|Screen")
	FGameplayTag GetLayerTag() const { return LayerTag; }

	/** True from the moment the subsystem pushed this screen until it deactivates. */
	UFUNCTION(BlueprintPure, Category = "Base UI|Screen")
	bool IsScreenOpen() const { return bScreenOpen; }

	/** Removes this screen from its layer through the UI subsystem. Safe to call from Blueprint buttons. */
	UFUNCTION(BlueprintCallable, Category = "Base UI|Screen")
	void CloseScreen();

	UFUNCTION(BlueprintPure, Category = "Base UI")
	UUIFrameSubsystem* GetUIFrameSubsystem() const;

	UFUNCTION(BlueprintPure, Category = "Base UI|Binding")
	APlayerController* GetBoundController() const;

	UFUNCTION(BlueprintPure, Category = "Base UI|Binding")
	APawn* GetBoundPawn() const;

	UFUNCTION(BlueprintPure, Category = "Base UI|Binding")
	APlayerState* GetBoundPlayerState() const;

	template <typename PawnType>
	PawnType* GetBoundPawnAs() const
	{
		return Cast<PawnType>(GetBoundPawn());
	}

	UFUNCTION(BlueprintPure, Category = "Base UI|Input")
	bool IsUsingGamepad() const;

	/** Moves keyboard / gamepad focus to the default focus widget now. */
	UFUNCTION(BlueprintCallable, Category = "Base UI|Input")
	void RequestDefaultFocus();

	/** Binds a CommonUI input action row. Automatically unbound when the widget is destroyed. */
	UFUNCTION(BlueprintCallable, Category = "Base UI|Input")
	void BindUIAction(const FDataTableRowHandle& ActionRow, bool bShowInActionBar, FBaseUIActionDelegate OnExecuted);

	FUIActionBindingHandle BindUIActionNative(const FDataTableRowHandle& ActionRow, bool bShowInActionBar, const FSimpleDelegate& OnExecuted);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;

	/** Widget that receives focus when the screen opens. Falls back to the Blueprint "Get Desired Focus Target" if implemented. */
	virtual UWidget* ResolveDefaultFocusWidget() const;

	// ---- Input -----------------------------------------------------------------------------------

	/** Input routing requested while this screen is the topmost active screen. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base UI|Input")
	EUIInputMode InputMode = EUIInputMode::UIOnly;

	/** Mouse capture used when the game receives input (GameOnly / GameAndUI). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base UI|Input")
	EMouseCaptureMode GameMouseCaptureMode = EMouseCaptureMode::CapturePermanently;

	/** Stops camera look input while active. Use with GameAndUI so the mouse does not turn the camera. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base UI|Input")
	bool bIgnoreLookInputWhileActive = false;

	/** Stops movement input while active. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base UI|Input")
	bool bIgnoreMoveInputWhileActive = false;

	/** Pauses the game while active (single player / listen server host). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base UI|Input")
	bool bPauseGameWhileActive = false;

	/** Optional widget named exactly DefaultFocusWidget. Receives focus on activation. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> DefaultFocusWidget;

	// ---- Binding ---------------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base UI|Binding")
	bool bAutoRegisterAsBinder = true;

	// ---- Transitions -----------------------------------------------------------------------------

	/** Optional UMG animation named exactly Anim_Activate. Played on activation. */
	UPROPERTY(Transient, BlueprintReadOnly, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_Activate;

	/** Optional UMG animation named exactly Anim_Deactivate. Played on deactivation. */
	UPROPERTY(Transient, BlueprintReadOnly, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_Deactivate;

	void ScheduleDefaultFocus();

private:
	void ApplyDefaultFocus();
	void ApplyActiveSideEffects();
	void ReleaseActiveSideEffects();

	FGameplayTag ScreenTag;
	FGameplayTag LayerTag;
	bool bScreenOpen = false;

	TWeakObjectPtr<APlayerController> SuppressedController;
	bool bLookSuppressed = false;
	bool bMoveSuppressed = false;
	bool bPausedByThisWidget = false;
};
```

### 4.7 `Foundation/BaseButton.h` — button parent

```cpp
// CoreFramework UI System - Foundation
#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "BaseButton.generated.h"

class UCommonTextBlock;

/**
 * Parent class for every button of a game project (menu buttons, tab buttons, popup buttons).
 *
 * It solves one beginner trap of CommonUI: giving each button instance its own text.
 *  - Add a Common Text widget named exactly LabelText inside the button Blueprint.
 *  - Type the text in the Button Label property of each placed button (visible in the Designer preview).
 *  - Change it at runtime with SetButtonLabel.
 * Appearance (colors, hover, pressed, selected) comes from the CommonButtonStyle asset the Blueprint
 * references. The framework ships no style and no art.
 */
UCLASS(Abstract, Blueprintable)
class COREFRAMEWORK_API UBaseButton : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Base UI|Button")
	void SetButtonLabel(const FText& InLabel);

	UFUNCTION(BlueprintPure, Category = "Base UI|Button")
	FText GetButtonLabel() const { return ButtonLabel; }

protected:
	virtual void NativePreConstruct() override;

	/** Fired whenever the label text is applied (designer preview, construct, SetButtonLabel). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Base UI|Button", meta = (DisplayName = "On Button Label Changed"))
	void BP_OnButtonLabelChanged(const FText& InLabel);

	/** Text shown by the button. Editable on every placed instance in the Details panel. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base UI|Button")
	FText ButtonLabel;

	/** Optional Common Text widget named exactly LabelText. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> LabelText;

private:
	void ApplyLabel();
};
```

### 4.8 `HUD/BaseHUDWidget.h` — HUD container

```cpp
// CoreFramework UI System
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UISystem/Foundation/BaseActivatableWidget.h"
#include "UISystem/Core/UITypes.h"
#include "BaseHUDWidget.generated.h"

class UBaseUserWidget;
class UPanelWidget;
struct FStreamableHandle;

/**
 * Master container for gameplay overlays. Pushed on the Game layer, input mode GameOnly.
 *
 * Binding: because it is an activatable binder it is told about the local controller, player state and pawn
 * (override OnUIPawnBound etc. in C++ or Blueprint). Child HUD elements derive from UBaseUserWidget and bind
 * themselves, so the HUD never has to know what a health bar is.
 *
 * Extension: designers or game code inject widgets into named slots with FUIHUDElementDescriptor
 * (UUIConfigDataAsset::HUDElements or UUIFrameSubsystem::RegisterHUDElement). Elements are async loaded and
 * created on demand, also when registered while the HUD is already on screen.
 */
UCLASS(Abstract, Blueprintable)
class COREFRAMEWORK_API UBaseHUDWidget : public UBaseActivatableWidget
{
	GENERATED_BODY()

public:
	UBaseHUDWidget(const FObjectInitializer& ObjectInitializer);

	/** Register a panel as the container for a slot tag. Call from Event Construct for every custom slot. */
	UFUNCTION(BlueprintCallable, Category = "Base UI|HUD")
	void RegisterElementSlot(FGameplayTag SlotTag, UPanelWidget* Container);

	UFUNCTION(BlueprintCallable, Category = "Base UI|HUD")
	void UnregisterElementSlot(FGameplayTag SlotTag);

	UFUNCTION(BlueprintPure, Category = "Base UI|HUD")
	UBaseUserWidget* FindElement(FName ElementId) const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Base UI|HUD", meta = (DisplayName = "On HUD Element Added"))
	void BP_OnElementAdded(FName ElementId, UBaseUserWidget* Element);

	UFUNCTION(BlueprintImplementableEvent, Category = "Base UI|HUD", meta = (DisplayName = "On HUD Element Removed"))
	void BP_OnElementRemoved(FName ElementId);

	/** Optional panel named exactly DefaultElementSlot. Registered under the HUDSlot.Default tag. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> DefaultElementSlot;

private:
	void HandleHUDElementRegistered(const FUIHUDElementDescriptor& Descriptor);
	void HandleHUDElementUnregistered(FName ElementId);

	void SpawnAllRegisteredElements();
	void SpawnElement(const FUIHUDElementDescriptor& Descriptor);
	void CreateElement(const FUIHUDElementDescriptor& Descriptor);
	void RemoveElement(FName ElementId);

	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<UPanelWidget>> ElementSlots;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UBaseUserWidget>> SpawnedElements;

	TMap<FName, TSharedPtr<FStreamableHandle>> PendingElementLoads;

	FDelegateHandle ElementRegisteredHandle;
	FDelegateHandle ElementUnregisteredHandle;
};
```

### 4.9 `HUD/BaseHUD.h` — bootstrap actor

```cpp
// CoreFramework UI System
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "GameplayTagContainer.h"
#include "BaseHUD.generated.h"

/**
 * Bootstrap actor for the UI system. Set it as the HUD class of the GameMode.
 *
 * AHUD exists only for local player controllers (listen-server host and clients, never on a dedicated server),
 * which makes BeginPlay the correct, multiplayer-safe moment to start the UI of that local player.
 * It draws nothing itself; all UI is UMG through UUIFrameSubsystem.
 *
 * Projects that already own an AHUD subclass can skip this class and call
 * UUIFrameSubsystem::InitializeForPlayerController / ReleasePlayerController from their own HUD.
 */
UCLASS()
class COREFRAMEWORK_API ABaseHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	/**
	 * Screens pushed when the UI starts for this map / game mode, in order. Put the HUD first, overlays after.
	 * Main menu map: [your main menu screen tag]. Gameplay map: [your HUD screen tag].
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Base UI")
	TArray<FGameplayTag> StartupScreens;
};
```

### 4.10 `Menu/BaseMenuWidget.h` — menus with a data driven tab bar

```cpp
// CoreFramework UI System
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UISystem/Foundation/BaseActivatableWidget.h"
#include "UISystem/Core/UITypes.h"
#include "BaseMenuWidget.generated.h"

class UBaseButton;
class UBaseTabWidget;
class UCommonButtonBase;
class UCommonTabListWidgetBase;
class UUITabSetDataAsset;
class UWidgetSwitcher;
struct FStreamableHandle;

/**
 * Base class for Main Menu, Pause Menu and Settings screens.
 *
 * Works with or without tabs. When the Blueprint contains a Common Tab List named TabList and a
 * Widget Switcher named TabSwitcher, the menu builds its tabs from data:
 *   DefaultTabSet (assigned on this widget)  <  UUIFrameSubsystem::RegisterMenuTab (runtime, plugins / game features)
 * Later sources override earlier ones by TabId, so a project can replace or disable (bEnabled = false)
 * a tab provided by another source. Tab content classes are async loaded.
 */
UCLASS(Abstract, Blueprintable)
class COREFRAMEWORK_API UBaseMenuWidget : public UBaseActivatableWidget
{
	GENERATED_BODY()

public:
	UBaseMenuWidget(const FObjectInitializer& ObjectInitializer);

	/** Collects descriptors from all sources, async loads the tab widgets and rebuilds the tab bar. */
	UFUNCTION(BlueprintCallable, Category = "Base UI|Menu")
	void RebuildTabs();

	UFUNCTION(BlueprintCallable, Category = "Base UI|Menu")
	bool SelectTab(FName TabId);

	UFUNCTION(BlueprintPure, Category = "Base UI|Menu")
	FName GetActiveTabId() const { return ActiveTabId; }

	UFUNCTION(BlueprintPure, Category = "Base UI|Menu")
	UBaseTabWidget* GetTabWidget(FName TabId) const;

	/** Applies every tab that reports pending changes. Wire to an Apply button. */
	UFUNCTION(BlueprintCallable, Category = "Base UI|Menu")
	void ApplyAllTabs();

	/** Reverts every tab that reports pending changes. Wire to a Revert / Cancel button. */
	UFUNCTION(BlueprintCallable, Category = "Base UI|Menu")
	void RevertAllTabs();

	UFUNCTION(BlueprintPure, Category = "Base UI|Menu")
	bool HasPendingChangesInAnyTab() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;
	virtual UWidget* ResolveDefaultFocusWidget() const override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Base UI|Menu", meta = (DisplayName = "On Tabs Built"))
	void BP_OnTabsBuilt();

	UFUNCTION(BlueprintImplementableEvent, Category = "Base UI|Menu", meta = (DisplayName = "On Active Tab Changed"))
	void BP_OnActiveTabChanged(FName TabId);

	/** Id used to receive tabs registered at runtime through the subsystem. Use the same tag you registered this screen with. Leave empty when no runtime tabs are expected. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base UI|Menu")
	FGameplayTag MenuTag;

	/** Tabs of this menu. Create a UITabSetDataAsset in the game project and assign it here. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base UI|Menu")
	TObjectPtr<UUITabSetDataAsset> DefaultTabSet;

	/** Button Blueprint (child of UBaseButton) instantiated for each tab. Required when tabs are used. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base UI|Menu")
	TSubclassOf<UBaseButton> TabButtonClass;

	/** Optional Common Tab List named exactly TabList. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTabListWidgetBase> TabList;

	/** Optional Widget Switcher (or Common Animated Switcher) named exactly TabSwitcher. Hosts the tab content. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidgetSwitcher> TabSwitcher;

private:
	UFUNCTION()
	void HandleTabSelected(FName TabId, UCommonButtonBase* TabButton);

	void HandleMenuTabsChanged(FGameplayTag ChangedMenuTag);

	void CollectDescriptors(TArray<FUITabDescriptor>& OutDescriptors) const;
	void BuildTabs(const TArray<FUITabDescriptor>& Descriptors);
	void ClearTabs();
	void ActivateTabContent(FName TabId);

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UBaseTabWidget>> TabContents;

	TSharedPtr<FStreamableHandle> TabLoadHandle;
	FDelegateHandle TabsChangedHandle;
	FName ActiveTabId = NAME_None;
	uint32 TabBuildGeneration = 0;
	bool bTabsBuilt = false;
	bool bTabsLoading = false;
};
```

### 4.11 `Menu/Tabs/BaseTabWidget.h` — tab page

```cpp
// CoreFramework UI System
#pragma once

#include "CoreMinimal.h"
#include "UISystem/Foundation/BaseUserWidget.h"
#include "UISystem/Core/UITypes.h"
#include "BaseTabWidget.generated.h"

class UWidget;

/**
 * Content of one tab (Settings category, Main Menu page...). Created and hosted by UBaseMenuWidget.
 *
 * Game-specific tabs are Blueprint or C++ children of this class; register them through a
 * UUITabSetDataAsset or UUIFrameSubsystem::RegisterMenuTab. Settings tabs override
 * HasPendingChanges / ApplyChanges / RevertChanges so the menu's Apply and Revert buttons work uniformly.
 */
UCLASS(Abstract, Blueprintable)
class COREFRAMEWORK_API UBaseTabWidget : public UBaseUserWidget
{
	GENERATED_BODY()

public:
	UBaseTabWidget(const FObjectInitializer& ObjectInitializer);

	/** Called once by the hosting menu right after creation. */
	void InitializeTab(const FUITabDescriptor& InDescriptor);

	/** Called by the hosting menu when this tab becomes / stops being the visible tab. */
	void NotifyTabSelected();
	void NotifyTabDeselected();

	UFUNCTION(BlueprintPure, Category = "Base UI|Tab")
	FName GetTabId() const { return Descriptor.TabId; }

	UFUNCTION(BlueprintPure, Category = "Base UI|Tab")
	FText GetTabDisplayName() const { return Descriptor.DisplayName; }

	UFUNCTION(BlueprintPure, Category = "Base UI|Tab")
	bool IsTabSelected() const { return bTabSelected; }

	bool ShouldFocusContentOnSelect() const { return bFocusContentOnSelect; }

	/** Moves keyboard / gamepad focus into the tab content (DefaultFocusWidget). */
	UFUNCTION(BlueprintCallable, Category = "Base UI|Tab")
	void FocusTabContent();

	/** True when the tab holds edits that were not applied yet. */
	UFUNCTION(BlueprintNativeEvent, Category = "Base UI|Tab")
	bool HasPendingChanges() const;

	/** Commit pending edits. */
	UFUNCTION(BlueprintNativeEvent, Category = "Base UI|Tab")
	void ApplyChanges();

	/** Discard pending edits and refresh the displayed values. */
	UFUNCTION(BlueprintNativeEvent, Category = "Base UI|Tab")
	void RevertChanges();

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Base UI|Tab", meta = (DisplayName = "On Tab Initialized"))
	void BP_OnTabInitialized(FName TabId, const FText& DisplayName);

	UFUNCTION(BlueprintImplementableEvent, Category = "Base UI|Tab", meta = (DisplayName = "On Tab Selected"))
	void BP_OnTabSelected();

	UFUNCTION(BlueprintImplementableEvent, Category = "Base UI|Tab", meta = (DisplayName = "On Tab Deselected"))
	void BP_OnTabDeselected();

	/** Optional widget named exactly DefaultFocusWidget. Focus target for FocusTabContent. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> DefaultFocusWidget;

	/** Move focus into the content when the tab is selected (mouse users). Leave off so gamepad bumpers keep focus on the tab bar. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base UI|Tab")
	bool bFocusContentOnSelect = false;

private:
	FUITabDescriptor Descriptor;
	bool bTabSelected = false;
};
```

The remaining headers (`Binding/UIBindingHub.h`, `Layout/BaseRootLayoutWidget.h`, `Core/UIFrameSettings.h`, `Core/UIGameplayTags.h`) are short and documented in place; see the manifest in section 9.

---

## 5. Input and navigation handling

### 5.1 Principle

A screen **declares** what it needs; CommonUI **applies** it. There is no central "SetInputMode" call to forget.

```
UBaseActivatableWidget (a game screen)
  InputMode            Default | GameOnly | UIOnly | GameAndUI
  GameMouseCaptureMode EMouseCaptureMode
        │  GetDesiredInputConfig()
        ▼
  CommonUI action router  ──►  topmost active screen decides the input configuration
```

| `EUIInputMode` | CommonUI mode | Typical screen | Behavior |
|---|---|---|---|
| `GameOnly` | `Game` | HUD (`UBaseHUDWidget` default) | Game gets input, mouse captured per `GameMouseCaptureMode`. |
| `UIOnly` | `Menu` | Menus (`UBaseMenuWidget` default), popups | UI gets input, cursor visible, game input blocked. |
| `GameAndUI` | `All` | Live overlays (quick inventory with mouse) | UI first, unconsumed input still reaches the game. Combine with *Ignore Look Input While Active*. |
| `Default` | none | Notifications, toasts | No request; the underlying screen's configuration stays active. |

Closing a menu reverts automatically to the configuration of the next topmost screen (the HUD). **Order matters:** CommonUI uses the most recently activated screen, so list the HUD first in `StartupScreens`.

### 5.2 Focus (keyboard / gamepad navigation)

Three levels, first match wins:

1. **Blueprint `Get Desired Focus Target`** (CommonUI's own hook) on the screen.
2. **`DefaultFocusWidget`**: a widget named exactly `DefaultFocusWidget`. Focused one tick after activation **only if nothing inside the screen has focus yet**.
3. **Menus with tabs**: when no explicit target exists, focus lands on the button of the active tab.

Tab pages have their own optional `DefaultFocusWidget` and `bFocusContentOnSelect` (off by default so gamepad bumpers keep switching tabs).

### 5.3 Back, modal, pause, suppression

| Need | How |
|---|---|
| Escape / gamepad B closes the screen | Tick **Is Back Handler** in the screen Blueprint's class defaults. Needs the default back action in the CommonUI input data (guide, Steps 2.5 and 3.2). |
| Popup blocks everything below | Tick **Is Modal**; push on the **Modal** layer. |
| Pause the game while open | `bPauseGameWhileActive` (single player / listen-server host). |
| Stop camera / movement while a live overlay is open | `bIgnoreLookInputWhileActive`, `bIgnoreMoveInputWhileActive`. Applied once per activation and always released. |
| Close from a button | `CloseScreen()` on the widget. |

### 5.4 Enhanced Input

* Gameplay input stays in your gameplay `UInputMappingContext`s (`ABaseCharacter` / `UInputConfigData`).
* UI actions (back, confirm, tab left/right) are **CommonUI input actions** (`FDataTableRowHandle` rows). With *Enable Enhanced Input Support* each row can reference an `UInputAction`.
* `BindUIAction` / `BindUIActionNative` bind a row to a delegate; removed automatically with the widget.
* `UIOnly` screens block gameplay input at the Slate level, so no mapping-context juggling is needed.

### 5.5 Mouse, keyboard, gamepad switching

Detection and cursor/focus switching: CommonUI (`CommonGameViewportClient` + input settings). Query: `IsUsingGamepad()` on the subsystem and on both widget bases.

### 5.6 Split screen

Every local player has its own `UUIFrameSubsystem`, root layout, binding hub and CommonUI router. Widgets resolve the subsystem through their **owning local player**.

---

## 6. Binding system (game and character without hardcoding)

### 6.1 Contract (`IBaseUIBinderInterface`)

Six events: Controller / PlayerState / Pawn × Bound / Unbound. Implemented natively by `UBaseUserWidget` and `UBaseActivatableWidget` (empty defaults), so a child widget overrides only what it needs, in C++ (`OnUIPawnBound_Implementation`) or Blueprint (override event).

### 6.2 Scenario table

| Scenario | What the hub does | What your widget sees |
|---|---|---|
| HUD created before the pawn exists (client join, map load) | Safety-net timer / `OnPossessedPawnChanged` detects the pawn when it arrives | `OnUIPawnBound(Pawn)` once |
| Widget created after the pawn exists | Replay at registration | Controller / PlayerState / Pawn Bound right after `Event Construct` |
| Unpossess (pawn becomes null) | Refresh | `OnUIPawnUnbound(oldPawn)` |
| Respawn (new pawn) | Refresh | `OnUIPawnUnbound(old)` then `OnUIPawnBound(new)` |
| Old pawn destroyed before the unbound call | Refresh | `OnUIPawnUnbound(nullptr)` — use what you cached |
| PlayerState arrives late / is replaced | Refresh | `OnUIPlayerStateUnbound/Bound` |
| Controller destroyed (travel, disconnect) | Refresh detects the stale controller, `Stop()` | `…Unbound` for pawn, player state, controller |
| Widget destroyed while bound | `UnregisterBinder` | Matching `…Unbound` calls so it can drop subscriptions |
| Dedicated server | No `AHUD`, no local player, no subsystem | Nothing is created |

### 6.3 Rules for implementers

1. Subscribe in `Bound`, unsubscribe in the matching `Unbound`.
2. In `Unbound`, never assume the argument is valid: keep a `TWeakObjectPtr` to what you subscribed to.
3. After subscribing, push the current value into the widget once (delegates only fire on change).
4. Never cast to a game class inside CoreFramework. Cast inside your own widget (`GetBoundPawnAs<AMyCharacter>()`).

---

## 7. Extensibility design

Nothing here requires editing a CoreFramework file. The framework ships no content, so "extending" means "creating the game's content".

### 7.1 Create screens (menus, popups, overlays)

1. Create a gameplay tag (`UI.Screen.Inventory`) in the game project.
2. Create a Widget Blueprint in the game project with the right parent (`UBaseMenuWidget` for menus, `UBaseActivatableWidget` for everything else on a layer, `UBaseHUDWidget` for the HUD).
3. Add a `FUIScreenDefinition` row (tag, class, layer) to `DA_UIConfig.Screens` — or call `RegisterScreen` at runtime.
4. `PushScreen(UI.Screen.Inventory)` works from anywhere.

### 7.2 Tabs (Settings, Main Menu)

Tab sources are merged in this order; a later source **replaces** a tab with the same `TabId`:

```
UBaseMenuWidget::DefaultTabSet                (UITabSetDataAsset assigned on the game's menu Blueprint)
  <  UUIFrameSubsystem::RegisterMenuTab       (runtime: game code, plugins, game features — keyed by MenuTag)
```

| Goal | Do |
|---|---|
| Add a "Gameplay" tab | New `WBP_Tab_Gameplay` (parent `UBaseTabWidget`) + a row in the menu's `DA_Settings_Tabs`. |
| Reorder | `SortOrder` (lower first). |
| Remove a tab at runtime | Register a descriptor with the same `TabId` and `Enabled = false`. |
| Add a tab from a plugin / game feature | `Subsystem->RegisterMenuTab(MenuTag, Descriptor);` — an open menu rebuilds immediately (needs `Menu Tag` set on the menu). |

### 7.3 HUD elements

1. Create the widget (parent `UBaseUserWidget` or a game C++ child; it binds itself — Step 5 of the guide).
2. Add `FUIHUDElementDescriptor { ElementId, SlotTag, WidgetClass }` to `DA_UIConfig.HUDElements`, **or** call `RegisterHUDElement` at runtime. A live HUD spawns it immediately; `UnregisterHUDElement` removes it.
3. Slots: the game's HUD Blueprint names one panel `DefaultElementSlot` (tag `CoreFramework.UI.HUDSlot.Default`) and registers any number of custom slots in `Event Construct` with `RegisterElementSlot(Tag, Panel)`. An element whose slot does not exist yet is placed as soon as the slot is registered.

### 7.4 Layers

Duplicate `WBP_BaseRootLayout` into the game project, add a stack, call `RegisterLayer(Tag, Stack)` in its `Event Construct`, point `DA_UIConfig.RootLayoutClass` at the duplicate, and reference the layer tag in screen definitions.

### 7.5 Subclass in C++

All base classes are `Blueprintable` and expose virtual hooks (`ResolveDefaultFocusWidget`, `NativeOnActivated`, the six binder `_Implementation` functions). Project C++ derives from them in the **game module**.

### 7.6 What stays out of CoreFramework

Any visual design, any screen, HP / mana / ammo widgets, inventory, quest tracker, any `#include` of game code.

---

## 8. Performance and memory notes

* **No hitch on open:** class streaming is asynchronous; classes already in memory push synchronously; `Preload And Keep Resident` removes even the first-open cost for hot screens.
* **Binding cost:** the safety-net timer compares three pointers per interval (default 0.2 s per local player). `BindingPollIntervalSeconds = 0` relies on events only.
* **Duplicate loads:** concurrent pushes of the same screen share one in-flight request.
* **Teardown:** a session counter invalidates queued requests, so late completions of a previous map cannot push into a new session.
* **Pooling:** CommonUI containers pool widget instances; registration is tied to `NativeConstruct/NativeDestruct`, which pooling pairs correctly.

---

## 9. Manifests

### 9.1 C++ (framework)

| Folder | Files | Contains |
|---|---|---|
| `Core/` | `UITypes.h`, `UIGameplayTags.h/.cpp`, `UIFrameSettings.h` | enums, descriptors, log category, native tags, Project Settings page |
| `Data/` | `UIConfigDataAsset.h` | `UUIConfigDataAsset`, `UUITabSetDataAsset` |
| `Manager/` | `UIFrameSubsystem.h/.cpp` | manager |
| `Binding/` | `BaseUIBinderInterface.h`, `UIBindingHub.h/.cpp` | observer contract and hub |
| `Layout/` | `BaseRootLayoutWidget.h/.cpp` | layer stacks container |
| `Foundation/` | `BaseUserWidget`, `BaseActivatableWidget`, `BaseButton` (.h/.cpp each) | parents of all game widgets |
| `HUD/` | `BaseHUD`, `BaseHUDWidget` (.h/.cpp each) | bootstrap actor, HUD container |
| `Menu/` | `BaseMenuWidget.h/.cpp` | menu with tab bar |
| `Menu/Tabs/` | `BaseTabWidget.h/.cpp` | tab page |

15 headers, 11 sources.

### 9.2 Blueprint content

| Location | Assets |
|---|---|
| Plugin `Content/UI/Layout/` | `WBP_BaseRootLayout` |
| Plugin `Content/UI/Input/` | `DT_UI_Actions`, `BP_CommonInputData` |
| Game `Content/UI/...` | everything else (Section 3.3) |

---

## 10. Audit results (v1 → v2)

### 10.1 Verdict per C++ class

| Class | Verdict | Reason |
|---|---|---|
| `UUIFrameSubsystem` | **Keep** | Core manager. |
| `UUIBindingHub`, `IBaseUIBinderInterface` | **Keep** | Player binding is a stated infrastructure responsibility. |
| `UBaseRootLayoutWidget` | **Keep** | Layer stack management. Its Blueprint is the one shipped asset. |
| `UBaseActivatableWidget` | **Keep** | Parent of every screen: input mode, focus, back, side effects. |
| `UBaseUserWidget` | **Keep** | Parent of HUD elements and anything that is not a screen. |
| `UBaseHUDWidget` | **Keep** | Slots + HUD behavior. Contains no layout; slots are optional named bindings. |
| `UBaseMenuWidget` | **Keep** | Data-driven tab building cannot be done in Blueprint cleanly. Tabs are optional, menus without tabs work. |
| `UBaseTabWidget` | **Keep** | Tab page parent (selected/deselected events, Apply/Revert contract). |
| `UBaseButton` | **New, replaces `UBaseTabButton`** | One parent for all buttons incl. tabs; fixes the "how do I give this button its own text" problem. |
| `ABaseHUD`, `UUIFrameSettings`, data asset classes | **Keep** | Startup actor, settings, data-driven config. |

**Answer to "keep all base classes?"** Yes. Every class has a distinct responsibility and none contains a layout. The 15 headers / 11 sources count is unchanged: `BaseButton` replaced `BaseTabButton`.

### 10.2 Things removed or changed

| # | Change | Why |
|---|---|---|
| 1 | Removed native tags `Screen.HUD`, `Screen.MainMenu`, `Screen.PauseMenu`, `Screen.Settings` (`FUITags::Screen*`) | They pre-declared specific menus. Screen ids are game-owned now. Layers and `HUDSlot.Default` remain (pure infrastructure). |
| 2 | Removed every `Categories = "CoreFramework.UI...."` tag filter | **Bug in v1:** the filter limited the tag picker to the framework's subtree, hiding the game's own tags (`UI.Screen.*`). |
| 3 | Removed `UUIConfigDataAsset::MenuTabSets` and `FUIMenuTabSetBinding` | Their purpose was extending framework-shipped menus; there are none anymore. A menu now simply references its `DefaultTabSet`; plugins still use `RegisterMenuTab`. One tab source fewer, one synchronous load fewer. |
| 4 | `UBaseTabButton` → `UBaseButton` (`ButtonLabel`, `LabelText`, `SetButtonLabel`) | See table above. |
| 5 | One shared `FBaseUIActionDelegate` (in `UITypes.h`) instead of one per widget base | Duplicate declaration. |
| 6 | Folder reorganisation | Responsibility folders (3.1). |
| 7 | Blueprint content: the plugin ships `WBP_BaseRootLayout` plus the two CommonUI input assets (`DT_UI_Actions`, `BP_CommonInputData`) and nothing visual | Menus / HUD / settings Blueprints live in the game (guide, Steps 3–5). The Back / Confirm / Tab actions are identical in every game, so they are created once in the framework repo instead of in each project. |
| 8 | Setup guide v3: the `Bootstrap` folder is called `Startup` and explained; the reusable buttons and styles are game-project assets (`Content/UI/Components/`) | Documentation only. |

### 10.3 Considered and kept

| Item | Decision |
|---|---|
| `DefaultElementSlot` + `HUDSlot.Default` | Keep: an optional named binding, not a placed box. Lets a beginner add HUD elements without creating a tag first. |
| `bPauseGameWhileActive`, look/move suppression | Keep: optional flags, off by default, cheap, input-flow behavior. |
| `HasPendingChanges / ApplyChanges / RevertChanges` | Keep: optional empty defaults; the only way several tabs can share one Apply / Revert button without per-game glue. |
| `ShowWidget / HideWidget` with `Anim_Show / Anim_Hide` | Keep: the "transition animations hook" requirement. |
| Duplicated one-line helpers in the two widget bases (`GetBound*`, `BindUIAction`) | Keep: UObject single inheritance makes sharing awkward; a shared helper would add a file for ~10 lines. |
| `PopTopOfLayer`, `ClearLayer`, `PreloadScreen`, `UnregisterScreen` | Keep: public API for game flow, a few lines each. |
| Empty wrapper Blueprints (`WBP_BaseHUD`, `WBP_BaseMenu`…) | **Not created**: the C++ class is the parent. |

---

## 11. Known limits

* A new level with a different HUD class recreates the root layout; screens do not survive map travel (by design).
* `bPauseGameWhileActive` follows engine pause rules: single player and the listen-server host, not a replicated pause for clients.
* Tab pages are created for all enabled tabs when the menu builds (async loaded, then instantiated). For heavy pages, construct heavy children in `On Tab Selected`.
* Input configuration follows CommonUI's rule (most recently activated screen). Keep startup order HUD first.
* Moving the folders changes include paths: code that included `"UISystem/BaseUserWidget.h"` must use `"UISystem/Foundation/BaseUserWidget.h"`.

---

## Appendix A — Engine API touchpoints

| API | File | If it differs |
|---|---|---|
| `UCommonActivatableWidget::GetDesiredInputConfig()` → `TOptional<FUIInputConfig>`; `FUIInputConfig(ECommonInputMode, EMouseCaptureMode)`; `NativeOnActivated/NativeOnDeactivated` | `Foundation/BaseActivatableWidget.*` | Mirror the signature used by Lyra's `ULyraActivatableWidget` in your engine version. |
| `UCommonActivatableWidgetContainerBase::AddWidget<T>(Class, InitFunc)`, `RemoveWidget`, `GetActiveWidget`, `ClearWidgets` | `Manager/UIFrameSubsystem.cpp`, `Layout/BaseRootLayoutWidget.cpp` | Same names are used by Lyra's `UPrimaryGameLayout`. |
| `UCommonTabListWidgetBase::RegisterTab`, `RemoveAllTabs`, `SelectTabByID`, `GetTabButtonBaseByID`, `OnTabSelected(FName, UCommonButtonBase*)` | `Menu/BaseMenuWidget.*` | Adjust the `HandleTabSelected` signature to the delegate declared in `CommonTabListWidgetBase.h`. |
| `UCommonUserWidget::RegisterUIActionBinding(FBindUIActionArgs)`, `FBindUIActionArgs(FDataTableRowHandle, bool, FSimpleDelegate)` | `Foundation/BaseUserWidget.cpp`, `Foundation/BaseActivatableWidget.cpp` | Use the constructor overload your version offers. |
| `UCommonButtonBase` as a parent; `UUserWidget::NativePreConstruct()` | `Foundation/BaseButton.*` | Both are stable; if a CommonButtonBase member named `ButtonLabel` or `LabelText` ever appears, rename ours. |
| `UCommonInputSubsystem::GetCurrentInputType()`, `ECommonInputType::Gamepad` | `Manager/UIFrameSubsystem.cpp` | Headers `CommonInputSubsystem.h`, `CommonInputTypeEnum.h`. |
| `AController::OnPossessedPawnChanged` (dynamic, 2 params) | `Binding/UIBindingHub.cpp` | The safety-net timer keeps the system correct even if you remove the binding. |
| `FStreamableManager::RequestAsyncLoad(path/paths, delegate, priority, bManageActiveHandle)` | `Manager`, `HUD/BaseHUDWidget.cpp`, `Menu/BaseMenuWidget.cpp` | Drop the last two arguments and hold the returned handle instead. |
| `UE_DEFINE_GAMEPLAY_TAG_COMMENT` | `Core/UIGameplayTags.cpp` | Use `UE_DEFINE_GAMEPLAY_TAG` (same effect, no comment). |
| `UUserWidget::AddToPlayerScreen(int32)` | `Manager/UIFrameSubsystem.cpp` | Return type changed across versions; the return value is ignored. |
| `UWidget::HasFocusedDescendants()` | `Foundation/BaseActivatableWidget.cpp` | Use `HasUserFocusedDescendants(GetOwningPlayer())`. |
