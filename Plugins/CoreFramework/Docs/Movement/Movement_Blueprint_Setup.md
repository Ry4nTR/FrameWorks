# Movement Framework - Blueprint Setup

How to configure `UBaseCharacterMovementComponent` in the editor, build test levels for every domain, and tune the feel. A summary of the control rules is in `Movement_Input_Routing.md`.

Prerequisite: `BP_BaseCharacter` exists (see `Character_Blueprint_Setup.md`). `ABaseCharacter` already uses this movement component, so there is nothing to add to the Blueprint.

---

## Step 1 - Open the movement settings

1. Open `BP_BaseCharacter`.
2. In the **Components** panel select **CharacterMovement**.
3. In **Details**, the framework properties are under these categories:

| Category | Contents |
|---|---|
| Movement > Domains | `Enable Ground Domain`, `Enable Fly Domain`, `Enable Water Domain` |
| Movement > Ground | `Sprint Speed` |
| Movement > Fly | `Orient Rotation To Flight`, `Flight Pitch Roll Rate`, `Flight Upright Interp Speed`, `Enable Glide`, `Glide Speed`, `Glide Sink Acceleration` |
| Movement > Water | `Fast Swim Speed`, `Orient Rotation To Swim`, `Swim Upright Interp Speed`, `Underwater Immersion Threshold`, `Surface Buoyancy`, `Neutral Buoyancy`, `Auto Resurface`, `Dive Acceleration Threshold`, `Resurface Vertical Speed` |
| Movement > Actions | `Ground Dash`, `Ground Dodge`, `Fly Dash`, `Fly Dodge`, `Underwater Dash`, `Server Cooldown Tolerance` |

The standard engine properties still apply and are also used by the framework: **Max Walk Speed**, **Max Walk Speed Crouched**, **Max Swim Speed**, **Max Fly Speed**, **Braking Deceleration Flying**, **Jump Z Velocity**, **Air Control**, **Rotation Rate**.

Always change values in the Blueprint defaults. Never change them from client-only logic, or client and server will disagree and every move will be corrected.

The character-level switch for double-jump takeoff is on the **Blueprint's Class Defaults**, not on the movement component: *Character > Input > Double Jump Activates Flight*.

## Step 2 - Choose the game type

Apply one recipe. Then repeat the same choice in the Animation Blueprint (Stage 3 documents). The state machine follows automatically.

| Game type | Enable Ground | Enable Fly | Enable Water | Input actions to leave empty in `DA_InputConfig` |
|---|---|---|---|---|
| Ground only | on | **off** | **off** | Fly, and Dash / Dodge if unused |
| Ground + Water | on | **off** | on | Fly |
| Ground + Flight | on | on | **off** | (none) |
| Flight only | **off** | on | **off** | Fly (flight cannot be toggled off anyway). Keep Crouch mapped: it is the Down button. |
| Everything | on | on | on | (none) |

Notes:

- There is no swim input to clear. Water works through the volumes you place in the level.
- With **Enable Fly Domain** off, the Fly key and the double-jump takeoff do nothing; the second Jump press becomes a normal jump again.
- **Flight only:** the character spawns in flight and the Fly key cannot land it. Keep `Crouch` mapped: it is the Down button.

## Step 3 - Set the base speeds

Recommended starting values (the framework defaults):

| Property | Value | Used in |
|---|---|---|
| Max Walk Speed | 400 | Ground |
| Sprint Speed | 700 | Ground sprint |
| Max Walk Speed Crouched | 200 | Crouch |
| Max Fly Speed | 800 | Fly, Standard mode |
| Glide Speed | 1400 | Fly, Camera Steering (Sprint held) |
| Max Swim Speed | 300 | Water, Standard mode |
| Fast Swim Speed | 600 | Water, Camera Steering (Sprint held) |

Keep each fast speed above its standard speed, otherwise holding Sprint will feel like nothing.

## Step 4 - Tune the burst actions

Each action has four fields: **Enabled**, **Speed** (cm/s), **Cooldown** (s), **State Duration** (s).

| Action | Speed | Cooldown | State Duration |
|---|---|---|---|
| Ground Dash | 1400 | 1.0 | 0.35 |
| Ground Dodge (roll) | 900 | 0.9 | 0.6 |
| Fly Dash | 2200 | 1.0 | 0.4 |
| Fly Dodge | 1400 | 0.8 | 0.5 |
| Underwater Dash | 1000 | 1.0 | 0.4 |

