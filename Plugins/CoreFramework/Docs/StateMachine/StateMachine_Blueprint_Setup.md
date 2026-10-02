# State Machine Framework - Blueprint Setup

How to use `UStateMachineComponent` in the editor and in Blueprints. `ABaseCharacter` already contains one and drives it, so for the standard character nothing needs to be built. This document covers configuring it, reading it, reacting to it, extending it, and using it on other actors.

---

## Step 1 - Confirm the tags exist

The tag vocabulary is native, so there is no data table and no `.ini` step.

1. Open any Gameplay Tag picker (for example the tag field of **Request Action** on a character node).
2. Type `Frameworks`. You should see `Frameworks.Domain.*` and `Frameworks.State.*`.

If the list is empty, the module did not compile or load. Fix the build first.

## Step 2 - Find the component on the character

1. Open `BP_BaseCharacter`.
2. In **Components**, select **StateMachine**.
3. In **Details** the framework properties are under **State Machine**:

| Category | Property | Default | Notes |
|---|---|---|---|
| Domains | `Enable Ground Domain`, `Enable Fly Domain`, `Enable Water Domain` | on | **Set automatically at game start** from the character's movement component. Leave them alone for a `BaseCharacter`. Change them only when you use the component on another actor. |
| Rules | `Validate Domain Membership` | on | Rejects tags that do not belong to the active domain |
| Rules | `Allow Action Interrupt` | on | A new action replaces a running one |
| Rules | `Default Action Duration` | 0.5 | Seconds. Used when no duration is passed |
| Rules | `Action Duration Overrides` | empty | Map of tag to seconds |

### Action duration overrides

Use the map for actions started by your own code with `Request Action` and no explicit duration (Slide, Mantle):

| Key (Gameplay Tag) | Value (seconds) |
|---|---|
| `Frameworks.State.Ground.Slide` | `0.8` |
| `Frameworks.State.Ground.Mantle` | `1.1` |

Dash and Dodge do not use the map, because the movement component passes their duration explicitly (their **State Duration** fields).

## Step 3 - Read the state in Blueprints

Get the component from any character with **Get State Machine** (on a `BaseCharacter`) or **Get Component By Class** (`StateMachineComponent`) on any actor.

| Question | Node |
|---|---|
| What is the character doing right now? | **Get Active State** (action if running, else locomotion) |
| What domain? | **Get Active Domain** |
| Is an action running? | **Has Active Action** |
| Is it on the ground at all? | **Is In State** with `Frameworks.State.Ground`, *Exact* off |
| Is it exactly running? | **Is In State** with `Frameworks.State.Ground.Run`, *Exact* on |
| Is it in the Water domain? | **Is In Domain** with `Frameworks.Domain.Water` |

Compare two tags with the **Equal (Gameplay Tag)** node. Use **Is In State** for hierarchy checks.

## Step 4 - React to changes

In the character's or a component's **Event BeginPlay**, get the state machine and bind:

| Event | Typical use |
|---|---|
| **On State Changed** (Old, New) | Footstep sounds by state, HUD icons, camera changes, gameplay rules |
| **On Domain Changed** (Old, New) | Switch input hints, change audio buses, enable or disable abilities |
| **On Action Started** (Tag, Duration) | Play effects, lock movement input, start a cooldown UI |
| **On Action Ended** (Tag) | Unlock input, stop effects |

Example: block player input while rolling.

```
On Action Started (Tag, Duration)
    Branch: Equal (Gameplay Tag) Tag == Frameworks.State.Ground.Roll
        True: Set Ignore Move Input (Player Controller) = true

On Action Ended (Tag)
    Branch: Tag == Frameworks.State.Ground.Roll
        True: Set Ignore Move Input = false
```

The same events also fire on other players' characters on your machine (they come from replication), so the same graph works for effects on remote players.

## Step 5 - Start your own actions (Slide, Mantle, custom)

Use the character node **Request Action** (Tag, Duration):

| Input | Value |
|---|---|
| Action Tag | `Frameworks.State.Ground.Mantle` |
| Duration | `-1` to use the override map, or a number of seconds |

