# Save System - Engine Setup

How to set the save system up, how it plugs into **any** game's GameInstance (it needs nothing from it), and how to use it from Blueprints and C++. Concepts are in `Save_Architecture_Overview.md`.

The framework ships **no save content**. Everything in the framework repo is C++. The only editor work there is an optional unstyled skeleton of the slot list page (Part A). Everything else is done per game project (Part B), and most of it takes minutes.

## Where each step is done

Same convention as `UI_HUD_Setup_Guide.md` and `Audio_Engine_Setup.md`:

| Badge | Meaning |
|---|---|
| `FRAMEWORK REPO · once` | You work in the `FrameWorks` project, inside `Plugins/CoreFramework` (Content Browser > Settings > **Show Plugin Content** > *CoreFramework Content*). Done one time, then exported with the subtree split of `HOW-TO-IMPORT-SUBMODULE.md`. |
| `GAME PROJECT · once per project` | Short setup every new game needs. |
| `GAME PROJECT · per game content` | Specific to the game: its saveable objects, its menus, its versions. |
| `GAME PROJECT · only if needed` | Skip unless the game has that need. |

| Step | What | Where | Effort per new game |
|---|---|---|---|
| A1 | Check that the C++ compiles and the settings page exists | Framework repo, once | none |
| A2 | Optional: unstyled `WBP_SaveSlotEntry` / `WBP_SaveSlotsTab` skeleton | Framework repo, once | saves rebuilding the widget tree |
| A3 | Export the plugin branch | Framework repo, after each change | none |
| B1 | Project Settings: save version | Game project, once | 1 minute |
| B2 | Plug into the GameInstance | Game project | nothing required |
| B3 | Make things saveable | Game project, per game content | the real work |
| B4 | The game flow: new game, save, load, continue | Game project, per game content | per game |
| B5 | Slot list menu (load / save / delete) | Game project, per game content | ~10 minutes |
| B6 | Settings pages stored in the profile | Game project, per game content | per page |
| B7 | Save Hooks | Game project, only if needed | |
| B8 | Versioning: bump the number, write a migration step | Game project, each time a release changes saves | |
| B9 | Multiplayer notes | Game project | |
| B10 | Tests | Game project | |

> **Never edit an asset inside the plugin from a game project.** The plugin is a git submodule: the edit would change the shared repository or be lost at the next update. Duplicate the asset into the game instead.

> **Could everything be done from zero in each game instead?** Yes. Part A is a convenience only. A game can build its own list page from the C++ parents with the table in step B5. It costs about ten minutes per game.

---

# Part A - Framework repo (once)

> **`FRAMEWORK REPO · once`**

## A1 - Check the build

1. Compile the project. The folder `SaveSystem/` is part of the `CoreFramework` module; no `.Build.cs` change is needed.
2. **Edit > Project Settings**: **CoreFramework > CoreFramework Save** must exist. If it does not, the module did not compile.

## A2 - Optional: the unstyled skeleton widgets

These two Blueprints give a working but plain slot list page, so a new game only restyles them. They contain no colors and no art, and use plain UMG buttons (the framework has no button style). Skip this step if you prefer to build the page in each game from the table in B5.

Make the plugin Content visible (Content Browser > Settings > **Show Plugin Content**), then in **CoreFramework Content** create the folders `SaveSystem/UI`.

### WBP_SaveSlotEntry (one row)

1. Right-click in `SaveSystem/UI` > **User Interface > Widget Blueprint**. In *Pick Parent Class* open **All Classes**, search `SaveSlotEntryWidget`, select it. Name it `WBP_SaveSlotEntry`.
2. In the **Designer** build this hierarchy (rename with **F2**, tick **Is Variable** on every named widget):

   | Widget | Name | Notes |
   |---|---|---|
   | Overlay | (root) | |
   | Button | `RowButton` | Fills the Overlay. Not a bound name: it is wired in the graph (step 4). |
   | Horizontal Box | `RowContent` | On top of the button. Set its **Visibility** to *Self Hit Test Invisible* so clicks reach the button. |
   | Text Block | `DisplayNameText` | in `RowContent` |
   | Text Block | `SavedAtText` | in `RowContent` |
   | Text Block | `PlaytimeText` | in `RowContent` |
   | Text Block | `MapText` | in `RowContent` |

   (`SlotNameText` is available too; add it if a design wants the technical name.)
