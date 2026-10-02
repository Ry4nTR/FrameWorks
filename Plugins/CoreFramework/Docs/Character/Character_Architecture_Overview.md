# Character Framework - Architecture Overview

Target: Unreal Engine 5.8 | Module macro: `FRAMEWORKS_API` | Files: `InputConfigData.h/.cpp`, `BaseCharacter.h/.cpp`

---

## 1. Purpose

`ABaseCharacter` is a ready-to-use `ACharacter` with three jobs:

1. **Input:** bind Enhanced Input actions from one data asset (`UInputConfigData`).
2. **View:** own a complete first-person and third-person rig and switch between them at runtime, in multiplayer.
3. **Wiring:** connect the Movement framework to the State Machine framework, so the Animation framework always receives a correct set of Gameplay Tags.

It is the only class in the library that knows about the other frameworks. Everything else stays independent.

---

## 2. Files and dependencies

```
Frameworks/Character/
    InputConfigData.h/.cpp                       Data asset + action struct
    BaseCharacter.h/.cpp                         The character
    Character_Architecture_Overview.md
    Character_Blueprint_Setup.md
```

| Dependency | Type | Why |
|---|---|---|
| `Core`, `CoreUObject`, `Engine`, `InputCore` | Module | Base engine types |
| `EnhancedInput` | Module | `UEnhancedInputComponent`, `UInputAction`, `UInputMappingContext` |
| `GameplayTags` | Module | Action tags passed to the state machine |
| Movement framework | Source | `UBaseCharacterMovementComponent` is the default movement component |
| State Machine framework | Source | `UStateMachineComponent` and the native tag vocabulary |

Compile-time, the Character folder needs the Movement and State Machine headers. At runtime each part is optional (section 9).

---

## 3. Component rig

All components are created in the C++ constructor, so a Blueprint child needs no setup to have a working rig.

| Component | Type | Attached to | Notes |
|---|---|---|---|
| `CapsuleComponent` | `UCapsuleComponent` | Root | Radius 34, half height 88 (the standard character capsule) |
| `Mesh` (`GetMesh()`) | `USkeletalMeshComponent` | Capsule | **Third-person mesh.** Replicated and visible to everyone. Hidden from the local player only while in first person. Casts a hidden shadow so the local shadow stays correct. Animation always ticks, because the first-person camera rides on it. |
| `FirstPersonCamera` | `UCameraComponent` | `Mesh`, socket `FirstPersonCameraSocketName` (default `head`) | Uses pawn control rotation. Inactive until first person is selected. |
| `FirstPersonMesh` | `USkeletalMeshComponent` | `FirstPersonCamera` | **First-person arms.** Owner-only rendering, no shadow, no collision. Hidden until first person is selected. |
| `ThirdPersonSpringArm` | `USpringArmComponent` | Capsule | Length 350, collision test on, uses pawn control rotation. |
| `ThirdPersonCamera` | `UCameraComponent` | Spring arm socket | Active by default. |
| `StateMachine` | `UStateMachineComponent` | (none) | Replicates its own properties. |
| `CharacterMovement` | `UBaseCharacterMovementComponent` | (none) | Swapped in through `SetDefaultSubobjectClass`. |

Exactly one camera is active at a time. The engine picks the first active camera component of the view target, so the framework toggles `SetActive` on both cameras together.

---

## 4. Frame flow

The character forces this order every frame with tick prerequisites set in `PostInitializeComponents`:

```
CharacterMovement tick            physics, rotation, predicted actions
        |
        v
ABaseCharacter::Tick              UpdateStateMachine() pushes domain + locomotion tags
        |
        v
Mesh / FirstPersonMesh tick       AnimationInstance reads the tags and computes AnimGraph variables
```

Because the meshes tick after the character, the animation never lags one frame behind the state.

Runtime events:

```
Movement executes Dash/Dodge  -->  OnActionStarted (C++ delegate)
                                        |
                                        v
                          ABaseCharacter::HandleMovementActionStarted
                                        |
                                        v
                          StateMachine->SetDomain + BeginAction(tag, duration)
```

---

## 5. Input system

### 5.1 Data asset

`UInputConfigData` holds:

- `DefaultMappingContext` and `MappingPriority`
- `Actions` (`FCharacterInputActions`), one `UInputAction` per feature

Any action left empty is simply not bound. A game with no flying, swimming, dash or dodge just leaves those entries empty.

In the editor, *Validate* on the asset reports an error when `DefaultMappingContext`, `Move` or `Look` is missing.

### 5.2 Bindings

