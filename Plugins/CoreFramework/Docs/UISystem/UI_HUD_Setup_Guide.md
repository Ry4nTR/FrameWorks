# CoreFramework UI & HUD System — Project Setup & Implementation Guide

**Document:** `UI_HUD_Setup_Guide.md`
**Companion:** `UI_HUD_System_Architecture.md` (design, diagrams, full headers)
**Engine target:** Unreal Engine 5.5 – 5.8

Follow the steps in order. Step 1 happens **once** in the framework repository, Step 2 **once** (assets that live in the plugin), Steps 3–5 happen **in every game project** that pulls the plugin.

| Step | Where | Result |
|---|---|---|
| 1. Framework C++ setup | `Plugins/CoreFramework` | Plugin compiles with the UI system |
| 2. Framework base Blueprints | `Plugins/CoreFramework/Content/UI/Base` | `WBP_BaseHUD`, `WBP_BaseMainMenu`, `WBP_BaseSettings`, tab templates, … |
| 3. Integrate into a game project | game project | Config asset, HUD class, first screens on screen |
| 4. Bind gameplay data | game project | HP / Mana example without touching the plugin |
| 5. Input and controller configuration | game project | Back button, tab bumpers, mouse / keyboard / gamepad switching |

> **Compile note.** The C++ was reviewed against the UE 5.5–5.8 CommonUI APIs but not compiled by its author. If your first build reports a signature mismatch, `UI_HUD_System_Architecture.md` → *Appendix A* names every engine call the system uses and the one-line fix.

---

## Step 1 — Framework C++ setup

### 1.1 Copy the files

From the package, copy:

```
Source/CoreFramework/Public/UISystem/*.h   →  Plugins/CoreFramework/Source/CoreFramework/Public/UISystem/
Source/CoreFramework/Private/UISystem/*.cpp →  Plugins/CoreFramework/Source/CoreFramework/Private/UISystem/
```