3. Go to the **Graph**. Select `RowButton` in the Details panel > **Events > On Clicked (RowButton)** > add the event.
4. Wire **On Clicked** to the node **Request Select** (self).
5. Create the event **On Selection Changed** (right-click > search *On Selection Changed*): drag a Border or set the button's **Background Color** to a lighter color when `bInSelected` is true. This is the only visual the skeleton needs; a game replaces it.
6. **Compile** and **Save**.

### WBP_SaveSlotsTab (the page)

1. Create a **Widget Blueprint** with parent **SaveSlotsTabWidget** (All Classes). Name it `WBP_SaveSlotsTab`.
2. Hierarchy:

   | Widget | Name | Notes |
   |---|---|---|
   | Vertical Box | (root) | |
   | Scroll Box | `SlotListScroll` | in the Vertical Box, Size: Fill |
   | Vertical Box | `SlotListPanel` | in the Scroll Box. **Exact name, required.** |
   | Text Block | `EmptyListWidget` | text "No saves yet". **Exact name.** |
   | Text Block | `StatusText` | **Exact name.** |
   | Throbber (or any widget) | `BusyIndicator` | **Exact name.** |
   | Horizontal Box | `Buttons` | |
   | Button | `UiSaveButton` | child text "Save" |
   | Button | `UiNewButton` | child text "New Save" |
   | Button | `UiLoadButton` | child text "Load" |
   | Button | `UiDeleteButton` | child text "Delete" |

   Tick **Is Variable** on all named widgets.
3. In **Class Defaults** set **Entry Class** = `WBP_SaveSlotEntry`, **Mode** = Save And Load.
4. **Graph**: for each of the four buttons add its **On Clicked** event and connect it to the matching node: `UiSaveButton` > **Request Save Selected**, `UiNewButton` > **Request Save To New Slot**, `UiLoadButton` > **Request Load Selected**, `UiDeleteButton` > **Request Delete Selected**.
5. The page refreshes itself and shows an error in `StatusText`. The skeleton does not hide buttons by mode (the auto-wired Common Buttons do that); a game that wants it uses the Common Buttons of step B5.
6. **Compile**, **Save**.

Because the skeleton uses UMG buttons, it works with the mouse. For gamepad navigation use the game's own `BaseButton` children (B5).

## A3 - Export

Commit, then run the subtree split and push of `HOW-TO-IMPORT-SUBMODULE.md` (section 1, Steps 1 to 3). Each game pulls the result with `git submodule update --remote --merge Plugins/CoreFramework` and commits the new pointer.

---

# Part B - Each game project

## B1 - Project Settings

> **`GAME PROJECT · once per project`**

**Edit > Project Settings > CoreFramework > CoreFramework Save**:

| Property | Set to | Meaning |
|---|---|---|
| Current Save Version | `1` | The version of this game's save data. Raise it by one when a release changes saves in a way old files cannot simply load (B8). |
| Migration Steps | empty | Upgrade steps (B8). |
| Save Hooks Class | empty | Optional game extension (B7). |
| Max Slots | `0` | 0 = unlimited. |
| Autosave Slot Name | `Autosave` | Slot written by Auto Save. |
| Auto Load Profile | on | Loads the profile of local player 0 when it exists. |
| World Save User Index | `0` | Platform user index of world slots. Leave at 0 on PC. |

Stored in the game's `Config/DefaultGame.ini`:

```ini
[/Script/CoreFramework.SaveSystemSettings]
CurrentSaveVersion=1
AutosaveSlotName=Autosave
bAutoLoadProfile=True
```

If you change nothing, the defaults work (version 1).

## B2 - Plug into the GameInstance

> **`GAME PROJECT`**

**There is nothing to plug in.** `USaveSubsystem` is a `UGameInstanceSubsystem`: the engine creates it for whatever GameInstance class the project uses, native or Blueprint, and its lifetime is the lifetime of the game instance. Your `U<Project>GameInstance` does not inherit from anything of the framework, does not register anything and does not hold save data. This is why the save system never conflicts with another framework or with a project's own GameInstance.

You may still reach the save system from the GameInstance if the game's start-up flow wants to (all optional):

