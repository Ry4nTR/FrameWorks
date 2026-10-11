# CoreFramework UI & HUD System — Architecture & Technical Specification

**Document:** `UI_HUD_System_Architecture.md`
**Module:** `CoreFramework` (plugin, export macro `COREFRAMEWORK_API`)
**Engine target:** Unreal Engine 5.5 – 5.8
**Companion:** `UI_HUD_Setup_Guide.md` (step by step implementation)

> **Verification status.** The code was written against the CommonUI / Enhanced Input / GameplayTags APIs of UE 5.5–5.8 and checked by careful review, but it was **not compiled** in the environment that produced it (no engine available there). Appendix A lists every engine API the system touches, with the one-line fix if a signature differs in your exact engine version. Expect at most a handful of trivial compile adjustments on first build.

---

## 1. Goals and key decisions

| Requirement | Decision | Why |
|---|---|---|
| Genre agnostic | The framework knows **screens, layers, slots, tabs, a local player context**. It never knows health, stamina, inventory or any game stat. | Game data reaches widgets through the project's own components/delegates (Step 4 of the setup guide). |
| C++ does the heavy lifting | All logic (stacks, async loading, input config, focus, binding, tab building) is C++. Blueprints only supply asset references, layout, styling and animation. | Matches the rest of CoreFramework. |
| Stack / layer management, back handling, input routing, gamepad focus | **Build on CommonUI** (`UCommonActivatableWidget`, `UCommonActivatableWidgetStack`, `UCommonTabListWidgetBase`, the CommonUI action router). | CommonUI already solves the hard parts (modal input blocking, back action, mouse/keyboard/gamepad switching, per-platform glyphs). Re-implementing them is a large maintenance burden and gets worse on consoles. The framework adds the missing declarative layer on top instead of competing with it. |
| Async UI loading | Every screen, HUD element and tab content class is a **soft class reference** streamed through `UAssetManager`'s streamable manager. Optional "preload and keep resident". | No hitch when opening a menu; memory only used when needed. |
| Multiplayer, split screen | One `UUIFrameSubsystem` per **`ULocalPlayer`**; bootstrap from `AHUD` (exists only for local controllers). | No UI objects on dedicated servers, correct per-player roots in split screen. |
| Extensible without editing framework code | Layers, screens, slots use **gameplay tags**; screens, HUD elements and tabs live in **data assets** and in **runtime registries**. | A game project adds content by creating assets, never by patching the plugin. |
| Safe possess / unpossess / respawn / late replication | **Observer hub** (`UUIBindingHub`) with a strict Bound/Unbound pairing guarantee, event driven plus a low-rate safety net. | Widgets never poll, never miss a pawn, never leak subscriptions to a dead pawn. |

---

## 2. High-level architecture

### 2.1 Components

| Component | Class | Responsibility |
|---|---|---|
| Manager | `UUIFrameSubsystem` (`ULocalPlayerSubsystem`) | Owns the root layout, pushes/pops screens by tag, async loads, holds the extension registries, owns the binding hub. |
| Bootstrap | `ABaseHUD` (`AHUD`) | Starts/stops the subsystem for its local controller. Draws nothing. |
| Root | `UBaseRootLayoutWidget` | One per local player. Contains one `UCommonActivatableWidgetStack` per layer. |
| Screens | `UBaseActivatableWidget` and children `UBaseHUDWidget`, `UBaseMenuWidget` | Anything that is pushed on a layer. Declares its own input mode, focus target, side effects. |
| Plain widgets | `UBaseUserWidget`, `UBaseTabWidget` | HUD elements, tab pages, list entries. Show/Hide hook, binding, input helpers. |
| Tab bar button | `UBaseTabButton` | Button class instantiated by the menu for each tab. |
| Binding | `IBaseUIBinderInterface`, `UUIBindingHub` | Controller / PlayerState / Pawn observer for widgets. |
| Data | `UUIConfigDataAsset`, `UUITabSetDataAsset`, `UUIFrameSettings` | Everything configurable, all soft references. |
| Ids | `FUITags` (native gameplay tags) | Layers, shipped screens, default HUD slot. Projects add their own tags. |

