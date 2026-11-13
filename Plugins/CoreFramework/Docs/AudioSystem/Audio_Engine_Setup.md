# Audio System - Engine Setup

How to configure the Unreal Editor so the audio system works, and how to use it from Blueprints. Concepts are explained in `Audio_Architecture_Overview.md`.

The framework ships **no sounds**. The few assets that are the same in every game (the Sound Classes and a template config) are created **once in the framework repo** and then arrive in every game project through the submodule. Everything that is specific to a game (its sounds, cue tags, music) is done **in that game project**.

## Where each step is done

Same convention as `UI_HUD_Setup_Guide.md`:

| Badge | Meaning |
|---|---|
| `FRAMEWORK REPO · once` | You work in the `FrameWorks` project, inside `Plugins/CoreFramework` (Content Browser > Settings > **Show Plugin Content** > *CoreFramework Content*). Done one time, then exported with the subtree split of `HOW-TO-IMPORT-SUBMODULE.md`. Every game project gets the result when it updates the submodule. |
| `GAME PROJECT · once per project` | Short, mostly copy and paste. Needed in every new game. |
| `GAME PROJECT · per game content` | Specific to that game's sounds, screens or levels. Cannot be shared. |

| Step | What | Where | Effort per new game |
|---|---|---|---|
| 1 | Sound Classes `SC_Master` ... `SC_UI` | **Framework repo, once** | none |
| 2 | Template config `DA_AudioConfig_Template` (categories already filled) | **Framework repo, once** | none |
| 2b | Export the plugin branch | **Framework repo, once** (and after each change) | none |
| 3 | Duplicate the template into the game, assign it in Project Settings | Game project, once | 2 minutes |
| 4 | Cue tags | Game project, once (paste an `.ini` block) | 2 minutes |
| 4b | Cue library: which sound plays for which tag | Game project, per game content | the real work of the game's audio |
| 5 | Attenuation and concurrency assets | Game project (optionally shared from the framework, see the step) | per game |
| 6 | MetaSound inputs | Game project, per sound | per game |
| 7 | UI sounds: config defaults, button styles, screens | Game project (the Common Button Style and screens belong to the game) | per game |
| 8 | Volume sliders in the options menu | Game project (the options menu belongs to the game) | per game |
| 9 | Playing sounds, emitters, anim notifies, GameState emitter | Game project, per game content | per game |
| 10 | Tests | Game project | per game |

> **Never edit an asset inside the plugin from a game project.** The plugin is a git submodule: the edit would change the shared repository or be lost at the next update. Duplicate the asset into the game instead (step 3).

> **Could everything be done from zero in each game instead?** Yes, nothing forces you to use the framework's Sound Classes or template. The steps then are 1 and 2 done in the game project (same instructions, game folder instead of plugin folder). It costs about ten minutes per game and gives each game its own Sound Class hierarchy. The recommended way is the shared one because the hierarchy and the category rows are identical in every game, and because a sound that is later moved between games keeps its Sound Class.

---

## Step 1 - Create the Sound Classes

> **`FRAMEWORK REPO · once`**

1. In the Content Browser (Show Plugin Content on) right-click **CoreFramework Content** > **New Folder** > `Audio`. Inside it create the folder `Classes`.
2. In `CoreFramework Content/Audio/Classes`: **Add > Audio > Sound > Sound Class**. Create five assets:

| Asset | Parent Class |
|---|---|
| `SC_Master` | None (root) |
| `SC_SFX` | `SC_Master` |
| `SC_Music` | `SC_Master` |
| `SC_Ambient` | `SC_Master` |
| `SC_UI` | `SC_Master` |

3. Set the parent by opening each child asset and choosing **Parent Class** in the Details panel, or in the Sound Class editor by dragging the child under `SC_Master`.
4. Leave **Volume** at `1.0` on all of them. The framework changes volume at runtime through a Sound Mix; do not set it to a value you want to control from the options menu.
5. Optional: on `SC_UI` tick **Is UI Sound** (it keeps the class audible in pause menus; the framework also does this by itself for the UI category).

Why a hierarchy: **Master scales every child.** The framework applies each category's mix adjuster to its class *and its children*, so moving Master moves every category below it.