```cpp
// U<Project>GameInstance : public UGameInstance      (the project's own class, no framework base)
#include "SaveSystem/Manager/SaveSubsystem.h"

void UMyGameInstance::StartNewCampaign()
{
	if (USaveSubsystem* Save = GetSubsystem<USaveSubsystem>())
	{
		Save->StartNewGame(TEXT("Slot1"));
	}
}

void UMyGameInstance::ContinueLastSave()
{
	USaveSubsystem* Save = GetSubsystem<USaveSubsystem>();
	if (!Save) { return; }

	// List first (fills the cache), then load the newest slot and open its level.
	Save->ListSlots([this, Save](ESaveResult Result, const FSaveSlotInfo&)
	{
		const TArray<FSaveSlotInfo> Slots = Save->GetCachedSlots();   // newest first
		if (Result != ESaveResult::Success || Slots.Num() == 0) { return; }

		Save->LoadSlot(Slots[0].SlotName, [this](ESaveResult LoadResult, const FSaveSlotInfo& Info)
		{
			if (LoadResult == ESaveResult::Success)
			{
				UGameplayStatics::OpenLevel(this, FName(*Info.MapName));
			}
		});
	});
}
```

In Blueprints use the **Save Function Library** nodes (they need only *self* as world context) or **Get Save Subsystem** from any object that has a world. A GameInstance Blueprint can call them from its events the same way.

## B3 - Make things saveable

> **`GAME PROJECT · per game content`**

### An actor (most cases)

1. Open the actor Blueprint > **Components > Add > Saveable**.
2. In the actor, and in any of its components, tick **Save Game** on every variable that must persist (select the variable > Details > *Advanced* > **Save Game**; in C++ `UPROPERTY(SaveGame)`).
3. Saveable settings (Details):

| Property | Meaning |
|---|---|
| Save Key | Leave empty for an actor placed in a level (automatic key). **Set it** for an actor spawned at runtime (for example `Pickup_` + a stable id). |
| Scope | World (default) for the playthrough, Profile for the local player's profile. |
| Save Version | Raise it when the meaning of a saved variable changes (B8). |
| Save Transform | Also saves and restores location, rotation, scale. |
| Save Owner Variables / Save Sibling Components | What is included besides the transform. |

4. React to a restore with the component's **On Restored** event (it passes the version the data was saved with): a collected pickup destroys itself, a door plays its open pose, a light switches on. **On Saving** fires just before the actor is captured.

Rules:

- A variable that holds a reference to another **placed** actor or an asset is saved as a path. References to runtime objects are not valid after a load: save a key or an id and look the object up in On Restored.
- Components that already have a Saveable of their own (they implement the interface) are skipped to avoid saving twice.
- Order is not guaranteed between actors. If A needs B, resolve it in On Restored of A after both exist (for example in a short delay), or keep the data in one place.

### Any other object (a subsystem, a PlayerState, a manager)

Implement `ISaveableInterface`, mark variables `SaveGame`, register and unregister.

C++:

```cpp
// MyInventorySubsystem.h
UCLASS()
class UMyInventorySubsystem : public UGameInstanceSubsystem, public ISaveableInterface
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	virtual void GetSaveDescriptor_Implementation(FSaveableDescriptor& OutDescriptor) const override
	{
		OutDescriptor.Key = TEXT("Inventory");        // unique and stable
		// Scope = World by default; Version = 1 by default
	}
	virtual void OnAfterLoad_Implementation(int32 SavedVersion) override { /* refresh anything derived */ }

	UPROPERTY(SaveGame) TArray<FName> Items;
	UPROPERTY(SaveGame) int32 Gold = 0;
};

void UMyInventorySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<USaveSubsystem>();      // make sure the save system exists first
	if (USaveSubsystem* Save = GetGameInstance()->GetSubsystem<USaveSubsystem>())
	{
		Save->RegisterSaveable(this);                        // restores now if the active slot holds "Inventory"
	}
}

void UMyInventorySubsystem::Deinitialize()
{
	if (USaveSubsystem* Save = GetGameInstance()->GetSubsystem<USaveSubsystem>())
	{
		Save->UnregisterSaveable(this, /*bCaptureState=*/false);
	}
	Super::Deinitialize();
}
```

In a Blueprint object: **Class Settings > Interfaces > Add > SaveableInterface**, implement **Get Save Descriptor** (set *Key*), then call **Register Saveable** on Begin Play and **Unregister Saveable** on End Play.

### Profile data

