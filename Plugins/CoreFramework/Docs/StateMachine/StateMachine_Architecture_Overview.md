# State Machine Framework - Architecture Overview

Target: Unreal Engine 5.8 | Module macro: `FRAMEWORKS_API` | Files: `StateMachineComponent.h/.cpp`

---

## 1. Purpose

`UStateMachineComponent` is a replicated, Gameplay Tag based **state store**. It holds three layers of state, tells the rest of the game when any of them changes, and keeps remote players in sync.

It does **not** inspect movement, physics or input. Something else (a driver) tells it what the state is. In this library the driver is `ABaseCharacter`. That is what keeps the component independent: it depends only on the `GameplayTags` module and can be added to any actor (a character, a mount, a vehicle, an AI).

---

## 2. Files and dependencies

```
Frameworks/StateMachine/
    StateMachineComponent.h/.cpp                  Component + native tag vocabulary
    StateMachine_Architecture_Overview.md
    StateMachine_Blueprint_Setup.md
```

| Dependency | Type |
|---|---|
| `Core`, `CoreUObject`, `Engine` | Module |
| `GameplayTags` | Module (Gameplay Tags and native tags) |

No other framework is required.

---

## 3. The three layers

| Layer | Tag family | Lifetime | Example |
|---|---|---|---|
| **Domain** | `Frameworks.Domain.*` | Persistent | `Frameworks.Domain.Fly` |
| **Locomotion state** | `Frameworks.State.<Domain>.*` | Persistent, one at a time | `Frameworks.State.Ground.Run` |
| **Action state** | `Frameworks.State.<Domain>.*` | Timed overlay, optional | `Frameworks.State.Ground.Roll` |

**Effective state** (`GetActiveState`) = the action if one is running, otherwise the locomotion state. This is what animation and most gameplay code should read.

`GetActiveTags` returns a container with the domain, locomotion and action tags together with all their parents, so a query such as "is the character on the ground at all" is a single hierarchical check on `Frameworks.State.Ground`.

---

## 4. Tag vocabulary

All tags are **native**: they are declared in C++ (`UE_DECLARE_GAMEPLAY_TAG_EXTERN` in the header, `UE_DEFINE_GAMEPLAY_TAG` in the source). They exist as soon as the module loads, so no `.ini` file or data table is needed, and they show up in every Gameplay Tag picker in the editor.

Declared C++ names use the pattern `TAG_Frameworks_State_Ground_Idle` and the tag string is `Frameworks.State.Ground.Idle`.

| Family | Tags |
|---|---|
| Domains | `Frameworks.Domain.Ground`, `.Fly`, `.Water` |
| State roots | `Frameworks.State.Ground`, `.Fly`, `.Water` |
| Ground | `Idle`, `Walk`, `Run`, `Jump`, `Slide`, `Crouch`, `Dash`, `Roll`, `Mantle` |
| Fly | `Idle`, `Move`, `Glide`, `Dash`, `Dodge` |
| Water, surface | `Water.Surface.Idle`, `.Move`, `.FastSwim` |
| Water, transition | `Water.Transition.ToSurface`, `.ToUnderwater` |
| Water, underwater | `Water.Underwater.Idle`, `.Move`, `.FastSwim`, `.Dash` |
| Water, other | `Water.Resurfacing` |

Every state tag is a child of its domain's root (`Frameworks.State.<Domain>`), and that hierarchy **is** the domain membership rule.

Use the tags from C++ through the extern objects and `.GetTag()`:

```cpp
if (StateMachine->IsInState(TAG_Frameworks_State_Ground_Run.GetTag(), /*bExact=*/true)) { ... }
```

Do not read a native tag inside a constructor. The tag manager is ready after engine initialization.

---

## 5. Data model

| Member | Replicated | Meaning |
|---|---|---|
| `ActiveDomain` | yes, RepNotify | Current domain tag |
| `LocomotionState` | yes, RepNotify | Current locomotion tag |
| `ActionReplication` | yes, RepNotify | `Tag`, `Duration`, `Sequence` of the running action (empty tag = no action) |
| `ActionState` | no (local view) | The running action tag on this machine |
| `ActiveTags` | no (derived) | Rebuilt after every change on every machine |
| `LastNotifiedState` | no | Used to fire `OnStateChanged` only when the effective state really changes |

`Sequence` increases on every action start and end. That guarantees the RepNotify runs even when the same action is started twice in a row.

---

## 6. API

### Commands

| Function | Behavior |
|---|---|
| `SetDomain(Tag)` | Fails for an invalid or disabled domain. On success it resets the locomotion state to the domain default (`Ground.Idle`, `Fly.Idle`, `Water.Surface.Idle`), ends an action that does not belong to the new domain, and fires `OnDomainChanged`. |
| `SetLocomotionState(Tag)` | Fails for an invalid tag, or when validation is on and the tag is not under the active domain's root. |
| `BeginAction(Tag, Duration)` | Starts a timed overlay. Fails for an invalid tag, a tag outside the active domain (validation on), or when an action is running and `bAllowActionInterrupt` is off. |
| `EndAction()` | Ends the running action now. |
| `ConfigureDomains(Ground, Fly, Water)` | Applies the three enable flags and moves to the first enabled domain if the current one was disabled. |

