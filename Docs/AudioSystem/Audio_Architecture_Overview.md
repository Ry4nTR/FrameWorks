# Audio System - Architecture Overview

Target: Unreal Engine 5.8 | Module macro: `COREFRAMEWORK_API` | Folder: `Source/CoreFramework/{Public,Private}/AudioSystem/`

---

## 1. Purpose

The audio system gives every game the same everyday audio features without per-game audio code:

- sound effects, 2D or spatialized in the world, one-shot or looping;
- UI sounds (clicks, hovers, popups);
- music and ambience with crossfade;
- volume per category (Master, SFX, Music, Ambient, UI) that is saved per player;
- any sound asset type: Sound Wave, Sound Cue, MetaSound Source.

It is built on the engine's own audio (Audio Components, Sound Classes, Sound Mix, MetaSounds). It adds a **name-based cue library**, **one local manager** and **one network component**, and nothing else.

Design rules:

| Rule | How it is met |
|---|---|
| Modular | Nothing in the audio system knows another framework. Other frameworks call it, never the reverse (UI widgets, the character and the animation notify are callers). |
| Genre agnostic | No game concepts: categories and cues are data (Gameplay Tags + one config asset). |
| Multiplayer ready | Events and state replicate, audio never does. Every machine plays through its own manager. |
| Simple | Game code only knows a cue tag. Sound asset, category, spatialization and variation are data. |

---

## 2. Files and dependencies

```
AudioSystem/
    Core/
        AudioTypes.h                        Enums, FAudioHandle, FAudioCueDefinition, FAudioCategoryDefinition, LogCoreAudio
        AudioGameplayTags.h/.cpp            Native category tags (FAudioTags)
        AudioFrameSettings.h                Project Settings entry that points at the config asset
    Data/
        AudioConfigDataAsset.h              The one data asset: categories, cues, fades, default UI cues
    Manager/
        AudioFrameSubsystem.h/.cpp          UAudioFrameSubsystem: local playback, volume, layers, handles
    Components/
        AudioEmitterComponent.h/.cpp        UAudioEmitterComponent: replicated cue events, loops, shared music
    Library/
        AudioFunctionLibrary.h/.cpp         Blueprint shortcuts (world-context nodes)
    Notifies/
        AnimNotify_PlayAudioCue.h/.cpp      "Play Audio Cue" animation notify
```

| Dependency | Type |
|---|---|
| `Core`, `CoreUObject`, `Engine` | Module (audio components, Sound Classes, `FAudioDevice`, asset streaming) |
| `GameplayTags` | Module (cue ids, categories, native tags) |
| `DeveloperSettings` | Module (Project Settings page) |

No new module dependency was added to `CoreFramework.Build.cs`. No other framework is required.

Touched existing classes (all optional hooks, none required for audio to work): `ABaseCharacter` (owns an `AudioEmitter`), `UBaseButton`, `UBaseActivatableWidget`, `UBaseUserWidget` (UI sounds). See section 9.

---

## 3. Class breakdown

| Class | Kind | Lives | Role |
|---|---|---|---|
| `UAudioFrameSubsystem` | `UGameInstanceSubsystem` | Per game instance (one per machine, one per PIE client) | Plays sounds on **this machine**. Owns volume state, music/ambient layers, the cue library, handles. |
| `UAudioConfigDataAsset` | `UPrimaryDataAsset` | Content Browser, one per game | The data: categories, Sound Classes, cues, fade defaults, default UI cues. |
| `UAudioFrameSettings` | `UDeveloperSettings` | `DefaultGame.ini` | Points at the config asset (Project Settings > CoreFramework > CoreFramework Audio). |
| `FAudioTags` | native Gameplay Tags | C++ | `CoreFramework.Audio.Category.{Master,SFX,Music,Ambient,UI}`. |
| `FAudioCueDefinition` | struct | In the config asset | A named sound plus how it is played. |
| `FAudioHandle` | struct | Returned by Play calls | Local id used to stop or adjust a sound later. |
| `UAudioEmitterComponent` | replicated `UActorComponent` | On any actor | Makes a sound event or loop happen on every machine that should hear it. |
| `UAudioFunctionLibrary` | Blueprint library | Static | Subsystem calls with a world-context pin. |
| `UAnimNotify_PlayAudioCue` | `UAnimNotify` | In animation assets | Plays a cue locally from animation. |