Same as above with **Scope = Profile** (and `LocalPlayerIndex` for the player). Use it for unlocks and statistics that belong to the player, not to a playthrough. Settings that are plain numbers and flags need no saveable: use **profile values** (B6).

## B4 - The game flow

> **`GAME PROJECT · per game content`**

All calls are asynchronous; each takes a completion delegate (Blueprint: the red delegate pin, wire a Custom Event) and also fires the subsystem's **On Operation Finished**.

| Moment | Call |
|---|---|
| **New Game** button | **Start New Game** (SlotName, for example from **Make Unique Slot Name**), then open the first level. Nothing is written yet. |
| Pause menu "Save" | The slot list page (B5), or **Save To Slot** (SlotName, DisplayName). |
| Quick save key | **Quick Save**. |
| Checkpoint, level exit | **Auto Save**. |
| Main menu "Continue" | **List Slots**, take the first of **Get Cached Slots**, **Load Slot**, then **Open Level** with the slot's *Map Name*. |
| Main menu "Load" | The slot list page (B5) in Load mode. |
| Delete | The slot list page, or **Delete Slot**. |

**The load flow, in order.** `Load Slot` reads the file and makes it the active data. Objects that are alive are restored at once. Then you open the level (`Info.MapName`): its actors restore themselves as they begin play. You never write ordering code.

**Autosave tip.** Call **Auto Save** from a trigger or a checkpoint actor. It writes the `Autosave` slot and does not change the active slot, so the player's manual slot is untouched.

**Result handling.** The completion delegate gives an **ESaveResult**; **Result To Text** returns a sentence for the UI. `NotAuthority` means a network client tried to save the world; `Blocked` means Save Hooks refused; `VersionTooNew` means the file is from a newer build.

## B5 - The slot list menu

> **`GAME PROJECT · per game content`**

The framework already contains the logic; the game supplies the look.

1. **Row widget.** Duplicate the plugin `WBP_SaveSlotEntry` into the game (or create a child of `SaveSlotEntryWidget`). Style it. Use the **On Slot Info Set** event to show game labels from `Info.CustomData` (chapter, difficulty), a thumbnail, an icon for autosave (`Info.bIsAutosave`). Named widgets it fills by itself: `SlotNameText`, `DisplayNameText`, `SavedAtText`, `PlaytimeText`, `MapText` (any Text Block) and `SelectButton` (a Common Button, auto-wired; or call **Request Select** from your own button).
2. **Page widget.** Duplicate the plugin `WBP_SaveSlotsTab`, or create a Widget Blueprint with parent **SaveSlotsTabWidget**. Required name: `SlotListPanel` (any Panel Widget). Optional names, auto-wired when they are Common Buttons (children of your `BaseButton`): `SaveButton`, `NewSlotButton`, `LoadButton`, `DeleteButton`; plus `StatusText`, `BusyIndicator`, `EmptyListWidget`. Use Common Buttons instead of UMG buttons to get gamepad navigation and hiding by mode for free.
3. **Class Defaults of the page:**

| Property | Typical value |
|---|---|
| Mode | **Load** in the main menu, **Save** in the pause menu, **Save And Load** for one page |
| Entry Class | your row widget |
| Show Autosave | on (Load mode) |
| Confirm Overwrite / Confirm Delete | on |
| Confirm Load | on in game (unsaved progress is lost), off in the main menu |
| Open Saved Map On Load | on for single player; leave off for multiplayer and override *On Slot Loaded* |
| New Slot Prefix | `Slot` |

4. **Register it as a tab** exactly like any other: add `{ Tab Id, Display Name, Tab Widget Class = your page, Sort Order }` to the menu's `UITabSetDataAsset` (`UI_HUD_Setup_Guide.md`), or call `RegisterMenuTab`. The main menu gets the Load page, the pause menu the Save page.
5. **Confirmation popup (optional).** Override the event **Request Confirmation (Action, Info)** in a child of your page: push your popup screen with the UI subsystem (`PushScreen`), and from its Yes / No buttons call **Confirm Pending Action** / **Cancel Pending Action** on the page. Without an override the action runs at once.
6. **Choose the world / what happens after a load.** Override **On Slot Loaded (Info)** in the child: open the level you want (`Info.MapName`, or anything in `Info.CustomData`), show a loading screen, close the menu. This is where a game decides "leave this world and enter that one".