**State Duration should match the length of the animation.** It only controls how long the animation state stays active. The distance covered comes from **Speed** together with friction and braking:

| Want | Change |
|---|---|
| Dash covers more ground | Raise Speed (ground: also lower Ground Friction / Braking Deceleration Walking) |
| Dash feels like a sudden stop | Lower **Braking Deceleration Walking** and **Ground Friction** so the burst decays more slowly |
| Dodge feels sluggish | Raise Speed, lower Cooldown |
| Disable an action in one domain | Untick its **Enabled** |

The direction follows the current control mode: in Standard mode it goes where the planar input points, in Camera Steering it goes where the camera points.

## Step 5 - Set up flight

1. Make sure **Enable Fly Domain** is on. Takeoff works in two ways, and either can be used alone:
   - **Fly key** (`IA_Fly` in the Input Config): toggles flight on and off.
   - **Double jump:** press Jump while airborne (no extra asset needed).
2. Controls while flying:

| Mode | How | Move | Up / Down | Speed |
|---|---|---|---|---|
| Standard | (default) | Planar, camera yaw. Looking up or down does **not** climb or dive. | `Jump` ascends, `Crouch` descends | Max Fly Speed |
| Camera Steering | Hold `Sprint` | Full 3D along the camera: look up + forward climbs | Ignored | Glide Speed |

3. Choose how the body behaves in Camera Steering:

| Setting | Result |
|---|---|
| **Orient Rotation To Flight** on | Body faces the flight vector, tilts up and down, levels out when idle |
| **Orient Rotation To Flight** off | Body stays upright, yaw follows the camera |

Standard mode always keeps the body upright.

4. Tune:

| Symptom | Change |
|---|---|
| Body tilts too slowly or not at all in Camera Steering | Raise **Flight Pitch Roll Rate** (default 240) |
| Body snaps upright when you stop or release Sprint | Lower **Flight Upright Interp Speed** (default 4) |
| Body stays tilted when idle | Raise **Flight Upright Interp Speed** |
| Camera Steering is not faster than Standard | Raise **Glide Speed**, and make sure **Enable Glide** is on |
| You want a slow sink while gliding | Raise **Glide Sink Acceleration** (default 0 = none) |
| You do not want the Glide animation state | Untick **Enable Glide**; Camera Steering still works, at Max Fly Speed |

## Step 6 - Build a water test volume

Swimming needs no input and no script.

1. **Place Actors** panel > **Volumes** > **Physics Volume** (drag it into the level).
2. Scale it to a pool. The character is 176 cm tall, and "underwater" needs the character almost entirely below the top face, so make the pool **at least 500 cm deep**.
3. In the volume's **Details** tick **Water Volume**. Optionally raise **Fluid Friction** for a heavier feel.
4. The **top face of the volume is the water surface**. Align it where you want the surface.
5. Make sure **Enable Water Domain** is on for the character.

Walk in, fall in, or fly in: the character switches to swimming automatically. Leaving the volume ends swimming automatically; if flight was active the character resumes flying.

Controls while swimming:

| Mode | How | Move | Up / Down | Speed |
|---|---|---|---|---|
| Standard | (default) | Planar, camera yaw. Looking down does **not** dive. | `Jump` ascends, `Crouch` descends (dives) | Max Swim Speed |
| Camera Steering | Hold `Sprint` | Full 3D along the camera: look down + forward dives | Ignored | Fast Swim Speed |

| Situation | Behavior |
|---|---|
| Release everything underwater | Floats back up when **Auto Resurface** is on |
| `Dash` | Underwater dash, only once below the surface zone |

### Water tuning

| Symptom | Change |
|---|---|
| Character bobs too high out of the water | Lower **Surface Buoyancy** (toward 1.0) |
| Character sinks at the surface | Raise **Surface Buoyancy** |
| Character never counts as underwater | Lower **Underwater Immersion Threshold** (for example 0.85), or make the pool deeper |
| Character hovers underwater instead of rising | **Auto Resurface** is off |
| Character still rises while trying to dive | Keep **Neutral Buoyancy** at `1.0`. In Standard mode hold `Crouch`; in Camera Steering look down while moving. |
| Diving feels like it needs too much input | Lower **Dive Acceleration Threshold** |
| Resurfacing state triggers too easily | Raise **Resurface Vertical Speed** |
| Body tilts when swimming in Camera Steering and you want it upright | Untick **Orient Rotation To Swim** |
| Body levels out too slowly or quickly | Change **Swim Upright Interp Speed** |