In a game project, assign these classes **also on the game's sound assets** (Sound Wave, Sound Cue, MetaSound > Sound Class), where practical. Sounds played through the framework get the right class anyway, but a placed Ambient Sound actor, or a sound played by an engine feature, only follows the sliders if its asset carries a class. The classes are listed under *Plugins > CoreFramework Content > Audio > Classes* in the game's Content Browser.

## Step 2 - Create the template config asset

> **`FRAMEWORK REPO · once`**

The config asset holds both things that are the same in every game (the category rows) and things that are game specific (the cues). A game cannot edit a plugin asset, so the framework ships it as a **template** with the categories done and the cue list empty. Each game duplicates it (step 3).

1. In `CoreFramework Content/Audio`: **Add > Miscellaneous > Data Asset**, choose **Audio Config Data Asset**, name it `DA_AudioConfig_Template`.
2. Open it and fill **Categories** with five rows (the Sound Class column points at the assets of step 1):

| Category (tag) | Sound Class | Default Volume | Play When Paused |
|---|---|---|---|
| `CoreFramework.Audio.Category.Master` | `SC_Master` | 1.0 | off |
| `CoreFramework.Audio.Category.SFX` | `SC_SFX` | 1.0 | off |
| `CoreFramework.Audio.Category.Music` | `SC_Music` | 0.8 | **on** |
| `CoreFramework.Audio.Category.Ambient` | `SC_Ambient` | 1.0 | off |
| `CoreFramework.Audio.Category.UI` | `SC_UI` | 1.0 | (always on) |

3. Leave **Master Sound Mix** empty. The framework then uses an internal mix. Assign a Sound Mix only if you want to add your own passive effects to it.
4. Other settings (defaults are fine to start; a game may change them in its own copy):

| Property | Default | Meaning |
|---|---|---|
| Volume Fade Time | 0.05 s | Smooths slider changes |
| Max Late Cue Seconds | 1.0 s | A cue that loads later than this is dropped |
| Default Music Fade Time | 2.0 s | Crossfade length of `PlayMusic` |
| Default Ambient Fade Time | 3.0 s | Crossfade length of `PlayAmbient` |
| Crossfade Curve | Equal Power | Keeps loudness steady while tracks overlap |

5. Leave **Cues** and **UI Defaults** empty. They are game specific.
6. **Save** the asset.

## Step 2b - Export the plugin

> **`FRAMEWORK REPO · once`** (repeat whenever you add or change a plugin asset or C++ file)

Commit, then run the subtree split and push of `HOW-TO-IMPORT-SUBMODULE.md` (section 1, Steps 1 to 3). Each game project then pulls the new assets and code with `git submodule update --remote --merge Plugins/CoreFramework` and commits the new pointer (section 3 of that guide).

## Step 3 - Duplicate the config and assign it

> **`GAME PROJECT · once per project`**

1. Content Browser, Show Plugin Content on: open *Plugins > CoreFramework Content > Audio*.
2. Right-click `DA_AudioConfig_Template` > **Asset Actions > Duplicate**, choose the game folder `Content/Audio` and the name `DA_AudioConfig`. The copy keeps the five category rows and keeps pointing at the plugin's Sound Classes.
3. **Edit > Project Settings > CoreFramework > CoreFramework Audio** > set **Default Audio Config** to `DA_AudioConfig` (your copy, not the template).

This is stored in the game's `Config/DefaultGame.ini`. It is a project setting, so a plugin cannot set it for you:

```ini
[/Script/CoreFramework.AudioFrameSettings]
DefaultAudioConfig=/Game/Audio/DA_AudioConfig.DA_AudioConfig
```

If it is not assigned, the output log shows `LogCoreAudio: No audio config assigned ...` at start-up.

**When the framework later changes the template** (for example a new category row), the game's copy does not follow: copy the new row by hand.

## Step 4 - Create your cue tags

> **`GAME PROJECT · once per project`**

Cue ids are Gameplay Tags **the game** creates, because the sounds are different in every game. Use the same naming in all your games (`Audio.Cue.<Area>.<Name>`) so this document, your Blueprints and your habits carry over.

Fastest way: paste this block into the game's `Config/DefaultGameplayTags.ini` (create the file if it does not exist, keep the section header once), then add game specific lines:

