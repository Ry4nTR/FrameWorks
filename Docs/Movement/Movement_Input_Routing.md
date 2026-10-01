# Movement Framework - Domain Transitions, Control Modes and Input Routing

Target: Unreal Engine 5.8 | Applies to: `ABaseCharacter`, `UBaseCharacterMovementComponent`, `UInputConfigData`

This page is the one-stop summary of how movement works after the swim/fly refactor. The deeper documents are `Movement_Architecture_Overview.md` and `Character_Architecture_Overview.md`.

---

## 1. What changed

| Area | Before | Now |
|---|---|---|
| Swim input | `IA_Swim` held to dive | **Removed.** Swimming is fully automatic through a `PhysicsVolume` with `bWaterVolume` enabled. |
| Starting flight | `IA_Fly` toggle | `IA_Fly` toggle **and** pressing Jump while airborne (double-jump takeoff). |
| Fly / Water steering | Camera pitch always steered | Two control modes, chosen by holding Sprint: **Standard** and **Camera Steering**. |
| Vertical buttons | Always active in Fly and Water | Active in Standard mode only. Ignored while Sprint is held. |
| Glide | Forced forward push and constant sink | Camera Steering flight at `GlideSpeed`. The sink is optional and off by default. |
| Body rotation | One flight setting | Standard keeps the body upright. Camera Steering can tilt it (flight and swim have separate switches). |
| Handler names | `Input_*Completed` | `Input_JumpEnded`, `Input_CrouchEnded`, `Input_SprintEnded`. `Input_SwimHeld` no longer exists. |

---

## 2. Domain transitions

A domain is derived from the engine movement mode.

| Domain | Movement modes |
|---|---|
| Ground | Walking, NavWalking, Falling |
| Fly | Flying |
| Water | Swimming |

| Transition | Trigger | Automatic? |
|---|---|---|
| Ground to Fly | `IA_Fly` pressed, **or** Jump pressed while the character is falling | No, player action |
| Fly to Ground | `IA_Fly` pressed again (the character then falls and lands normally) | No, player action |
| Any to Water | The character overlaps a `PhysicsVolume` with **Water Volume** enabled | **Yes** |
| Water to Ground | The character leaves the volume | **Yes** |
| Water to Fly | The character leaves the volume while flight is still wanted | **Yes** |
| Fly toggled while swimming | The intent is stored and applies after leaving the water | n/a |

Rules:

- Double-jump takeoff needs `bDoubleJumpActivatesFlight` on the character (default on) and the Fly domain enabled. Otherwise the second press is a normal jump again.
- The Fly input and double-jump takeoff are independent: clearing `Fly` in the Input Config leaves takeoff by double-jump working, and vice versa.
- Clearing **Enable Water Domain** prevents the engine from ever switching the character to swimming.
- Flight-only games (Ground disabled) start in flight and cannot land.

---

## 3. Control modes (Fly and Water)

The mode is **Camera Steering when the character is flying or swimming and Sprint is held**, otherwise **Standard**. On the ground Sprint only changes speed.

| | Standard | Camera Steering |
|---|---|---|
| How to get it | Default in Fly and Water | Hold `IA_Sprint` |
| `Move` (WASD) | Planar, along camera **yaw only**. Camera pitch never changes the direction. | Full 3D along the camera, **pitch included**. Look up + forward climbs, look down + forward dives. |
| Up (`IA_Jump`) | Ascend straight up | Ignored |
| Down (`IA_Crouch`) | Descend straight down | Ignored |
| Flying speed | `MaxFlySpeed` | `GlideSpeed` (if `bEnableGlide`, otherwise `MaxFlySpeed`) |
| Swimming speed | `MaxSwimSpeed` | `FastSwimSpeed` |
| Body | Always upright | May tilt toward the movement vector (`bOrientRotationToFlight`, `bOrientRotationToSwim`) |
| Animation state | Fly Move / Surface or Underwater Move | Fly Glide / Surface or Underwater Fast Swim |
| Diving in water | Hold Down | Look down while moving |

Diagonal movement in Standard mode: W plus Up produces a diagonal at the normal speed (the engine caps total speed), not faster.

---

## 4. Input routing

Each row is what the input does in each situation.

| Input | Ground | Fly, Standard | Fly, Steering | Water, Standard | Water, Steering |
|---|---|---|---|---|---|
| `Move` | planar, yaw | planar, yaw | 3D, camera | planar, yaw | 3D, camera |
| `Jump` (press) | jump; if already falling, takeoff | nothing | nothing | nothing | nothing |
| `Jump` (hold) | nothing | Up | ignored | Up | ignored |
| `Crouch` (press) | crouch | nothing | nothing | nothing | nothing |
| `Crouch` (hold) | nothing | Down | ignored | Down | ignored |
| `Sprint` | run | enter Steering | (held) | enter Steering | (held) |
| `Fly` | take off | land | land | intent stored | intent stored |
| `Dash` | ground dash | fly dash | fly dash | underwater dash, only when underwater | same |
| `Dodge` | roll | fly dodge | fly dodge | none | none |