The buoyancy and threshold defaults are starting points. Test them with your own capsule size and volume.

## Step 7 - Blueprint access

Get the component and cast it once:

```
Get Character Movement (from your character) -> Cast To BaseCharacterMovementComponent
```

| Node | Purpose |
|---|---|
| **Set Wants To Sprint** | Sprint on/off. In Fly and Water this also switches the control mode. |
| **Set Wants To Fly** / **Toggle Flying** | Take off or land |
| **Request Dash** / **Request Dodge** | Returns false if unavailable or cooling down |
| **Set Face Control Rotation** | Body follows camera yaw (first person style) or not. The character's perspective functions call it for you. |
| **Get Control Mode**, **Is Camera Steering** | Standard or Camera Steering |
| **Get Move Basis Rotation** (Control Rotation) | The rotation to build the Move direction from. Use it in AI or custom input so they follow the same rules. |
| **Accepts Vertical Buttons** | Whether Up / Down apply right now |
| **Get Movement Domain**, **Get Water Zone**, **Is Sprinting**, **Is Gliding**, **Wants To Fly**, **Has Movement Input**, **Get Immersion Depth** | Read-only facts for HUD and audio |

Call intent nodes on the **locally controlled** character (input events). The framework transmits them to the server. Calling them on a remote client's copy has no effect.

To react to a dash or dodge in Blueprint, use the state machine component's **On Action Started** event, not the movement component. The movement component's own event is C++ only.

## Step 8 - Test and verify

Single player:

| Test | Expected |
|---|---|
| Walk, sprint, crouch, jump | Speeds match the table in Step 3 |
| Jump, then Jump again in the air | Flight starts |
| `F` on the ground or in the air | Toggles flight |
| Fly, hold W and look up or down (no Sprint) | Stays level; pitch does not climb or dive |
| Fly, `Space` / `Ctrl` | Straight up / straight down |
| Fly, hold `Sprint` + W, look up / down | Climbs / dives along the camera; `Space` / `Ctrl` do nothing |
| Release Sprint in flight | Back to Standard immediately; body levels out |
| Walk into the pool | Swimming starts with no input |
| Swim, W, look down (no Sprint) | Stays level; `Ctrl` dives |
| Swim, `Sprint` + W, look down | Dives along the camera |
| Release everything underwater | Floats up |
| `Q` / `E` in each domain | Dash and Dodge; water dash only underwater |

Multiplayer (Play As Client, 2 or more players):

1. Console: `NetEmulation.PktLag 100` (adds latency) and optionally `NetEmulation.PktLoss 2`.
2. Console: `p.NetShowCorrections 1` draws every server correction.
3. Take off by double jump, toggle Sprint repeatedly in flight and in water, dash, dodge, and swim. **Occasional** corrections are normal. **Constant** corrections during one action mean a value differs between client and server or an action is being rejected.
4. Turn off emulation with `NetEmulation.PktLag 0` and `NetEmulation.PktLoss 0`.

## Troubleshooting

| Symptom | Likely cause |
|---|---|
| Fly key and double jump do nothing | **Enable Fly Domain** is off |
| Double jump only does a second jump | **Double Jump Activates Flight** is off on the character, or the Fly domain is disabled |
| Character cannot land | **Enable Ground Domain** is off |
| Character never swims | **Enable Water Domain** is off, or the volume is not a **Water Volume**, or the character never overlaps it |
| W climbs or dives while flying in Standard mode | Sprint is held (Camera Steering), or an old character Blueprint overrides `Input_Move` |
| Up / Down buttons do nothing in flight | Sprint is held, or `IA_Jump` / `IA_Crouch` has a *Pressed* trigger, or the action is empty in the Input Config |
| Dash does nothing in water | The character is at the surface zone. Dive first. |
| Dash does nothing at all | **Enabled** is off for that action, or it is on cooldown |
| Body does not tilt in Camera Steering | **Orient Rotation To Flight** / **Orient Rotation To Swim** is off, or **Flight Pitch Roll Rate** is 0 |
| Body stays tilted after landing | Same rate is too low; raise **Flight Pitch Roll Rate** |
| Sprint changes nothing | The fast speed is not above the standard speed |
| Repeated corrections | Movement values were changed at runtime on one machine only |
| Rubber-banding on dash in multiplayer | Very high latency together with a short cooldown; raise **Server Cooldown Tolerance** or the cooldown |