### Why a GameInstance subsystem

- Volumes and music must **outlive a map**; a world subsystem would stop the music at every travel.
- A machine has **one audio device**, also in split-screen, so a per-`ULocalPlayer` subsystem (like `UUIFrameSubsystem`) would duplicate state that is not per player.
- `Project_Integration_Guide.md` rule 3: reusable game-instance logic is a `UGameInstanceSubsystem`, never a `UGameInstance` subclass.

---

## 4. Categories and volume

A **category** is a Gameplay Tag mapped to a **Sound Class** in the config asset.

```
SC_Master                      CoreFramework.Audio.Category.Master
   +-- SC_SFX                  CoreFramework.Audio.Category.SFX
   +-- SC_Music                CoreFramework.Audio.Category.Music
   +-- SC_Ambient              CoreFramework.Audio.Category.Ambient
   +-- SC_UI                   CoreFramework.Audio.Category.UI
```

The Sound Class hierarchy is created by you in the editor (see `Audio_Engine_Setup.md`). A project may add categories (for example `CoreFramework.Audio.Category.Voice`): add a tag, a Sound Class and one row in the config asset.

### How volume is applied

1. The subsystem keeps one volume (0..1) and one mute flag per category.
2. On the first sound of a world, and again for every new world, it sets a **base Sound Mix** on that world's audio device. If the config names no mix, an internal empty mix is used. The mix only carries the volumes.
3. `SetCategoryVolume` calls `SetSoundMixClassOverride(Mix, SoundClass, Volume, ... bApplyToChildren = true)`.
4. `bApplyToChildren` is **true on purpose**. The audio device propagates the Sound Class *asset* properties down the hierarchy first and applies the sound mix adjusters afterwards, so an adjuster that does not apply to children changes only its own class (with `false`, the Master slider reached no sound at all: every sound lives in a child class). One adjuster per category class, applied recursively, multiplies Master into each child exactly once. (Fixed 2026-11-13.)
5. **Master** therefore scales every other category without any code of its own.

### Forced category

Every sound played through the framework gets its category's Sound Class as `SoundClassOverride` on the Audio Component. A sound whose asset has no Sound Class (or a wrong one) still obeys the sliders when it is played through a cue. Sounds that are **not** played through the framework (a placed Ambient Sound actor, an engine-spawned sound) only obey the sliders if their asset has a Sound Class assigned.

### Persistence

Volumes and mute flags are written to `GameUserSettings.ini`, section `[CoreFramework.Audio]`, keys `Volume.<CategoryTag>` and `Mute.<CategoryTag>`. They are loaded when the game instance starts and win over the config's `DefaultVolume`. Setting a value only updates the in-memory config; the file is written by `SaveAudioSettings` (Blueprint-callable) and when the game instance shuts down.

### Pause behavior

The Audio Component flag `bIsUISound` decides whether a sound keeps playing while the game is paused. It is set for the **UI** category always, and for any category with `bPlayWhenPaused` ticked in the config (tick it for Music).

---

## 5. Cues

A cue is one row of the config asset (`FAudioCueDefinition`). Game code passes the tag only.

| Field | Meaning |
|---|---|
| `CueTag` | Unique id (a Gameplay Tag you create, for example `Audio.Cue.Door.Open`). |
| `Sound` | Soft reference to any `USoundBase`: Sound Wave, Sound Cue or MetaSound Source. |
| `Category` | Which category the sound is routed to. Empty means SFX (UI functions use UI). |
| `VolumeMultiplier`, `VolumeRandomRange` | Base volume and a random spread (+/- fraction per play). |
| `PitchMultiplier`, `PitchRandomRange` | Base pitch and a random spread. Hides repetition (footsteps, impacts). |
| `AttenuationOverride` | Replaces the asset's attenuation for 3D playback. |
| `ConcurrencyOverride` | Replaces the asset's concurrency (voice limits). |
| `bPreload` | Load at start-up (default on). |
| `bAllowClientRequest`, `ClientRequestMinInterval` | Multiplayer: whether and how often a client may ask the server to replicate this cue. |
| `bReliable` | Multiplayer: server events for this cue are sent reliably. |

### Loading

