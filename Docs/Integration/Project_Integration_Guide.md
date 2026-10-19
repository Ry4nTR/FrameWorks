# CoreFramework — Project Integration Guide

**Document:** `Project_Integration_Guide.md` (plugin path `Docs/Integration/`)
**Companions:** `Docs/UISystem/UI_HUD_Setup_Guide.md` and `UI_HUD_System_Architecture.md`, the `Character`, `Movement`, `StateMachine` and `Animation` folders of `Docs/`, `Docs/HOW-TO-IMPORT-SUBMODULE.md`, and the starter kit in `Templates/` (its `README.md` explains the rename).
**Engine target:** Unreal Engine 5.8
**Written for:** someone who is still learning UMG and CommonUI. Every editor action says **where** to click, the **exact property name** and **why**.

> **Status.** Written by reading the repository at commit `849ee25` ("Prepared UI System"). **Nothing was compiled and nothing was run in the editor**, including the C++ of the starter kit and the rename script. Appendix B lists what was checked against the source and what was not.

## Contents

* **0. Read this first** — what this guide is, the decisions it follows, the badges
* **1. Who owns what** — one table, one diagram, the rules that follow
* **2. How the frameworks connect** — UI ↔ GameMode, PlayerController, Character; Character ↔ Camera / Movement / StateMachine / Animation; map travel; pause; multiplayer
* **3. New-project checklist** — 20 steps in order, with badges
* **4. Framework or project?** — the decision table and the promotion rule
* **5. Re-parenting existing GameModes and deleting `BP_BaseGameMode` safely**
* **6. Common mistakes and troubleshooting**
* **Appendix A** — map of the starter kit · **Appendix B** — what was and was not verified

---

# 0. Read this first

## 0.1 What this guide is for

Each framework has its own document and each one works alone. This guide answers the questions that appear when you put them in **one game project**: which object owns what, how the frameworks find each other, in which order a new project is set up, and where new code belongs.

## 0.2 The decisions this guide follows

1. **No `BaseGameMode` and no `BaseGameInstance` in CoreFramework.** A project has exactly one GameInstance class, so a framework base class would block every other framework. Match rules differ per game, so a GameMode base class would only be in the way.
2. **`BP_BaseGameMode` is deleted.** Project GameModes derive from *Game Mode Base* (or *Game Mode*) directly. In this guide the GameMode Blueprints use the project's own C++ skeleton `AMyGameMode` as parent. That class belongs to the game and itself derives from `AGameModeBase`, so no framework class sits in the chain.
3. **A reusable GameInstance need becomes a `UGameInstanceSubsystem`, never a subclass.** None exists today and none was built.

No technical reason to contradict these decisions was found while reading the code. The UI framework already follows them: it hangs on the *local player* (`ULocalPlayerSubsystem`), not on the GameInstance.

## 0.3 The badges

| Badge | Meaning |
|---|---|
| `FRAMEWORK REPO · once` | Done one time in the CoreFramework repository (`FrameWorks`). Every game gets the result by updating the plugin. |
| `GAME PROJECT · once per project` | Done one time for each new game that imports the plugin. |
| `GAME PROJECT · per screen` | Day-to-day work: a screen, a HUD element, a tab. |

## 0.4 Words used in this guide