C++ games override the same hooks as virtuals: `HandleSlotLoaded`, `HandleSlotSaved`, `HandleSlotDeleted`, `HandleError`, `RequestConfirmation_Implementation`.

## B6 - Settings pages stored in the profile

> **`GAME PROJECT · per game content`**

1. Create a Widget Blueprint with parent **SaveProfileTabWidget** (it is a `BaseTabWidget`, so it is registered like any settings tab).
2. For each control choose a **key** (`Opt.Subtitles`, `Opt.Sensitivity`) and:
   - in **Event Construct** (or *On Tab Selected*) set the control from **Get Float / Bool / Int / String** (Key, Default);
   - when the player changes it call **Stage Float / Bool / Int / String** (Key, Value).
3. The menu's **Apply** button (`ApplyAllTabs`) commits and saves the profile; **Revert** drops the staged values and fires **On Values Reverted**, where you set the controls again with the Get nodes.
4. Elsewhere in the game read a value with **Get Profile Float / Bool / Int** (Key, Default). Bind **On Profile Value Changed** (Get Save Subsystem) to react live.

Audio volumes already persist by themselves (`Audio_Engine_Setup.md`); mirror them into the profile only if you want profile portability.

## B7 - Save Hooks

> **`GAME PROJECT · only if needed`**

1. Create a Blueprint (or C++) child of **SaveHooks**. Override only what you need:

| Event | Use |
|---|---|
| `Can Save (SlotName)` | Return false to block saving in cutscenes, combat, tutorials. The result is `Blocked`. |
| `Build Slot Info (DefaultInfo)` | Return the info with labels added to *Custom Data* (Chapter, Difficulty, "Level 12 Mage") or another *Display Name*. The slot list shows them. |
| `On Before Save (Scope, SlotName)` | Last chance to update live data. |
| `On After Load (Scope, Info)` | After a slot or profile was restored. |

2. Assign it in Project Settings > CoreFramework Save > **Save Hooks Class**.

For a single label that never changes between saves, **Set Slot Custom Data (Key, Value)** is enough and needs no hooks class.

## B8 - Versioning (the update workflow)

> **`GAME PROJECT · each time a release changes the save format`**

First, decide which layer you need:

| You did this | Do this |
|---|---|
| Added or removed a **Save Game** variable | Nothing. |
| Changed what a variable **means** (units, enum order) | Raise the object's *Save Version* (Saveable component, or `Version` in the descriptor) and convert in **On Restored** / **OnAfterLoad(SavedVersion)** when `SavedVersion` is lower. |
| Renamed or moved a saveable key, merged or removed objects | A migration step (below), and raise **Current Save Version**. |

### A migration step

Example: release 2 renamed the key `Inv` to `Inventory`.

1. Raise **Current Save Version** in Project Settings from `1` to `2`.
2. Create a Blueprint with parent **SaveMigrationStep**, name it `MS_1_To_2`. In **Class Defaults**: *From Version* `1`, *To Version* `2`.
3. Override **Migrate (Data, Scope)**:
   - **Has Entry** (`Inv`) > **Rename Entry** (`Inv`, `Inventory`);
   - return **true**.
4. Add `MS_1_To_2` to Project Settings > CoreFramework Save > **Migration Steps**.

Now a version 1 file loads: the framework runs the step, the data has version 2, objects restore with the new key. The old file on disk stays version 1 until the player saves again. For release 3 add `MS_2_To_3` the same way; a version 1 file runs both in order.

Rules to remember:

- **Never lower** Current Save Version. A save from a higher version is refused (`VersionTooNew`) and never overwritten.
- Keep every old step in the list as long as old saves may exist. A missing link fails the load with `MigrationFailed` and changes nothing.
- A step must be safe to run on **Profile files** too: check the *Scope* argument if it only applies to one.
- Steps work on keys and values (`Has Entry`, `Remove Entry`, `Rename Entry`, `Get Entry Keys`, `Get Entry Version`, `Get/Set/Remove Value`). They do not rewrite a variable's contents; that is layer 2.

## B9 - Multiplayer notes

> **`GAME PROJECT`**