- `bPreload` cues are requested in one batch when the subsystem starts. The handle is kept, so they stay in memory.
- A cue that is not loaded is requested asynchronously and **plays when it has loaded**. The Play call returns a handle immediately; `StopSound` on that handle cancels the pending play.
- A cue that finishes loading more than `MaxLateCueSeconds` (default 1 s) after the request is **dropped**: a footstep that arrives a second late is worse than none.
- Cues that were loaded on demand stay resident until the game instance shuts down.

### Resolution of one play

`volume = request volume x cue.VolumeMultiplier x random(1 +/- VolumeRandomRange)`, the same for pitch. Category: cue category, else the caller's fallback (UI functions), else SFX.

---

## 6. Playback API

All on `UAudioFrameSubsystem`, mirrored by `UAudioFunctionLibrary` for Blueprints. Everything is **local**.

| Group | Functions |
|---|---|
| Cues | `PlayCue2D`, `PlayCueAtLocation`, `PlayCueAttached`, `PlayUICue`, `PlayUIEvent` |
| Raw sound assets | `PlaySound2D`, `PlaySoundAtLocation`, `PlaySoundAttached` (a category tag is passed instead of a cue) |
| Handles | `StopSound(Handle, Fade)`, `IsSoundPlaying`, `SetHandleVolume`, `SetHandlePitch`, `SetHandleFloat/Bool/IntParameter` |
| Layers | `PlayMusic`, `PlayMusicSound`, `StopMusic`, `PlayAmbient`, `PlayAmbientSound`, `StopAmbient`, `GetLayerHandle` |
| Volume | `SetCategoryVolume`, `GetCategoryVolume`, `SetCategoryMuted`, `IsCategoryMuted`, `SaveAudioSettings`, event `OnCategoryVolumeChanged` |
| Library | `HasCue`, C++ `FindCue` |

### 2D, 3D and spatialization

Which function you call decides:

- `...2D`: no position, no spatialization (UI, stings, music-like sounds).
- `...AtLocation`: a fixed point in the world. A short sound that is out of earshot is not created at all (the engine skips inaudible one-shots).
- `...Attached`: follows a component and optional socket; stops when its owner is destroyed.

Distance attenuation, panning and spatialization settings come from the cue's `AttenuationOverride`, else from the attenuation asset on the sound itself. A sound asset without attenuation plays without distance falloff.

### Handles

`FAudioHandle` is a local number. It stays safe after the sound ended (all functions ignore stale handles). Handles are **never** sent over the network. Keep one when the sound must be stopped or driven: loops, MetaSound parameters.

MetaSound inputs are driven with `SetHandleFloatParameter(Handle, "InputName", Value)` (also bool and int). The input name is the MetaSound's input node name.

### The sound asset decides whether it loops

The framework never forces a loop. A looping sound is a **Sound Wave with Looping ticked**, a Sound Cue with a Looping node, or a MetaSound that never finishes. A non-looping asset played as music or as a loop plays once and ends.

---

## 7. Music and ambience (layers)

Two layers, **Music** and **Ambient**. Each has one audible track and at most one track that is fading out.

```
PlayMusic(CueB)
   Current (CueA) --FadeOut--> Previous --stop--> destroyed
   new component  --FadeIn --> Current (CueB)
```

- Fade time: argument, or negative = `DefaultMusicFadeTime` / `DefaultAmbientFadeTime` of the config. `0` cuts.
- Fade curve: `CrossfadeCurve` of the config (default Equal Power, which keeps loudness steady while two tracks overlap).
- Asking for the track that already plays does nothing unless `bRestartIfSame` is true.
- A third request while the second fades cuts the oldest track at once.
- A newer request, or `StopMusic`, cancels a track that is still loading.
- Tracks are created against the **audio device**, not the world, and flagged to ignore level flushing, so **music keeps playing across map travel**. A new `PlayMusic` after travel crossfades normally.
- Category: the cue's category, else Music (or Ambient on the ambient layer).
- `GetLayerHandle(Layer)` returns the handle of the audible track, for example to drive a MetaSound "Intensity" input.

---

## 8. Multiplayer

**Principle: audio is presentation. Replicate events and state, never audio.** Every machine plays sounds through its own `UAudioFrameSubsystem`.

### 8.1 Four cases

