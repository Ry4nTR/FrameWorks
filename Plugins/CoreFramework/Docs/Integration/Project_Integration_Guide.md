# CoreFramework — Master Architectural Guide

**File:** `Docs/Integration/Project_Integration_Guide.md`
**Engine:** Unreal Engine 5.8 · **Plugin:** `CoreFramework` (export macro `COREFRAMEWORK_API`) · **Branch imported by projects:** `coreframework-branch`
**Purpose:** the primary reference for building a game project on CoreFramework. Attach it to a new chat together with the project name and the assistant knows which C++ classes and Blueprints to create, what they inherit from, and where every file goes.

> **Status.** Written from the repository `Ry4nTR/FrameWorks` at commit `5b795d5` and from the first game project built with it (`SunsetCove`). The C++ patterns of section 5 were syntax-checked against the real `BaseCharacter.h` with stub engine headers. Nothing was compiled with Unreal Engine and nothing was run in the editor.

---

## 0. Instructions for AI assistants (read first)

When this guide is attached and the user names a project, call it `<Project>` (example: `ProjectX`). Derive: `<PROJECT>` = upper case (`PROJECTX`), export macro `<PROJECT>_API`, module folder `Source/<Project>/`.

1. **Read the framework before writing code.** The source of truth is the repository (`https://github.com/Ry4nTR/FrameWorks`, branch `main`, folder `Plugins/CoreFramework/`). Read at least `Source/CoreFramework/Public/Character/BaseCharacter.h` and `Public/UISystem/Manager/UIFrameSubsystem.h`. If the project already exists, also read its `Source/`, `Config/*.ini` and `.gitmodules` (the submodule pointer says which framework commit the project really uses). If you cannot open a repository, **do not guess**: ask the user to paste the headers and the folder tree.
2. **Generate the project C++ classes** of section 3, step 3, as **complete files** (never partial snippets), following the contracts in section 5.
3. **Split every class** into `Public/<Area>/<Name>.h` and `Private/<Area>/<Name>.cpp`, in the macro-area folders of section 4. Includes are relative to `Public/` (`#include "Character/<Project>Character.h"`).
4. **Commit to one solution.** No menus of alternatives.
5. **Mark project logic** with `// <PROJECT> GAME LOGIC` comments at every hook.
6. **Do not use:** templates, generation scripts, a framework `BaseGameMode`, `BaseGameInstance` or `BasePlayerController`, camera components on the character, `Set Input Mode` calls next to the UI framework, game rules or game stats inside `CoreFramework`.
7. **Say plainly** what was not compiled or tested.
8. **Known drift in the older framework docs** (trust the headers, not the docs): some documents still say `FRAMEWORKS_API` (the macro is `COREFRAMEWORK_API`); `Character_Architecture_Overview.md` sections 3 and 6 and `Character_Blueprint_Setup.md` Step 6 still describe the first camera design (cameras as components of the character), while the code creates the camera rig at runtime in `UCameraManagerComponent` and there is no camera document yet; `UI_HUD_Setup_Guide.md` 3.1 names the branch `plugin-only` (the real branch is `coreframework-branch`).

---

## 1. Inheritance architecture and mindset

```
Layer 1  CoreFramework C++        ABaseCharacter, ABaseHUD, UUIFrameSubsystem, UBaseCharacterMovementComponent,
                                  UCameraManagerComponent, UStateMachineComponent, UAnimationInstance ...
            ^ inherits
Layer 2  Game project C++         A<Project>Character : ABaseCharacter
                                  A<Project>GameplayGameMode : AGameModeBase   (native class: no framework parent)
                                  A<Project>MainMenuGameMode : AGameModeBase   (native class: no framework parent)
                                  U<Project>GameInstance : UGameInstance       (native class: no framework parent)
                                  A<Project>PlayerController : APlayerController (native class: no framework parent)
            ^ is the parent of
Layer 3  Game project Blueprints  BP_Character, BP_GameMode_Gameplay, BP_GameMode_MainMenu, BP_GameInstance,
                                  BP_PlayerController
```