| Word | Meaning |
|---|---|
| **Local player** | A person playing on *this* machine (`ULocalPlayer`). Split-screen has several. A dedicated server has none. |
| **Owning client** | The machine of the player who owns an object (the player's own `PlayerController`, for example). |
| **Startup actor** | A Blueprint child of `ABaseHUD`. An invisible actor whose only job is to start the UI and open the first screens of a map type. |
| **Hard travel** | Changing map with *Open Level*, or a server travel without seamless travel. Almost everything is destroyed and rebuilt. |
| **Seamless travel** | Changing map through a transition map with `bUseSeamlessTravel` on. Some objects are carried over. |
| **Class Defaults** | The properties of the Blueprint itself. Blueprint editor → toolbar button **Class Defaults**. |
| **Show Plugin Content** | Content Browser → **Settings** (gear) → tick it. Without it the plugin's assets are invisible. |

---

# 1. Who owns what

## 1.1 The table

| Object | Exists on | Chosen in | On map travel | Put here | CoreFramework piece |
|---|---|---|---|---|---|
| **GameInstance** | Server **and** every client (each has its own copy; never replicated) | *Project Settings → Maps & Modes → Game Instance Class* | Stays for the whole run | Data that outlives a map: profile, settings, session handles | None. Project class `UMyGameInstance`. Frameworks may only add a `UGameInstanceSubsystem` |
| **GameMode** | **Server only** (dedicated, listen host, standalone). Never on a client | *World Settings → GameMode Override*, else *Project Settings → Maps & Modes → Default GameMode* | New one per map | Rules of one map type: who spawns, how a match flows | None. Project class `AMyGameMode`. It names `HUD Class`, `Default Pawn Class`, `Player Controller Class` |
| **GameState** | Server and **all** clients (replicated) | GameMode → `Game State Class` | New one per map | Match data every player must see | None |
| **PlayerController** | Server (one per player) and **that player's own client** only | GameMode → `Player Controller Class` | Hard: new. Seamless: carried over | Input that is not about the body (pause key); the link to the UI | `AMyPlayerController` (optional template) |
| **PlayerState** | Server and **all** clients (replicated) | GameMode → `Player State Class` | Hard: new. Seamless: copied into a new one | Per-player data other players may see | Observed by the UI binding hub |
| **Pawn / Character** | Server and all clients (owner = autonomous proxy, others = simulated proxies) | GameMode → `Default Pawn Class` | New one per spawn and per respawn | The body: movement, camera rig, state, animation | `ABaseCharacter` with Movement, Camera, StateMachine, Animation |
| **HUD actor** | **Local players only**: standalone, listen host, each client. **Never on a dedicated server** | GameMode → `HUD Class` | New one per map | Nothing visible. A startup signal | `ABaseHUD` child (`BP_Startup_*`) |
| **UI subsystem** (`UUIFrameSubsystem`) | **Local machine only**, one per local player | Automatic (engine creates it with the local player) | The subsystem stays. Root layout and open screens are torn down by the old HUD and rebuilt by the new one | Root layout, screen registry, HUD-element registry, binding hub | UI framework |

## 1.2 The diagram

```
PROCESS (every machine has its own)
+-- GameInstance ..................... server + every client, whole run, survives map travel
    +-- ULocalPlayer ................. local machine only (one per split-screen player)
        +-- UUIFrameSubsystem ........ local player only: root layout, registries, binding hub

WORLD (one per map; rebuilt on map travel)
+-- GameMode ......................... SERVER ONLY. Creates everything below by class name.
+-- GameState ........................ server + all clients (replicated)
+-- per player
    +-- PlayerController ............. server + that player's own client
    |   +-- HUD actor (BaseHUD child)  local machine only. NOT on a dedicated server.
    |   |     BeginPlay -> UUIFrameSubsystem::InitializeForPlayerController
    |   |        -> root layout -> Game / Menu / Modal / Overlay stacks -> screens
    |   +-- possesses --v
    +-- PlayerState .................. server + all clients (replicated)
    +-- Pawn / ABaseCharacter ........ server + all clients (replicated)
        +-- CameraManager ............ the camera rig is not created on a dedicated server
        +-- StateMachine ............. tags replicated to the other players
        +-- CharacterMovement ........ predicted on the owner, authority on the server
        +-- Mesh + AnimInstance ...... reads the StateMachine tags

LINKS (who knows whom)
  GameMode ---- class names only ----> HUD Class, Default Pawn Class, Player Controller Class,
                                       Game State Class, Player State Class
  UUIFrameSubsystem ---- observes ---> PlayerController, PlayerState, Pawn
                                       (it never owns them and never changes them)
  PlayerController ---- calls -------> UUIFrameSubsystem (PushScreen / PopScreen)
  ABaseCharacter ---- owns ----------> CameraManager, StateMachine, CharacterMovement, meshes
```

## 1.3 Rules that follow from the table

1. **The UI is local.** It lives on the local player, so it exists on a client and on a listen-server host, never on a dedicated server.
2. **The GameMode is server-only.** UI code and client code must never reach for it. Whatever a client needs to know goes into the GameState or the PlayerState.
3. **The UI observes, it never owns.** Gameplay objects expose delegates; widgets subscribe (section 2.3).
4. **The GameMode only names classes.** It does not create widgets and does not know about screens. The link to the UI is the *HUD Class* property.
5. **Anything that must survive a map change lives in the GameInstance or in a subsystem**, not in the GameMode, the PlayerController or the pawn.
6. **Frameworks do not subclass the "one per project" classes** (GameInstance, GameMode). They add components, subsystems and data assets.

---

# 2. How the frameworks connect

## 2.1 UI ↔ GameMode: the HUD Class is the startup actor

**The chain**

```
Map opens
  -> GameMode is created (server)                        World Settings > GameMode Override
  -> a local player joins; its PlayerController spawns the HUD actor of class "HUD Class"
  -> ABaseHUD::BeginPlay                                 only if the controller is local
  -> UUIFrameSubsystem::InitializeForPlayerController(PC, Startup Screens, this)
       loads DA_UIConfig, starts the binding hub, queues the Startup Screens,
       streams WBP_BaseRootLayout, creates it, then pushes the screens in list order
```

**What you set, and where**

| What | Where to click | Exact property | Value |
|---|---|---|---|
| Which startup actor a map type uses | GameMode Blueprint → **Class Defaults** → section **Classes** | `HUD Class` | `BP_Startup_Gameplay` or `BP_Startup_MainMenu` |
| Which screens open | Startup actor Blueprint → **Class Defaults** → search `Startup Screens` (category *Base UI*) | `Startup Screens` | `[UI.Screen.HUD]` or `[UI.Screen.MainMenu]` |
| Which GameMode a map uses | Open the map → **Window → World Settings** → section **Game Mode** | `GameMode Override` | `BP_GameMode_Gameplay` or `BP_GameMode_MainMenu` |
| Where the UI definition lives | **Edit → Project Settings** → search `CoreFramework UI` | `Default UI Config` | `DA_UIConfig` |

**Why one startup actor per map type.** The list `Startup Screens` answers "what opens when this kind of map starts". A main-menu map opens the menu, a gameplay map opens the HUD. A third map type (a lobby, for example) gets its own startup actor and its own GameMode.

**Rules**

* If you list several screens, put the **HUD first**. CommonUI uses the most recently activated screen to decide the input mode.
* One starter only. Do not also create the UI from a Level Blueprint or a character: you would get two root layouts.
* If a project already has its own `AHUD` subclass, it can skip `ABaseHUD` and call `UUIFrameSubsystem::InitializeForPlayerController` / `ReleasePlayerController` from its own HUD (the header of `ABaseHUD` says so).

## 2.2 UI ↔ PlayerController: the key belongs to the controller, the behavior to the screen

| Concern | Owner | Why |
|---|---|---|
| The **key** that opens the pause menu | `PlayerController` (`AMyPlayerController`, optional template) | It must work without a pawn, and it is not about the body |
| What pausing **means** (cursor, game input, pausing the world, Escape to close) | The **screen** (`WBP_PauseMenu`, class defaults) | CommonUI applies the configuration of the topmost active screen. Nobody calls `Set Input Mode` |
| Finding the UI from the controller | `UUIFrameSubsystem::GetFromController(PC)`. In Blueprint: **Get Local Player Subsystem** with class `UIFrameSubsystem` | The subsystem belongs to the local player |

**Input modes in this framework** (property `Input Mode` in the **Class Defaults** of every screen):

| Screen | Input Mode | Notes |
|---|---|---|
| HUD (`BaseHUDWidget`) | Game Only | Default of that class. Cursor hidden |
| Main menu, pause menu, settings | UI Only | Cursor visible, game input blocked |
| Popup | UI Only + `Is Modal` | Screens below stop reacting |
| Live overlay (quick inventory) | Game And UI + `Ignore Look Input While Active` | The mouse moves the cursor, not the camera |
| Toast | Keep Current | Must not steal input |

**What happens when the player presses the pause key**

```
Escape in gameplay (the HUD is the active screen, Game Only)
  -> Enhanced Input: IA_Pause, Started
  -> AMyPlayerController::HandlePauseAction -> TogglePauseScreen
  -> UUIFrameSubsystem::PushScreen(Pause Screen Tag)       (async; loads the class if needed)
  -> WBP_PauseMenu activates
       Input Mode = UI Only            -> cursor visible, game input blocked
       Pause Game While Active (on)    -> UGameplayStatics::SetGamePaused(true)
       Is Back Handler (on)            -> Escape / gamepad B now means "close"
Escape again
  -> CommonUI Back action (row of DT_UI_Actions) closes the screen
  -> the screen deactivates: the pause is released, the input mode returns to the HUD (Game Only)
```

Because game input is blocked while a *UI Only* screen is open, `IA_Pause` does **not** fire a second time. The screen closes itself. That is intended.

## 2.3 UI ↔ Character and PlayerState: the binding hub

* **One hub per local player** (`UUIBindingHub`, owned by the UI subsystem) watches *PlayerController → PlayerState* and *PlayerController → Pawn*.
* Every widget that is a child of `BaseUserWidget` or `BaseActivatableWidget` registers itself (the property `Auto Register As Binder`, on by default). It receives these events, in Blueprint under **Override**:

| Event | Fires when |
|---|---|
| `On UI Controller Bound` / `Unbound` | The local controller is known / goes away |
| `On UI Player State Bound` / `Unbound` | The player state arrives or changes |
| `On UI Pawn Bound` / `Unbound` | The pawn is possessed, respawned or lost |

* **Late arrival is handled.** A widget that registers after the pawn exists gets the *Bound* call at once. A pawn that replicates after the HUD exists is found by a safety-net timer (`Binding Poll Interval Seconds` in *Project Settings → CoreFramework UI*, default 0.2 s).
* **Every *Bound* is paired with exactly one *Unbound*.** Whatever you subscribed to in *Bound*, you drop in *Unbound*.
* On a **main-menu map there is no pawn**, so `On UI Pawn Bound` never fires there. Widgets must work without it.

**A worked example with something that already exists:** `ABaseCharacter` has the delegate `On Perspective Changed` (its header says "bind UI, crosshair or audio here"). To show a crosshair only in first person:

1. Create a widget Blueprint with parent **BaseUserWidget** (for example `WBP_Crosshair`) in `Content/UI/HUD/Elements/`.
2. **Graph** tab → in **My Blueprint → Functions → Override** choose **On UI Pawn Bound**. Drag from its *Pawn* pin → **Cast To BaseCharacter**. From the cast result, right-click → **Bind Event to On Perspective Changed**. Connect its event to a custom event that calls **Show Widget** / **Hide Widget**. Also read **Get Perspective** once, so the first state is right.
3. Override **On UI Pawn Unbound** → cast the same way → **Unbind Event from On Perspective Changed**.
4. Register the widget as a HUD element (`UI_HUD_Setup_Guide.md`, Step 5.4).

*Why the Unbound step:* after a respawn the old pawn is gone, and a forgotten subscription keeps the widget reacting to a dead object.

**Where gameplay data for the UI lives:** in components and delegates that **your game** adds to the pawn or the PlayerState. The binder interface carries no gameplay data on purpose. `UI_HUD_Setup_Guide.md`, Step 5, contains a complete C++ and Blueprint example.

## 2.4 Character ↔ Camera, Movement, StateMachine, Animation

`ABaseCharacter` is the **composition root**: it is the only class that knows all four frameworks. The others do not know each other.

```
Input (Enhanced Input, InputConfig data asset)
   |
   v
ABaseCharacter ---- creates ---> CameraManager (UCameraManagerComponent)
   |                             StateMachine (UStateMachineComponent)
   |                             FirstPersonMesh, uses UBaseCharacterMovementComponent
   |
   |  Move / Jump / Sprint / Fly ... --> the MOVEMENT component decides what they mean
   |  Look ----------------------------> CameraManager::AddLookInput
   |  Perspective (replicated state) --> CameraManager::SwitchToCamera(profile name)
   |  every frame (server + owner) ----> UpdateStateMachine -> StateMachine tags
   v
UAnimationInstance (in the Animation Blueprint) reads the StateMachine tags
```

| Link | How | Dependency |
|---|---|---|
| Character → Movement | `SetDefaultSubobjectClass` replaces the engine movement component. The component exposes facts (domain, control mode, water zone) and the event `OnActionStarted` | Movement knows nothing about StateMachine or Animation |
| Character → Camera | The component owns the spring arm and the camera, created at runtime. Profiles are data in its `Camera Profiles` map. The character maps *perspective* to a profile name (`First Person Camera Name`, `Third Person Camera Name`) | Camera depends only on the Engine module |
| Character → StateMachine | `UpdateStateMachine` pushes domain and locomotion tags in; actions come from `RequestAction` or from the movement event | StateMachine depends only on GameplayTags |
| StateMachine → Animation | The Anim Instance copies the tags in `NativeUpdateAnimation` and derives its variables on the worker thread. Without a state machine it derives a basic state from movement | Animation depends on the tag vocabulary |
| Character → UI | Delegates such as `On Perspective Changed`; the UI subscribes (2.3). The character never calls the UI | None in that direction |

**What you do in a project** (details in `Character_Blueprint_Setup.md`, `Animation_Blueprint_Setup.md`, `Movement_Blueprint_Setup.md`):

* Create a **child Blueprint** of the plugin's `BP_BaseCharacter` for your game. Set its meshes and its Animation Blueprint (parent class `AnimationInstance`).
* `Input Config` (category *Character|Input*) points at a `UInputConfigData` asset. The plugin ships `DA_InputConfig_Default`, `IMC_Character` and the `IA_*` actions, so a first run needs no input work.
* Tune cameras in the **CameraManager** component: `Camera Profiles`, `Default Camera Name`, `Look Sensitivity`.
* Swimming needs no input: place a **Physics Volume** with `Water Volume` ticked.
* **Split of input:** body input (move, look, jump) belongs to the character; input that must work without a pawn (pause, menus) belongs to the PlayerController. The two use separate mapping contexts, so they never overwrite each other.

## 2.5 Map travel

The UI lifetime is the lifetime of the **HUD actor**: `ABaseHUD::EndPlay` calls `ReleasePlayerController` (the whole root layout and every open screen are removed), and the next `ABaseHUD::BeginPlay` starts the UI again. **Screens do not survive map travel, by design.**

| Object | Hard travel | Seamless travel |
|---|---|---|
| GameInstance, LocalPlayer, UI subsystem | Stay | Stay |
| UI registries (screens from `DA_UIConfig`, runtime `Register*` calls, resident classes) | Stay (they live in the subsystem) | Stay |
| GameMode, GameState | New | New |
| PlayerController | New | Carried over |
| PlayerState | New | Copied into a new one (override `CopyProperties` in *your* PlayerState to carry data) |
| Pawn | New | New |
| HUD actor | New | New (the new GameMode assigns the HUD class again) |
| Root layout and every open screen | Destroyed with the old HUD, rebuilt by the new HUD | Same |

What this means in practice:

* **Every map type needs a GameMode whose `HUD Class` is set.** A map without a startup actor has no UI at all.
* The **pause is released** when the old HUD tears the layers down (the screens deactivate first), but closing the pause menu *before* `Open Level` is still the safe order in your Quit-to-Menu button: **Close Screen**, then **Open Level**.
* Data you want on the next map goes into the GameInstance (or a subsystem), not into the pawn or the controller.
* The rows for seamless travel describe engine behavior as the author knows it. It was not run (Appendix B).

## 2.6 Pause and the UI input mode

* The **screen** pauses the game: *Class Defaults* → `Pause Game While Active`. It applies once when the screen activates and releases once when it deactivates (also if the screen is destroyed).
* The **controller** only opens and closes the screen (`TogglePauseScreen`).
* **Committed choice for this guide:** the pause menu is **UI Only**, **Is Back Handler** on, **Pause Game While Active** on. This is the single-player and listen-host behavior.
* `Pause Game While Active` follows the engine pause rules. In a networked game the engine pause is a state of the *world* owned by the server; it is not a per-player pause, and a client cannot start it. **Do not use that flag in a multiplayer game's menu.** Use a pause screen with `Input Mode = Game And UI` plus `Ignore Look Input While Active` and `Ignore Move Input While Active`, and tick **Trigger When Paused** on `IA_Pause` if the game can also be paused.

## 2.7 Multiplayer notes

| Code or data | Runs / exists | Rule |
|---|---|---|
| GameMode | Server only | Never read it from UI or client code. Use the GameState |
| GameState, PlayerState, Pawn | Server and clients | Replicated. Put what the HUD shows here, behind delegates |
| PlayerController | Server and owning client | Create input and UI calls only where `IsLocalPlayerController()` is true (the template does) |
| HUD actor, UI subsystem, widgets | Local machine only | `ABaseHUD` checks `IsLocalController()`. A dedicated server never builds UI |
| Camera rig | Not on a dedicated server | Every rig function is a no-op there |

* **Late replication is normal.** The pawn or player state can arrive after the HUD. Use the binder events, never `BeginPlay` of a widget, to find the pawn.
* **The listen-server host is both:** it has a GameMode *and* a UI. Test as host and as client.
* **Split-screen:** each local player has its own subsystem, root layout, hub and CommonUI router. Widgets find their subsystem through their *owning* player, so create widgets with the right `PlayerController` as owner (the framework does).
* **Replication already handled by the character framework:** perspective (replicated, with a server RPC), actions (`RequestAction`), movement prediction flags, and state tags (replicated to other players, predicted on the owner). Your game code does not repeat that.
* Nothing in this guide was tested in a networked session (Appendix B).

---

# 3. New-project checklist

Follow the steps **in order**. Steps 8–16 use the editor; the exact property names are in **bold**. Sections of the UI guide are written as *UI guide 3.5*.

## Part A — the framework repository (once, before the first game uses the kit)

### Step 1 · Remove `BP_BaseGameMode`
`FRAMEWORK REPO · once`
Follow **section 5**. Do it **before** any game project updates its plugin pointer, or those projects will see a GameMode with a missing parent.

### Step 2 · Add the kit to the plugin and publish
`FRAMEWORK REPO · once`
Unzip the kit into `Plugins/CoreFramework/` of the `FrameWorks` project (it adds `Docs/Integration/` and `Templates/`). Then, in PowerShell at the project root:

```powershell
git add .
git commit -m "Add Project Integration Guide and game module starter kit"
git push origin main
git subtree split --prefix=Plugins/CoreFramework -b coreframework-branch
git push origin coreframework-branch
```

*Why:* game projects read the isolated branch, not `main`. These are the same commands as section 1 of `HOW-TO-IMPORT-SUBMODULE.md`, which also explains how a game project pulls the update.

### Step 3 · Confirm the shared UI assets exist
`FRAMEWORK REPO · once`
With **Show Plugin Content** on, the Content Browser root **CoreFramework Content** must contain `WBP_BaseRootLayout`, `DT_UI_Actions` and `BP_CommonInputData` (they sit in the plugin folder `UI/Layout` and `UI/Input`, below a folder that is itself named `Content`). They exist in the repository today. If you ever recreate them, follow *UI guide Step 2*.

## Part B — one new game project (once per project)

### Step 4 · Create the C++ project and the repository
`GAME PROJECT · once per project`
Unreal Project Browser → **Games** → *Blank* → **C++** (not Blueprint), Starter Content off, name it. *Why C++: the starter kit needs a game module.* Then create the repository and a `.gitignore` that excludes `Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/` and `.vs/`.

### Step 5 · Add the plugin as a submodule
`GAME PROJECT · once per project`
PowerShell at the project root:

```powershell
git init
if (!(Test-Path "Plugins")) { New-Item -ItemType Directory -Path "Plugins" }
git submodule add -b coreframework-branch https://github.com/Ry4nTR/FrameWorks.git Plugins/CoreFramework
git submodule update --init --recursive
```

*Why `coreframework-branch`:* it is the branch that exists on the remote and that `HOW-TO-IMPORT-SUBMODULE.md` uses. `UI_HUD_Setup_Guide.md` 3.1 names `plugin-only`, which is not on the remote at the time of writing (Appendix B). Use one name everywhere.

### Step 6 · Enable the plugin and regenerate
`GAME PROJECT · once per project`
Open the `.uproject` in a text editor and add the plugin (the build cannot find the module otherwise):

```json
"Plugins": [
	{ "Name": "CoreFramework", "Enabled": true }
]
```

Right-click the `.uproject` → **Generate Visual Studio project files** (or the equivalent of your IDE). CommonUI and Enhanced Input switch on through the plugin's own dependency list.

### Step 7 · Copy the starter kit into the game module
`GAME PROJECT · once per project`
Follow `Templates/README.md`, steps 1–6: run `Rename-Template.ps1`, fix `Build.cs`, build. **Expected result:** the game module compiles and the editor lists `<Name>GameMode`, `<Name>GameInstance` and `<Name>PlayerController` as parent classes.

### Step 8 · Project Settings (four settings, stored in the game's own config)
`GAME PROJECT · once per project`
**Edit → Project Settings**, and use the **search box** of the window for each:

| Search for | Property | Value |
|---|---|---|
| `game instance` | **Game Instance Class** | `<Name>GameInstance` |
| `viewport client` | **Game Viewport Client Class** | `CommonGameViewportClient` (restart when asked) |
| `Common Input` | **Input Data** | `BP_CommonInputData` (the plugin's asset) |
| `CoreFramework UI` | **Default UI Config** | `DA_UIConfig` (assign it after step 10) |

Also check: search `Default Input Component Class` → must be `EnhancedInputComponent` (it is in new UE5 projects).
*Why:* CommonUI only sees input through its viewport client; the Common Input data links CommonUI to the Back / Confirm actions; the UI config is the single link between the plugin and your screens.

### Step 9 · Gameplay tags: the ids of your screens
`GAME PROJECT · once per project`
**Project Settings** → search `Gameplay Tags` → **Manage Gameplay Tags…** → **+** for each: `UI.Screen.HUD`, `UI.Screen.MainMenu`, `UI.Screen.PauseMenu`, `UI.Screen.Settings`, `UI.Screen.ConfirmPopup`. Leave *Source* on `DefaultGameplayTags.ini`. (The four layer tags `CoreFramework.UI.Layer.*` come from the plugin.)

### Step 10 · UI content: folders, styles, `DA_UIConfig`
`GAME PROJECT · once per project`
Do *UI guide 3.3* (folders), *3.5* (text style, button styles, `WBP_Button_Primary`, `WBP_Button_Tab`) and *3.6* (`DA_UIConfig`, then assign it in step 8). **Skip *UI guide 3.7***: steps 11, 15 and 16 below replace it, because the GameModes now have the project's C++ class as parent.

### Step 11 · Startup actors
`GAME PROJECT · once per project`
In `Content/UI/Startup`: right-click → **Blueprint Class** → **All Classes** → type `BaseHUD` → **Select**.

1. Name it `BP_Startup_Gameplay`. Open it → **Class Defaults** → search `Startup Screens` → **+** → `UI.Screen.HUD`.
2. Repeat for `BP_Startup_MainMenu` with `[UI.Screen.MainMenu]`.

*Why:* a startup actor is the only thing that starts the UI (section 2.1).

### Step 12 · Input assets for the controller
`GAME PROJECT · once per project`
In `Content/Input/` (create the folder):

1. Right-click → **Input → Input Action** → `IA_Pause`. Open it: **Value Type** = **Digital (bool)**. Leave **Triggers** empty.
2. Right-click → **Input → Input Mapping Context** → `IMC_Player`. Open it → **+** next to *Mappings* → choose `IA_Pause` → bind key **Escape**; add a second mapping `IA_Pause` → key **Gamepad Special Right** (the Menu / Start button).

*Why a separate context:* the character's context is added when a pawn is possessed. The pause key must also work with no pawn, and it is not about the body.

### Step 13 · The PlayerController Blueprint
`GAME PROJECT · once per project`
In `Content/Game/Player/`: **Blueprint Class** → **All Classes** → `<Name>PlayerController` → name it `BP_PlayerController`. **Class Defaults**:

| Property | Value |
|---|---|
| **Pause Action** (category *MyGame\|Input*, with your project name) | `IA_Pause` |
| **Player Mapping Context** | `IMC_Player` |
| **Mapping Priority** | `0` |
| **Pause Screen Tag** (category *MyGame\|UI*) | `UI.Screen.PauseMenu` |

### Step 14 · The character Blueprint
`GAME PROJECT · once per project`
In `Content/Game/Character/`: **Blueprint Class** → **All Classes** → search `BP_BaseCharacter` (plugin asset, needs *Show Plugin Content*) → name it `BP_Character_<Name>`. In **Class Defaults** check **Input Config** (category *Character\|Input*) is `DA_InputConfig_Default`. Set the **Mesh** and its **Anim Class** as described in `Character_Blueprint_Setup.md` (Step 6 there) and `Animation_Blueprint_Setup.md`. Cameras are tuned in the **CameraManager** component, not through camera components (the camera rig is created at runtime by the component).

### Step 15 · The GameModes
`GAME PROJECT · once per project`
In `Content/Game/GameModes/`: **Blueprint Class** → **All Classes** → `<Name>GameMode`.

| Blueprint | Class Defaults → **Classes** |
|---|---|
| `BP_GameMode_Gameplay` | **HUD Class** = `BP_Startup_Gameplay` · **Default Pawn Class** = `BP_Character_<Name>` · **Player Controller Class** = `BP_PlayerController` |
| `BP_GameMode_MainMenu` | **HUD Class** = `BP_Startup_MainMenu` · **Default Pawn Class** = **None** · **Player Controller Class** left as it is (no pause key in the menu) |

### Step 16 · The maps
`GAME PROJECT · once per project`

1. Create `L_MainMenu` and `L_Gameplay` (**File → New Level**). Put a **Player Start** in `L_Gameplay`.
2. In each map: **Window → World Settings** → **Game Mode → GameMode Override** → the matching GameMode.
3. **Project Settings** → search `Game Default Map` → **Game Default Map** and **Editor Startup Map** = `L_MainMenu`. Search `Default GameMode` → **Default GameMode** = `BP_GameMode_Gameplay` (the safe default for a map that has no override).

### Step 17 · The first screens
`GAME PROJECT · per screen`
Build `WBP_HUD`, `WBP_MainMenu`, `WBP_PauseMenu` with *UI guide 4.1–4.3* and list them in `DA_UIConfig` (*UI guide 4.6*). Skip *UI guide 4.7*: your `BP_PlayerController` already opens the pause menu.
**For every screen, check these five things** (the same list applies to step 18):

1. A gameplay tag exists (step 9).
2. The parent class is right: `BaseHUDWidget`, `BaseMenuWidget` or `BaseActivatableWidget`.
3. **Class Defaults:** `Input Mode`, `Is Back Handler`, `Is Modal`, `Pause Game While Active` as in *UI guide 6.1*.
4. A row exists in `DA_UIConfig → Screens` (tag, class, layer).
5. Something opens it: a startup actor, a button (**Push Screen**), or the controller.

### Step 18 · Every further screen and HUD element
`GAME PROJECT · per screen`
Repeat the five checks above. For a HUD element that shows gameplay data use *UI guide Step 5* and section 2.3 of this guide.

### Step 19 · Smoke test
`GAME PROJECT · once per project`
Press **Play** (open `L_MainMenu` first):

| # | Check | Expected |
|---|---|---|
| 1 | Open `L_MainMenu` | Menu appears, cursor visible, no character spawned |
| 2 | Click **Play** (Open Level `L_Gameplay`) | HUD appears, cursor hidden, character moves and looks |
| 3 | Press **Escape** | Pause menu opens, world stops, cursor visible, camera does not turn |
| 4 | Press **Escape** again | Pause menu closes, world runs, cursor hidden |
| 5 | Open the pause menu, choose **Quit to Menu** | Main menu again, nothing frozen |
| 6 | Output Log, filter `LogCoreUI` and the log category of your game | No warnings about tags, classes, layers or the pause key |

### Step 20 · Commit
`GAME PROJECT · once per project`
Commit the project. The submodule pointer (`Plugins/CoreFramework`) is part of the commit. To take a newer framework later: `git submodule update --remote Plugins/CoreFramework`, then commit the new pointer.

---

# 4. Framework or project?

## 4.1 The decision table

| Thing | Belongs in | Why |
|---|---|---|
| A mechanism that any game type can use (input routing, layers, screens, binding, camera profiles, state tags, movement domains) | **Framework** | It repeats unchanged |
| The data that fills a mechanism (screen list, camera profile values, input config, tab sets) | **Project** | The plugin ships no game content |
| GameMode, match flow, win or lose rules | **Project** | Rules are game-specific. Never a framework class |
| The GameInstance class | **Project** | One class per project. A reusable need becomes a `UGameInstanceSubsystem` in the framework, after it repeats |
| GameState and PlayerState subclasses and their data | **Project** | They carry the vocabulary of the game |
| Keys on the PlayerController (pause, scoreboard, chat) | **Project** (`AMyPlayerController`) | Keys are game decisions |
| Every screen, HUD element, button style, text style | **Project** | Your look |
| Root layout, `DT_UI_Actions`, `BP_CommonInputData` | **Framework** | Identical in every game. Customize by child or duplicate (*UI guide W.4*) |
| Layer tags and character state tags | **Framework** (native tags) | Infrastructure vocabulary |
| Screen ids (`UI.Screen.*`) | **Project** | Names of your screens |
| Components with game vocabulary (resources, inventory, quests) | **Project** until it repeats | You cannot yet know what is shared |
| Save and load, settings persistence, online sessions | **Project** until it repeats | Then a subsystem in the framework |
| Meshes, animations, sounds, art | **Project** | The plugin only carries placeholders |

## 4.2 The promotion rule

> **Code moves into a framework only after it repeats in the 2nd–3rd project.**

1. **First project:** write it in the game module. Do not design it for reuse.
2. **Second project needs it:** copy it and compare the two copies. Do not promote yet.
3. **Third project needs it** (or the second, when the two copies are nearly identical): promote.
4. **Shape of a promoted piece:** a component, a subsystem, a data asset or an interface. **Never** a base class in the chain of GameMode, GameInstance or the character. Remove every game word, add a document next to the other framework docs, and make it independent of the other frameworks (as Movement, Camera and StateMachine are).
5. **Publish:** commit, run the `git subtree split` of step 2, then update the submodule pointer in every project.

---

# 5. Re-parenting existing GameModes and deleting `BP_BaseGameMode` safely

## 5.1 What was found in the repository

Reading the repository (not the editor) showed that `BP_BaseGameMode` is **not** a harmless empty asset:

| Fact | Consequence |
|---|---|
| `Config/DefaultEngine.ini` of the `FrameWorks` project has `GlobalDefaultGameMode=/CoreFramework/Content/Core/BP_BaseGameMode.BP_BaseGameMode_C` | The editor does **not** track ini strings. Deleting the asset leaves a dead path in the file |
| The plugin's demo map `L_Demo` names `BP_BaseGameMode` as its GameMode | A plugin asset must not depend on a project class. See 5.3 |
| The asset's name table contains `DefaultPawnClass` and a reference to `/CoreFramework/Content/Character/Blueprints/BP_BaseCharacter` | It very likely sets **Default Pawn Class = `BP_BaseCharacter`**. Children inherit that. This was read from the binary file, not confirmed in the editor: open `BP_BaseGameMode` → **Class Defaults** to confirm |

A child Blueprint stores only the values it overrides. After re-parenting, **every inherited value is gone**, so each GameMode must have `Default Pawn Class`, `HUD Class` and `Player Controller Class` set explicitly. A **main-menu** GameMode that silently inherited a pawn class would have spawned a character on the menu map.

## 5.2 The order (do not skip steps)

**`GAME PROJECT`: for each project that has GameModes with parent `BP_BaseGameMode`**

1. **List the users.** Content Browser → right-click `BP_BaseGameMode` → **Reference Viewer**. Note every child GameMode and every map. Then open `Config/DefaultEngine.ini` in a text editor and search `BP_BaseGameMode`.
2. **Write down the inherited values.** Open each child GameMode → **Class Defaults**. Note `Default Pawn Class`, `HUD Class`, `Player Controller Class` and any other value (inherited values are not shown in bold).
3. **Re-parent each child.** Open it → toolbar **Class Settings** → in **Details**, section **Class Options**, property **Parent Class** → choose `<Name>GameMode` (this guide) or *Game Mode Base*. **Compile**.
4. **Set the values explicitly** (**Class Defaults → Classes**): `Default Pawn Class` (a main-menu GameMode: **None**), `HUD Class`, `Player Controller Class`. **Compile**, **Save**.
5. **Fix the maps.** For every map from step 1: open it → **Window → World Settings** → **Game Mode → GameMode Override** → the child GameMode. Save the map.
6. **Fix the ini.** **Project Settings** → search `Default GameMode` → **Default GameMode** = one of your GameModes. Open `DefaultEngine.ini` again: the line `GlobalDefaultGameMode=` must no longer name `BP_BaseGameMode`.
7. **Check.** Reference Viewer on `BP_BaseGameMode` must show **no referencers**. Press **Play** in every map.

**`FRAMEWORK REPO`: the deletion (step 1 of the checklist), only after every game project has finished the list above**

1. Do the list above in the `FrameWorks` project itself (5.3 describes its special case).
2. Right-click `BP_BaseGameMode` → **Delete**. The dialog must say that nothing references it. If it lists a referencer, cancel and go back to step 1 of the list.
3. Commit, run `git subtree split` and push `coreframework-branch` (checklist step 2).
4. In each game project: `git submodule update --remote Plugins/CoreFramework`, commit the new pointer.

*Why this order:* a game that updates the plugin first would load its GameModes with a **missing parent class**, and their Blueprint would lose every inherited setting.

## 5.3 The `FrameWorks` project and `L_Demo`

`L_Demo` ships **inside the plugin**, so after the subtree split every game gets it. A plugin map cannot depend on a GameMode of the project.

1. In the `FrameWorks` project (not in the plugin) create `Content/Demo/BP_GameMode_Demo`, parent *Game Mode Base*, **Default Pawn Class** = `BP_BaseCharacter`, **HUD Class** = whatever your demo uses.
2. **Project Settings → Default GameMode** = `BP_GameMode_Demo` (this also rewrites `GlobalDefaultGameMode` in `DefaultEngine.ini`).
3. Open `L_Demo` → **World Settings → GameMode Override** = **None**, so the map uses the default GameMode of whichever project opens it. Save.
4. Continue with the deletion list above.

## 5.4 If a GameMode already shows "missing parent"

You updated the plugin before re-parenting. Check out the previous submodule commit (`git submodule update`, or `git -C Plugins/CoreFramework checkout <old commit>`), do the list in 5.2, then update the submodule again.

---

# 6. Common mistakes and troubleshooting

Always start with **Window → Output Log**, filter `LogCoreUI` and the log category of your game (`Log<Name>`).

| Symptom | Cause | Fix |
|---|---|---|
| No UI at all | The GameMode has no `HUD Class`, or `Default UI Config` is empty, or `Root Layout Class` is empty | Steps 8, 10, 15. `LogCoreUI` names the missing piece |
| `no screen registered for tag` | No row in `DA_UIConfig → Screens`, or a typo in the tag | Step 17, check 4 |
| The main-menu map spawns a character | The GameMode inherited a `Default Pawn Class` (old `BP_BaseGameMode`) | Section 5, step 4: set it to **None** explicitly |
| Pause key does nothing | The gameplay GameMode does not use `BP_PlayerController`; `Pause Action` or `Player Mapping Context` empty; `Pause Screen Tag` empty; `Default Input Component Class` is not Enhanced Input | Steps 12, 13, 15. The log of your game prints which one is unset |
| Pause menu opens, the game keeps running | `Pause Game While Active` is off on the screen | Class Defaults of `WBP_PauseMenu` |
| Pause menu opens, the camera still turns | `Input Mode` is not **UI Only** | Class Defaults of the screen (*UI guide 6.1*) |
| Escape opens the pause menu but does not close it | `Is Back Handler` off, `Input Data` not set, or the viewport client is not `CommonGameViewportClient` | Steps 8 and 17 |
| Pause menu can open and close in the same key press | The same key (Escape) reaches both `IA_Pause` and the Back action, which happens when the pause screen is *Game And UI* instead of *UI Only* | Keep the pause screen **UI Only**, or use a different key for `IA_Pause` |
| New map starts frozen | The pause was not released before travel | **Close Screen** before **Open Level** (section 2.5) |
| UI missing after `Open Level` | The new map's GameMode has no `HUD Class`, or the map has no `GameMode Override` | Steps 15 and 16 |
| Two HUDs or doubled input | The UI is started from two places (GameMode HUD *and* a Level Blueprint or character) | Keep only the startup actor |
| A HUD element does not update after respawn | The widget subscribed in *Bound* and never unsubscribed, or `Auto Register As Binder` is off | Section 2.3, *UI guide 5.2* |
| Compile error: header `UISystem/...` not found | Module `CoreFramework` missing in `Build.cs`, or the project files were not regenerated | Step 7, `Templates/README.md` step 3 and step 5 |
| Linker error for `UUIFrameSubsystem` | `CoreFramework` is not in `PublicDependencyModuleNames`, or the plugin is not enabled in the `.uproject` | Step 6 |
| `<Name>GameMode.generated.h` not found after renaming | A stale `Intermediate/` folder | Close the editor, delete `Intermediate/`, regenerate project files, rebuild |
| `<Name>GameMode` is not in the Blueprint parent list | The build did not finish, or the editor was opened before the build | Build from the IDE, then open the editor |
| The plugin's assets are invisible | **Show Plugin Content** is off | Content Browser → **Settings** → tick it |
| GameMode with "missing parent class" | The plugin was updated before the GameModes were re-parented | Section 5.4 |
| Widget created but `GetUIFrameSubsystem` is null | The widget has no owning local player (created without a `PlayerController` owner) | Create widgets through the screens of the framework, or with the controller as owner |

---

# Appendix A — Map of the starter kit

| Kit file (under `Templates/`) | Copy to (game module) | Connects to |
|---|---|---|
| `Public/Core/MyGameLog.h`, `Private/Core/MyGameLog.cpp` | `Public/Core/`, `Private/Core/` | Log category for the three classes |
| `Public/Core/MyGameMode.h`, `Private/Core/MyGameMode.cpp` | `Public/Core/`, `Private/Core/` | `HUD Class` (UI), `Default Pawn Class` (Character), `Player Controller Class` (template controller) |
| `Public/Core/MyGameInstance.h`, `Private/Core/MyGameInstance.cpp` | `Public/Core/`, `Private/Core/` | Nothing today; the header explains the subsystem rule |
| `Public/Player/MyPlayerController.h`, `Private/Player/MyPlayerController.cpp` | `Public/Player/`, `Private/Player/` | `UUIFrameSubsystem` (pause), Enhanced Input |
| `MyGame.Build.cs` | Compare with the module's `Build.cs` | Modules `EnhancedInput`, `GameplayTags`, `CoreFramework` |
| `Rename-Template.ps1` | Run from `Templates/` | Renames `MyGame` to the project name |

# Appendix B — What was and was not verified

**Read from the source (repository `Ry4nTR/FrameWorks`, `main`, commit `849ee25`):** `ABaseHUD`, `UUIFrameSubsystem` (header and source), `UUIBindingHub`, `IBaseUIBinderInterface`, `UBaseActivatableWidget` (header and source), `UBaseUserWidget`, `UBaseHUDWidget`, `UBaseMenuWidget`, `UBaseRootLayoutWidget`, `UUIFrameSettings`, `UUIConfigDataAsset`, `UITypes`, `ABaseCharacter` (header and constructor), `UInputConfigData`, `UCameraManagerComponent`, `UStateMachineComponent` and `UBaseCharacterMovementComponent` (headers), the Animation header, the UI setup and architecture documents, `Character_Blueprint_Setup.md`, `HOW-TO-IMPORT-SUBMODULE.md`, `DefaultEngine.ini`, the `.uplugin` and both `Build.cs` files.

**Not done:**

* **No C++ was compiled against Unreal Engine.** The only check was a syntax-only pass with `g++` against hand-written stub headers (it found no typos, and says nothing about the real engine API). API use was matched against the repository's own code where possible (for example `ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>` is used the same way in `ABaseCharacter.cpp`), but engine signatures for 5.8 were not checked by a build.
* **`Rename-Template.ps1` was never run** (no PowerShell was available to the author). Its replacement order was reproduced in Python for the name `Atlas`: no `MyGame` text was left over and the renamed files passed the same stub syntax check. The PowerShell syntax itself is unchecked.
* **Nothing was opened in the editor.** The facts about `BP_BaseGameMode` and `L_Demo` come from the strings inside the binary `.uasset` and `.umap` files. The `DefaultPawnClass` finding in particular is an inference.
* **Engine behavior** stated in sections 1, 2.5, 2.6 and 2.7 (what survives travel, the HUD actor being re-created, the engine pause in networked games, which objects exist where) is the author's knowledge of Unreal Engine, not something that was run or looked up in 5.8 source.
* **No networked test.**

**Observations about the repository (nothing was changed):**

1. `UI_HUD_Setup_Guide.md` 3.1 uses the branch `plugin-only`; `HOW-TO-IMPORT-SUBMODULE.md` and the remote have `coreframework-branch` (remote branches: `main`, `coreframework-branch`).
2. `Character_Blueprint_Setup.md` Step 6 lists the components `FirstPersonCamera`, `ThirdPersonSpringArm` and `ThirdPersonCamera`. The current code creates the camera rig at runtime inside `UCameraManagerComponent`, so that step reads as out of date.
3. `CoreFramework.Build.cs` contains stray `[cite: 1]` text in comments, and several framework headers say "keep `FRAMEWORKS_API`" while the export macro is `COREFRAMEWORK_API`. Comments only.
4. The plugin has a folder `Content/Content`, so asset paths read `/CoreFramework/Content/...` and the Content Browser shows a folder named `Content` inside **CoreFramework Content**.