| Case | Mechanism | Network cost |
|---|---|---|
| Local feedback and anything derived from replicated state (UI, ambience, footsteps from animation) | Local calls (`UAudioFunctionLibrary`, subsystem, anim notify) | None |
| One-shot that everyone near the source must hear | `UAudioEmitterComponent::PlayCue` | One unreliable multicast of a Gameplay Tag |
| Looping sound that follows an actor | `StartLoop` / `StopLoop` on the emitter | A small replicated array with a RepNotify |
| Music the whole match shares | `SetSharedMusic` on an emitter (normally on the GameState) | One replicated tag with a RepNotify |

Prefer the first row whenever possible. Footsteps, weapon-swing sounds and anything tied to an animation need no networking: animation already runs on every machine that sees the character, so each machine plays its own sound.

### 8.2 One-shot flow (`PlayCue`)

```
Server calls PlayCue                          Owning client calls PlayCue
   |                                              |
   |                                    plays at once (no latency)
   |                                              |
   |                                    ServerPlayCue (unreliable RPC)
   |                                              |
   |                       validate: cue exists, Allow Client Request on,
   |                                  not faster than Client Request Min Interval
   v                                              v
 Multicast (unreliable; reliable if cue.bReliable)
   |  bSkipOwner = false                          |  bSkipOwner = true
   v                                              v
 every machine plays through its own UAudioFrameSubsystem
 (the owner skips it when bSkipOwner: it already played)
```

- The event is a **Gameplay Tag** (net-indexed, one to two bytes) plus placement and socket name. No asset reference is sent.
- Who receives it is decided by the owning actor's **relevancy and net cull distance**. A far-away player is not sent the event at all.
- Placement `AtActor` plays at the actor's location when the event arrives; `Attached` follows the actor (optional socket on one of its skeletal meshes); `Global2D` is heard by everybody at full volume.
- Same pattern as `ABaseCharacter::PlayActionMontages` (owner predicts, server multicasts with skip-owner).

**Validation on the server.** A client could otherwise make any sound play on every other machine. The server ignores a request unless the cue has `bAllowClientRequest` ticked, and drops requests that arrive faster than `ClientRequestMinInterval`. Rejection is silent (Verbose log). `bAllowClientRequest` is off by default.

**Who may replicate.** Only the server, or the client that owns the actor (a Player Controller, a possessed pawn). A client that calls `PlayCue` on an actor it does not own hears the sound locally; nothing is sent. The owning actor must replicate (`bReplicates`); on a non-replicated actor the event stays on the server.

### 8.3 Loops

`StartLoop(LoopId, CueTag)` and `StopLoop(LoopId)` run on the **server** only. They edit a replicated array of `{LoopId, CueTag}`. Every machine, on a RepNotify, compares the array with what it plays and starts, replaces or stops local loops (attached to the owner, optional socket `LoopAttachSocket`, fade `LoopFadeOutTime`).

Because the loop is **state**, not an event, a client that joins later, or for whom the actor becomes relevant later, starts the correct loops. Loops stop when the component ends play.

### 8.4 Shared music

`SetSharedMusic(CueTag, Fade)` replicates one cue; every client plays it as music. `ClearSharedMusic` returns the property to its default, which clients see as "stop", and which players who join later never receive. A late joiner starts the track from its beginning (positions are not synchronized).

Use it on **one** actor per world, normally the GameState. A game with per-player or per-area music never calls it and drives `PlayMusic` locally.

### 8.5 Machine types

| Machine | Behavior |
|---|---|
| Dedicated server | The subsystem exists (its cue library validates client requests) but creates no audio objects. Every Play call returns an invalid handle. The emitter still relays. |
| Listen server | The host is a normal local player: the server branch plays locally too. |
| Client | Plays locally, and predicts for owned actors. |
| Split-screen | One device and one mix per machine, so there is one subsystem per game instance, not per local player. |
| PIE with several clients in one process | Each PIE client has its own game instance and therefore its own subsystem. |

---

## 9. Integration with other frameworks

### Character

`ABaseCharacter` creates an `AudioEmitter` (`UAudioEmitterComponent`) next to `StateMachine` and `CameraManager`. Use it for sounds that other players must hear and that animation does not drive (voice lines, ability casts, a vehicle engine loop). Nothing else in the character changed.

### Animation