**Rule 1 — Frameworks hold mechanisms.** Framework classes (`ABaseCharacter`, `UUIFrameSubsystem`, ...) contain genre-agnostic mechanisms: Enhanced Input routing, `CameraManager` runtime rigs, `StateMachine` tag drivers, Movement domains, UI layers and screens.

**Rule 2 — Inherit a framework class when one exists.** If CoreFramework has a parent class for a system (the character), the project C++ class **must** inherit from it: `A<Project>Character : public ABaseCharacter`. Never copy framework code into the project.

**Rule 3 — Inherit the native class when none exists.** GameMode, GameInstance and PlayerController have no framework parent, so the project classes inherit the Unreal classes directly: `AGameModeBase`, `UGameInstance`, `APlayerController`. **Why these three never get a framework base:** a project has exactly one class of each, so a framework base would block every other framework that needs its own. Reusable behavior that repeats across projects becomes a **component or a subsystem** (`UGameInstanceSubsystem` for the GameInstance), never a parent class.

**Rule 4 — No game rules in CoreFramework.** No game stats, no match rules, no screens, no art. Code moves into the framework only after it repeats in the 2nd–3rd project, and then as a component, subsystem, data asset or interface.

**Rule 5 — Organize by macro area of competence.** Scripts live in area folders (`Core`, `Character`, `Player`, `UI`, `Gameplay`, ...), mirrored in `Public/` and `Private/`. Section 4 is the layout.

**Rule 6 — Blueprints are configuration.** Game Blueprints are children of the **project's** C++ classes, never of the framework's directly. One exception, because the project has no C++ to add: **UI assets** are children of the framework classes (`BP_Startup_*` from `ABaseHUD`, screens from `BaseHUDWidget` / `BaseMenuWidget` / `BaseActivatableWidget`). Create `A<Project>HUD : ABaseHUD` only if the HUD itself needs C++ code.

---

## 2. Core components: connections and references

```
Project Settings --> Game Instance Class --> BP_GameInstance (U<Project>GameInstance)   lives for the whole run
World Settings --> GameMode Override --> BP_GameMode_Gameplay | BP_GameMode_MainMenu    server only, one per map
      Class Defaults: Default Pawn Class ......> BP_Character      (A<Project>Character : ABaseCharacter)
                      HUD Class ...............> BP_Startup_*      (ABaseHUD) starts the UI on the local machine
                      Player Controller Class .> BP_PlayerController   pause key -> UUIFrameSubsystem
UUIFrameSubsystem (ULocalPlayerSubsystem) <-- ABaseHUD::BeginPlay                       local player only, never on a dedicated server
```

### 2.1 Character

* **C++:** `A<Project>Character : public ABaseCharacter` (`#include "Character/BaseCharacter.h"`). **Blueprint:** `BP_Character`.
* The framework already provides Enhanced Input routing (`UInputConfigData`), movement domains and predicted actions, the runtime camera rig, perspective, replication of state tags, and the `StateMachine` driver. The project class only adds game logic through overrides and hooks (section 5.1).
* `BP_Character` is **not** a child of the plugin's `BP_BaseCharacter` (a Blueprint has one parent). Open `BP_BaseCharacter` as a reference and set the same things in your Blueprint: `Input Config`, the skeletal mesh and `Anim Class`, the `FirstPersonMesh` animation, and the cameras in the `CameraManager` component.
* **References:** `Docs/Character/Character_Architecture_Overview.md` and `Docs/Character/Character_Blueprint_Setup.md` for the full character, camera and input details; also `Docs/Movement/`, `Docs/StateMachine/`, `Docs/Animation/`. In the Blueprint setup document, create your Blueprint from `A<Project>Character` where it says to create it from the base character.

### 2.2 GameMode (two classes)

* **C++:** `A<Project>GameplayGameMode : public AGameModeBase` and `A<Project>MainMenuGameMode : public AGameModeBase`. **Blueprints:** `BP_GameMode_Gameplay` and `BP_GameMode_MainMenu`.
* **Why two classes:** a menu map has no character and no match rules. A separate class keeps the gameplay GameMode free of menu cases and guarantees that the menu never spawns a pawn. The two share nothing, so they share no parent either.
* Server only. In **Class Defaults → Classes**:

| Property | `BP_GameMode_Gameplay` | `BP_GameMode_MainMenu` |
|---|---|---|
| `Default Pawn Class` | `BP_Character` | **None** (the C++ constructor already sets it) |
| `HUD Class` | `BP_Startup_Gameplay` | `BP_Startup_MainMenu` |
| `Player Controller Class` | `BP_PlayerController` | left as the engine default (the menu has no pause key) |

* The gameplay class sets the project's C++ pawn and controller in its constructor as a safety net; the Blueprint values win.
* A map picks its GameMode in **World Settings → Game Mode → GameMode Override**.
* Match rules live in the gameplay class and are game-specific. UI and client code never read a GameMode: shared data goes in the GameState or PlayerState.

### 2.3 GameInstance

* **C++:** `U<Project>GameInstance : public UGameInstance`. **Blueprint:** `BP_GameInstance`, assigned in **Project Settings → Maps & Modes → Game Instance Class**.
* Holds what must outlive a map (profile, settings, session handles). It never holds UI: the UI lives on the local player.
* **Reusable logic shared by several projects must be a `UGameInstanceSubsystem`, never a subclass** of `UGameInstance`, so frameworks can be combined freely.

### 2.4 PlayerController

* **C++:** `A<Project>PlayerController : public APlayerController`. **Blueprint:** `BP_PlayerController`.
* **What it is for, in plain words.** The pawn is the body, the controller is the person driving it. The body can die and be replaced, the controller stays. So the controller owns everything that must keep working without a body: Pause, menu keys, scoreboard, chat. Input about the body (move, look, jump) stays in the character.
* **What it needs:** a mapping context with its own actions (Pause) and the link to the UI. That link is `UUIFrameSubsystem::GetFromController(this)` followed by `PushScreen(Tag)` / `PopScreen(Tag)` / `IsScreenOpen(Tag)`. It never calls `Set Input Mode` and never pauses the game itself: the **screen** decides (`Input Mode`, `Pause Game While Active`, `Is Back Handler` in its Class Defaults).
* **Why there is no framework PlayerController.** A project has one controller class, so a framework base would block every other framework that needs its own. The only part every game repeats is the pause toggle (about 40 lines), and it has not yet repeated in a second project. If it does, it becomes a component that any controller can add (Rule 4), never a base class.

### 2.5 UI / HUD system

* **Startup actors:** Blueprint children of `ABaseHUD` (`BP_Startup_Gameplay`, `BP_Startup_MainMenu`), one per map type. The list `Startup Screens` (screen gameplay tags) says what opens first. The GameMode's `HUD Class` selects the startup actor, and `ABaseHUD::BeginPlay` calls `UUIFrameSubsystem::InitializeForPlayerController`.
* **`UUIFrameSubsystem`** is a `ULocalPlayerSubsystem`: it owns the root layout, the screen registry and the binding hub that tells widgets when the controller, player state and pawn change. Its lifetime is the lifetime of the HUD actor, so **screens do not survive map travel** (every map type needs a GameMode whose `HUD Class` is set).
* **References:** `Docs/UISystem/UI_HUD_Setup_Guide.md` (step by step) and `Docs/UISystem/UI_HUD_System_Architecture.md` (internals).

---

## 3. Step-by-step new project setup

### Step 1 — Add the submodule

```powershell
git init
if (!(Test-Path "Plugins")) { New-Item -ItemType Directory -Path "Plugins" }
git submodule add -b coreframework-branch https://github.com/Ry4nTR/FrameWorks.git Plugins/CoreFramework
git submodule update --init --recursive
```

Enable the plugin in the `.uproject` (the build cannot find the module otherwise):

```json
"Plugins": [
	{ "Name": "CoreFramework", "Enabled": true }
]
```

The project uses the framework commit that the submodule pointer names, not the branch head. Update it on purpose (`git submodule update --remote Plugins/CoreFramework`, then commit the pointer).

### Step 2 — Configure `<Project>.Build.cs`

Add `"CoreFramework"`, `"GameplayTags"` and `"EnhancedInput"` to `PublicDependencyModuleNames`, next to the modules the project already has:

```csharp
using UnrealBuildTool;

public class <Project> : ModuleRules
{
	public <Project>(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "InputCore",
			"EnhancedInput",
			"GameplayTags",
			"CoreFramework"
		});
	}
}
```

