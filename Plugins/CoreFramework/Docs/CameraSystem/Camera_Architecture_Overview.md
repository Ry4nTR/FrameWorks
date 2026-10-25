# Camera Framework - Architecture Overview

Target: Unreal Engine 5.8 | Module macro: `COREFRAMEWORK_API` | Files: `CameraManagerComponent.h/.cpp`

---

## 1. Purpose

`UCameraManagerComponent` owns the camera rig (one spring arm and one camera) of its owning actor and everything that happens to it:

- Named **camera profiles** and blending between them (third person, first person, aim, any you add).
- Field of view, zoom, and look input.
- **Attachment:** the rig hangs on the owner's root (the capsule of a character), on a scene component placed relative to it, or on a socket of a mesh, with an offset in the space of that attach point.
- Blending the player's view to and from external camera actors (cinematics).

It depends only on the Engine module and knows nothing about any character class, so it can be added to any pawn. Look input needs a pawn owner. Every other feature works on any actor with a root component.

---

## 2. Files and dependencies

```
Frameworks/CameraSystem/
    CameraManagerComponent.h/.cpp                The component, FCameraProfile, events
    (documents in Docs/CameraSystem/)
```

| Dependency | Type | Why |
|---|---|---|
| `Core`, `CoreUObject`, `Engine` | Module | Spring arm, camera, player controller, camera manager of the player |

No Character, Movement or State Machine header is included.

---

## 3. The rig

The rig is created at runtime in `InitializeComponent`:

| Part | Type | Settings |
|---|---|---|
| `CameraManager_SpringArm` | `USpringArmComponent` | Attached to the owner's root, uses pawn control rotation |
| `CameraManager_Camera` | `UCameraComponent` | Attached to the spring arm socket, does not use control rotation itself |

- Both are registered as instance components, so they are **not visible in the Blueprint Components panel** and cannot be moved in the Blueprint viewport. Place them with profile data (section 4). `ABaseCharacter` adds a visible placement helper for first person (`FirstPersonCameraAnchor`, section 11).
- **Dedicated servers create no rig.** Every rig function is a no-op there.
- Get the parts with `GetCameraComponent()` and `GetSpringArmComponent()`.

---

## 4. Camera profiles

A profile is a named set of values (`FCameraProfile`). The component holds them in the `CameraProfiles` map. A new game adds entries and never needs new camera code.

| Category | Field | Default | Meaning |
|---|---|---|---|
| Camera | `FieldOfView` | 90 | Horizontal field of view in degrees (5 to 170) |
| Camera | `ArmLength` | 300 | Spring arm length. 0 puts the camera on the arm origin (first person) |
| Camera | `SocketOffset` | 0 | Offset at the end of the arm, in **camera space** (shoulder offset) |
| Camera | `TargetOffset` | 0 | Offset at the arm origin, in **world space** |
| Zoom | `bAllowZoom`, `MinArmLength`, `MaxArmLength` | on, 120, 500 | Range of `AddZoomInput` |
| Behavior | `bDoCollisionTest` | on | Spring arm collision |
| Behavior | `bEnableCameraLag`, `CameraLagSpeed` | off, 10 | Camera lag |
| Behavior | `ViewPitchMin`, `ViewPitchMax` | -89.9, 89.9 | Pitch limits applied to the owning player's camera manager |
| Behavior | `LookSensitivityScale` | 1 | Multiplies look input (use below 1 for aiming) |
| Behavior | `BlendInTime` | 0.25 | Seconds to blend into this profile |
| Behavior | `bFirstPerson` | off | Lets the owner react through `IsFirstPerson()` |
| Attachment | `AttachSocketName` | none | Socket the rig attaches to (for example a head socket) |
| Attachment | `bAttachToSocketParent` | off | Attach to the component set with `SetSocketParent` instead of the owner's root |
| Attachment | `AttachOffset` | 0 | Offset of the rig origin in the **local space of the attach point** |

**Which offset to use**

| Goal | Field |
|---|---|
| Place the camera relative to the capsule (eye height, a step forward) | `AttachOffset` |
| Shoulder offset, a camera beside the character | `SocketOffset` |
| A fixed world-space height regardless of capsule orientation | `TargetOffset` |

Built-in profiles (added by the component constructor, so it works out of the box): `ThirdPerson` (arm 300, shoulder offset, head height), `FirstPerson` (arm 0, no zoom, no collision test, `AttachOffset` Z 64, first person flag) and `Aim` (field of view 70, arm 140, look scale 0.6, blend 0.15). `DefaultCameraName` is `ThirdPerson`. `ABaseCharacter` replaces the `FirstPerson` and `ThirdPerson` entries in its own constructor.

---

## 5. Attachment

The attach point is chosen for each profile:

| Profile | The rig attaches to |
|---|---|
| Default (`bAttachToSocketParent` off, no socket) | The owner's root component, at `AttachOffset` |
| `bAttachToSocketParent` on, no socket | The socket parent (set with `SetSocketParent`), at its origin plus `AttachOffset` |
| Names a socket (`AttachSocketName`) | The socket parent at that socket plus `AttachOffset`. A profile that names a socket always uses the socket parent |
| Names a socket that does not exist on the socket parent | The origin of the socket parent, with one warning in `LogCameraManager`. A missing bone never breaks the view |

- The socket parent is any `USceneComponent`: the skeletal mesh that carries a head socket, or a plain scene component placed relative to the capsule.
- **Switching between profiles with different attach points is always an instant cut.** The camera would jump farther than a blend can cover. Profiles that share an attach point blend, including their `AttachOffset`.
- `SetSocketParent` re-applies the attachment of the active profile.

