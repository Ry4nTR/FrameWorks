# UI System - Settings Widgets (v2.2)

Generic building blocks for a settings menu on top of `UBaseMenuWidget` / `UBaseTabWidget`. The framework ships the logic and the contracts (widget names); the game makes the Widget Blueprints, so one row Blueprint per kind styles every page. Input-related parts use the input system (`Docs/InputSystem/`).

---

## 1. Rows

All rows derive from `UBaseSettingRowWidget` (`UISystem/Settings/`).

| Class | Look | Required widgets | Optional widgets | Event |
|---|---|---|---|---|
| `UBaseSettingRowWidget` | label only (also used as a category header) | - | `LabelText`, `FocusHighlight` | On Row Highlight Changed |
| `USliderSettingRowWidget` | `Label   ----o----   80%` | `ValueSlider` (Slider) | `ValueText` | On Value Changed (Row, Value) |
| `UOptionSettingRowWidget` | `Label   <  High  >` | `SelectorButton` (UBaseButton BP) | `PreviousButton`, `NextButton` (UBaseButton BPs), `ValueText` | On Selection Changed (Row, Index) |
| `UToggleSettingRowWidget` | `Label   <  On  >` | as option | as option | On Toggled (Row, bIsOn) |
| `UKeyBindingRowWidget` | `Action   [ E ]   [ (A) ]` | - | `KeyboardBinding`, `GamepadBinding` (UKeyBindingButton BPs) | On Binding Clicked (Row, bGamepad) |

Shared behavior:

- **Label**: the `Label` property of each placed row (visible in the designer).
- **Highlight**: `FocusHighlight` is visible while the row has keyboard / gamepad focus or the mouse.
- **Focus**: the control (slider, selector, binding box) is focusable, the row is not. Up / Down move between rows (Slate navigation). Left / Right change the value: sliders by one step (no "press Accept to lock"), option rows through `NativeOnNavigation` on the row. The option arrows are mouse-only (never focusable).
- **Values from code**: `SetValue`, `SetOptions`, `SetSelectedIndex`, `SetIsOn` never broadcast unless asked (`bBroadcast`), so pages can refresh rows without loops.

Slider value formats: `Percent` (0.8 -> 80%), `Number`, `Integer`, `Multiplier` (1.2x), with `Decimals`.

## 2. Graphics tab - `UGraphicsSettingsTabWidget`

Rows by name, all optional: `QualityPresetRow`, `WindowModeRow`, `ResolutionRow`, `VSyncRow` (toggle), `FrameRateLimitRow`, `ResolutionScaleRow` (slider), and the ten quality groups `ViewDistanceRow`, `ShadowsRow`, `GlobalIlluminationRow`, `ReflectionsRow`, `AntiAliasingRow`, `TexturesRow`, `EffectsRow`, `PostProcessingRow`, `FoliageRow`, `ShadingRow`.

```
open tab -> Snapshot = engine values (UGameUserSettings), Staged = Snapshot, rows show Staged
preset row -> every quality group = level -> rows refresh
one quality row -> Staged.Quality[group] -> preset row:
      all groups equal  -> options Low..Ultra, selected = that level
      groups differ     -> options Low..Ultra + "Custom", selected = Custom
Apply  -> Staged written to UGameUserSettings -> ApplySettings (saves GameUserSettings.ini) -> Snapshot = Staged
Revert -> Staged = Snapshot -> rows refresh
```

"Custom" is only an option while the groups differ, so it can be shown but not chosen. `StageAutoDetectedQuality()` stages the engine benchmark result. Texts (level names, window modes, frame rate list) are Class Defaults.

## 3. Controls tab - `UControlsSettingsTabWidget`

Rows by name, optional: `MouseSensitivityRow`, `GamepadSensitivityRow` (sliders, range from CoreFramework Input settings), `InvertXRow`, `InvertYRow`, `AimAssistRow`, `VibrationRow` (toggles), `KeyBindingsButton` (UBaseButton). Class Default **Key Bindings Screen Tag**: the screen the button pushes (hidden when key bindings are unavailable).

Values apply at once through `UInputFrameSubsystem::SetPreferences` (the player feels the sensitivity immediately); Apply writes them to disk, Revert restores the values of when the page opened.

## 4. Key bindings screen - `UKeyBindingsScreenWidget`

A screen (`UBaseActivatableWidget`, back handler, UI Only) registered in the UI config like any other screen.

| Widget | Name | Required |
|---|---|---|
| Panel receiving the rows (Vertical Box inside a Scroll Box) | `RowsContainer` | yes |
| Reset button | `ResetAllButton` | no |
| Feedback line | `StatusText` | no |

Class Defaults: **Row Class** (a `UKeyBindingRowWidget` BP), **Category Header Class** (a `UBaseSettingRowWidget` BP, optional), keyboard / gamepad columns, and every text (`{Key}` / `{Action}` placeholders).

Flow: click a box -> `BeginKeyCapture` (all input swallowed) -> the box shows "Press a key" -> key -> `RebindKey` (conflict policy) -> rows refresh, `StatusText` explains a swap / rejection, focus returns to the box. Cancel keys or the timeout stop without change.

`UKeyBindingButton` (a `UBaseButton`): `LabelText` shows the key name or the listening text, optional `KeyGlyph` (Image) shows the gamepad icon.

## 5. Tab transitions

`UBaseMenuWidget` switches content with `TabSwitcher->SetActiveWidget`. Make `TabSwitcher` a **Common Animated Switcher** and set **Transition Type = Fade Only**, a soft **Transition Curve** (Cubic In Out / Ease In Out) and **Transition Duration** (0.25-0.4 s): the old page fades out, the new one fades in, no movement. No code.

## 6. Screen stack changes (v2.2)

### Covered screens keep their state

Before v2.2 a screen covered by another screen of the same stack (Pause Menu -> Settings) was treated as closed: it released the game pause (the game ran behind Settings), played its close sound and reported `IsScreenOpen() == false`. Now `UBaseActivatableWidget` tells "covered" from "closed": at deactivation, a widget that has a newer widget above it in its layer is **covered**.

| | Covered | Closed |
|---|---|---|
| `IsScreenOpen` | true | false |
| `IsCoveredByAnotherScreen` | true | false |
| Pause / look / move suppression | kept (`bKeepActiveEffectsWhileCovered`, default on) | released |
| Close cue | not played | played |
| Open cue when shown again | not played | (next push) played |

Effects of a screen closed while covered (ClearLayer) are released in `NativeDestruct`, as before.

### PausedBackground

`UBaseMenuWidget` shows an optional widget named `PausedBackground` (Background Blur, dim image) only when the game is paused at activation. One Settings Blueprint then looks right in both places: over the Pause Menu (paused: blurred background) and over the Main Menu (not paused: the live level camera).

### GetLayer

`UUIFrameSubsystem::GetLayer(LayerTag)` returns the container of a layer (stack / queue) for advanced use.
