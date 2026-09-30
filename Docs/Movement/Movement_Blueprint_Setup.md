# Movement Framework - Blueprint Setup

How to configure `UBaseCharacterMovementComponent` in the editor, build a test level for every domain, and tune the feel.

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
| Movement > Water | `Fast Swim Speed`, `Underwater Immersion Threshold`, `Surface Buoyancy`, `Neutral Buoyancy`, `Auto Resurface`, `Dive Acceleration Threshold`, `Resurface Vertical Speed` |
| Movement > Actions | `Ground Dash`, `Ground Dodge`, `Fly Dash`, `Fly Dodge`, `Underwater Dash`, `Server Cooldown Tolerance` |

The standard engine properties still apply and are also used by the framework: **Max Walk Speed**, **Max Walk Speed Crouched**, **Max Swim Speed**, **Max Fly Speed**, **Braking Deceleration Flying**, **Jump Z Velocity**, **Air Control**, **Rotation Rate**.

Always change values in the Blueprint defaults. Never change them from client-only logic, or client and server will disagree and every move will be corrected.

## Step 2 - Choose the game type

Apply one recipe. Then repeat the same choice in the state machine (automatic, see below) and in the Animation Blueprint (Stage 3).

| Game type | Enable Ground | Enable Fly | Enable Water | Input actions to leave empty in `DA_InputConfig` |
|---|---|---|---|---|
| Ground only | on | **off** | **off** | Fly, Swim, Dash/Dodge if unused |
| Ground + Water | on | **off** | on | Fly |
| Ground + Flight | on | on | **off** | Swim |
| Flight only | **off** | on | **off** | Swim, Crouch |
| Everything | on | on | on | (none) |

What is automatic:

- `ABaseCharacter` copies these three flags into its `StateMachine` component when the game starts, so the state machine agrees with the movement component.
- The Animation Blueprint has its own domain toggles (`Enable Ground Domain` and so on in its Class Defaults). Set them to the same values.

For a **flight-only** game, the character spawns in flight and the Fly key does nothing (flight cannot be switched off). Keep `Orient Rotation To Flight` on for a diving, aligned body, or off for an upright body that turns with the camera.

## Step 3 - Set the base speeds

Recommended starting values (they are the framework defaults):

| Property | Value |
|---|---|
| Max Walk Speed | 400 |
| Sprint Speed | 700 |
| Max Walk Speed Crouched | 200 |
| Max Swim Speed | 300 |
| Fast Swim Speed | 600 |
| Max Fly Speed | 800 |
| Glide Speed | 1400 |

Make sure **Sprint Speed** is above **Max Walk Speed**, and **Glide Speed** above **Max Fly Speed**; otherwise sprinting will feel like nothing.

## Step 4 - Tune the burst actions

Each action has four fields: **Enabled**, **Speed** (cm/s), **Cooldown** (s), **State Duration** (s).

| Action | Speed | Cooldown | State Duration |
|---|---|---|---|
| Ground Dash | 1400 | 1.0 | 0.35 |
| Ground Dodge (roll) | 900 | 0.9 | 0.6 |
| Fly Dash | 2200 | 1.0 | 0.4 |
| Fly Dodge | 1400 | 0.8 | 0.5 |
| Underwater Dash | 1000 | 1.0 | 0.4 |

**State Duration should match the length of the animation** (or the part you want to show). It only controls how long the animation state stays active. The distance covered comes from **Speed** together with friction and braking:

| Want | Change |
|---|---|
| Dash covers more ground | Raise Speed (ground: also lower Ground Friction / Braking Deceleration Walking) |
| Dash feels like a sudden stop | Lower **Braking Deceleration Walking** and **Ground Friction** so the burst decays more slowly |
| Dodge feels sluggish | Raise Speed, lower Cooldown |
| Disable an action in one domain | Untick its **Enabled** |

## Step 5 - Set up flight

1. Make sure `IA_Fly` is set in your Input Config and **Enable Fly Domain** is on.
2. Choose the rotation style:

| Setting | Result |
|---|---|
| **Orient Rotation To Flight** on | Body faces the flight vector, tilts up and down, levels out when idle |
| **Orient Rotation To Flight** off | Body stays upright, yaw follows the camera |

3. Tune the tilt:

| Symptom | Change |
|---|---|
| Body tilts too slowly or not at all | Raise **Flight Pitch Roll Rate** (default 240) |
| Body snaps upright when you stop | Lower **Flight Upright Interp Speed** (default 4) |
| Body stays tilted when idle | Raise **Flight Upright Interp Speed** |

4. Glide: hold Sprint while flying.

| Symptom | Change |
|---|---|
| Glide sinks too fast | Lower **Glide Sink Acceleration** (default 250) |
| Glide barely descends | Raise **Glide Sink Acceleration** |
| Glide is not much faster than flying | Raise **Glide Speed** |
| No glide at all | **Enable Glide** is off |

5. Controls while flying: `Move` steers along the camera (looking up climbs), `Jump` ascends, `Crouch` descends, `Sprint` glides, `Dash` and `Dodge` use the Fly settings.