---

## 6. Blending model

The component interpolates five values on the rig: field of view, arm length, `SocketOffset`, `TargetOffset` and `AttachOffset`. Everything else (collision test, lag, pitch limits) cannot be interpolated and is applied at once.

- A blend uses `BlendCurve` when set, otherwise an ease in and out with `BlendEaseExponent`.
- A blend time at or below zero is a cut. A negative blend time passed to `SwitchToCamera` means "use the profile's `BlendInTime`".
- The component ticks only while a blend or a zoom change is running, then turns its own tick off.
- `OnCameraBlendFinished` fires when a blend ends. It fires last, so a listener may start a new blend.

---

## 7. API

**Switching**

| Function | Purpose |
|---|---|
| `SwitchToCamera(Name, BlendTime = -1)` | Blends the rig to a profile. Returns false if the profile or the rig is missing |
| `RevertToPreviousCamera(BlendTime)` | Returns to the profile that was active before |
| `RegisterCameraProfile(Name, Profile)` | Adds or replaces a profile at runtime. Re-applies it if it is the active one |

**Adjustments**

| Function | Purpose |
|---|---|
| `SetFOV(NewFOV, BlendTime = 0)` | Sets the field of view until the next profile switch |
| `AddZoomInput(Axis)` | Positive zooms in. Ignored if the profile forbids zoom |
| `AddLookInput(Axis2D)` | Forwards to the owning pawn's controller, scaled by `LookSensitivity`, the profile's look scale and `bInvertPitch` |
| `SetSocketParent(Component)` | Sets the component that profiles attach to |

**External cameras:** `BlendToTarget(Actor, BlendTime, Function, Exp, bLockOutgoing)` uses the engine view target blend on the owning player; `BlendBackToOwner(...)` returns the view to this rig. Call them on the owning client. `OnCameraBlendFinished` fires when the blend time has passed.

**Queries:** `GetActiveCameraName`, `IsFirstPerson`, `IsBlending`, `IsViewTargetOwner`, `GetCameraComponent`, `GetSpringArmComponent`.

**Events:** `OnCameraChanged(Previous, New)` at the start of a profile blend, `OnCameraBlendFinished`.

**Configuration:** `CameraProfiles`, `DefaultCameraName`, `LookSensitivity`, `bInvertPitch`, `ZoomStep` (40), `ZoomInterpSpeed` (10), `BlendCurve`, `BlendEaseExponent` (2).

---

## 8. Pitch limits and look input

- `ViewPitchMin` and `ViewPitchMax` of the active profile are written to the owning player's `PlayerCameraManager` when a profile becomes active, when the controller changes, and at `BeginPlay`.
- `AddLookInput` needs a pawn owner. It calls `AddControllerYawInput` and `AddControllerPitchInput`. Mouse inversion belongs in the input mapping context; `bInvertPitch` is a second, per-component switch.

---

## 9. Multiplayer

The camera is purely local. The component does not replicate, each machine builds its own rig, and nothing about the camera is sent over the network. Gameplay code that decides which profile to show runs on the machine that views it. `ABaseCharacter` does this from its replicated perspective state. On a dedicated server there is no rig.

---

## 10. Graceful degradation

| Situation | Behavior |
|---|---|
| Owner has no root component | A warning is logged and no rig is created |
| `SwitchToCamera` with an unknown name | A warning is logged and the function returns false |
| Default camera name not in `CameraProfiles` | A warning is logged; the rig keeps its construction values |
| A named socket does not exist on the socket parent | One warning; the rig attaches at the origin of the socket parent |
| No socket parent set but a profile asks for it | The rig attaches to the owner's root |
| Owner is not a pawn | Look input is ignored; every other feature works |
| Dedicated server | Everything is a no-op |

---

## 11. Using it with `ABaseCharacter`

`ABaseCharacter` owns a `CameraManager` component and maps its replicated perspective to two profile names (`FirstPersonCameraName`, `ThirdPersonCameraName`). For first person it adds a placement helper:

| Part | Role |
|---|---|
| `FirstPersonCameraAnchor` | A scene component under the capsule (default 64 cm up). The `FirstPerson` profile hangs the rig on it. Move it in the Blueprint viewport |
| `FirstPersonMesh` | The arms. A child of the anchor at design time, re-attached to the camera at runtime with the same relative transform |
| Profile names a valid socket | The rig hangs on that socket of the body mesh instead (visible body, animated view) |

No head bone is required. Details: `Docs/Character/Character_Architecture_Overview.md`, sections 3.1 and 6.4.

---

## 12. Extending

| Goal | How |
|---|---|
| A new camera (aim, vehicle, cinematic) | Add an entry to `CameraProfiles`, then call `SwitchToCamera` from your own logic |
| A crouch eye height | A profile with a lower `AttachOffset` Z, switched in with a short `BlendTime` |
| A camera at runtime | `RegisterCameraProfile` |
| Another pawn | Add the component, fill `CameraProfiles`, set `DefaultCameraName`, and call `SetSocketParent` if a profile attaches to a mesh or a placement component |
| React to view changes | Bind `OnCameraChanged` and `OnCameraBlendFinished` |

---

## 13. Assumptions and limitations

- There is no camera shake layer, no automatic aim-down-sights and no collision push-in beyond the spring arm test.
- The rig follows the control rotation. The rotation of the attach component is ignored; only its position counts.
- Offsets in `AttachOffset` rotate with the attach point. Keep first-person offsets mostly on Z.
- Nothing here was compiled or run in the editor. Follow `Camera_Blueprint_Setup.md` after the first build.