```ini
[/Script/GameplayTags.GameplayTagsSettings]
+GameplayTagList=(Tag="Audio.Cue.UI.Hover",DevComment="")
+GameplayTagList=(Tag="Audio.Cue.UI.Click",DevComment="")
+GameplayTagList=(Tag="Audio.Cue.UI.PopupOpen",DevComment="")
+GameplayTagList=(Tag="Audio.Cue.UI.PopupClose",DevComment="")
+GameplayTagList=(Tag="Audio.Cue.Music.MainMenu",DevComment="")
+GameplayTagList=(Tag="Audio.Cue.Music.Gameplay",DevComment="")
+GameplayTagList=(Tag="Audio.Cue.Ambient.Forest",DevComment="")
+GameplayTagList=(Tag="Audio.Cue.Footstep.Walk",DevComment="")
+GameplayTagList=(Tag="Audio.Cue.Door.Open",DevComment="")
```

Or in the editor: **Project Settings > Project > Gameplay Tags > Manage Gameplay Tags > Add New Gameplay Tag**.

Only the first four lines are likely to be the same in every game; delete or rename the rest. The five category tags (`CoreFramework.Audio.Category.*`) already exist: they are native and come with the framework.

## Step 4b - Fill the cue library

> **`GAME PROJECT · per game content`**

In the game's `DA_AudioConfig > Cues` add one row per sound:

| Field | What to enter |
|---|---|
| Cue Tag | One of your tags |
| Sound | Any Sound Wave, Sound Cue or MetaSound Source |
| Category | UI for interface sounds, Music for music, Ambient for beds, SFX (or empty) for the rest |
| Volume / Pitch Multiplier | Base level, usually 1 |
| Volume / Pitch Random Range | `0.05` to `0.1` on repeated sounds (footsteps, impacts) hides repetition |
| Attenuation Override | Optional, for 3D sounds (step 5) |
| Concurrency Override | Optional, limits how many copies play at once |
| Preload | On for UI and common sounds, off for rare or big ones |
| Allow Client Request / Min Interval | Only for cues a player's client may ask to replicate (step 9) |
| Reliable | Only for rare, important cues |

**Looping.** The framework never makes a sound loop. For music, ambience and `StartLoop` sounds, make the asset itself loop:

- Sound Wave: tick **Looping**.
- Sound Cue: use a **Looping** node.
- MetaSound Source: build a generator that does not finish (it plays until stopped).

**Long music.** Set the Sound Wave **Loading Behavior** to a streaming mode so only a header stays in memory.

## Step 5 - Spatialization (3D sounds)