The module's `Public/` and `Private/` folders are on the include path automatically.

### Step 3 — Create the project C++ classes (manually)

Create each class in the IDE or with *Tools → New C++ Class*, **in its area folder** (section 4), then regenerate the project files. Complete contracts are in section 5.

| Class | Parent | Header and source | Includes the framework header |
|---|---|---|---|
| `A<Project>Character` | `ABaseCharacter` | `Public/Character/<Project>Character.h`, `Private/Character/<Project>Character.cpp` | `Character/BaseCharacter.h` |
| `A<Project>GameplayGameMode` | `AGameModeBase` | `Public/Core/<Project>GameplayGameMode.h`, `Private/Core/<Project>GameplayGameMode.cpp` | none |
| `A<Project>MainMenuGameMode` | `AGameModeBase` | `Public/Core/<Project>MainMenuGameMode.h`, `Private/Core/<Project>MainMenuGameMode.cpp` | none |
| `U<Project>GameInstance` | `UGameInstance` | `Public/Core/<Project>GameInstance.h`, `Private/Core/<Project>GameInstance.cpp` | none |
| `A<Project>PlayerController` | `APlayerController` | `Public/Player/<Project>PlayerController.h`, `Private/Player/<Project>PlayerController.cpp` | `UISystem/Manager/UIFrameSubsystem.h` |
| `Log<Project>` (log category) | none | `Public/Core/<Project>Log.h`, `Private/Core/<Project>Log.cpp` | none |

### Step 4 — Create the matching Blueprints (in the project's `Content/`)

Right-click in the Content Browser → **Blueprint Class → All Classes** → search the C++ class. Enable **Show Plugin Content** (Content Browser → Settings) to see the plugin's assets.

| Blueprint | Folder | Parent | Class Defaults to set |
|---|---|---|---|
| `BP_Character` | `Character/Blueprints/` | `A<Project>Character` | `Input Config` (plugin asset `DA_InputConfig_Default` or your own), skeletal mesh and `Anim Class`, `FirstPersonMesh` animation, cameras in `CameraManager`. Reference: the plugin's `BP_BaseCharacter` |
| `BP_GameMode_Gameplay` | `Core/` | `A<Project>GameplayGameMode` | see 2.2 |
| `BP_GameMode_MainMenu` | `Core/` | `A<Project>MainMenuGameMode` | see 2.2 |
| `BP_GameInstance` | `Core/` | `U<Project>GameInstance` | nothing required |
| `BP_PlayerController` | `Player/` | `A<Project>PlayerController` | `Pause Action` = `IA_Pause`, `Player Mapping Context` = `IMC_Player`, `Pause Screen Tag` = `UI.Screen.PauseMenu` |
| `BP_Startup_Gameplay`, `BP_Startup_MainMenu` | `UI/Startup/` | `ABaseHUD` | `Startup Screens`: `[UI.Screen.HUD]`, `[UI.Screen.MainMenu]` |

Create `IA_Pause` (*Input → Input Action*, value type **Digital (bool)**, no triggers) and `IMC_Player` (*Input → Input Mapping Context*, `IA_Pause` on **Escape** and **Gamepad Special Right**) in `Content/Player/Input/`. In each map: **Window → World Settings → Game Mode → GameMode Override** = the matching GameMode.

**Blueprints that already exist with another parent** (for example GameModes created from the retired `BP_BaseGameMode`): open the Blueprint → toolbar **Class Settings** → **Details → Class Options → Parent Class** → pick the new C++ class → **Compile** → **Save**. A child Blueprint stores only the values it overrides, so inherited values disappear: note `Default Pawn Class`, `HUD Class` and `Player Controller Class` first and check them afterwards. Do this **before** the plugin drops the old Blueprint, otherwise the GameMode loads with a missing parent. Maps that pointed at a renamed GameMode may hold a redirector: right-click the folder → **Fix Up Redirectors in Folder**, then re-save those maps.

### Step 5 — Project Settings (CommonUI and the UI config)

**Edit → Project Settings**, use the search box:

| Search | Property | Value |
|---|---|---|
| `viewport client` | **Game Viewport Client Class** | `CommonGameViewportClient` (restart when asked) |
| `Common Input` | **Input Data** | `BP_CommonInputData` (plugin asset) |
| `CoreFramework UI` | **Default UI Config** | `DA_UIConfig` (your asset, created with `UI_HUD_Setup_Guide.md` 3.6) |
| `game instance` | **Game Instance Class** | `BP_GameInstance` |
| `Game Default Map`, `Default GameMode` | **Game Default Map**, **Default GameMode** | the main-menu map, `BP_GameMode_Gameplay` |

Create the screen gameplay tags (`UI.Screen.HUD`, `UI.Screen.MainMenu`, `UI.Screen.PauseMenu`, ...) and the screens with `UI_HUD_Setup_Guide.md` (3.2 explains the first two settings, 3.4–3.6 tags, styles and `DA_UIConfig`, Step 4 the screens). Skip its 3.7 and 4.7: Step 4 above creates the startup actors and GameModes with the project's own C++ parents, and `A<Project>PlayerController` already opens the pause menu.

**Done when:** the main-menu map shows the menu with no character; the gameplay map spawns `BP_Character` with the HUD; Escape opens and closes the pause menu; the Output Log (filter `LogCoreUI` and `Log<Project>`) shows no warnings.

---

## 4. Project folder organization (macro areas of competence)

One folder per macro area, mirrored in `Public/` and `Private/`. Create an area folder only when its first class exists.

```
Source/<Project>/
|-- <Project>.Build.cs, <Project>.cpp/.h        module files
|-- Public/                                     headers (includes are relative to this folder)
|   |-- Core/        game flow and rules: GameMode(s), GameState, GameInstance, module log
|   |-- Character/   the playable character (A<Project>Character) and classes bound to the pawn
|   |-- Player/      PlayerController, PlayerState, controller-level input
|   |-- UI/          C++ classes for widgets or HUD, only when a screen needs C++
|   |-- Gameplay/    game systems, ONE sub-folder per system (Health/, Inventory/, Weapons/, Interaction/ ...)
|   |-- World/       placed actors, volumes, level-bound classes
|   |-- AI/          AI controllers, tasks, perception
|   `-- Data/        data assets, structs, enums, native gameplay tags
`-- Private/                                    sources: the SAME sub-folders as Public/

Content/                                        the project's own Content root, areas mirror the code
|-- Core/  Character/ (Blueprints/)  Player/ (Input/)  UI/ (Startup/, Config/, Components/)  Gameplay/  Levels/  Data/
```

**Rules**

1. **Mirror.** `Public/<Area>/X.h` and `Private/<Area>/X.cpp` always share the same sub-folder.
2. **One class per file pair.** The file name is the class name without its prefix (`ASunsetCoveCharacter` → `SunsetCoveCharacter.h`).
3. **The project name prefixes every C++ class and file** (`SunsetCove...`). This also keeps include paths unique: includes are relative to the `Public/` folder of every module, so a project file must never be named like a framework file (never `Character/BaseCharacter.h`). Blueprints do not carry the project name: they live in the project's own Content (`BP_Character`).
4. **Place by responsibility.** Ask what the class is *about*: the rules of a map → `Core`; the body → `Character`; the person at the keyboard → `Player`; screens → `UI`; a game system → `Gameplay/<System>`; things placed in a level → `World`; data → `Data`.
5. **Systems are components.** Health, inventory, weapons and interaction are components or objects in `Gameplay/<System>/`. The character creates them and forwards events; it never contains their logic.
6. **Content follows code.** Blueprint names `BP_`, widgets `WBP_`, data assets `DA_`, input `IA_` / `IMC_`, levels `L_`, animation `ABP_`. A Blueprint sits in the Content folder of the area of its C++ parent.

---

## 5. Class contracts (what each project class contains)

### 5.1 `A<Project>Character : ABaseCharacter`

A reference implementation exists for the first project (`ASunsetCoveCharacter`); when it is attached, copy its structure exactly.