- Call World save / load only on the **server** (host, dedicated server, standalone). From a client you get `NotAuthority`; show **Result To Text**.
- **Profile** calls work on every machine for its local players.
- A persistent character on a dedicated server: the server's world slot holds one entry per player, keyed by the player's unique id (`"Player." + UniqueId`). The key must be known **when the object registers**, and a Saveable component registers in Begin Play, so give the player-specific object (for example the PlayerState) a C++ `ISaveableInterface` whose `GetSaveDescriptor` builds that key, and call `RegisterSaveable` from the server code that runs once the unique id is set (for example after login). A player who comes back registers with the same key and is restored; a new player simply has no entry.
- Do not call `Start New Game` or `Load Slot` on a client.
- On a listen server the host loads normally; clients receive the restored state through replication.

## B10 - Tests

> **`GAME PROJECT`**

Single player:

1. Add a Saveable component to a test actor with an integer variable (*Save Game* ticked) and a *Save Transform* tick. Place it in a level.
2. Play, change the variable and move the actor. Call **Save To Slot** `Test`. Output Log: no `LogCoreSave` warnings.
3. Change the variable and move the actor again. Call **Load Slot** `Test`: both return to the saved values.
4. Call **List Slots** and print **Get Cached Slots**: `Test` is listed with a timestamp and the level name. **Delete Slot** removes it.
5. Stop and start the game, **Load Slot** `Test`, **Open Level** with the slot's map: the actor restores itself.
6. Raise Current Save Version to 2 without a step and load `Test`: `MigrationFailed`, nothing changes. Add an empty step 1 -> 2: it loads.
7. Lower the version back to 1 and load a version 2 file: `VersionTooNew`.

Slot list page: open the menu tab, create a new save, overwrite it (confirmation if enabled), load it, delete it. The spinner shows while the operation runs; the error text shows `Result To Text`.

Settings page: change a value, press Apply, restart the game: the control shows the saved value. Change another value and press Revert: it returns to the saved value.

Multiplayer (Play As Listen Server, then 2 clients):

| Check | Expected |
|---|---|
| Client calls Save To Slot | `NotAuthority`, status text shown |
| Host saves, then loads | Objects restore on the host and replicate to clients |
| Client changes a profile value and applies | A `CF_Profile_0` file is written on that client's machine |
| Two local players (split-screen) | Each profile keeps its own values |
| Dedicated server | World saves work on the server; `Save Profile` returns `NoLocalPlayer` |

Where the files are: `Saved/SaveGames/` of the project, files `CF_Index.sav`, `CF_Slot_<name>.sav`, `CF_Profile_<n>.sav`. Set `LogCoreSave` to Verbose for detail.

---

## Troubleshooting

| Symptom | Cause and fix |
|---|---|
| `RegisterSaveable: ... does not implement ISaveableInterface, or its Save Key is empty` | The object lacks the interface, or Get Save Descriptor left *Key* empty. |
| `Two saveables use the Save Key ...` | Two live objects share a key. Spawned actors need a unique **Save Key**. |
| An actor does not restore | Its variables are not ticked **Save Game**; or it was spawned (random name) and has no explicit Save Key; or the slot was saved before the actor was added (nothing to restore, defaults apply). |
| An actor restores in the wrong place | *Save Transform* is on but another system moves it after Begin Play. Restore position in **On Restored**. |
| A reference to another actor is empty after load | It pointed at a runtime object. Save a key or id and resolve it in **On Restored**. |
| `NotAuthority` | A network client called a World operation. |
| `NoLocalPlayer` | A profile call before local player 0 exists, or on a dedicated server. |
| `VersionTooNew` | The file is from a higher save version or newer framework. Raise **Current Save Version** (or use the newer build). The file is untouched. |
| `MigrationFailed` | No step from the file's version to the current one, or a step returned false. Add or fix the step. |
| `Blocked` | Save Hooks `Can Save` returned false. |
| `TooManySlots` | **Max Slots** reached. Overwrite a slot or delete one. |
| The slot list is empty although files exist | `CF_Index` is missing or was rebuilt: slots appear again as they are saved. Files are never deleted by the framework. |
| The list page shows nothing | **Entry Class** is empty (a warning says so), or `SlotListPanel` is missing (the Blueprint will not compile). |
| A button of the page does nothing | It is not named exactly, or it is not a Common Button (children of `BaseButton`). UMG buttons must be wired to the **Request ...** nodes in the graph. |
| The first load after a start is late | The save system reads files asynchronously; use the operation's completion or **On Busy Changed** to wait before opening the level. |
