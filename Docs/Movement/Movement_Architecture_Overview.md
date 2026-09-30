# Movement Framework - Architecture Overview

Target: Unreal Engine 5.8 | Module macro: `FRAMEWORKS_API` | Files: `BaseCharacterMovementComponent.h/.cpp`

---

## 1. Purpose

`UBaseCharacterMovementComponent` extends `UCharacterMovementComponent` with three movement **domains** (Ground, Fly, Water), a set of predicted actions (Sprint, Fly toggle, Dash, Dodge), glide, and a rotation policy that supports both upright and flight-aligned bodies.

It is fully independent. It does not include or reference the State Machine or Animation frameworks. It exposes plain facts and one C++ event, and `ABaseCharacter` connects those to the rest of the library.

Dependencies: `Core`, `CoreUObject`, `Engine` only.

---

## 2. Domains

A domain is derived from the engine movement mode on demand. Nothing is stored separately, so it can never disagree with the engine.

| Domain | Engine movement modes |
|---|---|
| Ground | `MOVE_Walking`, `MOVE_NavWalking`, `MOVE_Falling` (jumping and falling are part of Ground) |
| Fly | `MOVE_Flying` |
| Water | `MOVE_Swimming` |

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

Because the switch happens inside the movement step, the client predicts it and the server performs it on the same move.

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

**All four custom bits are now in use.** Adding another predicted input needs extended network move data (see section 9).

### 3.1 One-shot actions

Dash and Dodge are requested by setting a flag. The next movement step executes the action and clears the flag, on both the client and the server. Replays after a correction run the same code with the saved flag, so the result is reproduced.

The velocity change itself is stateless: it sets the velocity to `direction * Speed` once. Friction and braking then decay it. There are no timers inside the movement component, which is what keeps replays deterministic.

### 3.2 Cooldowns

| Where | Check |
|---|---|
| Owning client | `RequestDash()` / `RequestDodge()` refuse until the cooldown time has passed. |
| Server, remote players | When executing, the server accepts the action only if `now + ServerCooldownTolerance >= next allowed time`. A rejection is corrected by the engine. |
| Listen-server host | Same as an owning client. |
| Replays | Cooldowns are not evaluated and the event is not fired again. |

`ServerCooldownTolerance` (default 0.15 s) absorbs small clock differences between client and server.

---

## 4. Ground

| Property | Default | Meaning |
|---|---|---|
| `SprintSpeed` | 700 | Speed while sprinting on the ground or in the air |
| `MaxWalkSpeed` (engine) | 400 | Walking speed |
| `MaxWalkSpeedCrouched` (engine) | 200 | Crouched speed |

`IsSprinting()` is true when sprint intent is set and the character is not crouching. Sprint speed is kept while falling so jumps preserve momentum.

---

## 5. Fly

### 5.1 Speeds

| Mode | Speed used |
|---|---|
| Flying | `MaxFlySpeed` (default 800) |
| Gliding | `GlideSpeed` (default 1400) |

### 5.2 Glide

Glide is sprint intent while flying (`bEnableGlide` must be on). While gliding, the flight physics step adds:

- a forward push along the body's forward vector (input still steers because it is blended in), and
- a constant downward acceleration of `GlideSinkAcceleration` (default 250 cm/s^2).

The result is fast forward travel that sinks slowly.

### 5.3 Rotation: `bOrientRotationToFlight`

| Value | Body while flying | Camera relation |
|---|---|---|
| `true` | Turns to face the flight vector, including pitch. Levels out (`FlightUprightInterpSpeed`) when there is no input. | Camera is independent (third person) |
| `false` | Stays upright and turns only in yaw, following the camera | Body yaw follows the camera |

`FlightPitchRollRate` (default 240 deg/s) is copied into `RotationRate.Pitch` and `.Roll` at `BeginPlay` when those are lower. The engine's rotation code needs non-zero pitch and roll rates to tilt the body in flight and to bring it upright again after landing.

### 5.4 Rotation policy on Ground and Water

The movement component owns two flags on the character: "use controller yaw" and "orient to movement". It sets them from one place (`RefreshRotationPolicy`), called on every movement mode change and when `SetFaceControlRotation` is called.

| Domain | Face control rotation = true (first person) | Face control rotation = false (third person) |
|---|---|---|
| Ground, Water | Body yaw follows the camera | Body turns toward the movement direction |
| Fly | `bOrientRotationToFlight` decides (table above); the face flag is not used | |

`ABaseCharacter` calls `SetFaceControlRotation` on both client and server when the perspective changes. Any other code that changes the policy must do the same.

---

## 6. Water

### 6.1 Surface versus underwater

`GetWaterZone()` measures how deep the character is using the engine's immersion depth (0 = out of the water, 1 = fully submerged):