Add **Play Audio Cue** (`UAnimNotify_PlayAudioCue`) to any animation or montage. It plays locally and sends nothing. Properties: cue, socket, `bAttachToMesh`, volume/pitch multipliers, and **Audience**:

| Audience | Use |
|---|---|
| Everyone | Footsteps on the body mesh, seen by all |
| Owner Only | First-person arm animations (only the owner sees the arms) |
| Others Only | Body animations whose first-person version already plays on the arms, to avoid a double sound |

The notify does nothing in the animation editor preview (no game instance); test in Play In Editor.

### UI

| Class | Addition |
|---|---|
| `UBaseButton` | `HoverCue`, `ClickCue`, `bSilent`. An empty cue uses `DefaultButtonHoverCue` / `DefaultButtonClickCue` of the config. Leave the Hovered / Clicked sound slots of the CommonUI button style **empty**: they go through Slate, not through the Sound Classes, and would double up. |
| `UBaseActivatableWidget` | `ActivatedCue`, `DeactivatedCue`, `bUseDefaultScreenCues`. Default off, so the gameplay HUD (also a screen) stays silent. Open plays on the first activation only. Close plays on the first deactivation. A screen that is covered and re-activated on the same CommonUI stack does not repeat the open sound. |
| `UBaseActivatableWidget`, `UBaseUserWidget` | `PlayUISound(CueTag)` for custom sounds. |

UI cues go to the UI category unless the cue names another one, and keep playing while the game is paused.

### Game code

```cpp
// Local, anywhere with a world context
UAudioFunctionLibrary::PlayCueAtLocation(this, FGameplayTag::RequestGameplayTag("Audio.Cue.Door.Open"), GetActorLocation());

// Heard by everybody (server, or the owning client)
AudioEmitter->PlayCue(DoorOpenCue, EAudioPlacement::AtActor);

// Music
UAudioFunctionLibrary::PlayMusic(this, MusicTagForThisLevel);
```

Keep tags in a project tag struct or use Blueprint tag pickers; do not request tags by string in hot paths.

---

## 10. Extending

| Goal | How |
|---|---|
| New category (Voice, Footsteps) | Create the tag, a Sound Class under `SC_Master`, add a row to `Categories`. Use it on cues and in volume calls. No C++. |
| New sound | One new row in `Cues` and a tag. No C++. |
| Different attenuation for one sound | `AttenuationOverride` on the cue. |
| More than two layers | Add values to `EAudioLayer` and a `FAudioLayerState` member plus its two cases in `GetLayer`. The playback code is generic. |
| Ducking (music lowers under dialogue) | Add a Sound Mix with a Music adjuster to the config and push or pop it. The volume mix is a normal `USoundMix`, so engine ducking tools apply. Not built in. |
| Surface-based footsteps | Resolve the cue tag from the physical surface in game code, then call a Play function. The framework stays surface agnostic. |

---

## 11. Graceful degradation and limits

| Situation | Behavior |
|---|---|
| No config asset assigned | One warning in `LogCoreAudio`. Cues and category volumes are unavailable. `PlaySound*` with raw assets still works. |
| Cue tag not in the library | Warning, invalid handle, nothing plays. |
| Category without a Sound Class | Warning at start. Its volume cannot be applied. |
| Dedicated server, `-nosound` | No audio objects. All calls return invalid handles. |
| Widget or notify in an editor preview | No game instance, no sound, no error. |

Limits:

- **Cues stay in memory** after their first use (until the game instance ends). Turn `bPreload` off for rare sounds, and make long music tracks streaming Sound Waves (Sound Wave > Loading Behavior), which keep only a small header resident.
- **Shared music does not synchronize the playback position.** A late joiner starts the track from the beginning.
- **Unreliable one-shots can be lost.** Tick `bReliable` only for rare, important cues.
- **`AtActor` is a snapshot.** A fast-moving source should use `Attached`.
- **A client cannot replicate a sound from an actor it does not own.** Route it through a server function on an owned actor (for example the Player Controller).
- **`SetBaseSoundMix`** is used for the volume mix: a project that sets its own base mix through World Settings would replace it. Add your effects to the framework's mix instead (config `MasterSoundMix`).
- Not included: ducking, music playlists or a music stack, voice-over queue, a ready-made volume settings tab.
- **Nothing in this system was compiled or run in the editor.** Expect to fix compile errors on the first build.