| Action | Value | Trigger events used | Effect |
|---|---|---|---|
| `Move` | Axis2D (X right, Y forward) | Triggered | Direction comes from the movement component: camera **yaw only** on the ground and in Standard mode; the **full camera rotation, pitch included**, while Sprint is held in Fly or Water (Camera Steering). |
| `Look` | Axis2D (X yaw, Y pitch) | Triggered | Controller yaw and pitch input. Negate Y in the mapping context for standard mouse look. |
| `Jump` | Digital | Started, Triggered, Ended | Ground, grounded: `Jump()`. Ground, **falling**: starts flight (`bDoubleJumpActivatesFlight`). Fly and Water, Standard mode: ascend while held. Ignored as a vertical button while Sprint is held. |
| `Crouch` | Digital | Started, Triggered, Ended | Ground: crouch while held. Fly and Water, Standard mode: descend while held. Ignored as a vertical button while Sprint is held. |
| `Sprint` | Digital | Started, Ended | Sets the movement component's sprint intent. Ground: Run. Fly and Water: switches to Camera Steering with the fast speed (Glide, Fast Swim). |
| `Fly` | Digital | Started | Toggles flight. |
| `Dash` | Digital | Started | `RequestDash()` on the movement component. |
| `Dodge` | Digital | Started | `RequestDodge()`: roll on the ground, side-step in flight. |
| `PerspectiveToggle` | Digital | Started | `TogglePerspective()`. |

**There is no swim action.** Swimming starts and ends automatically when the character enters or leaves a `PhysicsVolume` with `bWaterVolume` enabled.

`Jump` and `Crouch` rely on the **Triggered** event firing every frame while the key is held. Keep them on the default (no) trigger or a *Down* trigger. A *Pressed* trigger would fire once and give a single-frame ascend or descend.

### 5.2.1 Input routing

The character only forwards input. The movement component decides what it means:

| Question | Answered by |
|---|---|
| Which rotation turns `Move` into a world direction? | `GetMoveBasisRotation(ControlRotation)`: yaw only, or yaw and pitch in Camera Steering |
| Do the Up / Down buttons apply right now? | `AcceptsVerticalButtons()`: true in Fly or Water in Standard mode |
| Is the character falling (double-jump takeoff)? | `IsFalling()` on the movement component |

Full matrix: `Movement_Input_Routing.md`.

### 5.3 Mapping context lifecycle

The mapping context is added in `PawnClientRestart`, which runs on the owning client every time the pawn is possessed. The context is removed and re-added first, so respawns never stack duplicates.

### 5.4 Overriding behavior

Every handler is `protected virtual` (`Input_Move`, `Input_Look`, `Input_JumpStarted` ...). A child class overrides only what it changes and calls `Super` for the rest.

---

## 6. Perspective system

### 6.1 State and API

| Member | Purpose |
|---|---|
| `DefaultPerspective` | Perspective at spawn. |
| `bAllowPerspectiveToggle` | Set false to lock the view. |
| `SetPerspective(New)` / `TogglePerspective()` | Switch the view. Runs only on the locally controlled character. |
| `GetPerspective()` | Current perspective. |
| `OnPerspectiveChanged` | Dynamic delegate (bind from Blueprint or C++). |
| `BP_OnPerspectiveChanged` | Blueprint event with the same data, for overriding in a child Blueprint. |

### 6.2 Visibility matrix

| Machine | Perspective | Third-person camera | First-person camera | Third-person mesh | First-person mesh |
|---|---|---|---|---|---|
| Owning player | Third | active | inactive | visible | hidden |
| Owning player | First | inactive | active | hidden from owner (shadow kept) | visible |
| Any other player | (any) | active | inactive | visible | hidden (owner-only) |

### 6.3 What a switch does

1. Sets `ActivePerspective` locally.
2. Tells the movement component whether the body should follow the camera yaw (first person) or turn toward the movement direction (third person). **This runs on the server too**, otherwise client and server would rotate the body differently and every move would be corrected.
3. Toggles cameras and mesh visibility.
4. Fires `OnPerspectiveChanged` and `BP_OnPerspectiveChanged`.
5. If the caller is a client, sends `ServerSetPerspective` (reliable).

Use the delegate for HUD, crosshair, field of view, audio or material changes. Nothing in the framework hard-codes those.

---

## 7. State machine driver

`UpdateStateMachine()` runs every tick on the server and on the owning client. Simulated proxies skip it and mirror the server through replication.

### 7.1 Domain

The movement domain is pushed with `SetDomain`: Ground, Fly or Water, taken from the current movement mode.

### 7.2 Locomotion state selection

| Domain | Rule (first match wins) | Resulting tag |
|---|---|---|
| Ground | falling | `Ground.Jump` |
| Ground | crouching | `Ground.Crouch` |
| Ground | no input, or horizontal speed at or below `MovingSpeedThreshold` | `Ground.Idle` |
| Ground | sprinting | `Ground.Run` |
| Ground | otherwise | `Ground.Walk` |
| Fly | Sprint held (Camera Steering) with glide enabled, and moving | `Fly.Glide` |
| Fly | 3D speed above `MovingSpeedThreshold` | `Fly.Move` |
| Fly | otherwise | `Fly.Idle` |
| Water, underwater | resurfacing (rising without dive input) | `Water.Resurfacing` |
| Water, underwater | not moving / moving / sprinting | `Water.Underwater.Idle` / `.Move` / `.FastSwim` |
| Water, surface | not moving / moving / sprinting | `Water.Surface.Idle` / `.Move` / `.FastSwim` |