| Zone | Rule |
|---|---|
| `Surface` | swimming and immersion below `UnderwaterImmersionThreshold` (default 0.95) |
| `Underwater` | swimming and immersion at or above the threshold |
| `None` | not swimming |

Immersion is measured against the water volume's brush, so the **top face of the Physics Volume is the water surface**.

`GetWaterZone()` performs a trace. Call it once per frame and pass the result to `IsResurfacing(Zone)`.

### 6.2 Buoyancy

Swimming physics runs with a buoyancy value chosen each step:

| Condition | Buoyancy |
|---|---|
| Diving (downward input acceleration below `-DiveAccelerationThreshold`) | `NeutralBuoyancy` (1.0) |
| Not diving and `bAutoResurface` is on | `SurfaceBuoyancy` (1.15) |
| Not diving and `bAutoResurface` is off | `NeutralBuoyancy` (the character hovers where it stops) |

Diving is inferred from acceleration, which is identical on client and server, so no extra flag is needed. Looking down while swimming forward, holding Swim, or holding Crouch all count as diving.

`SurfaceBuoyancy` and `UnderwaterImmersionThreshold` are starting values chosen from the engine's buoyancy behavior and not measured in your project. Tune them in a real water volume (see the Movement setup document).

### 6.3 Resurfacing

`IsResurfacing(Zone)` is true when the zone is Underwater, upward speed is above `ResurfaceVerticalSpeed` (40 cm/s), and there is no downward input. `ABaseCharacter` uses it for the `Water.Resurfacing` state.

### 6.4 Speeds

| Condition | Speed |
|---|---|
| Swimming | `MaxSwimSpeed` (default 300) |
| Swimming and sprinting | `FastSwimSpeed` (default 600) |

---

## 7. Burst actions (Dash and Dodge)

Each action has its own settings (`FMovementActionSettings`): `bEnabled`, `Speed`, `Cooldown`, `StateDuration`.

| Property | Domain | Action name in gameplay | Default speed | Cooldown | State duration |
|---|---|---|---|---|---|
| `GroundDash` | Ground | Dash | 1400 | 1.0 | 0.35 |
| `GroundDodge` | Ground | Roll | 900 | 0.9 | 0.6 |
| `FlyDash` | Fly | Fly Dash | 2200 | 1.0 | 0.4 |
| `FlyDodge` | Fly | Fly Dodge | 1400 | 0.8 | 0.5 |
| `UnderwaterDash` | Water | Underwater Dash | 1000 | 1.0 | 0.4 |

Water has no dodge, and the water dash only works while the zone is Underwater.

Direction rules:

| Action | Domain | With input | Without input |
|---|---|---|---|
| Dash | Ground | Input direction, flattened | Body forward, flattened |
| Dash | Fly, Water | Input direction, full 3D | Body forward |
| Dodge | Ground | Input direction, flattened | Backward |
| Dodge | Fly | Input direction, full 3D | Body right |

On the ground only the horizontal velocity is replaced. In flight and water the whole velocity is replaced.

`StateDuration` is not used by the movement component. It travels with the `OnActionStarted` event so the animation layer knows how long to show the action.

---

## 8. Facts and events

| Member | Type | Use |
|---|---|---|
| `GetMovementDomain()` | function | Ground, Fly or Water |
| `GetWaterZone()` | function | None, Surface, Underwater (performs a trace) |
| `IsResurfacing(Zone)` | function | Rising underwater without dive input |
| `GetImmersionDepth()` | function | 0..1 |
| `IsSprinting()`, `IsGliding()`, `WantsToFly()`, `HasMovementInput()` | functions | Intent and input facts |
| `OnActionStarted(Action, Domain, StateDuration)` | C++ multicast delegate | Fired once per executed Dash or Dodge, on the owning client and on the server, never during replays |

`OnActionStarted` is a native delegate, so C++ code binds it (ABaseCharacter does). Blueprints listen through the State Machine component's events instead.

---

## 9. Extending and limits

| Goal | Notes |
|---|---|
| Change speeds or feel | Edit the properties; no code needed. |
| Add a new burst action | Add a settings struct and a case in `GetActionSettings`. It needs its own predicted input bit. |
| Add another predicted input | All four custom bits are used. Use extended network move data (`FCharacterNetworkMoveData` and its container) to carry more inputs. |
| New domain (for example Climb) | Add a value to `EMovementDomain`, a custom movement mode, a case in `GetMovementDomain`, and matching tags in the State Machine framework. |
| Slide, Mantle | Not implemented here; they are gameplay-triggered actions on the character (`RequestAction`). |

Assumptions:

- All tuning values must match between client and server. Change Blueprint defaults, not runtime values on one side.
- Dash and Dodge are velocity impulses. There is no distance-exact dash or root-motion dash.
- Fly is a toggled mode, not a physics-based lift model.
- Nothing here was compiled or run in the editor. Use the checklist in `Movement_Blueprint_Setup.md` after the first build.
