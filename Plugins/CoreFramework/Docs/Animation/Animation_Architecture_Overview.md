# Animation Framework - Architecture Overview

Target: Unreal Engine 5.8 | Module macro: `FRAMEWORKS_API` | Files: `AnimationInstance.h/.cpp`

---

## 1. Purpose

`UAnimationInstance` is the C++ parent class for every Animation Blueprint in the library. It reads the character's Gameplay Tag state, computes all locomotion values on a worker thread, and hands the Animation Blueprint a small set of plain variables. The Animation Blueprint contains poses and blends only. It has no gameplay logic, no casts and no Event Graph.

Feature summary:

- Thread-safe update with `NativeThreadSafeUpdateAnimation`.
- Domain toggles (`bEnableGroundDomain`, `bEnableFlyDomain`, `bEnableWaterDomain`) that make a ground-only, water-only or flight-only Animation Blueprint valid.
- Exposed state enums for Ground, Fly and Water, plus the raw Gameplay Tags.
- Upper and lower body separation through a Layered Blend per Bone weight.
- A root motion switch.
- A montage API for one-off actions.

---

## 2. Files and dependencies

```
Frameworks/Animation/
    AnimationInstance.h/.cpp
    Animation_Architecture_Overview.md
    Animation_Blueprint_Setup.md
```

| Dependency | Type | Purpose |
|---|---|---|
| `Core`, `CoreUObject`, `Engine` | Module | `UAnimInstance` lives in Engine |
| `GameplayTags` | Module | Tag types |
| State Machine framework | Source | The native tag vocabulary. At runtime it is optional (section 5). |

No Movement or Character header is included. Movement facts are read through engine base classes (`APawn`, `UPawnMovementComponent`, `UCharacterMovementComponent`).

---

## 3. Threading model

```
GAME THREAD                                       WORKER THREAD
-----------                                       -------------
NativeInitializeAnimation
  cache Pawn, movement, StateMachine
  apply root motion mode

NativeUpdateAnimation
  GatherSnapshot()  --- FAnimationSnapshot --->   NativeThreadSafeUpdateAnimation
  (velocity, rotation, movement flags,               ComputeAnimationData()
   state tags, upper-body switch)                       domain, states, speeds, flags, weights
                                                                  |
                                                                  v
                                                        AnimGraph evaluation
                                                        (reads UPROPERTY variables only)
```

The snapshot holds **value types only**. The worker thread never touches the Pawn, the movement component or the state machine component, which removes data races.

Rules for anything you add:

| Where | Allowed |
|---|---|
| `GatherSnapshot` / `NativeUpdateAnimation` (game thread) | Read gameplay objects, components, subsystems |
| `ComputeAnimationData` (worker thread) | Read the snapshot, config properties and this object's own members. Write this object's variables. |

Settings that make it run on the worker thread (both are on by default):

- *Project Settings > Engine > Animation*: **Allow Multi Threaded Animation Update**
- Animation Blueprint *Class Settings*: **Use Multi Threaded Animation Update**

If either is off, the same code runs on the game thread with identical results.

---

## 4. Data flow from state to pose

```
Movement facts --> ABaseCharacter --> StateMachineComponent (tags)
                                              |
                                        GatherSnapshot (game thread)
                                              |
                                        ComputeAnimationData (worker thread)
                                              |
        Domain / GroundState / FlyState / WaterState / flags / weights
                                              |
                                        Animation Blueprint poses
```

---

## 5. Domain resolution and toggles

Each update resolves the domain in three steps:

1. **Requested domain.**
   - With a state machine on the pawn: from its domain tag.
   - Without one: from movement (`swimming` gives Water, `flying` gives Fly, otherwise Ground).
2. **Enabled domain.** If the requested domain's toggle is on it is used. Otherwise the first enabled domain wins in the order Ground, Fly, Water. If all three are off (a configuration mistake) Ground is used.
3. **State.**
   - If the domain was not substituted and a state machine supplies a tag the class knows, the tag is mapped to the enum.
   - Otherwise the state is derived from motion (table below).

Motion-based fallback states:

| Domain | Rule |
|---|---|
| Ground | falling: `Jump`; crouching: `Crouch`; not moving: `Idle`; ground speed at or above `RunSpeedThreshold`: `Run`; else `Walk` |
| Fly | moving: `Move`, else `Idle` |
| Water | moving: `SurfaceMove`, else `SurfaceIdle` |

This gives the guarantees you asked for:

| Game | Toggles | Result |
|---|---|---|
| Ground only | Ground on, Fly off, Water off | The graph is only ever in Ground. Fly and Water branches are never evaluated and can be deleted. |
| Flight only | Ground off, Fly on, Water off | The graph is only ever in Fly, even if some code briefly reports Ground. Ground and Water branches are never evaluated. |
| Ground + Water | Fly off | Fly requests fall back to Ground. |
| No state machine | any | Domain and basic states come from movement, so the graph still animates. |

