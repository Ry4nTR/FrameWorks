# Input System - Engine Setup

How to configure a game project so the input system works: device events, prompts with glyphs, preferences and key remapping. Concepts: `Input_Architecture_Overview.md`. Settings widgets: `../UISystem/UI_Settings_Widgets.md`.

The framework ships **no glyph textures and no input actions**: icons and mappings belong to each game.

| Step | What | Where | Needed for |
|---|---|---|---|
| 1 | Glyph textures | Game project | prompts with icons |
| 2 | CommonUI controller data (one per gamepad family) | Game project | prompts with icons |
| 3 | Register the controller data for the platform | Project Settings (game) | prompts with icons |
| 4 | Enable Enhanced Input user settings | Project Settings (game) | key remapping |
| 5 | Make the actions player mappable | Input Actions / mapping contexts (game) | key remapping |
| 6 | CoreFramework Input settings | Project Settings (game) | remapping, preferences ranges |
| 7 | Prompts, settings pages, key bindings screen | Widget Blueprints (game) | UI |

---

## Step 1 - Glyph textures

One texture per button, white on transparent (tint them in the widgets), square, 64 or 128 px. Import into the game, for example `Content/UI/Input/Glyphs/<Family>/T_Glyph_<Family>_<Button>`. Texture settings: Compression **UserInterface2D**, Texture Group **UI**, Mip Gen **NoMipmaps**.

Minimum set for a menu: face buttons (A/B/X/Y or Cross/Circle/Square/Triangle), shoulders and triggers, D-pad, sticks (L3/R3), Menu, View.

## Step 2 - CommonUI controller data

For each gamepad family: **Add > Blueprint Class > Common Input Base Controller Data**, for example `BP_InputData_Gamepad_Xbox`.

| Property | Value |
|---|---|
| Input Type | Gamepad |
| Gamepad Name | the family name CommonUI uses (`Generic` is the Windows default; `XSX`, `PS5` ...) |
| Gamepad Display Name | shown in tools |
| Input Brush Data Map | one row per key: Key = `Gamepad Face Button Bottom` ..., Key Brush = the texture, Image Size 64x64 |

Optional: a controller data with Input Type **Mouse and Keyboard** and brushes for keyboard keys, if prompts should show key icons on PC (`Glyph` display).

## Step 3 - Register the controller data

**Project Settings > Game > Common Input Settings > Platform Input > Windows > Default > Controller Data**: add each Blueprint. Set **Default Gamepad Name** to the family used when the hardware is unknown (usually `Generic`).

Stored in `Config/Windows/WindowsGame.ini`:

```ini
[CommonInputPlatformSettings_Windows CommonInputPlatformSettings]
DefaultGamepadName=Generic
+ControllerData=/Game/UI/Input/BP_InputData_Gamepad_Xbox.BP_InputData_Gamepad_Xbox_C
```

CommonUI's own action widgets (`CommonActionWidget`, bound action bars) and the framework prompts now use the same icons.

## Step 4 - Enhanced Input user settings

**Project Settings > Engine > Enhanced Input > User Settings > Enable User Settings** = on. Leave the default User Settings Class and Key Profile Class.

```ini
[/Script/EnhancedInput.EnhancedInputDeveloperSettings]
bEnableUserSettings=True
```

Bindings are saved by Enhanced Input in its own save slot per player.

## Step 5 - Player mappable actions

For every action the player may rebind, open the **Input Action** > **User Settings** > **Player Mappable Key Settings**: create the settings object and fill

| Field | Example |
|---|---|
| Name | `Interact` (unique, used to store the binding) |
| Display Name | `Interact` (shown in the key bindings screen) |
| Display Category | `Actions` (screen header) |

One action with several directional keys (Move with W / A / S / D) needs one name per key: in the **mapping context**, on each mapping, set **Setting Behavior = Override Settings** and give each its own Player Mappable Key Settings (`MoveForward`, `MoveBackward` ...).

Actions of a read-only content folder (a plugin) cannot be edited: duplicate the mapping context into the game and use the per-mapping override there.

## Step 6 - CoreFramework Input settings

**Project Settings > CoreFramework > CoreFramework Input** (`DefaultGame.ini`, `[/Script/CoreFramework.InputFrameSettings]`):

| Property | Default | Meaning |
|---|---|---|
| Default Preferences | 1, 1, off, off, on, on | Values before the player changes them |
| Min / Max Sensitivity, Sensitivity Step | 0.1 / 3.0 / 0.05 | Range of the Controls sliders |
| Keyboard Prompt Display | Key Text | What prompts set to Project Default show on PC |
| Remappable Contexts | empty | **Fill it**: the contexts listed in the key bindings screen, in this order |
| Reserved Keys | Escape, Gamepad Special Right | Never bindable |
| Cancel Capture Keys | Escape, Gamepad Special Left | Stop "press any key" |
| Key Capture Timeout | 8 s | Listening stops by itself |
| Conflict Policy | Swap | Swap / Unbind / Allow / Reject |
| Allow Stick Direction Keys | off | Stick directions as bindings |
| Save Key Bindings On Change | on | Write after every change |

## Step 7 - Widgets

See `../UISystem/UI_Settings_Widgets.md` for the row Blueprints, the Graphics / Controls tabs and the key bindings screen. For prompts:

1. **Add > User Interface > Widget Blueprint**, parent **Input Prompt Widget**, for example `WBP_InputPrompt`.
2. Add, by name: `GlyphImage` (Image), `KeyText` (Common Text), `LabelText` (Common Text). Any layout (horizontal box: glyph, key text, label).
3. Place it and set **Input Action** (gameplay prompts) or **UI Action** (a CommonUI row such as Back), the **Label**, and **Keyboard Display** (`None` for a clean text-only PC look).

## Step 8 - Use from game code

| Need | Call |
|---|---|
| React to device changes | Get Input Frame Subsystem > bind **On Input Device Changed** |
| Key of an action now | **Get Key For Action** (library) / `UInputFrameSubsystem::GetKeyForActionOnCurrentDevice` |
| Aim assistance | **Is Aim Assist Active** |
| Disable look preferences on a character | Class Defaults > Character > Input > **Apply Input Preferences** off |

## Troubleshooting

| Symptom | Fix |
|---|---|
| Gamepad prompt shows text, not an icon | No controller data for that key / family: step 2 and 3. Check Gamepad Name. |
| Key bindings screen says not available | Step 4. |
| Key bindings screen is empty | Step 5 (actions not player mappable) or step 6 (Remappable Contexts empty). |
| A remap does not change gameplay | The context used in gameplay is not the registered one (a copy?), or the action is not player mappable there. |
| Sensitivity sliders do nothing | The character is not an `ABaseCharacter`, or **Apply Input Preferences** is off. |
| Vibration toggle has no effect | The game plays force feedback through another path than the player controller. |