### 2.2 Who does what (C++, data assets, Blueprints)

| Layer of the project | Contains | Never contains |
|---|---|---|
| **C++ in CoreFramework** | Subsystem, widget base classes, hub, registries, tag accessors, data asset classes | Game stats, game specific screens, any `#include` of game code |
| **Data assets** (`DA_UIConfig`, `DA_*Tabs`) | Which widget class is which screen, which layer, which HUD elements, which tabs | Logic |
| **Framework Blueprints** (`WBP_Base*`, in the plugin's Content) | Neutral layout and styling of the base screens | Game stats |
| **Game project Blueprints / C++** | Child widgets, game components and delegates, game tags, game specific data assets | Edits to plugin files |

---

## 3. Class diagram and data flow

### 3.1 Inheritance

```
UCommonUserWidget
 ├─ UBaseUserWidget  ········ [IBaseUIBinderInterface]  Show/Hide hook, binding, input helpers
 │   └─ UBaseTabWidget ······ tab page (HasPendingChanges / ApplyChanges / RevertChanges)
 │        └─ WBP_Tab_*  (game / framework Blueprints)
 └─ UBaseRootLayoutWidget ·· layer stacks

UCommonActivatableWidget
 └─ UBaseActivatableWidget · [IBaseUIBinderInterface]  input mode, focus, pause/suppress, anim hook
     ├─ UBaseHUDWidget ····· Game layer container, named element slots
     │    └─ WBP_BaseHUD → game HUD
     └─ UBaseMenuWidget ···· Main / Pause / Settings, data driven tab bar
          └─ WBP_BaseMainMenu, WBP_BaseSettings → game menus

UCommonButtonBase
 └─ UBaseTabButton ········· tab bar button

UPrimaryDataAsset
 ├─ UUIConfigDataAsset ····· root class, screens, HUD elements, tab set bindings
 └─ UUITabSetDataAsset ····· list of FUITabDescriptor

UDeveloperSettings ─ UUIFrameSettings   (Project Settings > CoreFramework > CoreFramework UI)
AHUD               ─ ABaseHUD           (bootstrap)
ULocalPlayerSubsystem ─ UUIFrameSubsystem
UObject            ─ UUIBindingHub      (owned by the subsystem)
```

### 3.2 Ownership and lifetime

```
ULocalPlayer  (one per split-screen player, lives for the session)
 └─ UUIFrameSubsystem ................................ lifetime = local player
     ├─ UUIBindingHub ................................ Controller → PlayerState / Pawn observer
     ├─ Registries (not UPROPERTY, plain maps)
     │    ├─ ScreenRegistry     FGameplayTag → FUIScreenDefinition
     │    ├─ HUDElementRegistry FName        → FUIHUDElementDescriptor
     │    └─ MenuTabRegistry    FGameplayTag → [FUITabDescriptor]
     ├─ ResidentHandles ............................... keep "preload" classes in memory
     └─ UBaseRootLayoutWidget ......................... lifetime = HUD actor of the controller
         ├─ [Layer.Game]    Stack ─ UBaseHUDWidget ─┬─ DefaultElementSlot ─ HUD element widgets
         │                                          └─ custom slots (RegisterElementSlot)
         ├─ [Layer.Menu]    Stack ─ UBaseMenuWidget ─┬─ TabList (UBaseTabButton × N)
         │                                           └─ TabSwitcher ─ UBaseTabWidget × N
         ├─ [Layer.Modal]   Stack ─ popups (Is Modal)
         └─ [Layer.Overlay] Stack ─ notifications

APlayerController
 └─ ABaseHUD ............ BeginPlay → InitializeForPlayerController(PC, StartupScreens, this)
                         EndPlay   → ReleasePlayerController(PC, this)
```

### 3.3 Start-up sequence

```
GameMode.HUDClass = BP_*HUD (child of ABaseHUD, StartupScreens = [Screen.HUD])
        │
 local PlayerController spawns its HUD
        │
 ABaseHUD::BeginPlay
        │  UUIFrameSubsystem::InitializeForPlayerController
        │     ├─ EnsureConfigLoaded      DA_UIConfig (tiny, soft refs only) → fills the 3 registries,
        │     │                           preloads screens flagged "Preload And Keep Resident"
        │     ├─ UIBindingHub::Start(PC) → Controller bound, Refresh() → PlayerState / Pawn bound if present
        │     ├─ PushScreenAsync(each StartupScreen)   (queued: root layout does not exist yet)
        │     └─ BeginRootLoad           RequestAsyncLoad(RootLayoutClass)
        ▼
 root class streamed in (async)
        │  CreateWidget<UBaseRootLayoutWidget>(PC) → AddToPlayerScreen(ZOrder)
        │  RootLayout::NativeOnInitialized registers Game / Menu / Modal / Overlay stacks
        │  FlushDeferredPushes → PushScreenAsync for each queued screen, in order
        ▼
 UBaseHUDWidget on the Game layer → activated → input config "Game" applied by CommonUI
        │  NativeConstruct: registers as binder (receives current Controller/PlayerState/Pawn),
        │                   registers DefaultElementSlot, spawns registered HUD elements (async)
```

### 3.4 Pushing a screen (async data flow)

```
Blueprint / C++:  UIFrameSubsystem->PushScreen(Screen.Settings)
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

### 3.5 Binding flow (possess, respawn, late replication)

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

All headers below are the **actual source files** of the package (`Source/CoreFramework/Public/UISystem/`).

### 4.1 `UITypes.h` — enums, screen / tab / HUD element descriptors

```cpp
// CoreFramework UI System
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UITypes.generated.h"

COREFRAMEWORK_API DECLARE_LOG_CATEGORY_EXTERN(LogCoreUI, Log, All);

class UBaseActivatableWidget;
class UBaseTabWidget;
class UBaseUserWidget;

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
 */
USTRUCT(BlueprintType)
struct COREFRAMEWORK_API FUIScreenDefinition
{
	GENERATED_BODY()

	/** Unique id used with PushScreen / PopScreen. Add your own tags in Project Settings > Gameplay Tags. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Screen", meta = (Categories = "CoreFramework.UI.Screen"))
	FGameplayTag ScreenTag;

	/** Soft reference: nothing is loaded until the screen is requested (or preloaded). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Screen")
	TSoftClassPtr<UBaseActivatableWidget> WidgetClass;

	/** Layer (stack) the screen is pushed onto. Empty falls back to the Menu layer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Screen", meta = (Categories = "CoreFramework.UI.Layer"))
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

	/** Set false on an overriding descriptor to remove a tab that a framework default provides. */
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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD Element", meta = (Categories = "CoreFramework.UI.HUDSlot"))
	FGameplayTag SlotTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD Element")
	TSoftClassPtr<UBaseUserWidget> WidgetClass;
};
```

### 4.2 `UIConfigDataAsset.h` — the data-driven entry point

```cpp
// CoreFramework UI System
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "UISystem/UITypes.h"
#include "UIConfigDataAsset.generated.h"

class UBaseRootLayoutWidget;

/** A reusable list of tabs. Create one per menu (or one per feature) in the Content Browser. */
UCLASS(BlueprintType)
class COREFRAMEWORK_API UUITabSetDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tabs", meta = (TitleProperty = "TabId"))
	TArray<FUITabDescriptor> Tabs;
};

