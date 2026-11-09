# Save System - Architecture Overview

Target: Unreal Engine 5.8 | Module macro: `COREFRAMEWORK_API` | Folder: `Source/CoreFramework/{Public,Private}/SaveSystem/`

---

## 1. Purpose

The save system gives every game the same persistence features without per-game save code:

- **asynchronous** saving and loading (no game-thread stall on file I/O);
- **multiple slots** with metadata (name, timestamp, playtime, map, game labels), create / overwrite / delete / list;
- **versioning and migration**, so an update never breaks old saves;
- a **flexible serialization pattern**: any actor, component, subsystem or plain object saves itself with a checkbox on its variables;
- **profiles** per local player (settings, unlocks) next to the **world slots** of a playthrough;
- correct behaviour in **multiplayer and split-screen**;
- ready **UI parents** for the classic slot list and for settings pages.

Design rules:

| Rule | How it is met |
|---|---|
| Decoupled from the game's `UGameInstance` | The manager is a `UGameInstanceSubsystem`. The engine creates it for whatever GameInstance class the game uses. No base class, no registration, no code in the GameInstance. |
| Modular | Nothing in the save core knows another framework. The UI parents (`SaveSystem/UI`) depend on the UI system and on the save core; the reverse never happens. |
| Genre agnostic | No game concepts. Keys, versions and labels are free data. |
| Multiplayer ready | World saves belong to the authority, profiles to the machine of the local player. See section 9. |
| Simple | A game needs one number (the save version) and a checkbox per variable. Everything else is optional. |

---

## 2. Files and dependencies

```
SaveSystem/
    Core/
        SaveTypes.h                     Enums, FSaveSlotInfo, FSaveableDescriptor, FSaveEntry, delegates, LogCoreSave
        SaveSystemSettings.h            Project Settings page (version, migration steps, hooks, slot limits)
        SaveSerialization.h/.cpp        (Private) write/read the SaveGame variables of an object to bytes
    Data/
        SaveSlotData.h/.cpp             USaveSlotData (the container), USaveSlotIndex (the slot list)
        SaveMigrationStep.h/.cpp        USaveMigrationStep (one version upgrade)
        SaveHooks.h/.cpp                USaveHooks (the game's extension point)
    Interface/
        SaveableInterface.h             ISaveableInterface
    Manager/
        SaveSubsystem.h/.cpp            USaveSubsystem: lifecycle, queue, public entry points
        SaveSubsystem_Operations.cpp    slot index, save / load / delete / list, profiles, versioning
        SaveSubsystem_Registry.cpp      registry, capture / restore, profile values
    Components/
        SaveableComponent.h/.cpp        USaveableComponent: drop-in saving for an actor
    Library/
        SaveFunctionLibrary.h/.cpp      Blueprint shortcuts and text helpers
    UI/
        SaveSlotEntryWidget.h/.cpp      one row of the slot list
        SaveSlotsTabWidget.h/.cpp       the slot list page (load / save / delete)
        SaveProfileTabWidget.h/.cpp     base of settings pages stored in the profile
```

| Dependency | Type |
|---|---|
| `Core`, `CoreUObject`, `Engine` | Module (`USaveGame`, `UGameplayStatics` slot API, subsystems, `FTickableGameObject`) |
| `GameplayTags`, `DeveloperSettings` | already present |
| `UMG`, `CommonUI` | already present (UI parents only) |

No new module dependency was added to `CoreFramework.Build.cs`.

Change to an existing class: `UBaseTabWidget::NotifyTabSelected` / `NotifyTabDeselected` became `virtual`, so a C++ tab can refresh when it is selected. Nothing else of the existing frameworks was touched.

---

## 3. Class breakdown

