# Movement Framework - Architecture Overview

Target: Unreal Engine 5.8 | Module macro: `FRAMEWORKS_API` | Files: `BaseCharacterMovementComponent.h/.cpp`

For a one-page summary of domain transitions and input routing see `Movement_Input_Routing.md`.

---

## 1. Purpose

`UBaseCharacterMovementComponent` extends `UCharacterMovementComponent` with three movement **domains** (Ground, Fly, Water), two **control modes** for Fly and Water (Standard and Camera Steering), a set of predicted actions (Sprint, Fly toggle, Dash, Dodge), and a rotation policy that keeps the body upright or lets it tilt toward the movement vector.

It is fully independent. It does not include or reference the State Machine or Animation frameworks. It exposes plain facts, input helpers and one C++ event, and `ABaseCharacter` connects those to the rest of the library.

Dependencies: `Core`, `CoreUObject`, `Engine` only.

---

## 2. Domains

A domain is derived from the engine movement mode on demand. Nothing is stored separately, so it can never disagree with the engine.

| Domain | Engine movement modes | How it is entered |
|---|---|---|
| Ground | `MOVE_Walking`, `MOVE_NavWalking`, `MOVE_Falling` (jumping and falling are part of Ground) | Default. Landing, toggling flight off, leaving water. |
| Fly | `MOVE_Flying` | The Fly input, or pressing Jump while falling (both call `SetWantsToFly(true)`) |
| Water | `MOVE_Swimming` | **Automatically**, when the character overlaps a `PhysicsVolume` with `bWaterVolume` enabled |

There is no swim input or swim intent. The engine's physics-volume logic switches the character into and out of swimming.

### 2.1 Domain toggles

| Property | Effect when unticked |
|---|---|
| `bEnableGroundDomain` | The character starts in flight and cannot land. Requires `bEnableFlyDomain`. |
| `bEnableFlyDomain` | Fly requests are ignored; the character never enters `MOVE_Flying` on its own. |
| `bEnableWaterDomain` | `bCanSwim` is cleared at `BeginPlay`, so the engine never switches the character to `MOVE_Swimming`. |

These flags are read on the client and the server, so set them in the Blueprint defaults rather than at runtime on one side.

### 2.2 Entering and leaving flight

Flight is driven by the intent flag `bWantsToFly`, evaluated at the start of every movement step (`UpdateCharacterStateBeforeMovement`):

| Situation | Result |
|---|---|
| Wants to fly, in Walking, NavWalking or Falling | Switch to `MOVE_Flying` |
| Wants to fly, currently swimming | Stay swimming. Flight resumes when the character leaves the water, because the intent stays set. |
| Does not want to fly, currently flying | Switch to `MOVE_Swimming` if inside a water volume and water is enabled, otherwise to `MOVE_Falling` |
| Ground domain disabled | Intent is forced on; flight cannot be switched off |

Who sets the intent:

- `IA_Fly` calls `ToggleFlying()`.
- `ABaseCharacter::Input_JumpStarted` calls `SetWantsToFly(true)` when Jump is pressed while `IsFalling()`, `bDoubleJumpActivatesFlight` is on and the Fly domain is enabled. On the ground the same press is a normal jump.

Because the mode switch happens inside the movement step, the client predicts it and the server performs it on the same move.

---

## 3. Prediction model

Sprint, Fly, Dash and Dodge are player intent that must reach the server and be replayed after corrections. They travel in the four custom bits of the saved move's compressed flags.

| Flag | Meaning | Lifetime |
|---|---|---|
| `FLAG_Custom_0` | `bWantsToSprint` | Held state |
| `FLAG_Custom_1` | `bWantsToFly` | Held state |
| `FLAG_Custom_2` | `bWantsToDash` | One-shot |
| `FLAG_Custom_3` | `bWantsToDodge` | One-shot |

The implementation has three pieces:

- `FSavedMove_BaseCharacter` stores the four bits (`SetMoveFor`), writes them into the compressed byte (`GetCompressedFlags`), restores them before a replay (`PrepMoveFor`), and refuses to combine moves whose bits differ (`CanCombineWith`).
- `FNetworkPredictionData_Client_BaseCharacter` allocates that saved move type.
- `UpdateFromCompressedFlags` on the server reads the bits back.

**All four custom bits are in use.** Adding another predicted input needs extended network move data (see section 10).

The **control mode is not a separate piece of state.** It is computed from the movement mode and `bWantsToSprint`, so client and server always agree on it, and replays reproduce it.

### 3.1 One-shot actions

Dash and Dodge are requested by setting a flag. The next movement step executes the action and clears the flag, on both the client and the server. Replays after a correction run the same code with the saved flag, so the result is reproduced.

The velocity change is stateless: it sets the velocity to `direction * Speed` once. Friction and braking then decay it. There are no timers inside the movement component, which keeps replays deterministic.

### 3.2 Cooldowns

| Where | Check |
|---|---|
| Owning client | `RequestDash()` / `RequestDodge()` refuse until the cooldown time has passed. |
| Server, remote players | When executing, the server accepts the action only if `now + ServerCooldownTolerance >= next allowed time`. A rejection is corrected by the engine. |
| Listen-server host | Same as an owning client. |
| Replays | Cooldowns are not evaluated and the event is not fired again. |

---

## 4. Ground

| Property | Default | Meaning |
|---|---|---|
| `SprintSpeed` | 700 | Speed while sprinting on the ground or in the air |
| `MaxWalkSpeed` (engine) | 400 | Walking speed |
| `MaxWalkSpeedCrouched` (engine) | 200 | Crouched speed |

`IsSprinting()` is true when sprint intent is set and the character is not crouching. Sprint speed is kept while falling so jumps preserve momentum.

---

## 5. Control modes (Fly and Water)

Sprint selects between two ways of controlling the character while flying or swimming.

| | Standard | Camera Steering |
|---|---|---|
| Active when | Flying or swimming, Sprint **not** held | Flying or swimming, Sprint **held** |
| Move direction | Planar, from the camera **yaw only** | Full 3D, from the camera **yaw and pitch** |
| Up and Down buttons | Ascend / descend straight up and down | Ignored |
| Speed | `MaxFlySpeed` / `MaxSwimSpeed` | `GlideSpeed` / `FastSwimSpeed` |
| Body | Upright | Tilts toward the movement vector (configurable) |

Implementation, all in the movement component:

| Function | Purpose |
|---|---|
| `IsCameraSteering()` | Flying or swimming and sprinting |
| `GetControlMode()` | `Standard` or `CameraSteering` (always `Standard` on the ground) |
| `GetMoveBasisRotation(ControlRotation)` | Yaw-only rotation in Standard mode and on the ground; yaw and pitch in Camera Steering. The character builds the `Move` direction from it. |
| `AcceptsVerticalButtons()` | True in Fly or Water in Standard mode. The character's Up / Down handlers check it. |

The component only **answers** these questions. `ABaseCharacter` calls `AddMovementInput` with the result, which reaches the server as ordinary movement acceleration. AI or other controllers can use the same functions.

In Standard mode, pressing a horizontal direction together with Up or Down gives a diagonal at the normal speed, because the engine caps the total speed.

---

## 6. Fly

### 6.1 Speeds

| Mode | Speed used |
|---|---|
| Standard | `MaxFlySpeed` (default 800) |
| Camera Steering | `GlideSpeed` (default 1400) when `bEnableGlide` is on, otherwise `MaxFlySpeed` |

`IsGliding()` is "flying, sprinting and `bEnableGlide`". `GlideSinkAcceleration` (default 0) adds an optional downward acceleration while gliding. Leave it at 0 for pure camera steering.

### 6.2 Rotation

