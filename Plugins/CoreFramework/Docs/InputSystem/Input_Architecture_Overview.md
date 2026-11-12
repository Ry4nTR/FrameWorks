# Input System - Architecture Overview

Target: Unreal Engine 5.8 | Module macro: `COREFRAMEWORK_API` | Folders: `Source/CoreFramework/{Public,Private}/InputSystem/` and `.../UISystem/Input/`, `.../UISystem/Settings/` (widgets) | Since framework v2.2

---

## 1. Purpose

Every game repeats the same input chores around Enhanced Input and CommonUI. The input system does them once:

- **Device detection with an event**: keyboard / mouse or gamepad, and which gamepad family, with a Blueprint event when it changes.
- **Prompts**: the key an action uses (player remaps included) and its icon, in a widget that switches by itself between a gamepad glyph and plain key text.
- **Player input preferences**: mouse sensitivity, gamepad sensitivity, invert X / Y, aim assist, vibration; saved and applied.
- **Key remapping**: list, rebind (with a conflict policy), reset, "press any key" capture; on the Enhanced Input user settings.
- **Settings widgets** (UI system): generic rows (slider, option, toggle), a Graphics tab, a Controls tab and a Key Bindings screen.

Design rules:

| Rule | How it is met |
|---|---|
| No duplicate data | Glyphs are the CommonUI controller data assets (Project Settings > Common Input Settings), not a second icon table. Key bindings are the Enhanced Input user settings, not a second save. |
| Modular | `InputSystem/` knows no widget. The UI system calls it (prompts, settings pages). The character calls it for look preferences. |
| Genre agnostic | Aim assist is only a stored wish (`IsAimAssistActive`): the game decides what assistance means. |
| Data driven | Ranges, reserved keys, conflict policy, remappable contexts: Project Settings > CoreFramework > CoreFramework Input. Texts are Class Defaults. Rows bind by widget name. |

---

## 2. Files

```
InputSystem/
    Core/
        InputFrameTypes.h          FInputPreferences, FInputFrameKeyBinding, EInputPromptKeyDisplay,
                                   EKeyRebindConflictPolicy, EKeyRebindResult, LogCoreInput
        InputFrameSettings.h       UInputFrameSettings (Project Settings page)
    Manager/
        InputFrameSubsystem.h      UInputFrameSubsystem (ULocalPlayerSubsystem)
        KeyCaptureInputProcessor   (Private) "press any key" Slate pre-processor
    Library/
        InputFunctionLibrary.h     Blueprint shortcuts with a world context
UISystem/
    Input/
        InputPromptWidget.h        UInputPromptWidget: glyph / key text / label
    Settings/
        BaseSettingRowWidget.h     label + focus highlight + focus target
        SliderSettingRowWidget.h   slider + formatted value
        OptionSettingRowWidget.h   < value > selector (Left / Right)
        ToggleSettingRowWidget.h   On / Off (an option row with two values)
        GraphicsSettingsTabWidget.h  scalability preset with automatic Custom, display settings
        ControlsSettingsTabWidget.h  input preferences + entry to key bindings
        KeyBindingButton.h         one binding box (glyph or key name, listening text)
        KeyBindingRowWidget.h      action name + keyboard box + gamepad box
        KeyBindingsScreenWidget.h  the remapping screen
```

Touched existing classes: `ABaseCharacter` (look preferences in `Input_Look`, `bApplyInputPreferences`), `UBaseActivatableWidget` (covered screens keep their pause, see `UI_Settings_Widgets.md` section 6), `UBaseMenuWidget` (optional `PausedBackground`), `UUIFrameSubsystem` (`GetLayer`).

No new module dependency: `EnhancedInput`, `CommonInput`, `CommonUI`, `UMG`, `Slate`, `DeveloperSettings` were already listed.

---

## 3. `UInputFrameSubsystem`

One per local player (split-screen safe), created by the engine. Find it with `UInputFrameSubsystem::Get(AnyObject)`: a widget uses its owning player, a controller / pawn its local player, anything else the first local player of its world (world-space widgets, actors).

### 3.1 Device

```
CommonUI UCommonInputSubsystem (already tracks the last used device)
   OnInputMethodChangedNative (type change and gamepad family change)
        -> UInputFrameSubsystem::HandleInputMethodChanged (ignores exact repeats)
              -> OnInputDeviceChanged(ECommonInputType, GamepadName)   Blueprint event
```

`IsUsingGamepad`, `GetCurrentInputType`, `GetCurrentGamepadName` are pure queries. Widgets bind to the event instead of polling.

### 3.2 Keys and glyphs

`GetKeyForAction(Action, bGamepad)` looks in three places, first hit wins:

1. The mappings applied to the player now (`QueryKeysMappedToAction`, player remaps included).
2. The player-mappable rows of the Enhanced Input user settings (contexts registered but not applied, for example the gameplay context in the main menu).
3. The default mappings of the **Remappable Contexts** (when user settings are off).