### Queries

`GetActiveDomain`, `GetLocomotionState`, `GetActionState`, `GetActiveState`, `HasActiveAction`, `GetActiveTags`, `IsInState(Tag, bExact)`, `IsInDomain`, `IsDomainEnabled`, `BelongsToDomain`, and the static helpers `GetDomainStateRoot` and `GetDefaultStateForDomain`.

`IsInState` is hierarchical by default: `IsInState(Frameworks.State.Ground)` is true for every ground state. Pass `bExact = true` for a single state.

### Events (Blueprint-assignable)

| Event | Fires when |
|---|---|
| `OnDomainChanged(Old, New)` | The domain changes |
| `OnStateChanged(Old, New)` | The **effective** state changes (locomotion change, action start, or action end) |
| `OnActionStarted(Tag, Duration)` | An action starts |
| `OnActionEnded(Tag)` | An action ends (timer, `EndAction`, or domain change) |

The same events fire on simulated proxies, driven by the RepNotifies.

---

## 7. Rules and configuration

| Property | Default | Meaning |
|---|---|---|
| `bEnableGroundDomain`, `bEnableFlyDomain`, `bEnableWaterDomain` | on | Disabled domains cannot be entered through `SetDomain`. `ABaseCharacter` copies these from the movement component at startup. |
| `bValidateDomainMembership` | on | Reject locomotion and action tags from another domain |
| `bAllowActionInterrupt` | on | A new action replaces a running one |
| `DefaultActionDuration` | 0.5 s | Used when no duration is given and no override exists |
| `ActionDurationOverrides` | empty | Per-tag durations (seconds) |

Duration resolution for `BeginAction(Tag, Duration)`:

1. `Duration >= 0` is used as given (the movement component passes its own value for Dash and Dodge).
2. Otherwise the tag's entry in `ActionDurationOverrides`.
3. Otherwise `DefaultActionDuration`.

The initial state is the first enabled domain (Ground, then Fly, then Water) with that domain's default locomotion state.

---

## 8. Network model

| Machine | Behavior |
|---|---|
| Server | Runs the driver, owns the action timer, writes the replicated values. |
| Owning client | Runs the same driver locally (prediction) and its own action timer. Receives nothing for these properties (`COND_SkipOwner`), so replication never overwrites its predicted state. |
| Simulated proxies | Run no driver and no timer. Mirror the server through `OnRep_ActiveDomain`, `OnRep_LocomotionState` and `OnRep_Action`, which rebuild the tags and fire the same events. |

Late joiners receive the current values in the initial replication and the RepNotifies bring their local view up to date.

The component replicates its own properties (`SetIsReplicatedByDefault(true)`), so the owning actor only has to replicate itself.

---

## 9. Writing your own driver

The component is passive, so any code can drive it. For an actor that is not a character:

```cpp
// On the server and, if you want prediction, on the owning client:
StateMachine->SetDomain(TAG_Frameworks_Domain_Ground.GetTag());
StateMachine->SetLocomotionState(bMoving ? TAG_Frameworks_State_Ground_Run.GetTag()
                                          : TAG_Frameworks_State_Ground_Idle.GetTag());

// Timed overlay:
StateMachine->BeginAction(TAG_Frameworks_State_Ground_Mantle.GetTag(), 0.8f);
```

Rules for a driver:

- Call `SetDomain` before `SetLocomotionState` in the same frame (a domain change resets the locomotion state).
- Do not run a driver on simulated proxies.
- Calling a setter with the value already active is cheap and does nothing, so a driver can call it every tick.

---

## 10. Extending

| Goal | How |
|---|---|
| Add a state to an existing domain | Declare and define a native tag under that domain's root, for example `Frameworks.State.Ground.Climb`. It belongs to Ground automatically because membership is hierarchical. Tags added in `DefaultGameplayTags.ini` work the same way. |
| Make the animation layer use the new state | Add an entry to the animation enums and mapping table (see the Animation documents). Until then the animation instance falls back to motion-based states, and the graph can still read the tag with `HasActiveTag`. |
| Add a domain | Add domain and root tags, an enable flag, and a case in `IsDomainEnabled`, `GetDomainStateRoot`, `GetDefaultStateForDomain` and `PickFirstEnabledDomain`. |
| Change how actions time out | Set `ActionDurationOverrides`, or pass a duration to `BeginAction`. |

---

## 11. Graceful degradation and limits

| Situation | Behavior |
|---|---|
| Nothing drives the component | It stays at the default domain and locomotion state. Nothing errors. |
| A domain is disabled | `SetDomain` refuses it. Setters return `false`. |
| A tag from the wrong domain is supplied | Rejected while `bValidateDomainMembership` is on. |
| The owner is destroyed while an action runs | The timer is cleared in `EndPlay`. |

Limits:

- **Short actions can be missed remotely.** Remote clients receive the latest replicated value only. An action shorter than the actor's replication interval may start and end between two updates and never appear on a remote client. Keep gameplay-critical actions longer than a few network updates.
- The action timer runs on the server and the owning client only. If an owning client and the server disagree about when an action started, each ends it by its own timer.
- `GetActiveTags` returns a reference to internal data. Copy it if you keep it across frames.
- Nothing in this document was compiled or run in the editor.