| Mode | Body while flying |
|---|---|
| Standard | Always upright. First person: yaw follows the camera. Third person: yaw turns toward the planar movement direction. Pitch and roll level out at `FlightUprightInterpSpeed`. |
| Camera Steering, `bOrientRotationToFlight` on | Turns to face the flight vector, pitch included. Levels out (`FlightUprightInterpSpeed`) when there is no input. |
| Camera Steering, `bOrientRotationToFlight` off | Upright, yaw follows the camera. |

`FlightPitchRollRate` (default 240 deg/s) is copied into `RotationRate.Pitch` and `.Roll` at `BeginPlay` when those are lower. The engine's rotation code needs non-zero pitch and roll rates to tilt the body and to bring it upright again after landing.

---

## 7. Water

### 7.1 Automatic swimming

The engine enters swimming when the character overlaps a `PhysicsVolume` with `bWaterVolume` enabled and leaves it when the character exits. The framework adds nothing on top except the domain toggle (`bCanSwim`). There is no input to start, stop or dive.

### 7.2 Surface versus underwater

`GetWaterZone()` measures how deep the character is using the engine's immersion depth (0 = out of the water, 1 = fully submerged):

| Zone | Rule |
|---|---|
| `Surface` | swimming and immersion below `UnderwaterImmersionThreshold` (default 0.95) |
| `Underwater` | swimming and immersion at or above the threshold |
| `None` | not swimming |

Immersion is measured against the water volume's brush, so the **top face of the Physics Volume is the water surface**. `GetWaterZone()` performs a trace: call it once per frame and pass the result to `IsResurfacing(Zone)`.

### 7.3 Control and speeds

| Mode | Move | Up / Down | Speed | Body |
|---|---|---|---|---|
| Standard | Planar, camera yaw | Ascend / descend | `MaxSwimSpeed` (300) | Upright, turns toward the planar direction (third person) or follows the camera yaw (first person) |
| Camera Steering | Full 3D, camera | Ignored | `FastSwimSpeed` (600) | Tilts toward the swim vector if `bOrientRotationToSwim`, otherwise upright and follows the camera yaw |

`SwimUprightInterpSpeed` (default 4) controls how fast the body levels out.

### 7.4 Buoyancy and diving

Swimming physics runs with a buoyancy value chosen each step:

| Condition | Buoyancy |
|---|---|
| Diving (downward input acceleration below `-DiveAccelerationThreshold`) | `NeutralBuoyancy` (1.0) |
| Not diving and `bAutoResurface` is on | `SurfaceBuoyancy` (1.15) |
| Not diving and `bAutoResurface` is off | `NeutralBuoyancy` (the character hovers where it stops) |

Diving is inferred from acceleration, which is identical on client and server, so no extra flag is needed. In Standard mode it comes from the Down button. In Camera Steering it comes from looking down while moving.

`SurfaceBuoyancy` and `UnderwaterImmersionThreshold` are starting values chosen from the engine's buoyancy behavior and not measured in your project. Tune them in a real water volume.

### 7.5 Resurfacing

`IsResurfacing(Zone)` is true when the zone is Underwater, upward speed is above `ResurfaceVerticalSpeed` (40 cm/s), and there is no downward input. `ABaseCharacter` uses it for the `Water.Resurfacing` state.

---

## 8. Rotation policy in one place