| Class | Kind | Role |
|---|---|---|
| `USaveSubsystem` | `UGameInstanceSubsystem` + `FTickableGameObject` | The only entry point. Owns the registry, the active data, the slot index, the operation queue, the playtime. |
| `ISaveableInterface` | `UINTERFACE` | The contract of a saveable object: `GetSaveDescriptor`, `OnBeforeSave`, `OnAfterLoad(SavedVersion)`, optional C++ `SerializeSaveData`. |
| `USaveableComponent` | `UActorComponent` + interface | Makes an actor saveable. Saves the actor, its components' `SaveGame` variables, and optionally its transform. |
| `USaveSlotData` | `USaveGame` | The on-disk container of a World slot and of a Profile file. |
| `USaveSlotIndex` | `USaveGame` | The file that lists the World slots (desktop platforms cannot list saves themselves). |
| `FSaveSlotInfo` | struct | Metadata of a slot. |
| `USaveMigrationStep` | abstract, Blueprintable | One upgrade step `FromVersion -> ToVersion`. |
| `USaveHooks` | Blueprintable `UObject` | The game's extension point (`CanSave`, `BuildSlotInfo`, `OnBeforeSave`, `OnAfterLoad`). |
| `USaveSystemSettings` | `UDeveloperSettings` | Project Settings > CoreFramework > CoreFramework Save. |
| `USaveFunctionLibrary` | Blueprint library | World-context nodes, `ResultToText`, `FormatPlaytime`, `MakeUniqueSlotName`. |
| `USaveSlotEntryWidget` | `UBaseUserWidget` | One row of a slot list. |
| `USaveSlotsTabWidget` | `UBaseTabWidget` | The classic load / save / delete page. |
| `USaveProfileTabWidget` | `UBaseTabWidget` | Base of settings pages with Apply / Revert. |

### Why a GameInstance subsystem, and why the subsystem is not subclassed

- A project has exactly one `UGameInstance` class, so a framework base class would block every other framework that needs its own. A subsystem is created by the engine for any GameInstance (`Project_Integration_Guide.md`, rule 3).
- Saves and the active playthrough must outlive maps, so a world subsystem would be wrong.
- The engine instantiates subsystem subclasses by its own rules, so game behaviour is added through `USaveHooks`, saveables, migration steps and child widgets instead of subclassing the subsystem.

---

## 4. Concepts

### Scopes

| Scope | What | Files | Who may save / load |
|---|---|---|---|
| **World** | A playthrough: world state, quests, inventory, anything that belongs to "this game". Many slots. | `CF_Slot_<name>`, `CF_Index` | The authority: standalone, listen host, dedicated server. A network client gets `NotAuthority`. |
| **Profile** | Things of one local player that are not part of a playthrough: settings, unlocks, statistics. One per local player. | `CF_Profile_<n>` | Any machine that has that local player. Never a dedicated server. |

Every saveable declares one scope (World by default).

### Registering = restoring, unregistering = capturing

- An object **registers** itself when it comes to life. If the active data already holds an entry for its key, the object is restored **right then**. That removes every ordering problem: "load the slot, then open its map" works because the map's actors restore themselves as they begin play; objects that are already alive are restored when the load finishes.
- An object **unregisters** when it goes away (level unload, destroy). Its current state is copied back into the active data, so progress survives level changes before the next save. (Not on quit or end of Play In Editor.)
- A save captures every object that is registered **now** and keeps the entries of objects that are not (other levels, destroyed actors). Saving in level B never drops level A.

### Keys

A key is any string that stays the same between runs: `Player.76561198`, `Door_12`, `Inventory`. The framework never parses it. Placed actors get an automatic key (`<Level>.<ActorName>`); actors spawned at runtime have random names, so they must set their key. Two live objects with the same key log a warning and the newest wins.

### Not in this version

Respawning dynamically spawned actors. The pattern: a world-state saveable (for example a subsystem) records what was spawned, writes it in its own `SaveGame` variables, and spawns it again in `OnAfterLoad`. The format leaves room for built-in support later (entries are keyed bytes, the header has a version).

---

## 5. Data model and files

```
USaveSlotData  (one file)
    FormatVersion   layout of the container, written by the framework (CoreSave::FormatVersion)
    GameVersion     the game's data version (Project Settings > Current Save Version)
    Info            FSaveSlotInfo: SlotName, DisplayName, SavedAt, PlaytimeSeconds, MapName, GameVersion, bIsAutosave, CustomData
    Entries         key -> { Version, Bytes }       one per saveable object
    Values          name -> text                    the profile's key/value store (settings pages)

USaveSlotIndex (CF_Index)
    FormatVersion
    Slots           FSaveSlotInfo for every World slot
```

| File | Content |
|---|---|
| `CF_Slot_<name>` | One World slot. Name: letters, digits, `_`, `-`, up to 64 characters. |
| `CF_Index` | The slot list. Needed because `ISaveGameSystem::GetSaveGameNames` is only implemented on iOS and GDK, not on desktop. |
| `CF_Profile_<n>` | The profile of local player `n`. Uses that player's platform user index. |

The prefix `CF_` keeps the files apart from any save the game writes by itself. Files are written through `UGameplayStatics::AsyncSaveGameToSlot` and read through `AsyncLoadGameFromSlot`, the engine's platform-safe slot API.