/** Attaches a tab set to a menu id so a child project can extend a framework menu without editing it. */
USTRUCT(BlueprintType)
struct COREFRAMEWORK_API FUIMenuTabSetBinding
{
	GENERATED_BODY()

	/** Must match UBaseMenuWidget::MenuTag of the menu that should receive the tabs. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tabs", meta = (Categories = "CoreFramework.UI.Screen"))
	FGameplayTag MenuTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tabs")
	TSoftObjectPtr<UUITabSetDataAsset> TabSet;
};

/**
 * The single data-driven entry point of the UI system. One asset per project, assigned in
 * Project Settings > CoreFramework > CoreFramework UI. Everything is soft-referenced.
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

	/** Tab sets attached to menus by tag. Several entries may target the same menu; their tabs are merged. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Menus", meta = (TitleProperty = "MenuTag"))
	TArray<FUIMenuTabSetBinding> MenuTabSets;
};
```

### 4.3 `UIFrameSubsystem.h` — the manager

```cpp
// CoreFramework UI System
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "UISystem/UITypes.h"
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

### 4.4 `BaseUIBinderInterface.h` — observer contract

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

### 4.5 `BaseUserWidget.h` — plain widget base

```cpp
// CoreFramework UI System
#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Engine/DataTable.h"
#include "Input/UIActionBindingHandle.h"
#include "UISystem/BaseUIBinderInterface.h"
#include "BaseUserWidget.generated.h"

class APawn;
class APlayerController;
class APlayerState;
class UUIFrameSubsystem;
class UWidgetAnimation;

DECLARE_DYNAMIC_DELEGATE(FBaseUIActionDelegate);

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

### 4.6 `BaseActivatableWidget.h` — screen base (CommonUI)

```cpp
// CoreFramework UI System
#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Engine/DataTable.h"
#include "Engine/GameViewportClient.h"
#include "GameplayTagContainer.h"
#include "Input/UIActionBindingHandle.h"
#include "UISystem/BaseUIBinderInterface.h"
#include "UISystem/UITypes.h"
#include "BaseActivatableWidget.generated.h"

class APawn;
class APlayerController;
class APlayerState;
class UUIFrameSubsystem;
class UWidget;
class UWidgetAnimation;

DECLARE_DYNAMIC_DELEGATE(FBaseActivatableActionDelegate);

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
	void BindUIAction(const FDataTableRowHandle& ActionRow, bool bShowInActionBar, FBaseActivatableActionDelegate OnExecuted);

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

### 4.7 `BaseHUDWidget.h` — master HUD container

```cpp
// CoreFramework UI System
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UISystem/BaseActivatableWidget.h"
#include "UISystem/UITypes.h"
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

### 4.8 `BaseMenuWidget.h` — menus with a data driven tab bar

```cpp
// CoreFramework UI System
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UISystem/BaseActivatableWidget.h"
#include "UISystem/UITypes.h"
#include "BaseMenuWidget.generated.h"

class UBaseTabButton;
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
 *   DefaultTabSet (this widget)  <  UUIConfigDataAsset::MenuTabSets  <  UUIFrameSubsystem::RegisterMenuTab
 * Later sources override earlier ones by TabId, so a project can replace or disable (bEnabled = false)
 * a framework tab without touching framework assets. Tab content classes are async loaded.
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

	/** Id used to receive tabs registered through the config asset or the subsystem. Leave empty for menus without extensions. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base UI|Menu", meta = (Categories = "CoreFramework.UI.Screen"))
	FGameplayTag MenuTag;

	/** Tabs that ship with this menu Blueprint. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base UI|Menu")
	TObjectPtr<UUITabSetDataAsset> DefaultTabSet;

	/** Button Blueprint (child of UBaseTabButton) instantiated for each tab. Required when tabs are used. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base UI|Menu")
	TSubclassOf<UBaseTabButton> TabButtonClass;

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

### 4.9 `BaseTabWidget.h` — tab page

```cpp
// CoreFramework UI System
#pragma once

#include "CoreMinimal.h"
#include "UISystem/BaseUserWidget.h"
#include "UISystem/UITypes.h"
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

### 4.10 `BaseHUD.h` — bootstrap actor

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
	 * Main menu map: [Screen.MainMenu]. Gameplay map: [Screen.HUD].
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Base UI", meta = (Categories = "CoreFramework.UI.Screen"))
	TArray<FGameplayTag> StartupScreens;
};
```

The remaining headers (`UIBindingHub.h`, `BaseRootLayoutWidget.h`, `BaseTabButton.h`, `UIFrameSettings.h`, `UIGameplayTags.h`) are listed in the file manifest (section 9); they are short and documented in place.

---

## 5. Input and navigation handling

### 5.1 Principle

A screen **declares** what it needs; CommonUI **applies** it. There is no central "SetInputMode" call to forget.

```
UBaseActivatableWidget
  InputMode            Default | GameOnly | UIOnly | GameAndUI
  GameMouseCaptureMode EMouseCaptureMode
        │  GetDesiredInputConfig()
        ▼
  CommonUI action router  ──►  topmost active screen decides the input configuration
```

| `EUIInputMode` | CommonUI mode | Typical user | Behavior |
|---|---|---|---|
| `GameOnly` | `Game` | `UBaseHUDWidget` (default) | Game gets input, mouse captured per `GameMouseCaptureMode`. |
| `UIOnly` | `Menu` | `UBaseMenuWidget` (default), popups | UI gets input, cursor visible, game input blocked. |
| `GameAndUI` | `All` | Live overlays (quick inventory with mouse) | UI first, unconsumed input still reaches the game. Combine with *Ignore Look Input While Active* so the mouse does not turn the camera. |
| `Default` | none | Notifications, toasts | Does not request a configuration; the underlying screen's stays active. |

Closing a menu reverts automatically to the configuration of the next topmost screen (the HUD). **Order matters:** CommonUI uses the most recently activated screen, so list the HUD first in `StartupScreens`.

### 5.2 Focus (keyboard / gamepad navigation)

Three levels, first match wins:

1. **Blueprint `Get Desired Focus Target`** (CommonUI's own hook) on the screen — full control.
2. **`DefaultFocusWidget`**: a widget named exactly `DefaultFocusWidget` in the screen. The framework focuses it one tick after activation **only if nothing inside the screen has focus yet**.
3. **Menus with tabs**: when no explicit target exists, focus lands on the button of the active tab (`UBaseMenuWidget::ResolveDefaultFocusWidget`).

Inside a screen, directional navigation is Slate's: keep buttons focusable and laid out in a grid/list. When the player switches device, CommonUI re-applies focus / hides the cursor for you.

Tab pages (`UBaseTabWidget`) have their own optional `DefaultFocusWidget` and `bFocusContentOnSelect` (off by default so gamepad bumpers keep switching tabs).

### 5.3 Back, modal, pause, suppression

| Need | How |
|---|---|
| Escape / gamepad B closes the screen | Tick **Is Back Handler** in the screen Blueprint's class defaults (CommonUI). Requires the default back action in the CommonUI input data (setup guide, Step 5). |
| Popup blocks everything below | Tick **Is Modal**; push on the **Modal** layer. |
| Pause the game while open | `bPauseGameWhileActive` (single player / listen-server host). Released on deactivation and in a destruct safety net. |
| Stop camera / movement while a live overlay is open | `bIgnoreLookInputWhileActive`, `bIgnoreMoveInputWhileActive`. Counters on the controller are applied once per activation and always released. |
| Close from a button | `CloseScreen()` on the widget (Blueprint callable). |

### 5.4 Enhanced Input

* Gameplay input (move, look, jump…) stays in your gameplay `UInputMappingContext`s, handled by `ABaseCharacter`/`UInputConfigData` as before.
* UI actions (back, confirm, tab left/right) are **CommonUI input actions** (`FDataTableRowHandle` rows). With *Enable Enhanced Input Support* in the CommonUI input settings, each row can reference an `UInputAction`, so UI and gameplay share the same rebinding source.
* `BindUIAction` / `BindUIActionNative` bind such a row to a delegate; the binding is removed automatically with the widget.
* `UIOnly` screens block gameplay input at the Slate level, so no mapping-context juggling is needed.

### 5.5 Mouse, keyboard, gamepad switching

* Detection and cursor/focus switching: CommonUI (`CommonGameViewportClient` + input settings).
* Query: `UUIFrameSubsystem::IsUsingGamepad()`, `UBaseUserWidget::IsUsingGamepad()`, `UBaseActivatableWidget::IsUsingGamepad()`.
* Glyphs: CommonUI buttons/action bars use the per-platform controller data assets (Step 5).

### 5.6 Split screen

Every local player has its own `UUIFrameSubsystem`, root layout, binding hub and CommonUI router. Widgets always resolve the subsystem through their **owning local player**.

---

## 6. Binding system (game and character without hardcoding)

### 6.1 Contract (`IBaseUIBinderInterface`)

Six events: Controller / PlayerState / Pawn × Bound / Unbound. Implemented natively by `UBaseUserWidget` and `UBaseActivatableWidget` (empty defaults), so a child widget overrides only what it needs, in C++ (`OnUIPawnBound_Implementation`) or in Blueprint (override event).

### 6.2 Scenario table

| Scenario | What the hub does | What your widget sees |
|---|---|---|
| HUD created before the pawn exists (client join, map load) | Safety-net timer / `OnPossessedPawnChanged` detects the pawn when it arrives | `OnUIPawnBound(Pawn)` once |
| Widget created after the pawn exists | Replay at registration | `OnUIControllerBound`, `OnUIPlayerStateBound`, `OnUIPawnBound` immediately after `Event Construct` |
| Unpossess (pawn becomes null) | Refresh | `OnUIPawnUnbound(oldPawn)` |
| Respawn (new pawn) | Refresh | `OnUIPawnUnbound(old)` then `OnUIPawnBound(new)` |
| Old pawn destroyed before the unbound call | Refresh | `OnUIPawnUnbound(nullptr)` — use what you cached |
| PlayerState arrives late / is replaced | Refresh | `OnUIPlayerStateUnbound/Bound` |
| Controller destroyed (travel, disconnect) | Refresh detects the stale controller, `Stop()` | `…Unbound` for pawn, player state, controller |
| Widget destroyed while bound | `UnregisterBinder` | Matching `…Unbound` calls so it can drop subscriptions |
| Host / listen server | Same code path (local controller) | Same events |
| Dedicated server | No `AHUD`, no local player, no subsystem | Nothing is created |

### 6.3 Rules for implementers

1. Subscribe in `Bound`, unsubscribe in the matching `Unbound`.
2. In `Unbound`, never assume the argument is valid: keep a `TWeakObjectPtr` to what you subscribed to.
3. After subscribing, push the current value into the widget once (delegates only fire on change).
4. Never cast to a game class inside CoreFramework. Cast inside your own widget (`GetBoundPawnAs<AMyCharacter>()`).

---

## 7. Extensibility design

Nothing in this chapter requires editing a CoreFramework file.

### 7.1 Add or change tabs (Settings, Main Menu)

Tab sources are merged in this order; a later source **replaces** an earlier tab with the same `TabId`:

```
UBaseMenuWidget::DefaultTabSet                (tabs that ship with the framework menu Blueprint)
  <  UUIConfigDataAsset::MenuTabSets          (data asset of the game project, keyed by MenuTag)
  <  UUIFrameSubsystem::RegisterMenuTab       (runtime: game code, plugins, game features)
```

| Goal | Do |
|---|---|
| Add a "Gameplay" tab | Create `WBP_Tab_Gameplay` (child of `UBaseTabWidget`), create `DA_Game_SettingsTabs` (`UITabSetDataAsset`) with a descriptor, add `{ MenuTag = Screen.Settings, TabSet = DA_Game_SettingsTabs }` to `DA_UIConfig.MenuTabSets`. |
| Replace the Graphics tab | Same, with `TabId = Graphics` and your own widget class. |
| Remove the Controls tab | Descriptor with `TabId = Controls`, `Enabled = false`. |
| Add a tab from a plugin at runtime | `Subsystem->RegisterMenuTab(FUITags::ScreenSettings(), Descriptor);` — an open menu rebuilds immediately. |
| Reorder | `SortOrder` (lower first). |

C++ example:

```cpp
FUITabDescriptor Tab;
Tab.TabId = TEXT("Accessibility");
Tab.DisplayName = NSLOCTEXT("MyGame", "TabAccessibility", "Accessibility");
Tab.TabWidgetClass = TSoftClassPtr<UBaseTabWidget>(FSoftObjectPath(TEXT("/Game/UI/Settings/WBP_Tab_Accessibility.WBP_Tab_Accessibility_C")));
Tab.SortOrder = 50;
UI->RegisterMenuTab(FUITags::ScreenSettings(), Tab);
```

### 7.2 Add HUD elements

1. Create the widget (child of `UBaseUserWidget`, it binds itself — see Step 4 of the guide).
2. Add `FUIHUDElementDescriptor { ElementId, SlotTag, WidgetClass }` to `DA_UIConfig.HUDElements`, **or** call `UUIFrameSubsystem::RegisterHUDElement` at runtime (e.g. when a game feature activates). A live HUD spawns it immediately; `UnregisterHUDElement` removes it.
3. Slots: the HUD Blueprint exposes `DefaultElementSlot` (tag `CoreFramework.UI.HUDSlot.Default`) and any number of custom slots registered in `Event Construct` with `RegisterElementSlot(Tag, Panel)`. An element whose slot does not exist yet is placed as soon as the slot is registered.

### 7.3 Add game-specific menus and popups

Add a tag (`Game.UI.Screen.Inventory`), a Blueprint child of `UBaseMenuWidget` / `UBaseActivatableWidget`, and a `FUIScreenDefinition` row in `DA_UIConfig.Screens` (or `RegisterScreen` at runtime). `PushScreen(Game.UI.Screen.Inventory)` works from anywhere.

### 7.4 Add layers

Add a tag (`Game.UI.Layer.Cinematic`), add a `Common Activatable Widget Stack` to your root layout child Blueprint, call `RegisterLayer(Tag, Stack)` in its `Event Construct`, and reference the tag in a screen definition.

### 7.5 Subclass in C++

All base classes are `Blueprintable` and expose virtual hooks (`ResolveDefaultFocusWidget`, `NativeOnActivated`, the six binder `_Implementation` functions). Project C++ derives from them in the **game module**.

### 7.6 What stays out of CoreFramework

HP / mana / ammo widgets, inventory, quest tracker, any `#include` of game code. Those are game-module widgets registered through 7.2.

---

## 8. Performance and memory notes

* **No hitch on open:** class streaming is asynchronous; classes already in memory push synchronously; `Preload And Keep Resident` removes even the first-open cost for hot screens (pause menu).
* **Binding cost:** the safety-net timer compares three pointers per interval (default 0.2 s per local player). Set `BindingPollIntervalSeconds = 0` to rely on events only if you control every pawn change path.
* **Duplicate loads:** concurrent pushes of the same screen share one in-flight request.
* **Teardown:** root layout, handles and queued requests are invalidated by a session counter, so late completions from a previous map cannot push into a new session.
* **Pooling:** CommonUI containers pool widget instances; registration/unregistration is tied to `NativeConstruct/NativeDestruct`, which pooling pairs correctly.

---

## 9. File manifest

```
Source/CoreFramework/Public/UISystem/
  UITypes.h                 enums, descriptors, log category
  UIGameplayTags.h          FUITags (native tags: layers, screens, default HUD slot)
  UIConfigDataAsset.h       UUIConfigDataAsset, UUITabSetDataAsset, FUIMenuTabSetBinding
  UIFrameSettings.h         Project Settings > CoreFramework > CoreFramework UI
  UIFrameSubsystem.h        manager
  UIBindingHub.h            observer hub
  BaseUIBinderInterface.h   IBaseUIBinderInterface
  BaseUserWidget.h          plain widget base
  BaseActivatableWidget.h   screen base
  BaseHUDWidget.h           HUD container
  BaseMenuWidget.h          menu + tab bar
  BaseTabWidget.h           tab page
  BaseTabButton.h           tab bar button
  BaseRootLayoutWidget.h    layer stacks
  BaseHUD.h                 bootstrap AHUD
Source/CoreFramework/Private/UISystem/
  (one .cpp per header except UITypes / UIConfigDataAsset / UIFrameSettings / interface)
Setup/                      Build.cs, uplugin and ini snippets
Docs/                       this document + setup guide
```

Include style: `#include "UISystem/BaseUserWidget.h"` (works for both a `Public/` layout and a flat module root).

---

## 10. Known limits

* A new level that uses a different HUD class recreates the root layout; screens do not survive map travel (by design — the UI belongs to the HUD's lifetime).
* `bPauseGameWhileActive` follows engine pause rules: effective in single player and for the listen-server host, not a replicated pause for clients.
* Tab pages are created for all enabled tabs when the menu builds (async loaded, then instantiated). For very heavy tab pages, make the page itself lazy (construct heavy children in `OnTabSelected`).
* Input configuration follows CommonUI's rule (most recently activated screen). Keep startup order HUD first.

---

## Appendix A — Engine API touchpoints

These are the engine APIs the code depends on. If your exact engine version differs, the fix is local to the named file.

| API | File | If it differs |
|---|---|---|
| `UCommonActivatableWidget::GetDesiredInputConfig()` → `TOptional<FUIInputConfig>`; `FUIInputConfig(ECommonInputMode, EMouseCaptureMode)`; `NativeOnActivated/NativeOnDeactivated` | `BaseActivatableWidget.*` | Mirror the signature used by Lyra's `ULyraActivatableWidget` in your engine version. |
| `UCommonActivatableWidgetContainerBase::AddWidget<T>(Class, InitFunc)`, `RemoveWidget`, `GetActiveWidget`, `ClearWidgets` | `UIFrameSubsystem.cpp`, `BaseRootLayoutWidget.cpp` | Same names are used by Lyra's `UPrimaryGameLayout`. |
| `UCommonTabListWidgetBase::RegisterTab`, `RemoveAllTabs`, `SelectTabByID`, `GetTabButtonBaseByID`, `OnTabSelected(FName, UCommonButtonBase*)` | `BaseMenuWidget.*` | Adjust the `HandleTabSelected` signature to the delegate declared in `CommonTabListWidgetBase.h`. |
| `UCommonUserWidget::RegisterUIActionBinding(FBindUIActionArgs)`, `FBindUIActionArgs(FDataTableRowHandle, bool, FSimpleDelegate)` | `BaseUserWidget.cpp`, `BaseActivatableWidget.cpp` | Use the constructor overload your version offers. |
| `UCommonInputSubsystem::GetCurrentInputType()`, `ECommonInputType::Gamepad` | `UIFrameSubsystem.cpp` | Header names `CommonInputSubsystem.h`, `CommonInputTypeEnum.h`. |
| `AController::OnPossessedPawnChanged` (dynamic, 2 params) | `UIBindingHub.cpp` | The safety-net timer keeps the system correct even if you remove the binding. |
| `FStreamableManager::RequestAsyncLoad(path/paths, delegate, priority, bManageActiveHandle)` | `UIFrameSubsystem.cpp`, `BaseHUDWidget.cpp`, `BaseMenuWidget.cpp` | Drop the last two arguments and hold the returned handle instead. |
| `UE_DEFINE_GAMEPLAY_TAG_COMMENT` | `UIGameplayTags.cpp` | Use `UE_DEFINE_GAMEPLAY_TAG` (same effect, no comment). |
| `UUserWidget::AddToPlayerScreen(int32)` | `UIFrameSubsystem.cpp` | Return type changed across versions; the return value is ignored. |
| `UWidget::HasFocusedDescendants()` | `BaseActivatableWidget.cpp` | Use `HasUserFocusedDescendants(GetOwningPlayer())`. |
