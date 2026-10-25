# Character Framework - Architecture Overview

Target: Unreal Engine 5.8 | Module macro: `COREFRAMEWORK_API` | Files: `InputConfigData.h/.cpp`, `BaseCharacter.h/.cpp`

---

## 1. Purpose

`ABaseCharacter` is a ready-to-use `ACharacter` with three jobs:

1. **Input:** bind Enhanced Input actions from one data asset (`UInputConfigData`).
2. **View:** drive a first-person and a third-person view through the camera manager and switch between them at runtime, in multiplayer. A pure first-person character needs no body mesh and no head socket.
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
| Camera framework | Source | `UCameraManagerComponent` owns the camera rig and the profiles (see `Docs/CameraSystem/`) |

Compile-time, the Character folder needs the Movement, State Machine and Camera headers. At runtime each part is optional (section 9).

---

## 3. Component rig

The character creates its body, arms and logic components in the C++ constructor. The camera rig itself (a spring arm and a camera) is created at runtime by the `CameraManager` component and does not exist on a dedicated server, so it never appears in the Blueprint Components panel. A Blueprint child needs no setup to have a working rig.

| Component | Type | Attached to | Notes |
|---|---|---|---|
| `CapsuleComponent` | `UCapsuleComponent` | Root | Radius 34, half height 88 (the standard character capsule) |
| `Mesh` (`GetMesh()`) | `USkeletalMeshComponent` | Capsule | **Third-person body mesh. Optional:** a pure first-person character leaves it without a mesh asset. Replicated and visible to everyone. Hidden from the local player only while in first person. Casts a hidden shadow so the local shadow stays correct. Animation always ticks, so a camera on one of its sockets stays in sync. |
| `FirstPersonCameraAnchor` | `USceneComponent` | Capsule | **Eye point of the first-person view.** Default: 64 cm above the capsule center. Select it in the Components panel and move it to place the first-person camera. It needs no mesh and no socket. |
| `FirstPersonMesh` | `USkeletalMeshComponent` | `FirstPersonCameraAnchor` at design time, the managed camera at runtime | **First-person arms.** Owner-only rendering, no shadow, no collision. Hidden until first person is selected. The relative transform is kept when it moves to the camera. |
| `CameraManager` | `UCameraManagerComponent` | (none) | Creates the spring arm and the camera at runtime and switches between named camera profiles. Profiles are data (`Camera Profiles`). See `Docs/CameraSystem/`. |
| `StateMachine` | `UStateMachineComponent` | (none) | Replicates its own properties. |
| `CharacterMovement` | `UBaseCharacterMovementComponent` | (none) | Swapped in through `SetDefaultSubobjectClass`. |

### 3.1 Placing the first-person camera and the arms

Three values place the first-person view. Each has one job, and all of them are plain relative transforms, as in the standard Unreal first-person templates.

| What | Where you set it | Relative to |
|---|---|---|
| Base eye position | Location of `FirstPersonCameraAnchor` (Components panel) | The capsule |
| Per-profile adjustment (a lower eye while crouching, an aim position) | `Attach Offset` on the camera profile (`CameraManager` > `Camera Profiles` > `FirstPerson`) | The anchor. Blends between profiles |
| Arms | Location, Rotation and Scale of `FirstPersonMesh` (Components panel) | The anchor at design time, the camera at runtime. At rest the camera sits at the anchor, so both are the same place |

What the first-person camera attaches to is decided once, in `PostInitializeComponents`:

| First-person profile | The camera hangs on |
|---|---|
| Names no socket (default) | `FirstPersonCameraAnchor`. No body mesh and no head bone are needed |
| Names a socket (`Attach Socket Name`) that exists on `Mesh` | That socket of the body mesh: a visible body with an animated view. The anchor then plays no part for the camera |
| Names a socket that does not exist on `Mesh` | `FirstPersonCameraAnchor`, and one warning is logged (`LogBaseCharacter`) |

Notes:

- The rig follows the control rotation, so the rotation of the anchor is ignored. Only its position counts.
- An offset is in the local space of the capsule, which faces the camera yaw in first person. Keep eye offsets mostly on Z (height) and use small values on X and Y.
- A profile with a non-zero `Attach Offset` or `Socket Offset` moves the camera away from the anchor. The arms follow the camera, so adjust the arms transform if you change those.
- A Blueprint that saved its own copy of the `Camera Profiles` map keeps working: the character fills in the attachment fields at startup.

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

### 5.5 Input that does not belong to the character

The character binds only input that is about the body: move, look, jump, crouch, sprint, fly, dash, dodge and perspective. Input that must keep working when there is no pawn (pause, menu keys, scoreboard, chat) belongs to the **PlayerController** of the game project.

| Question | Answer |
|---|---|
| Who owns the pause key? | The project PlayerController, in its own mapping context. The character never sees it |
| How does it open the pause menu? | `UUIFrameSubsystem::GetFromController(this)`, then `PushScreen` or `PopScreen` with the screen's gameplay tag (see `Docs/UISystem/UI_HUD_Setup_Guide.md`) |
| Who decides what pausing means? | The pause screen (`Input Mode`, `Pause Game While Active`, `Is Back Handler`), not the controller and not the character |
| Can the two mapping contexts clash? | No. The character adds its context in `PawnClientRestart`; the controller adds its own context when its input component is set up. Give them different priorities if a key is ever shared |
| Is there a framework PlayerController? | No, on purpose: a project has one controller class, so a framework base would block other frameworks. Each project writes `A<Project>PlayerController : APlayerController` (see `Docs/Integration/Project_Integration_Guide.md`, section 2.4) |