World slots use `WorldSaveUserIndex` (Project Settings, default 0).

---

## 6. Serialization flow

### How one object is written

```
CaptureObject(Object)
   1. ISaveableInterface::OnBeforeSave          (update anything that is not kept live)
   2. if the C++ class overrides SerializeSaveData and returns true  -> those bytes
      else                                                            -> reflection:
         FMemoryWriter -> FObjectAndNameAsStringProxyArchive (ArIsSaveGame = true) -> Object->Serialize
         only variables marked SaveGame are written, with their NAMES (tagged properties)
   3. Entries[Key] = { Descriptor.Version, bytes }
```

Because properties are stored by name, **adding or removing a `SaveGame` variable needs no migration**: a new variable keeps its default when an old file lacks it, a removed one is ignored. Object references are stored as paths (valid for placed actors and assets; use keys for runtime objects).

### How one object is restored

```
RestoreObject(Object, Entry)
   1. SerializeSaveData (C++ custom data)   or   reflection read of the SaveGame variables
   2. ISaveableInterface::OnAfterLoad(Entry.Version)   (refresh anything derived; Version = the version the data was SAVED with)
```

### `USaveableComponent` blob

The component implements `SerializeSaveData` itself and writes one composite entry for its actor:

```
BlobVersion | has transform + FTransform | part count | for each part: name + bytes
parts = the actor ("@Actor") and every sibling component that has SaveGame variables
        (components that implement ISaveableInterface themselves are skipped: they register on their own)
```

A part that no longer exists after an update is skipped. A blob written by a newer layout is left alone.

### Full save, step by step (World)

```
SaveToSlot(Name)  -> queue
   validate name, authority, Save Hooks CanSave, slot limit
   Hooks.OnBeforeSave
   capture all registered World saveables into the active data           (game thread, small byte blobs)
   build the header (BuildInfo + Hooks.BuildSlotInfo)
   AsyncSaveGameToSlot(slot file)       the engine serializes now; the file write is asynchronous
        -> on success: update the index, AsyncSaveGameToSlot(index)
        -> result delegate + OnOperationFinished + OnSlotListChanged
```

The payload is written **before** the index, so a crash can leave an unlisted file but never a listed slot without a file.

### Full load, step by step (World)

```
LoadSlot(Name)  -> queue
   validate, authority, file exists
   AsyncLoadGameFromSlot
        -> FormatVersion / GameVersion checks            too new  -> VersionTooNew  (nothing changes)
        -> migration steps while GameVersion < Current   missing  -> MigrationFailed (nothing changes)
        -> the data becomes the active data; slot name, playtime, labels are adopted
        -> restore every registered World saveable
        -> Hooks.OnAfterLoad
        -> result delegate (Info.MapName = the level to open) + OnOperationFinished
```

A failed load leaves the running game and the file on disk untouched.

---

## 7. Operations, queue and session

All operations are asynchronous and go through **one FIFO queue**, one at a time, so two operations never race on the index or a slot file. `IsBusy` and `OnBusyChanged` tell the UI when to show a spinner.

| Operation | Notes |
|---|---|
| `StartNewGame(Slot)` | Immediate. Forgets the active data, makes `Slot` the active slot, resets playtime and labels. Nothing is written until the first save. |
| `SaveToSlot(Slot, DisplayName)` | Creates or overwrites. Empty display name keeps the old one. |
| `QuickSave` | Saves into the active slot. `SlotNotFound` if there is none. |
| `AutoSave` | Saves into `AutosaveSlotName`. The active slot does not change. |
| `LoadSlot(Slot)` | See section 6. |
| `DeleteSlot(Slot)` | The file delete is synchronous (the engine has no async delete); it is a tiny file. The active slot name is cleared if it was that slot. |
| `ListSlots` | Reads the index, drops entries whose file vanished, fills `GetCachedSlots` (newest first). |
| `LoadProfile(n)` / `SaveProfile(n)` | See section 9. |

Every operation reports through its delegate (`FSaveOperationDelegate`: `Result`, `Info`) and through the subsystem event `OnOperationFinished`. C++ callers may pass a `TFunction` instead of a delegate.

`ESaveResult`: `Success`, `InvalidSlotName`, `NotAuthority`, `NoLocalPlayer`, `SlotNotFound`, `FileError`, `VersionTooNew`, `MigrationFailed`, `TooManySlots`, `Blocked`. `USaveFunctionLibrary::ResultToText` gives a sentence for each.