* `UCLASS(Blueprintable) class <PROJECT>_API A<Project>Character : public ABaseCharacter`, with `#include "Character/BaseCharacter.h"`.
* **Constructor `(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)`.** Mandatory: the base constructor replaces the movement component class through the initializer.
* **Overrides of existing base virtuals** (verify the names in `BaseCharacter.h`): `PostInitializeComponents`, `BeginPlay`, `EndPlay`, `Tick`, `GetLifetimeReplicatedProps`, `PawnClientRestart`, `PossessedBy`, `UnPossessed`, `SetupPlayerInputComponent`, `UpdateStateMachine`, and the protected handlers `Input_Move`, `Input_Look`, `Input_JumpStarted`, `Input_JumpHeld`, `Input_JumpEnded`, `Input_CrouchStarted`, `Input_CrouchHeld`, `Input_CrouchEnded`, `Input_SprintStarted`, `Input_SprintEnded`, `Input_Fly`, `Input_Dash`, `Input_Dodge`, `Input_PerspectiveToggle`.
* **Input handler pattern:** code before `Super::` may veto the input by returning; code after `Super::` reacts. The three `...Ended` handlers always call `Super::`.
* **New hooks of the project class:** `Setup<Project>Input(UEnhancedInputComponent*)` (called after `Super::SetupPlayerInputComponent`), `On<Project>PerspectiveChanged` (a `UFUNCTION()` bound with `OnPerspectiveChanged.AddUniqueDynamic` in `PostInitializeComponents`), `On<Project>MovementActionStarted` (bound with `GetBaseMovement()->OnActionStarted.AddUObject` in `PostInitializeComponents`).
* **Optional project input context:** `<Project>MappingContext` plus `<Project>MappingPriority` (default 1), added in `PawnClientRestart` after `Super::PawnClientRestart()` with remove-then-add.
* Never re-implement movement, input binding, state machine or camera logic.

### 5.2 `A<Project>GameplayGameMode` and `A<Project>MainMenuGameMode : AGameModeBase`

Both `UCLASS(Blueprintable)`. Header comment: what a GameMode is (server only), the wiring of 2.2, how a map selects it.

* **Gameplay:** constructor sets `DefaultPawnClass = A<Project>Character::StaticClass()` and `PlayerControllerClass = A<Project>PlayerController::StaticClass()` (safety net, the Blueprint wins); it does **not** set `HUDClass`. Overrides `InitGame`, `PostLogin`, `Logout`, each calling `Super::`, with `// <PROJECT> GAME LOGIC` hooks for the match rules. Need match states? Change the parent to `AGameMode` and the GameState to `AGameState`.
* **Main menu:** constructor sets `DefaultPawnClass = nullptr` and nothing else. No overrides unless the menu needs server logic.

### 5.3 `U<Project>GameInstance : UGameInstance`

`UCLASS(Blueprintable)`. Overrides `Init`, `OnStart`, `Shutdown` (each calls `Super::`). Comment block: what belongs here, why frameworks only add `UGameInstanceSubsystem`, and that the UI is not stored here.

### 5.4 `A<Project>PlayerController : APlayerController`

`UCLASS(Blueprintable)`. Properties (all `EditDefaultsOnly`): `PlayerMappingContext` (`UInputMappingContext`), `MappingPriority`, `PauseAction` (`UInputAction`), `PauseScreenTag` (`FGameplayTag`, set in the Blueprint, never hardcoded). `SetupInputComponent`: call `Super::`, return unless `IsLocalPlayerController()`, add the mapping context through `UEnhancedInputLocalPlayerSubsystem`, bind `PauseAction` with `ETriggerEvent::Started` on the `UEnhancedInputComponent`. `TogglePauseScreen` (`BlueprintCallable`): get `UUIFrameSubsystem::GetFromController(this)`, then `IsScreenOpen(PauseScreenTag)` ? `PopScreen` : `PushScreen`; log a warning when the tag, the action, the context or the subsystem is missing. Header comment: the plain-words explanation of 2.4. Include `UISystem/Manager/UIFrameSubsystem.h`.

### 5.5 Module log

`Public/Core/<Project>Log.h`: `<PROJECT>_API DECLARE_LOG_CATEGORY_EXTERN(Log<Project>, Log, All);` and `Private/Core/<Project>Log.cpp`: `DEFINE_LOG_CATEGORY(Log<Project>);`.