**Important:** the three state enums keep their last value while their domain is inactive. Read `GroundState` only when `Domain == Ground`, `FlyState` only when `Domain == Fly`, `WaterState` only when `Domain == Water`. The Animation Blueprint guide does this by construction.

---

## 6. Variable reference (what the AnimGraph reads)

Config (Class Defaults):

| Property | Default | Purpose |
|---|---|---|
| `bEnableGroundDomain`, `bEnableFlyDomain`, `bEnableWaterDomain` | on | Domain toggles |
| `bEnableRootMotion` | off | Root motion switch (section 9) |
| `EnabledRootMotionMode` | Root Motion From Montages Only | Mode used when the switch is on |
| `MovingSpeedThreshold` | 3.0 | cm/s above which the character counts as moving |
| `RunSpeedThreshold` | 300.0 | Walk to Run threshold for the motion-based fallback only |
| `LandedFlagDuration` | 0.15 | Seconds `bJustLanded` stays true |
| `bRequireMovementInputToMove` | on | `bShouldMove` also needs input |
| `bUpperBodyLayerAllowed` | on | Master switch for the upper-body layer |
| `bSuppressUpperBodyDuringActions` | on | Drop the layer while an action state is active |
| `UpperBodyBlendInterpSpeed` | 12.0 | Blend speed of the weight (0 = instant) |

State and locomotion values:

| Variable | Type | Meaning |
|---|---|---|
| `Domain` | `EAnimationDomain` | Active graph branch: Ground, Fly, Water |
| `GroundState` | `EGroundAnimState` | Idle, Walk, Run, Jump, Slide, Crouch, Dash, Roll, Mantle |
| `FlyState` | `EFlyAnimState` | Idle, Move, Glide, Dash, Dodge |
| `WaterState` | `EWaterAnimState` | SurfaceIdle, SurfaceMove, SurfaceFastSwim, TransitionToSurface, TransitionToUnderwater, UnderwaterIdle, UnderwaterMove, UnderwaterFastSwim, UnderwaterDash, Resurfacing |
| `ActiveStateTag` | `FGameplayTag` | Effective tag (action else locomotion). Invalid without a state machine. |
| `ActiveTags` | `FGameplayTagContainer` | Domain + locomotion + action tags with parents |
| `bActionActive` | bool | A timed action state is running |
| `bIsUnderwater` | bool | Water domain in an underwater state or Resurfacing |
| `bIsGliding` | bool | Fly domain in Glide |
| `bIsCrouching` | bool | Ground domain in Crouch |
| `bIsFalling` | bool | Ground domain and the movement component is falling |
| `bIsRising` | bool | `bIsFalling` and moving upward (jump start) |
| `bJustLanded` | bool | True for `LandedFlagDuration` seconds after landing on the ground |
| `Velocity` | vector | World velocity |
| `Speed` | float | 3D speed (cm/s). Fly and Water. |
| `GroundSpeed` | float | Horizontal speed (cm/s). Ground. |
| `VerticalSpeed` | float | Z velocity (cm/s), positive up |
| `Direction` | float | -180..180 degrees between facing and velocity; 0 forward, positive right |
| `bHasMovementInput` | bool | Input or acceleration is non-zero |
| `bShouldMove` | bool | Above `MovingSpeedThreshold` (Ground: horizontal speed; Fly and Water: 3D speed), and with input when required |
| `UpperBodyBlendWeight` | float | Weight for the Layered Blend per Bone node |

Functions:

| Function | Thread-safe | Purpose |
|---|---|---|
| `HasActiveTag(Tag)` | yes | Hierarchical tag query for transition and blend rules |
| `PlayActionMontage`, `StopActionMontage` | no (game thread) | One-off actions |
| `SetUpperBodyLayerAllowed(bool)` | no (game thread) | Runtime switch for the upper-body layer |

---

## 7. Tag to enum mapping

The mapping is a lookup table in `MapStateTag`, one table per domain, evaluated with plain tag equality (thread-safe).

| Enum value | Tag |
|---|---|
| `EGroundAnimState::Idle` .. `Mantle` | `Frameworks.State.Ground.Idle`, `.Walk`, `.Run`, `.Jump`, `.Slide`, `.Crouch`, `.Dash`, `.Roll`, `.Mantle` |
| `EFlyAnimState::Idle`, `Move`, `Glide`, `Dash`, `Dodge` | `Frameworks.State.Fly.Idle`, `.Move`, `.Glide`, `.Dash`, `.Dodge` |
| `EWaterAnimState::SurfaceIdle`, `SurfaceMove`, `SurfaceFastSwim` | `Frameworks.State.Water.Surface.Idle`, `.Move`, `.FastSwim` |
| `TransitionToSurface`, `TransitionToUnderwater` | `Frameworks.State.Water.Transition.ToSurface`, `.ToUnderwater` |
| `UnderwaterIdle`, `UnderwaterMove`, `UnderwaterFastSwim`, `UnderwaterDash` | `Frameworks.State.Water.Underwater.Idle`, `.Move`, `.FastSwim`, `.Dash` |
| `Resurfacing` | `Frameworks.State.Water.Resurfacing` |