**Playtime** counts real time while a playthrough is active and the world is not paused, and is written to the header at every save.

**Custom labels.** `SetSlotCustomData(Key, Value)` or `USaveHooks::BuildSlotInfo` add game labels (chapter, difficulty, "Level 12 Mage") to the header; the slot list shows them without loading anything.

---

## 8. Versioning

Three layers, from "nothing to do" to "explicit step":

| Layer | When | Mechanism |
|---|---|---|
| 1. Automatic | A `SaveGame` variable was added or removed | Tagged-property serialization (section 6). No code. |
| 2. Per object | The **meaning** of a variable changed (units, enum order) | The object's `Version` is stored in its entry and handed back as `SavedVersion` in `OnAfterLoad`. The object converts its own data. |
| 3. Per slot | The **structure** changed: a key was renamed, two objects merged, an object was removed | A `USaveMigrationStep` chain, run on the loaded data before anything is restored. |

Rules of the chain:

- The game's data version is `CurrentSaveVersion` (Project Settings). It is stored in every file as `GameVersion`.
- On load, if `GameVersion < CurrentSaveVersion` the framework looks for a step whose `FromVersion` equals the file's version, runs it, moves to its `ToVersion`, and repeats until it reaches the current version.
- A missing link in the chain fails the load with `MigrationFailed`. The file is not touched.
- A file with a **higher** `GameVersion` (a save from a newer build) or a higher framework `FormatVersion` fails with `VersionTooNew`. It is never overwritten: a world slot is simply not loaded, and a profile that cannot be read is replaced in memory by an empty one that is **locked** (saving it returns `VersionTooNew`), so the unreadable file is never destroyed.
- Migrated data is written back only by the next explicit save.
- Steps work on `USaveSlotData` with `HasEntry`, `RemoveEntry`, `RenameEntry`, `GetEntryVersion`, `GetEntryKeys`, `GetValue`, `SetValue`, `RemoveValue`, for World slots and Profile files alike (the `Scope` argument says which).

The framework's own `FormatVersion` is separate: it changes only when the container layout changes, and the upgrade code for it lives in C++ (`PrepareLoadedData`).

---

## 9. Multiplayer, profiles and sessions