> **`GAME PROJECT · per game content`** (optional shortcut: if you want the same falloff in every game, create generic `ATT_*` and `CC_*` assets once in `CoreFramework Content/Audio` like the Sound Classes and reuse them. Falloff distances depend on the game's scale, so most games make their own.)

A cue is 3D when you play it with **Play Cue At Location** or **Play Cue Attached**, and the sound has attenuation. Without attenuation it plays flat in the world.

1. **Add > Audio > Sound > Sound Attenuation**, for example `ATT_SFX_Short` (falloff 100 to 1500 cm, spatialization on).
2. Either assign it on the sound asset (**Attenuation Settings**), or in the cue (**Attenuation Override**), which wins.
3. Optional: **Add > Audio > Sound > Sound Concurrency** to limit voices (for example 4 footsteps at once) and assign it as the cue's **Concurrency Override**.

## Step 6 - MetaSounds

> **`GAME PROJECT · per game content`** (the MetaSounds are the game's sounds)

MetaSound Sources work like any other sound. To drive one at runtime:

1. In the MetaSound, add an **Input** (float, bool or int) and name it, for example `Intensity`.
2. Play the cue and keep the returned handle (`FAudioHandle`).
3. **Set Handle Float Parameter** (Handle, `Intensity`, value). Bool and int versions exist on the audio subsystem.

For music, **Get Layer Handle (Music)** returns the handle of the audible track, so a gameplay script can raise the intensity without keeping its own handle.

## Step 7 - UI sounds

> **`GAME PROJECT · once per project`** for the config defaults, **`per game content`** for buttons and screens. The framework classes (`BaseButton`, `BaseActivatableWidget`) already contain the sound code; the game's button Blueprints, Common Button Styles and screens are made in the game, as the UI system already requires.

### Buttons

1. In `DA_AudioConfig > UI Defaults` set **Default Button Hover Cue** and **Default Button Click Cue** (for example `Audio.Cue.UI.Hover` / `Audio.Cue.UI.Click`, whose cue category is UI).
2. Every `UBaseButton` child now plays them. On a single placed button, **Base UI > Audio** lets you set **Hover Cue**, **Click Cue**, or tick **Silent**.
3. In your **Common Button Style** assets leave the **Hovered Sound**, **Pressed Sound** and **Clicked Sound** slots **empty**. Those slots bypass the Sound Classes and would play a second sound.

### Screens (menus and popups)

1. In `UI Defaults` set **Default Screen Open Cue** and **Default Screen Close Cue**.
2. In the screen Blueprint (child of `BaseActivatableWidget`): **Class Defaults > Base UI > Audio**: tick **Use Default Screen Cues**, or set **Activated Cue** / **Deactivated Cue** for a screen with its own sounds.
3. Leave it off for the gameplay HUD (it stays silent).

### Custom sounds in a widget

Use the node **Play UI Sound** (Cue Tag) from any `BaseUserWidget` or `BaseActivatableWidget`. Outside widgets use **Play UI Cue** from the Audio function library.

## Step 8 - Volume sliders (options menu)

> **`GAME PROJECT · per game content`** (the options menu is the game's UI; only the nodes below come from the framework. Build the slider row once as a reusable widget in the first game, and copy it to the next game with *Migrate* instead of rebuilding it.)

In a settings tab widget:

1. For each slider, in **Event Construct** set its value with **Get Category Volume** (Category tag). Use the pure nodes **Category: Music**, **Category: SFX** and so on, or pick the tag.
2. **On Value Changed** > **Set Category Volume** (Category tag, Volume).
3. Optional: a mute checkbox > **Set Category Muted**.
4. To refresh sliders when something else changes a volume, bind the subsystem event **On Category Volume Changed** (Get Audio Subsystem > Bind Event).
5. Volumes are saved to `GameUserSettings.ini` when the game instance shuts down. Call **Save Audio Settings** on your Apply button to write them at once.

## Step 9 - Playing sounds

> **`GAME PROJECT · per game content`** (which sound plays when is game logic). Nothing to set up for it in the framework: `BaseCharacter` already contains the **Audio Emitter**, and the nodes already exist.

### Local (single machine)

| Need | Node |
|---|---|
| UI or non-positional sound | **Play Cue 2D** |
| Sound in the world | **Play Cue At Location** |
| Sound that follows an object | **Play Cue Attached** (component, optional socket) |
| Music | **Play Music** (Cue Tag, Fade Time optional, Restart If Same optional) |
| Stop music | **Stop Music** (Fade Out optional) |
| Ambient bed | **Play Ambient** / **Stop Ambient** |
| Stop or fade a sound | **Stop Sound** (Handle, Fade Out) |

Start the level's music from the level Blueprint, a GameMode-driven screen, or the character's BeginPlay, for example `Play Music (Audio.Cue.Music.Gameplay)`. Starting the same track again does nothing, so calling it on every level load is safe. The music keeps playing across level travel until you call **Stop Music** or play another track.

### Network (everyone hears it)

1. Use the actor's **Audio Emitter** (`BaseCharacter` already has one). For other actors, **Add Component > Audio Emitter** in the actor Blueprint. The actor must have **Replicates** ticked.
2. **Play Cue** (Cue Tag, Placement, Socket Name):
   - **At Actor**: plays at the actor's location.
   - **Attached To Actor**: follows the actor. Socket Name optional.
   - **Global 2D**: everybody hears it at full volume.
3. Call it **on the server**, or on the **client that owns the actor** (a possessed character). For a client call, tick **Allow Client Request** on that cue in the config, and set **Client Request Min Interval** so the server throttles it (for example 0.1 s).
4. **Start Loop** (Loop Id, Cue Tag) and **Stop Loop** (Loop Id): server only. For example an engine hum that every client hears and that late joiners hear too. The cue's sound must loop.

### Footsteps and animation sounds (no networking)

1. Open an animation sequence or montage.
2. Right-click the notify track > **Add Notify > Play Audio Cue**.
3. Set **Cue Tag** (for example `Audio.Cue.Footstep.Walk`), **Socket Name** (`foot_l`, `foot_r`), **Audience**:
   - **Everyone** for the body mesh,
   - **Owner Only** for first-person arm animations,
   - **Others Only** for a body animation whose first-person counterpart already plays on the arms.
4. Give the cue small **Volume / Pitch Random Range** values and a Concurrency Override.

The notify runs on every machine that plays the animation, so footsteps are correct for all players without any RPC. It does nothing in the animation editor preview; test in Play In Editor.

### Shared match music (optional)

1. Open your project's **GameState** Blueprint and **Add Component > Audio Emitter**.
2. On the server, call **Set Shared Music** (Cue Tag, Fade Time). Every client plays it, including players who join later (from the start of the track). **Clear Shared Music** stops it.
3. Use this on **one** actor only. Games with per-player music skip it and call **Play Music** locally.

## Step 10 - Test it

> **`GAME PROJECT`**. The `FrameWorks` host project is itself a game project, so you can prove the framework side before any real game uses it: do steps 3 and 4b there (duplicate the template into `Content/Audio` of the host project, add one temporary cue) and run the single-player checks below. Do not put that test copy inside `Plugins/CoreFramework`.

### Single player

1. Press Play. The Output Log should not show `LogCoreAudio` warnings.
2. Trigger a UI cue, a 2D cue and a 3D cue; check each hears the right category.
3. Move the SFX and Music sliders; the matching sounds change, Master changes everything.
4. Call **Play Music** with a second track: the first fades out while the second fades in.
5. Open the pause menu: UI sounds and music (if Play When Paused is on) keep playing.
6. Restart the game: the volumes are back.

### Multiplayer

Play > Net Mode **Play As Listen Server**, then **Play As Client** with 2 players, and repeat with a **dedicated server** (editor preferences: Run Under One Process).

| Check | Expected |
|---|---|
| A client uses a cue flagged Allow Client Request | The owner hears it once, immediately; other players hear it once |
| The same call on a cue without Allow Client Request | The owner hears it; nobody else does |
| Another player walks past | Footsteps from the anim notify are heard, positioned |
| Player far away (beyond Net Cull Distance) | No event received, no sound |
| Server calls Start Loop, a third client then joins | The late client hears the loop |
| Dedicated server | No `LogCoreAudio` errors and no sound playing on the server |

Set `LogCoreAudio` to Verbose (Output Log > filter, or console `Log LogCoreAudio Verbose`) to see rejected client requests and late cues.

---

## Troubleshooting

| Symptom | Cause and fix |
|---|---|
| `No audio config assigned` | Step 3 was not done, or the asset path in `DefaultGame.ini` is wrong. |
| `Cue X is not in the audio config` | The tag exists, but no row in `Cues` uses it. |
| `Cue X has no sound assigned` | Fill the **Sound** field of that row. |
| `Audio category X has no Sound Class` | Fill the **Sound Class** of that category row. |
| Sliders have no effect | The category row has no Sound Class; or the sound is not played through the framework and its asset has no Sound Class. |
| Master slider seems too quiet or too loud | Check that the Sound Classes are **children of `SC_Master`** and that no class has a Volume other than 1. |
| Music stops when the level changes | It does not by default. Something calls **Stop Music**, or the level Blueprint starts a different track. |
| Music plays once and ends | The sound asset does not loop (step 4b). |
| A UI button plays two sounds | The CommonUI button style still has a Hovered/Clicked sound. Clear it. |
| The HUD plays a "menu open" sound | **Use Default Screen Cues** is ticked on the HUD screen. Untick it. |
| 3D sound has no falloff | The sound has no attenuation (step 5). |
| Other players do not hear a replicated cue | The actor does not replicate; the cue lacks Allow Client Request (client call); or the call happened on a client that does not own the actor. |
| Notify is silent in the animation editor | By design. Test in Play In Editor. |
| First sound after start is late or missing | The cue is not preloaded: tick **Preload**, or accept that a cue loading longer than **Max Late Cue Seconds** is dropped. |