## Step 6 - Build a water test volume

1. **Place Actors** panel > **Volumes** > **Physics Volume** (drag it into the level).
2. Scale it to a pool. The character is 176 cm tall, and "underwater" needs the character almost entirely below the top face, so make the pool **at least 500 cm deep**.
3. In the volume's **Details** tick **Water Volume**. Optionally raise **Fluid Friction** for a heavier feel.
4. The **top face of the volume is the water surface**. Align it where you want the surface.
5. Make sure **Enable Water Domain** is on for the character.

Walk in from the edge or drop in from above: the character switches to swimming automatically.

| Control | Water behavior |
|---|---|
| `Move` | Swim along the camera (looking down dives) |
| `Swim` (hold) | Dive |
| `Crouch` (hold) | Dive |
| `Jump` (hold) | Ascend |
| `Sprint` | Fast swim |
| `Dash` | Underwater dash (only once below the surface zone) |
| Release everything underwater | Floats back up when **Auto Resurface** is on |

### Water tuning

| Symptom | Change |
|---|---|
| Character bobs too high out of the water | Lower **Surface Buoyancy** (toward 1.0) |
| Character sinks at the surface | Raise **Surface Buoyancy** |
| Character never counts as underwater | Lower **Underwater Immersion Threshold** (for example 0.85), or make the pool deeper |
| Character hovers underwater instead of rising | **Auto Resurface** is off |
| Character still rises while trying to dive | Keep **Neutral Buoyancy** at `1.0` and check that dive input reaches the character (`Swim` or `Crouch` mapped, or the camera pitched down while moving) |
| Diving feels like it needs too much input | Lower **Dive Acceleration Threshold** |
| Resurfacing state triggers too easily | Raise **Resurface Vertical Speed** |

The buoyancy and threshold defaults are starting points. Test them with your own capsule size and volume, and adjust.

## Step 7 - Blueprint access

Get the component and cast it once:

```
Get Character Movement (from your character) -> Cast To BaseCharacterMovementComponent
```

| Node | Purpose |
|---|---|
| **Set Wants To Sprint** | Sprint on/off (for example from an AI or a UI button) |
| **Set Wants To Fly** / **Toggle Flying** | Take off or land |
| **Request Dash** / **Request Dodge** | Returns false if unavailable or cooling down |
| **Set Face Control Rotation** | Body follows camera yaw (first person style) or not. Call on server and client, or use the character's perspective functions instead |
| **Get Movement Domain**, **Get Water Zone**, **Is Sprinting**, **Is Gliding**, **Wants To Fly**, **Has Movement Input**, **Get Immersion Depth** | Read-only facts for HUD and audio |

Call intent nodes on the **locally controlled** character (input events). The framework transmits them to the server. Calling them on a remote client's copy has no effect.

To react to a dash or dodge in Blueprint, use the state machine component's **On Action Started** event (Stage 3), not the movement component. The movement component's own event is C++ only.

## Step 8 - Test and verify

Single player:

| Test | Expected |
|---|---|
| Walk, sprint, crouch, jump | Speeds match the table in Step 3 |
| `F` | Takes off. `F` again lands (unless flight-only). |
| Hold `Sprint` while flying | Faster travel, slow sink |
| Fly, then release all input | Body levels out (with Orient To Flight on) |
| Walk into the pool | Swimming starts; the surface is the top face of the volume |
| Hold `Swim` | Dives until Underwater |
| Release | Floats up (with Auto Resurface on) |
| `Q` / `E` in each domain | Dash and Dodge move the character; Water dash only underwater |

Multiplayer (Play As Client, 2 or more players):

1. Console: `NetEmulation.PktLag 100` (adds latency) and optionally `NetEmulation.PktLoss 2`.
2. Console: `p.NetShowCorrections 1` draws every server correction.
3. Sprint, take off, land, glide, dash, dodge and swim. **Occasional** corrections are normal. **Constant** corrections during one action mean a value differs between client and server or an action is being rejected.
4. Turn off emulation with `NetEmulation.PktLag 0` and `NetEmulation.PktLoss 0`.

## Troubleshooting

| Symptom | Likely cause |
|---|---|
| Fly key does nothing | **Enable Fly Domain** is off, or `IA_Fly` is empty in the Input Config |
| Character cannot land | **Enable Ground Domain** is off |
| Character never swims | **Enable Water Domain** is off, or the volume is not a **Water Volume** |
| Dash does nothing in water | The character is at the surface zone. Dive first. |
| Dash does nothing at all | **Enabled** is off for that action, or it is on cooldown |
| Body does not tilt in flight | **Orient Rotation To Flight** is off, or **Flight Pitch Roll Rate** is 0 |
| Body stays tilted after landing | Same rate is too low; raise **Flight Pitch Roll Rate** |
| Sprint changes nothing | **Sprint Speed** is not above **Max Walk Speed** |
| Repeated corrections while sprinting or flying | Movement values were changed at runtime on one machine only |
| Rubber-banding on dash in multiplayer | Very high latency together with a short cooldown; raise **Server Cooldown Tolerance** or the cooldown |