Who decides what:

- `ABaseCharacter` only forwards input.
- `UBaseCharacterMovementComponent` answers the questions: `GetMoveBasisRotation(ControlRotation)` for the Move direction, `AcceptsVerticalButtons()` for Up / Down, `IsCameraSteering()` / `GetControlMode()` for the mode. AI or other input sources can call the same functions.

---

## 5. Body rotation

| Domain and mode | Third person | First person (face camera) |
|---|---|---|
| Ground | Turns toward the movement direction | Follows camera yaw |
| Fly / Water, Standard | Upright, turns toward the planar movement direction | Upright, follows camera yaw |
| Fly, Steering | `bOrientRotationToFlight` on: tilts toward the flight vector, levels out with no input. Off: upright, follows camera yaw. | same |
| Water, Steering | `bOrientRotationToSwim` on: tilts toward the swim vector. Off: upright, follows camera yaw. | same |

The policy is re-evaluated at the start of every movement step, so toggling Sprint switches the rotation behavior on the same move for client and server.

---

## 6. Movement states reported to the state machine

| Domain | Rule | Tag |
|---|---|---|
| Ground | falling | `Ground.Jump` |
| Ground | crouching | `Ground.Crouch` |
| Ground | no input or not moving | `Ground.Idle` |
| Ground | moving and Sprint | `Ground.Run` |
| Ground | moving | `Ground.Walk` |
| Fly | moving and Steering with glide enabled | `Fly.Glide` |
| Fly | moving (Standard, or glide off) | `Fly.Move` |
| Fly | not moving | `Fly.Idle` |
| Water, surface | not moving / moving / moving + Sprint | `Water.Surface.Idle` / `.Move` / `.FastSwim` |
| Water, underwater | not moving / moving / moving + Sprint | `Water.Underwater.Idle` / `.Move` / `.FastSwim` |
| Water, underwater | rising without downward input | `Water.Resurfacing` |

Crossing the water surface also plays a short `Water.Transition.ToUnderwater` or `ToSurface` action. The animation layer needs no change.

---

## 7. Migration checklist (assets and defaults)

### Input assets

1. **Compile.** The `Swim` field disappears from `FCharacterInputActions`. Open `DA_InputConfig_Default`: the Swim entry is gone. Nothing else in it changes.
2. **Delete** `IA_Swim` and remove its mapping (`C`) from `IMC_Character`.
3. Keep `IA_Jump`, `IA_Crouch` and `IA_Sprint` with **no triggers**. The Up / Down buttons need the held event every frame.
4. `IA_Fly` stays as it is (for example `F`).
5. Nothing new needs to be added to the Input Config.

### Character Blueprint

| Property | Where | Default | Note |
|---|---|---|---|
| Double Jump Activates Flight | Class Defaults, Character > Input | on | Off keeps Jump as a plain (multi-)jump while airborne |

### Movement component

| Property | Status | Default |
|---|---|---|
| Orient Rotation To Swim | new | on |
| Swim Upright Interp Speed | new | 4 |
| Glide Sink Acceleration | default changed | 0 (was 250; an explicitly saved value is kept) |
| Orient Rotation To Flight | meaning narrowed | applies in Camera Steering only |
| Flight Upright Interp Speed | meaning widened | applies in Standard mode and in Camera Steering without input |

### Level

Water is a **Physics Volume with Water Volume ticked**. The top face of the volume is the surface. No script or trigger is needed.

### Animation

No Animation Blueprint change is needed. The Glide pose now plays only while the character is moving.

---

## 8. Test checklist

| Test | Expected |
|---|---|
| Jump on the ground | One normal jump |
| Jump, then Jump again in the air | Flight starts |
| `F` on the ground and in the air | Toggles flight |
| Fly, hold W, look up and down (no Sprint) | Stays level, camera pitch does not climb or dive |
| Fly, hold `Space` / `Ctrl` | Straight up / straight down |
| Fly, hold Sprint + W, look up | Climbs toward the camera. `Space` / `Ctrl` do nothing. |
| Fly, hold Sprint + W, look down | Dives. Body tilts with Orient To Flight on. |
| Walk into a Water Volume | Swimming starts on its own; no key needed |
| Swim, hold W, look down (no Sprint) | Stays level; `Ctrl` dives |
| Swim, hold Sprint + W, look down | Dives along the camera; buttons ignored |
| Leave the volume while flying | Flight resumes |
| Two-player test with `p.NetShowCorrections 1` | Only occasional corrections while switching modes |