`GetKeyGlyph(Key)` asks `UCommonInputPlatformSettings::TryGetInputBrush` for the gamepad family in use, falling back to the platform default family. `GetKeyDisplayText(Key)` is the short engine name ("E", "Esc", "LMB").

`OnInputBindingsChanged` fires after a remap / reset (user settings changed) and when Enhanced Input rebuilt the mappings (a context added or removed), so prompts follow both.

### 3.3 Preferences

`FInputPreferences` lives in memory and in `GameUserSettings.ini`, section `[CoreFramework.Input]` (`[CoreFramework.Input.1]` for local player 1 ...). `SetPreferences` applies at once and writes the in-memory config; `SavePreferences` flushes to disk (also done when the local player goes away), same pattern as the audio volumes.

| Preference | Applied by |
|---|---|
| Mouse / gamepad sensitivity, invert X / Y | `ApplyLookPreferences`, called by `ABaseCharacter::Input_Look` when `bApplyInputPreferences` (default on). The device is the one CommonUI reports at that moment. |
| Vibration | `APlayerController::bForceFeedbackEnabled` of the local player (on change and when the controller changes). |
| Aim assist | Nothing. `IsAimAssistActive()` = wish and gamepad in use. The game reads it (wider interaction traces, target magnetism ...). |

Sensitivity is clamped to `MinSensitivity`..`MaxSensitivity` of the settings.

### 3.4 Key bindings

A thin layer over `UEnhancedInputUserSettings` (requires Project Settings > Engine > Enhanced Input > **Enable User Settings**):

- The **Remappable Contexts** are registered when the local player starts (and again when the controller changes, in case Enhanced Input creates its settings late), so the screen works in the main menu.
- `GetKeyBindings()` returns one `FInputFrameKeyBinding` per player-mappable mapping name, ordered like the contexts and their mappings. The device of a slot is the device of its **default** key, so a keyboard slot stays the keyboard slot while it is unbound.
- `RebindKey(Name, bGamepad, NewKey)`:
  1. Reserved keys and keys of the wrong device are refused.
  2. Conflicts = other mappings whose current key is `NewKey`, **except** mappings whose default key equals this mapping's default key (actions that share a button by design, for example two context-dependent uses of the left mouse button).
  3. Conflict policy: `Swap` (default, the other action gets the old key), `Unbind`, `Allow`, `Reject`.
  4. `MapPlayerKey`, `ApplySettings`, `AsyncSaveSettings` (when Save Key Bindings On Change).
- `ResetKeyBinding`, `ResetAllKeyBindings`, `SaveKeyBindings`.

### 3.5 Key capture

`BeginKeyCapture(bGamepad, Callback)` registers `FKeyCaptureInputProcessor` as the **first** Slate input pre-processor:

```
press on the binding box (CommonUI click)
   -> BeginKeyCapture -> processor registered at index 0
next key down (any widget, CommonUI action, game: nobody sees it)
   cancel key (Escape, gamepad View) -> Callback(Invalid, cancelled)
   key of the other device / analog / stick direction -> swallowed, keep listening
   valid key -> Callback(Key, false)
timeout (Key Capture Timeout) -> Callback(Invalid, cancelled)
afterwards: the processor swallows the releases of the presses it swallowed, then unregisters itself on the next tick
```

The release of the key that **started** the capture (gamepad A, Enter) is not swallowed, so CommonUI never keeps that button pressed.

---

## 4. Prompt widget

`UInputPromptWidget` (UI system) resolves its key from, in order: explicit `KeyboardKey` / `GamepadKey`, `InputAction` (through the subsystem), `UIAction` (a CommonUI input action row such as Back). Display:

| Device | Shows |
|---|---|
| Gamepad | `GlyphImage` with the icon; `KeyText` only when no icon exists |
| Keyboard / mouse | per `KeyboardDisplay`: `None` (label only, clean PC look), `Key Text` ("Esc"), `Glyph` (icon when the controller data has one), `Project Default` (the setting) |

It refreshes on `OnInputDeviceChanged` and `OnInputBindingsChanged`. In the designer it previews the gamepad look with the platform default icons. Use it in menu footers, HUD hints and world-space interaction prompts.

---

## 5. Limits

- Preferences are per machine (`GameUserSettings.ini`), per local player index; key bindings are per player (Enhanced Input user settings save file).
- Key bindings need the Input Actions (or the context mappings) to be **player mappable** with a name: a mapping without Player Mappable Key Settings is invisible to the screen.
- Device switches follow CommonUI's thresholds (a resting stick does not switch to gamepad).
- Aim assist has no built-in behavior.
- Nothing in this system was compiled in the framework repository itself: it was compiled once in a scratch copy of a game project (see `FrameworkPatches/README.md` of the delivering project).