`RefreshRotationPolicy()` sets two engine flags (`bOrientRotationToMovement` and the character's `bUseControllerRotationYaw`). It runs at the start of every movement step, on mode changes and from `SetFaceControlRotation`.

| Domain and mode | `bOrientRotationToMovement` | `bUseControllerRotationYaw` |
|---|---|---|
| Ground | `!bFaceControlRotation` | `bFaceControlRotation` |
| Fly / Water, Standard | false | `bFaceControlRotation` |
| Fly / Water, Camera Steering, orient on | true | false |
| Fly / Water, Camera Steering, orient off | false | true |

`PhysicsRotation` adds what the engine does not do: in Fly and Water it levels pitch and roll, and in Standard mode in third person it turns the yaw toward the planar movement direction. While the engine is tilting the body in Camera Steering with input, it leaves the rotation alone.

`bFaceControlRotation` is set by `SetFaceControlRotation`, which `ABaseCharacter` calls on both the client and the server when the perspective changes.

---

## 9. Burst actions (Dash and Dodge)

Each action has its own settings (`FMovementActionSettings`): `bEnabled`, `Speed`, `Cooldown`, `StateDuration`.

| Property | Domain | Action name in gameplay | Default speed | Cooldown | State duration |
|---|---|---|---|---|---|
| `GroundDash` | Ground | Dash | 1400 | 1.0 | 0.35 |
| `GroundDodge` | Ground | Roll | 900 | 0.9 | 0.6 |
| `FlyDash` | Fly | Fly Dash | 2200 | 1.0 | 0.4 |
| `FlyDodge` | Fly | Fly Dodge | 1400 | 0.8 | 0.5 |
| `UnderwaterDash` | Water | Underwater Dash | 1000 | 1.0 | 0.4 |

Water has no dodge, and the water dash only works while the zone is Underwater.

| Action | Domain | With input | Without input |
|---|---|---|---|
| Dash | Ground | Input direction, flattened | Body forward, flattened |
| Dash | Fly, Water | Input direction, full 3D | Body forward |
| Dodge | Ground | Input direction, flattened | Backward |
| Dodge | Fly | Input direction, full 3D | Body right |

The input direction follows the current control mode, so a dash in Standard mode goes where the planar input points and a dash in Camera Steering goes where the camera points. On the ground only the horizontal velocity is replaced. In flight and water the whole velocity is replaced.

`StateDuration` is not used by the movement component. It travels with the `OnActionStarted` event so the animation layer knows how long to show the action.

---

## 10. Facts, events, extending

| Member | Type | Use |
|---|---|---|
| `GetMovementDomain()` | function | Ground, Fly or Water |
| `GetControlMode()`, `IsCameraSteering()` | functions | Standard or Camera Steering |
| `GetMoveBasisRotation(Rot)`, `AcceptsVerticalButtons()` | functions | Input routing |
| `GetWaterZone()` | function | None, Surface, Underwater (performs a trace) |
| `IsResurfacing(Zone)` | function | Rising underwater without dive input |
| `GetImmersionDepth()` | function | 0..1 |
| `IsSprinting()`, `IsGliding()`, `WantsToFly()`, `HasMovementInput()` | functions | Intent and input facts |
| `OnActionStarted(Action, Domain, StateDuration)` | C++ multicast delegate | Fired once per executed Dash or Dodge, on the owning client and on the server, never during replays |

`OnActionStarted` is a native delegate, so C++ code binds it (ABaseCharacter does). Blueprints listen through the State Machine component's events instead.

| Goal | Notes |
|---|---|
| Change speeds or feel | Edit the properties; no code needed. |
| Add a new burst action | Add a settings struct and a case in `GetActionSettings`. It needs its own predicted input bit. |
| Add another predicted input | All four custom bits are used. Use extended network move data (`FCharacterNetworkMoveData` and its container) to carry more inputs. |
| New domain (for example Climb) | Add a value to `EMovementDomain`, a custom movement mode, a case in `GetMovementDomain`, and matching tags in the State Machine framework. |
| Slide, Mantle | Not implemented here; they are gameplay-triggered actions on the character (`RequestAction`). |

---

## 11. Assumptions and limits

- All tuning values must match between client and server. Change Blueprint defaults, not runtime values on one side.
- Dash and Dodge are velocity impulses. There is no distance-exact dash or root-motion dash.
- There is no automatic landing while flying. The Fly input toggles flight off, then the character falls.
- Double-jump takeoff treats **any** falling state as airborne, including a fall off a ledge.
- Standard mode swims or flies in the horizontal plane only; it cannot steer with the camera by design.
- The `SurfaceBuoyancy` and immersion defaults are untested starting values.
- Nothing here was compiled or run in the editor. Use the checklist in `Movement_Blueprint_Setup.md` after the first build.