Tag names are `Frameworks.State.<...>`. "Moving" in water means input present and 3D speed above the threshold.

### 7.3 Water transitions

The driver remembers the previous water zone. Crossing the surface starts a short timed action of `WaterTransitionDuration` seconds:

| Crossing | Action tag |
|---|---|
| Surface to Underwater | `Water.Transition.ToUnderwater` |
| Underwater to Surface | `Water.Transition.ToSurface` |

### 7.4 Burst actions

`HandleMovementActionStarted` translates the movement component's event into an action tag:

| Movement action | Ground | Fly | Water |
|---|---|---|---|
| Dash | `Ground.Dash` | `Fly.Dash` | `Water.Underwater.Dash` |
| Dodge | `Ground.Roll` | `Fly.Dodge` | (not supported) |

Duration comes from the movement component's `StateDuration` for that action.

### 7.5 Gameplay-triggered actions

`RequestAction(Tag, Duration)` starts any timed action from game code (Slide, Mantle, or your own tags). It runs locally at once, then forwards to the server with a reliable RPC (duration clamped to 10 s). The state machine rejects tags that do not belong to the active domain.

Slide and Mantle have tags and animation support but no built-in trigger. Your game logic decides when they happen and calls `RequestAction`.

---

## 8. Multiplayer model

| Data | Owner of the truth | Replication |
|---|---|---|
| Position, velocity, movement mode | Movement component | Engine character replication and prediction |
| Sprint, Fly, Dash, Dodge intent | Owning client | Saved-move flags (see the Movement documents) |
| `ActivePerspective` | Owning client | Owner applies it at once; server learns through `ServerSetPerspective`; other clients receive it with `COND_SkipOwner` |
| Domain, locomotion state, action state | Computed on server and owner | State machine replicates to simulated proxies only (`COND_SkipOwner`) |
| First-person visuals | Local machine | Never replicated |

`GetLifetimeReplicatedProps` is implemented in `ABaseCharacter`, `UStateMachineComponent`. Movement replication is handled by the engine's character movement code.

RPCs:

| RPC | Reliability | Sender | Purpose |
|---|---|---|---|
| `ServerSetPerspective` | Reliable | Owning client | Keep the server's rotation policy in sync |
| `ServerRequestAction` | Reliable | Owning client | Let the server start and replicate a gameplay action |

---

## 9. Graceful degradation

| Situation | Behavior |
|---|---|
| `InputConfig` not set | A warning is logged and no input is bound. The character still spawns, moves under AI and replicates. |
| An action is empty in `InputConfig` | That single binding is skipped. |
| Fly disabled on the movement component | The Fly input toggles nothing, and the second Jump press while airborne is an ordinary jump again. Jump and Crouch never become ascend and descend because the domain is never Fly. |
| Water disabled on the movement component | The character cannot enter water movement, even inside a Water Volume. |
| No state machine (a C++ child uses `DoNotCreateDefaultSubobject(TEXT("StateMachine"))`) | `UpdateStateMachine` and action forwarding return immediately. Movement and input are unaffected. |
| No first-person mesh asset assigned | First person shows only the camera view. |
| The `head` socket is missing on the third-person mesh | The first-person camera sits at the mesh origin. Set `FirstPersonCameraSocketName` to an existing socket or bone. |

---

## 10. Extending

| Goal | How |
|---|---|
| New input action | Add a `TObjectPtr<UInputAction>` to `FCharacterInputActions`, bind it in `SetupPlayerInputComponent`, add a virtual handler. |
| Different behavior for an action | Override the matching `Input_*` function in a child class. |
| Extra locomotion rules | Override `UpdateStateMachine()` (call `Super` first, then adjust). |
| New action tag (Slide, Mantle ...) | Call `RequestAction` from your own logic. |
| Different movement component | Set your own class through `SetDefaultSubobjectClass` in a child constructor. Input and perspective keep working; only sprint, fly, dash and dodge need a component that provides them. |

---

## 11. Assumptions and limitations

- Mouse look expects the Look action to deliver Y already negated by the mapping context.
- Both cameras use the character's control rotation; there is no aim-down-sights or camera shake layer.
- Slide and Mantle are hooks only.
- The framework has no ability or stamina system. Dash and Dodge are limited by cooldowns only.
- Nothing here was compiled or run in the editor. Follow the checklist in `Character_Blueprint_Setup.md` after the first build.