| Case | Behaviour |
|---|---|
| Standalone, listen host | Full World and Profile saving. |
| Dedicated server | World slots work, on the server's disk. No Profile (no local player: `NoLocalPlayer`). |
| Network client | World operations return `NotAuthority`. World saveables on a client are neither captured nor restored (their state comes from the server by replication). Profile works. |
| Loading on a listen host | The host restores its objects; clients receive the result through normal replication. |
| Per-player data on a dedicated server (persistent characters) | Plain World saveables with a key built from the player's unique id (`"Player." + UniqueId`). No extra API. |
| Split-screen | One profile per local player index (`CF_Profile_<n>`, that player's platform user index). `LocalPlayerIndex` on a saveable picks the profile. |

**Authority is checked at the time of the call** from the net mode of the current world. In a main menu (standalone) World operations are allowed; once connected as a client they are not.

**Profile loading.** With `bAutoLoadProfile` (default) the subsystem loads the profile of local player 0 as soon as that player exists. Profile saveables restore themselves when the profile arrives, or when they register later. A missing file is not an error: the profile starts empty. Profile saving is explicit (`SaveProfile`); the settings tab does it on Apply.

**Profile values** (`SetProfileFloat/Int/Bool/String`, `GetProfile...`) are a small key/value store inside the profile file, for settings that need no saveable object. `OnProfileValueChanged` tells the UI.

**Session** = the active slot: its name, playtime, labels and entries live in the subsystem across map travel until `StartNewGame` or `LoadSlot` replaces them.

---

## 10. UI integration

The UI system already has menus with data-driven tabs and an Apply / Revert contract. The save system adds parents that plug into exactly that. Dependency direction: `SaveSystem/UI` -> `UISystem` and the save core.

### `USaveSlotsTabWidget` (the load / save page)

Modes: **Load** (main menu), **Save** (pause menu, autosave hidden), **Save And Load**. It builds one `USaveSlotEntryWidget` per slot inside the panel `SlotListPanel`, tracks the selection, auto-wires optional buttons named `SaveButton`, `NewSlotButton`, `LoadButton`, `DeleteButton`, shows `StatusText`, `BusyIndicator` and `EmptyListWidget`, and refreshes on tab select and whenever the subsystem's list changes. Buttons the mode does not allow are collapsed; the others are enabled only when they make sense (selection, not busy, not the autosave).

Game specific behaviour goes in a child (Blueprint events or C++ virtuals):

| Hook | Default | A game uses it to |
|---|---|---|
| `RequestConfirmation(Action, Info)` | confirms at once | show its own popup, then call `ConfirmPendingAction` / `CancelPendingAction` |
| `HandleSlotLoaded(Info)` / `On Slot Loaded` | opens `Info.MapName` if `bOpenSavedMapOnLoad` | choose the world, travel with options, show a loading screen, close the menu |
| `HandleSlotSaved`, `HandleSlotDeleted` | selects the new slot | play feedback |
| `HandleError(Operation, Result)` | shows `ResultToText` in `StatusText` | custom messages |
| Row Blueprint (`On Slot Info Set`) | name, date, playtime, map | show a thumbnail, chapter, difficulty from `Info.CustomData` |

Confirmation flags: `bConfirmOverwrite` and `bConfirmDelete` on, `bConfirmLoad` off by default.

### `USaveProfileTabWidget` (settings pages)

`Stage Float / Int / Bool / String(Key, Value)` while the player edits; `Get ...` return staged, else saved, else default. `HasPendingChanges` is true when a staged value differs from the saved one; `ApplyChanges` writes the values to the profile and saves it; `RevertChanges` drops them and fires `On Values Reverted`. It plugs into `UBaseMenuWidget::ApplyAllTabs` / `RevertAllTabs`.

The profile used is the one of the local player that owns the widget (`LocalPlayerIndexOverride` pins it).

### Limits of the UI parents

- The widgets are logic only: no styling, no art. Buttons are `UCommonButtonBase` children named exactly as documented; they are auto-wired when present.
- The slot list refreshes every time its tab is selected; that is cheap because the index is cached in memory.
- The framework ships no button Blueprint (a Common Button needs a style, which belongs to the game), so the unstyled skeleton in `Save_Engine_Setup.md` uses plain UMG buttons wired in the graph.

---

## 11. How a game integrates (summary)

1. Set `CurrentSaveVersion` in Project Settings. That is enough to save and load.
2. Make things saveable: add `Saveable` to actors and tick Save Game on variables, or implement `ISaveableInterface` on any object and register it.
3. Use `USaveFunctionLibrary` / `USaveSubsystem` from game code, or add the UI parent tabs to the menus.
4. Optional: a `USaveHooks` child, migration steps when the version changes, child classes of the UI parents.

The GameInstance, GameMode and PlayerController are not involved. `Save_Engine_Setup.md` has the step-by-step version.

---

## 12. Extending

| Goal | How |
|---|---|
| Save an object that is not an actor | Implement `ISaveableInterface`, mark `SaveGame` variables, register / unregister. |
| Fully custom data | Override `SerializeSaveData(FArchive&)` (C++). |
| Game labels in the slot list | `USaveHooks::BuildSlotInfo` or `SetSlotCustomData`. |
| Block saving in some states | `USaveHooks::CanSave`. |
| Rename a saved object | `USaveMigrationStep` with `RenameEntry`. |
| Different slot list look | Child of `USaveSlotEntryWidget` and of `USaveSlotsTabWidget`. |
| Thumbnails, cloud sync, compression, encryption | Not built in. Entries are bytes and the header has a version; add them in the container layer and raise `CoreSave::FormatVersion`. |

---

## 13. Graceful degradation and limits

| Situation | Behaviour |
|---|---|
| No saveables registered | A save writes a header and no entries. |
| Object without `SaveGame` variables | Its entry is empty; `OnAfterLoad` still runs. |
| Slot index unreadable | Warning, empty list; slots reappear as they are saved. |
| Slot file missing | `SlotNotFound`; `ListSlots` drops it from the index. |
| Profile file unreadable (too new, migration failed) | The game runs with an empty profile; that file is never overwritten. |

Limits:

- **Capture and the engine's serialization run on the game thread.** Only the file write is asynchronous. Entries are small; very large worlds should split data across many saveables or write a custom `SerializeSaveData`.
- **References to runtime objects are not saved.** Save keys or ids and resolve them in `OnAfterLoad`.
- **No dynamic actor respawn** (section 4).
- **Delete is synchronous.**
- **A load does not change the level.** The game opens `Info.MapName` itself (the slots tab can do it for single player).
- **Two saveables with the same key overwrite each other.**
- **Nothing in this system was compiled or run in the editor.** Expect to fix compile errors on the first build.