An unknown tag is not an error. The mapping fails and the state is derived from motion, so the pose stays sensible.

---

## 8. Upper and lower body separation

The lower body follows the locomotion poses. The upper body can be overridden by a layer, for example a montage in the `UpperBody` slot while the legs keep running.

The C++ side supplies the **weight** for that layer:

```
target = 1  if bUpperBodyLayerAllowed and not (bSuppressUpperBodyDuringActions and an action is active)
         0  otherwise
UpperBodyBlendWeight = FInterpTo(current, target, UpperBodyBlendInterpSpeed)
```

The Animation Blueprint feeds `UpperBodyBlendWeight` into the **Blend Weights** pin of a **Layered Blend per Bone** node (branch filter on the first spine bone). Result:

| Situation | Upper-body layer |
|---|---|
| Normal locomotion, upper-body montage playing | Fully applied (weight 1) |
| Dash, Roll, Mantle, water transition (an action state) | Fades out so the full-body pose is clean (with `bSuppressUpperBodyDuringActions`) |
| Game code calls `SetUpperBodyLayerAllowed(false)` (climbing, cutscene) | Fades out, back with `true` |

The bone name and slot names are set in the graph nodes (they are not pin inputs), so they live in the Animation Blueprint, not in C++.

---

## 9. Root motion switch

Applied once in `NativeInitializeAnimation`:

| `bEnableRootMotion` | Applied mode |
|---|---|
| off | `IgnoreRootMotion` (root motion is extracted and discarded, so the mesh never slides) |
| on | `EnabledRootMotionMode` (default `RootMotionFromMontagesOnly`) |

This overrides the mode saved in the Animation Blueprint's own Class Settings, so the class default is the single control.

Guidance:

- Dash and Dodge move the character with a velocity impulse from the movement component. Leave root motion **off** (or montage-only) for the state sequences, otherwise the character would move twice.
- Use root motion for montages only (attack lunges, mantle climbs). The montage asset must also have root motion enabled.
- Play a root-motion montage on the owning client and on the server from the same replicated gameplay event, and test it with network emulation.

---

## 10. Montages (one-off actions)

Locomotion states describe what the character is *doing continuously*; montages describe things that *happen* (attack, interact, hit reaction).

```
PlayActionMontage(Montage, PlayRate, StartSection)  -->  returns duration (0 = failed)
OnActionMontageEnded(Montage, bInterrupted)         -->  fires when it finishes or is interrupted
StopActionMontage(Montage or null, BlendOutTime)
```

- Failure causes: null montage, missing slot in the AnimGraph, or a skeleton mismatch. A warning names the montage.
- Montages in the same Slot Group interrupt each other. Put `UpperBody` and `DefaultSlot` in the same group for one action at a time.
- Keep action state (`bIsAttacking`) on the character, never in the Animation Blueprint. Use the end delegate to unlock it.

---

## 11. Graceful degradation

| Situation | Behavior |
|---|---|
| No pawn owner (Animation Blueprint editor preview) | All values stay at defaults: Ground, Idle, zero velocity. No errors. |
| No state machine on the pawn | Domain and basic states come from movement. |
| Requested domain disabled | First enabled domain is used; state is derived from motion. |
| Unknown tag | Falls back to a motion-based state. |
| Non-Character pawn | Works if its movement component derives from `UPawnMovementComponent` (falling, swimming, flying, crouching queries) |
| Unwired pose pin in the graph | Only shown if that state is ever selected. Wire unused states to the Idle pose to be safe. |

---

## 12. Extending

| Goal | Steps |
|---|---|
| New state in a domain (for example `Ground.Climb`) | 1) declare the tag; 2) add an enum value; 3) add a row to that domain's table in `MapStateTag`; 4) the `Blend Poses` node gains a pin automatically after recompiling |
| New domain | Add to `EAnimationDomain`, a toggle, `IsDomainEnabled`, `ResolveEnabledDomain`, `MapStateTag`, `DeriveKinematicState`, a state enum, and a state in the graph |
| Extra numeric inputs (lean, aim offset) | Override `GatherSnapshot` (game thread) to copy the raw value, and `ComputeAnimationData` (call `Super` first) to derive the variable |
| Project-specific class | Subclass `UAnimationInstance` and use the subclass as the Animation Blueprint parent |

---

## 13. Assumptions and limits

- The framework changes nothing about your skeleton or animations. Assets are skeleton-specific.
- Walk versus Run comes from the character's sprint state, not from speed, when a state machine is present.
- Foot IK, lean, aim offsets and turn-in-place are not included.
- `Slide` and `Mantle` states exist in the enums and mapping but are triggered only by your game code through `RequestAction`.
- Nothing in this document was compiled or run in the editor.