(If your module uses a flat layout without `Public/` and `Private/`, copy both folders' contents into `Source/CoreFramework/UISystem/`. Every include in the code is written as `"UISystem/<File>.h"`, which works in both layouts.)

Files (15 headers, 11 sources):

| Header | Source | Purpose |
|---|---|---|
| `UITypes.h` | — | Enums, descriptors, log category `LogCoreUI` |
| `UIGameplayTags.h` | `UIGameplayTags.cpp` | Native tags for layers, shipped screens, default HUD slot |
| `UIConfigDataAsset.h` | — | `UUIConfigDataAsset`, `UUITabSetDataAsset` |
| `UIFrameSettings.h` | — | Project Settings page |
| `UIFrameSubsystem.h` | `UIFrameSubsystem.cpp` | Manager |
| `UIBindingHub.h` | `UIBindingHub.cpp` | Controller / PlayerState / Pawn observer |
| `BaseUIBinderInterface.h` | — | `IBaseUIBinderInterface` |
| `BaseUserWidget.h` | `BaseUserWidget.cpp` | Plain widget base |
| `BaseActivatableWidget.h` | `BaseActivatableWidget.cpp` | Screen base |
| `BaseHUDWidget.h` | `BaseHUDWidget.cpp` | HUD container |
| `BaseMenuWidget.h` | `BaseMenuWidget.cpp` | Menu + tab bar |
| `BaseTabWidget.h` | `BaseTabWidget.cpp` | Tab page |
| `BaseTabButton.h` | `BaseTabButton.cpp` | Tab bar button |
| `BaseRootLayoutWidget.h` | `BaseRootLayoutWidget.cpp` | Layer stacks |
| `BaseHUD.h` | `BaseHUD.cpp` | Bootstrap `AHUD` |

### 1.2 Module dependencies

Open `Plugins/CoreFramework/Source/CoreFramework/CoreFramework.Build.cs` and add the modules from `Setup/CoreFramework.Build.cs.additions.txt`:

```csharp
PublicDependencyModuleNames.AddRange(new string[]
{
	"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput",
	"GameplayTags", "UMG", "Slate", "SlateCore",
	"CommonUI", "CommonInput", "DeveloperSettings"
});
```

### 1.3 Plugin dependencies

In `Plugins/CoreFramework/CoreFramework.uplugin` add (see `Setup/CoreFramework.uplugin.additions.txt`):

```json
"Plugins": [
	{ "Name": "CommonUI",      "Enabled": true },
	{ "Name": "EnhancedInput", "Enabled": true }
]
```

Also make sure the plugin can hold Blueprint assets (needed for Step 2): `"CanContainContent": true`.

### 1.4 Build and verify

1. Regenerate project files, build the editor target.
2. Open the editor and check:
   * **Project Settings → CoreFramework → CoreFramework UI** exists.
   * Gameplay Tag picker lists `CoreFramework.UI.Layer.*`, `CoreFramework.UI.Screen.*`, `CoreFramework.UI.HUDSlot.Default`.
   * Content Browser → *Add → Miscellaneous → Data Asset* offers **UIConfigDataAsset** and **UITabSetDataAsset**.
   * *Add → User Interface → Widget Blueprint* → parent class picker lists `BaseHUDWidget`, `BaseMenuWidget`, `BaseTabWidget`, `BaseUserWidget`, `BaseActivatableWidget`, `BaseRootLayoutWidget`, `BaseTabButton`.
3. Commit, then publish the plugin branch (adapt names to your setup):

```bash
git subtree split --prefix=Plugins/CoreFramework -b plugin-only
git push origin plugin-only
```

---

## Step 2 — Create the framework base Blueprints

Create these in the **plugin** content folder `Plugins/CoreFramework/Content/UI/Base/` (Content Browser → *Settings → Show Plugin Content*). They contain only neutral layout and reference only framework classes, so they are safe to ship in the submodule. Game projects create **child** Blueprints of them.

> Naming rule for every `BindWidget` below: the widget in the Designer must have **exactly** the name given. Optional bindings can be omitted.

### 2.1 `WBP_BaseRootLayout` (parent `BaseRootLayoutWidget`)

Designer hierarchy (order = draw order, bottom to top):

```
[Overlay]  (fills the screen)
  ├─ Common Activatable Widget Stack   name: GameLayerStack
  ├─ Common Activatable Widget Stack   name: MenuLayerStack
  ├─ Common Activatable Widget Stack   name: ModalLayerStack
  └─ Common Activatable Widget Stack   name: OverlayLayerStack
```

For each stack: Horizontal and Vertical Alignment **Fill**, **Visibility = Self Hit Test Invisible** (so an empty stack never blocks the mouse). Transition: Game stack *None*; Menu, Modal *Fade* (duration ≈ 0.15 s); Overlay *None*. Compile.

Extra project layers: add another stack and, in the Blueprint's `Event Construct`, call `Register Layer (Layer Tag, Stack)`.

### 2.2 `WBP_BaseTabButton` (parent `BaseTabButton`)

* Create a **Common Button Style** asset (`BP_BaseTabButtonStyle`: normal / hovered / pressed / selected brushes) and assign it to the button's *Style*.
* Add a **Common Text Block** named `TabLabelText` inside it.
* Set *Is Selectable* (tab buttons are toggle-like). Compile.

### 2.3 `WBP_BaseHUD` (parent `BaseHUDWidget`)

```
[Canvas Panel]
  └─ Overlay   name: DefaultElementSlot     (anchors: fill)
```

* Class Defaults: *Input Mode = Game Only* (already the default). Leave *Is Back Handler* **off**.
* Optional named slots for the game: add a Vertical Box (e.g. `BottomLeftBox`, anchored bottom-left). In the Event Graph:

```
Event Construct
  └─ Register Element Slot ( Slot Tag = Game.UI.HUDSlot.BottomLeft , Container = BottomLeftBox )
```

Compile. Elements registered for these slots (Step 4) are placed automatically.

### 2.4 `WBP_BaseMainMenu` (parent `BaseMenuWidget`)

```
[Canvas Panel]
  └─ Vertical Box
       ├─ Button (Common Button)  name: DefaultFocusWidget     text "Play"
       ├─ Button                  text "Settings"
       └─ Button                  text "Quit"
```

* Naming the first button `DefaultFocusWidget` is all that is needed for keyboard / gamepad focus on open.
* Class Defaults: *Input Mode = UI Only* (default), *Is Back Handler* off (root screen).
* Buttons (Event Graph):
  * **Settings** → `Get UI Frame Subsystem` → `Push Screen (CoreFramework.UI.Screen.Settings)`
  * **Play** → open your gameplay map (`Open Level`)
  * **Quit** → `Quit Game`
* No tabs here, so no `TabList` / `TabSwitcher`; the menu works without them.

### 2.5 `WBP_BaseSettings` (parent `BaseMenuWidget`)

```
[Vertical Box]
  ├─ Common Tab List            name: TabList
  ├─ Common Animated Switcher   name: TabSwitcher      (fill)
  └─ Horizontal Box
       ├─ Button "Apply"
       ├─ Button "Revert"
       └─ Button "Back"
```

Class Defaults:

| Property | Value |
|---|---|
| Is Back Handler | **On** (Escape / gamepad B closes it) |
| Menu Tag | `CoreFramework.UI.Screen.Settings` |
| Tab Button Class | `WBP_BaseTabButton` |
| Default Tab Set | `DA_Base_SettingsTabs` (2.6) |

`TabList` details: set *Next Tab Input Action Data* and *Previous Tab Input Action Data* to the `NextTab` / `PreviousTab` rows of your UI actions table (Step 5.1) and tick *Auto Listen For Input*. This gives gamepad shoulder-button and keyboard tab switching.

Buttons:

* Apply → `Apply All Tabs`
* Revert → `Revert All Tabs`
* Back → `Close Screen`

### 2.6 Tab templates

**`WBP_Tab_Template`** (parent `BaseTabWidget`):

```
[Scroll Box]
  └─ Vertical Box
       └─ (first interactive control)   name: DefaultFocusWidget
```

Override the three native events (Blueprint *Override Function*):

| Event | Implement |
|---|---|
| `Has Pending Changes` | return true when any control differs from the stored value |
| `Apply Changes` | write the values to your settings storage |
| `Revert Changes` | reload stored values into the controls |

Available events: `On Tab Initialized`, `On Tab Selected`, `On Tab Deselected`, plus everything from `BaseUserWidget` (binding events, `Show / Hide`).

Create three children from the template, each with its own content:

| Tab | Typical content |
|---|---|
| `WBP_Tab_Graphics` | quality presets, resolution, vsync (your `UGameUserSettings` wrapper) |
| `WBP_Tab_Audio` | master / music / effects volume |
| `WBP_Tab_Controls` | sensitivity, key rebinding (Enhanced Input user settings) |

The framework ships the structure; the actual setting widgets are the game's business.

**`DA_Base_SettingsTabs`** (Data Asset, class `UITabSetDataAsset`):

| Tab Id | Display Name | Tab Widget Class | Sort Order | Enabled |
|---|---|---|---|---|
| `Graphics` | Graphics | `WBP_Tab_Graphics` | 0 | ✔ |
| `Audio` | Audio | `WBP_Tab_Audio` | 10 | ✔ |
| `Controls` | Controls | `WBP_Tab_Controls` | 20 | ✔ |

### 2.7 Pause menu and popup

* **`WBP_BasePauseMenu`** (parent `BaseMenuWidget`): *Pause Game While Active* ✔, *Is Back Handler* ✔, *Input Mode = UI Only*. Buttons: Resume → `Close Screen`; Settings → `Push Screen (…Screen.Settings)`; Quit to menu → `Open Level` (main menu map). Name the Resume button `DefaultFocusWidget`.
* **`WBP_BasePopup`** (parent `BaseActivatableWidget`): *Is Modal* ✔, *Is Back Handler* ✔, *Input Mode = UI Only*. Push it on the **Modal** layer from a screen definition.

---

## Step 3 — Integrate into a new game project

### 3.1 Pull the submodule

```bash
git submodule add -b plugin-only https://github.com/Ry4nTR/FrameWorks.git Plugins/CoreFramework
git submodule update --init --recursive
```

Right-click the `.uproject` → *Generate Visual Studio project files*, build, open the editor. CommonUI and Enhanced Input are enabled automatically through the plugin's dependencies.

Add the module to the game's own `Build.cs` if game C++ will include CoreFramework headers:

```csharp
PublicDependencyModuleNames.AddRange(new string[] { "CoreFramework", "GameplayTags", "UMG", "CommonUI" });
```

### 3.2 Project-level configuration

1. **CommonUI prerequisites** — do Step 5.1 now (viewport client and input data). Without it, screens appear but Back and tab shortcuts do nothing.
2. **Gameplay tags** — add your own ids (Project Settings → Gameplay Tags, or `Setup/DefaultGameplayTags.ini.example.txt`):
   `Game.UI.Screen.Inventory`, `Game.UI.HUDSlot.BottomLeft`, …
3. **Project Settings → CoreFramework → CoreFramework UI → Default UI Config** = `DA_UIConfig` (3.3). (Alternative: `Setup/DefaultGame.ini.additions.txt`.)

### 3.3 Create `DA_UIConfig` (Data Asset, class `UIConfigDataAsset`) in `Content/UI/Config/`

| Section | Entry |
|---|---|
| **Root Layout Class** | `WBP_BaseRootLayout` (or your child) |
| **Screens** | see table below |
| **HUD Elements** | filled in Step 4 |
| **Menu Tab Sets** | empty for now (7.1 of the architecture document shows how a game adds tabs) |

Screens:

| Screen Tag | Widget Class | Layer Tag | Multiple | Preload |
|---|---|---|---|---|
| `CoreFramework.UI.Screen.HUD` | `WBP_Game_HUD` (child of `WBP_BaseHUD`) | `…Layer.Game` | ✘ | ✔ |
| `CoreFramework.UI.Screen.MainMenu` | `WBP_Game_MainMenu` (child of `WBP_BaseMainMenu`) | `…Layer.Menu` | ✘ | ✘ |
| `CoreFramework.UI.Screen.PauseMenu` | `WBP_Game_PauseMenu` | `…Layer.Menu` | ✘ | ✔ |
| `CoreFramework.UI.Screen.Settings` | `WBP_Game_Settings` (child of `WBP_BaseSettings`) | `…Layer.Menu` | ✘ | ✘ |

You can point directly at the plugin's `WBP_Base*` classes for a quick start; child Blueprints are where project styling lives. Game-only screens (inventory…) are added as extra rows with your own tags.

### 3.4 HUD classes and GameMode

Create two Blueprint classes with parent **`BaseHUD`**:

| Blueprint | Startup Screens | Used by |
|---|---|---|
| `BP_GameHUD` | `[CoreFramework.UI.Screen.HUD]` | gameplay map GameMode |
| `BP_MenuHUD` | `[CoreFramework.UI.Screen.MainMenu]` | main menu map GameMode |

Create (or reuse) a GameMode Blueprint per map type and set **HUD Class** accordingly:

```
BP_GameMode_Gameplay   HUD Class = BP_GameHUD
BP_GameMode_MainMenu   HUD Class = BP_MenuHUD   (Default Pawn Class = None)
```

Assign each GameMode in the map's *World Settings → GameMode Override*.

Push order inside `Startup Screens` matters: the **most recently activated screen decides the input mode**, so list the HUD first and overlays after it.

### 3.5 PlayerController

The framework needs **no** custom PlayerController. Typical additions are game-owned and use the subsystem. Example: open the pause menu from an Enhanced Input action (game module):

```cpp
#include "UISystem/UIFrameSubsystem.h"
#include "UISystem/UIGameplayTags.h"

void AMyPlayerController::HandlePauseAction()
{
	if (UUIFrameSubsystem* UI = UUIFrameSubsystem::GetFromController(this))
	{
		UI->PushScreen(FUITags::ScreenPauseMenu());   // already open → no duplicate
	}
}
```

Closing is handled by the screen itself (Back action, Resume button → `CloseScreen`). While a `UIOnly` screen is open the game does not receive the pause key, which is the correct behavior.

Blueprint equivalent: `Get Local Player Subsystem (UIFrameSubsystem)` → `Push Screen`.

### 3.6 Smoke test

1. Open the gameplay map and press Play. The HUD widget appears (empty is fine) and the cursor stays hidden.
2. Trigger the pause action: the pause menu opens, the cursor appears, gameplay input stops, Resume has focus.
3. Press Escape or gamepad B: the pause menu closes, game input returns.
4. Open the main menu map: the main menu appears with the cursor visible and the first button focused.

---

## Step 4 — Bind gameplay data (HP example, no framework changes)

Goal: a health bar that follows the local player's current pawn through spawn, death, respawn and possess changes.
Everything below lives in the **game project**.

### 4.1 The game's own component and delegate

```cpp
// MyGame/HealthComponent.h
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HealthComponent.generated.h"

class UHealthComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnHealthChanged, UHealthComponent*, Component, float, NewHealth, float, MaxHealth);

UCLASS(ClassGroup = (Game), meta = (BlueprintSpawnableComponent))
class MYGAME_API UHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHealthComponent();

	UPROPERTY(BlueprintAssignable, Category = "Health")
	FOnHealthChanged OnHealthChanged;

	UFUNCTION(BlueprintPure, Category = "Health") float GetHealth() const { return Health; }
	UFUNCTION(BlueprintPure, Category = "Health") float GetMaxHealth() const { return MaxHealth; }

	/** Server only. */
	UFUNCTION(BlueprintCallable, Category = "Health")
	void SetHealth(float NewHealth);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UFUNCTION()
	void OnRep_Health();

	UPROPERTY(ReplicatedUsing = OnRep_Health)
	float Health = 100.f;

	UPROPERTY(EditDefaultsOnly, Replicated, Category = "Health")
	float MaxHealth = 100.f;
};
```

```cpp
// MyGame/HealthComponent.cpp
#include "HealthComponent.h"
#include "Net/UnrealNetwork.h"

UHealthComponent::UHealthComponent()
{
	SetIsReplicatedByDefault(true);
}

void UHealthComponent::SetHealth(float NewHealth)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	Health = FMath::Clamp(NewHealth, 0.f, MaxHealth);
	OnHealthChanged.Broadcast(this, Health, MaxHealth);   // server and listen-server host
}

void UHealthComponent::OnRep_Health()
{
	OnHealthChanged.Broadcast(this, Health, MaxHealth);   // clients
}

void UHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UHealthComponent, Health);
	DOREPLIFETIME(UHealthComponent, MaxHealth);
}
```

Add `UHealthComponent` to your character Blueprint. A mana component follows the identical pattern; nothing about either is known to CoreFramework.

### 4.2 The bound widget (C++)

```cpp
// MyGame/HealthBarWidget.h
#pragma once

#include "CoreMinimal.h"
#include "UISystem/BaseUserWidget.h"
#include "HealthBarWidget.generated.h"

class UHealthComponent;
class UProgressBar;

UCLASS(Abstract, Blueprintable)
class MYGAME_API UHealthBarWidget : public UBaseUserWidget
{
	GENERATED_BODY()

public:
	//~ IBaseUIBinderInterface
	virtual void OnUIPawnBound_Implementation(APawn* Pawn) override;
	virtual void OnUIPawnUnbound_Implementation(APawn* Pawn) override;

protected:
	/** Optional: a Progress Bar named HealthBar. Blueprint children can use the event instead. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> HealthBar;

	UFUNCTION(BlueprintImplementableEvent, Category = "Health", meta = (DisplayName = "On Health Changed"))
	void BP_OnHealthChanged(float Normalized, float Current, float Max);

private:
	UFUNCTION()
	void HandleHealthChanged(UHealthComponent* Component, float NewHealth, float MaxHealth);

	/** What we subscribed to. The Unbound pawn can already be destroyed, so never rely on the argument. */
	TWeakObjectPtr<UHealthComponent> BoundHealth;
};
```

```cpp
// MyGame/HealthBarWidget.cpp
#include "HealthBarWidget.h"
#include "Components/ProgressBar.h"
#include "GameFramework/Pawn.h"
#include "HealthComponent.h"

void UHealthBarWidget::OnUIPawnBound_Implementation(APawn* Pawn)
{
	UHealthComponent* Health = Pawn ? Pawn->FindComponentByClass<UHealthComponent>() : nullptr;
	if (!Health)
	{
		return;   // this pawn has no health: the widget simply stays idle
	}

	BoundHealth = Health;
	Health->OnHealthChanged.AddUniqueDynamic(this, &UHealthBarWidget::HandleHealthChanged);

	// Delegates only fire on change: push the current value once.
	HandleHealthChanged(Health, Health->GetHealth(), Health->GetMaxHealth());
}

void UHealthBarWidget::OnUIPawnUnbound_Implementation(APawn* Pawn)
{
	if (UHealthComponent* Health = BoundHealth.Get())
	{
		Health->OnHealthChanged.RemoveDynamic(this, &UHealthBarWidget::HandleHealthChanged);
	}
	BoundHealth.Reset();

	if (HealthBar)
	{
		HealthBar->SetPercent(0.f);
	}
}

void UHealthBarWidget::HandleHealthChanged(UHealthComponent* Component, float NewHealth, float MaxHealth)
{
	const float Normalized = MaxHealth > 0.f ? NewHealth / MaxHealth : 0.f;
	if (HealthBar)
	{
		HealthBar->SetPercent(Normalized);
	}
	BP_OnHealthChanged(Normalized, NewHealth, MaxHealth);
}
```

Create `WBP_HealthBar` as a Blueprint child (Progress Bar named `HealthBar`, optional text, optional `Anim_Show`).

### 4.3 Blueprint-only variant (no C++ widget)

`WBP_HealthBar` with parent **`BaseUserWidget`**:

1. Variables: `BoundHealth` (Health Component reference).
2. *Override* event **On UI Pawn Bound (Pawn)**: `Get Component By Class (Health Component)` → store in `BoundHealth` → `Bind Event to On Health Changed` → call your update function once with the current value.
3. *Override* event **On UI Pawn Unbound**: if `BoundHealth` is valid → `Unbind Event` → clear `BoundHealth`.

The event fires on each respawn automatically.

### 4.4 Show it on the HUD

1. In `WBP_Game_HUD` (child of `WBP_BaseHUD`), `Event Construct` registers the slot (Step 2.3).
2. In `DA_UIConfig` → **HUD Elements** add:

| Element Id | Slot Tag | Widget Class |
|---|---|---|
| `Health` | `Game.UI.HUDSlot.BottomLeft` | `WBP_HealthBar` |
| `Mana` | `Game.UI.HUDSlot.BottomLeft` | `WBP_ManaBar` |

Optional runtime alternative from game code (e.g. when a feature activates):

```cpp
FUIHUDElementDescriptor Element;
Element.ElementId = TEXT("Health");
Element.SlotTag = FGameplayTag::RequestGameplayTag(TEXT("Game.UI.HUDSlot.BottomLeft"));
Element.WidgetClass = TSoftClassPtr<UBaseUserWidget>(FSoftObjectPath(TEXT("/Game/UI/HUD/WBP_HealthBar.WBP_HealthBar_C")));
UI->RegisterHUDElement(Element);       // a live HUD spawns it immediately
// UI->UnregisterHUDElement(TEXT("Health"));
```

### 4.5 Variants

* **Attributes on the PlayerState** (ability system, replicated stats): override `OnUIPlayerStateBound / Unbound` instead of the pawn events; the PlayerState survives respawns, so you receive one bind per player state change.
* **Typed access in C++**: `GetBoundPawnAs<AMyCharacter>()` anywhere in a `UBaseUserWidget` / `UBaseActivatableWidget`.
* **Several widgets, one source**: each widget binds on its own; they do not know each other.

### 4.6 Verify

1. Play, damage the pawn (`SetHealth` from a test input): the bar follows.
2. Respawn / possess another pawn: the bar resets and rebinds with no extra code.
3. Multiplayer (2 clients, listen server): each client's bar follows its own pawn; late join shows the correct value immediately.

---

## Step 5 — Input and controller configuration

### 5.1 CommonUI prerequisites (once per game project)

1. **Viewport client**: Project Settings, search for **Game Viewport Client Class** (Engine, Default Classes) and set it to **`CommonGameViewportClient`** (or `Setup/DefaultEngine.ini.additions.txt`).
2. **UI actions table** `DT_UIActions` (Data Table, row structure **Common Input Action Data Base**):

| Row | Keyboard | Gamepad |
|---|---|---|
| `Confirm` | `Enter` | `Gamepad_FaceButton_Bottom` |
| `Back` | `Escape` | `Gamepad_FaceButton_Right` |
| `NextTab` | `E` | `Gamepad_RightShoulder` |
| `PreviousTab` | `Q` | `Gamepad_LeftShoulder` |

3. **Input data**: create a Blueprint class with parent **`CommonUIInputData`** (`BP_CommonInputData`). Set *Default Click Action* = row `Confirm`, *Default Back Action* = row `Back`.
4. **Project Settings → Game → Common Input Settings**: *Input Data* = `BP_CommonInputData`. In *Platform Input* (Windows and each console) set the default gamepad and add controller data assets if you want button glyphs.
5. *(Optional)* Enable **Enhanced Input Support** in the same page; then each `DT_UIActions` row can reference an `Input Action` asset, so UI actions share your rebinding source.
6. Make sure the project's *Default Player Input Class* / *Default Input Component Class* are the Enhanced Input classes (already the case if `ABaseCharacter` uses Enhanced Input).

### 5.2 Per-screen input settings

| Screen | Input Mode | Is Back Handler | Is Modal | Extra |
|---|---|---|---|---|
| HUD (`WBP_BaseHUD`) | Game Only | ✘ | ✘ | — |
| Main menu | UI Only | ✘ | ✘ | name first button `DefaultFocusWidget` |
| Pause menu | UI Only | ✔ | ✘ | *Pause Game While Active* ✔ |
| Settings | UI Only | ✔ | ✘ | `TabList` next / previous actions set |
| Popup / confirm | UI Only | ✔ | ✔ | push on the Modal layer |
| Quick inventory over live gameplay | Game And UI | ✔ | ✘ | *Ignore Look Input While Active* ✔ |
| Toast / notification | Keep Current | ✘ | ✘ | Overlay layer |

"Is Back Handler" and "Is Modal" are CommonUI properties in the Blueprint's **Class Defaults**.

### 5.3 What happens automatically

| Event | Result |
|---|---|
| A `UIOnly` screen activates | UI-only input, cursor visible, game input blocked |
| That screen closes | Input configuration of the next topmost screen (HUD → Game, cursor hidden) |
| Last input device changes (mouse ↔ keyboard ↔ gamepad) | CommonUI switches cursor visibility and focus handling; `IsUsingGamepad()` reflects it; glyph widgets update |
| Escape / gamepad B on a Back Handler | The topmost back handler closes |
| Shoulder buttons / `Q` `E` in Settings | Previous / next tab |

### 5.4 Test matrix

| # | Action | Expected |
|---|---|---|
| 1 | Open pause menu with keyboard | Cursor visible, Resume focused, camera does not turn |
| 2 | Move the mouse, click Settings | Settings opens on top, first tab selected |
| 3 | Press a gamepad button | Cursor hides, focus ring appears on the focused button |
| 4 | D-pad down / up | Focus moves between buttons |
| 5 | Shoulder buttons in Settings | Tab changes |
| 6 | B / Escape | Settings closes, then pause menu closes, game input returns |
| 7 | Move the mouse again | Cursor reappears, hover works |
| 8 | Open a popup from a menu | Menu below ignores input until the popup closes |

### 5.5 Troubleshooting

| Symptom | Cause | Fix |
|---|---|---|
| Nothing appears | No Default UI Config / no Root Layout Class / HUD class not set | Check Output Log for `LogCoreUI` messages; Step 3.2–3.4 |
| `no screen registered for tag` | Screen row missing in `DA_UIConfig.Screens` | Add the row; tag must match exactly |
| `layer … is not registered` | Stack name typo in the root layout | Names must be exactly `GameLayerStack`, `MenuLayerStack`, `ModalLayerStack`, `OverlayLayerStack` |
| Back / Escape does nothing | `Is Back Handler` off, or no Default Back Action | Step 5.1 (3), class defaults |
| Gamepad cannot move focus | Nothing focusable / no focus target | Name a widget `DefaultFocusWidget`; use Common Buttons |
| Mouse blocked by empty HUD | A stack not set to *Self Hit Test Invisible* | Step 2.1 |
| Tab bar empty | `Tab Button Class` unset, no tab source, or tab widget class not a `BaseTabWidget` | Step 2.5; check `LogCoreUI` |
| HUD element missing | Slot never registered | `Register Element Slot` in `Event Construct`, or use `DefaultElementSlot` |
| Widget does not rebind after respawn | Unbound override missing the unsubscription, or `Auto Register As Binder` off | Section 4.2 pattern |
| Input stays blocked after closing a screen | A screen was destroyed without deactivating on an old build | Use `CloseScreen`; the framework releases suppression in a destruct safety net |

---

## Final checklist

- [ ] Plugin compiles; Project Settings page and tags visible (1.4)
- [ ] `WBP_BaseRootLayout`, `WBP_BaseHUD`, `WBP_BaseMainMenu`, `WBP_BaseSettings`, tab button and tab template exist (Step 2)
- [ ] Viewport client + CommonUI input data configured (5.1)
- [ ] `DA_UIConfig` assigned in Project Settings with root layout and screens (3.3)
- [ ] `BP_GameHUD` / `BP_MenuHUD` set as HUD class per GameMode (3.4)
- [ ] Smoke test passes (3.6)
- [ ] First gameplay widget bound through `OnUIPawnBound` (Step 4)
- [ ] Test matrix passes with mouse, keyboard and gamepad (5.4)