---

## 6. Perspective system

### 6.1 State and API

| Member | Purpose |
|---|---|
| `DefaultPerspective` | Perspective at spawn. |
| `bAllowPerspectiveToggle` | Set false to lock the view. |
| `FirstPersonCameraName`, `ThirdPersonCameraName` | Names of the camera profiles used for each view (defaults `FirstPerson` and `ThirdPerson`). |
| `SetPerspective(New)` / `TogglePerspective()` | Switch the view. Runs only on the locally controlled character. |
| `GetPerspective()` | Current perspective. |
| `OnPerspectiveChanged` | Dynamic delegate (bind from Blueprint or C++). |
| `BP_OnPerspectiveChanged` | Blueprint event with the same data, for overriding in a child Blueprint. |

### 6.2 Visibility matrix

| Machine | Perspective | Camera profile | Third-person mesh | First-person mesh |
|---|---|---|---|---|
| Owning player | Third | `ThirdPersonCameraName` | visible | hidden |
| Owning player | First | `FirstPersonCameraName` | hidden from owner (shadow kept) | visible |
| Any other player | (any) | third-person profile (nobody views through that rig) | visible | hidden (owner-only) |

A pure first-person project has no body mesh, so other players see nothing of the character. Assign a mesh to `Mesh` if they must see a body.

### 6.3 What a switch does

1. Sets `ActivePerspective` locally.
2. Tells the movement component whether the body should follow the camera yaw (first person) or turn toward the movement direction (third person). **This runs on the server too**, otherwise client and server would rotate the body differently and every move would be corrected.
3. Switches the camera manager to the matching profile as an instant cut (blend time 0), shows or hides the arms, and hides the body mesh from its owner in first person.
4. Fires `OnPerspectiveChanged` and `BP_OnPerspectiveChanged`.
5. If the caller is a client, sends `ServerSetPerspective` (reliable).

Use the delegate for HUD, crosshair, audio or material changes. Nothing in the framework hard-codes those. Field of view, aim and other camera behavior are camera profiles: see `Docs/CameraSystem/`.

### 6.4 Pure first-person setup

A first-person game with arms only, no body and no head bone:

1. Class Defaults: `Default Perspective` = First Person, `Allow Perspective Toggle` = off.
2. Leave `Mesh` without a mesh asset. Assign the arms mesh and its own Animation Blueprint to `FirstPersonMesh`.
3. Move `FirstPersonCameraAnchor` to the eye height you want and position `FirstPersonMesh` relative to it (section 3.1).
4. Tune field of view, pitch limits and look scale on the `FirstPerson` camera profile.

Nothing else is required. `Docs/CameraSystem/Camera_Blueprint_Setup.md`, Step 3, has the click-by-click version.

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
| No body mesh assigned to `Mesh` (pure first person) | Nothing breaks. The first-person camera hangs on `FirstPersonCameraAnchor`. Other players see no body. |
| The first-person profile names a socket the body mesh does not have | One warning in `LogBaseCharacter`; the camera uses `FirstPersonCameraAnchor`. It never falls to the mesh origin. |

---

## 10. Extending

| Goal | How |
|---|---|
| New input action | Add a `TObjectPtr<UInputAction>` to `FCharacterInputActions`, bind it in `SetupPlayerInputComponent`, add a virtual handler. |
| Different behavior for an action | Override the matching `Input_*` function in a child class. |
| Extra locomotion rules | Override `UpdateStateMachine()` (call `Super` first, then adjust). |
| New action tag (Slide, Mantle ...) | Call `RequestAction` from your own logic. |
| Different movement component | Set your own class through `SetDefaultSubobjectClass` in a child constructor. Input and perspective keep working; only sprint, fly, dash and dodge need a component that provides them. |
| A different first-person camera position, a crouch eye height, an aim view | Move `FirstPersonCameraAnchor`, or add a camera profile and switch to it (`GetCameraManager()->SwitchToCamera`). No character code. See `Docs/CameraSystem/`. |
| A visible body in first person | Set `Attach Socket Name` (for example `head`) on the `FirstPerson` camera profile and set the profile's `Attach Offset` to zero. |
| A character for a game project | Derive `A<Project>Character : ABaseCharacter` in the game module. Its constructor takes `const FObjectInitializer&` and passes it to `Super`, because the base constructor replaces the movement component class through it. Create the Blueprint from the project class. Hooks, input overrides and an example: `Docs/Integration/Project_Integration_Guide.md`, section 5.1. |
| Project-specific input (interact, fire) | Add a project mapping context and bind the actions in the project character's `SetupPlayerInputComponent` override, after `Super`. The framework context is untouched. |
| Input that works without a pawn (pause, menus) | The project PlayerController, section 5.5. |

---

## 11. Assumptions and limitations

- Mouse look expects the Look action to deliver Y already negated by the mapping context.
- The camera rig uses the character's control rotation. Aim-down-sights is a camera profile you add; there is no camera shake layer.
- Switching perspective is an instant cut.
- Slide and Mantle are hooks only.
- The framework has no ability or stamina system. Dash and Dodge are limited by cooldowns only.
- Nothing here was compiled or run in the editor. Follow the checklist in `Character_Blueprint_Setup.md` after the first build.