Call it on the **locally controlled** character. It starts the action at once for the owner and asks the server to start it too, which then replicates to everyone else. The return value is `false` when the tag is invalid, not in the active domain, or blocked by `Allow Action Interrupt`.

The animation layer plays the matching state automatically. Slide and Mantle have poses in the Ground animation set.

## Step 6 - Add your own tags

Two ways. Pick the one that matches where the tag lives.

| Where | How |
|---|---|
| Library tag (part of the framework) | Add `UE_DECLARE_GAMEPLAY_TAG_EXTERN` in `StateMachineComponent.h` and `UE_DEFINE_GAMEPLAY_TAG` in the `.cpp`, with a string that starts with `Frameworks.State.<Domain>.` |
| Project tag (game specific) | *Project Settings > Project > Gameplay Tags > Manage Gameplay Tags* > add `Frameworks.State.Ground.Climb`, for example |

Either way, a tag under `Frameworks.State.Ground.` belongs to the Ground domain automatically.

The animation layer maps only the framework's known state tags. A new tag is still usable:

- Call **Request Action** with it, or set it with **Set Locomotion State**.
- In the Animation Blueprint read it with **Has Active Tag** (see the Animation setup document), or extend the mapping in C++.

## Step 7 - Use the component on another actor

1. Open the actor Blueprint (a mount, a vehicle, an AI pawn).
2. **Add Component > State Machine**.
3. In its Details tick only the domains that actor uses.
4. Drive it from your own logic on the **server** (and on the owning client for predicted actors):

| Node | When |
|---|---|
| **Set Domain** (`Frameworks.Domain.Ground`) | When the actor changes domain. Call this before Set Locomotion State. |
| **Set Locomotion State** | Every time the state changes (calling it with the same value every tick is fine) |
| **Begin Action** / **End Action** | Timed overlays |

Do not call the setters on simulated proxies. They receive the state by replication.

Make sure the actor replicates (**Replicates** ticked in its Class Defaults). The component replicates its own properties.

## Step 8 - Test

Single player. Add this to the character's **Event BeginPlay**:

```
Get State Machine -> Assign On State Changed
    Event: Print String  ("State: " + Old + " -> " + New)
```

| Action | Printed transition (examples) |
|---|---|
| Stand still, then walk | `Ground.Idle -> Ground.Walk` |
| Hold Sprint | `Ground.Walk -> Ground.Run` |
| Jump | `... -> Ground.Jump`, then back on landing |
| Press Dash | `... -> Ground.Dash`, then back after the duration |
| Press Fly | `Ground.* -> Fly.Idle` (domain change resets the state to `Fly.Idle`) |
| Enter the pool | `Ground.* -> Water.Surface.Idle` |
| Dive | `... -> Water.Transition.ToUnderwater` for the transition time, then `Water.Underwater.*` |

Multiplayer (Play As Client, 2 players):

1. Keep the same Print String on both windows, plus a **Print String** of **Get Active State** on a timer (0.5 s) for a character that is **not** locally controlled (guard with **Is Locally Controlled** = false).
2. Move, sprint, jump and dash on window 1. Window 2 must print the same states for that character.
3. With `NetEmulation.PktLag 100` the remote state arrives late but in order.
4. A very short action (well under 0.1 s) may not appear on the remote machine. This is expected (see the limits in the overview).

## Troubleshooting

| Symptom | Likely cause |
|---|---|
| State never leaves `Ground.Idle` | Nothing drives the component (custom actor), or it runs only on simulated proxies. |
| **Begin Action** / **Request Action** returns false | The tag is not under the active domain's root, the domain is disabled, or an action is running with `Allow Action Interrupt` off. |
| Set Locomotion State returns false | The tag belongs to another domain. Call **Set Domain** first. |
| A domain never activates | Its **Enable** flag is off on the component (on a `BaseCharacter`, on the movement component). |
| Events do not fire on remote characters | The actor does not replicate, or the component was created at runtime without `Set Is Replicated`. |
| Remote client shows a different state | The owner and server drivers disagree; check that movement values match on both sides. |
| Tags not found in pickers | The module did not build. Check the compile log. |
