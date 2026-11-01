# Animation Framework - Blueprint Setup

Step-by-step construction of the Animation Blueprints in the UE 5.8 editor, for **full-body characters, pure first-person (arms-only) characters and hybrids**. Every rule and pose selector below reads a variable that `UAnimationInstance` already computes, so the graphs need no Event Graph logic. One-off animations (grab, inspect, hit reaction) are covered in Steps 13 to 18.

Prerequisites: the four frameworks compile, `BP_BaseCharacter` exists, and you have the animation sequences listed in Step 1.

## Supported configurations

All three are first-class. Pick yours once and follow the column.

| | **A. Full body** | **B. Pure first person (arms only)** | **C. Hybrid** |
|---|---|---|---|
| What the owner sees | The body (third person, or a body-only first person) | The arms | Arms in first person, body in third person |
| What other players see | The body | Nothing (no body mesh) | The body |
| Mesh with an Animation Blueprint | `Mesh` | `FirstPersonMesh` | `Mesh` and `FirstPersonMesh` |
| Animation Blueprint(s) | `ABP_Character` | `ABP_FirstPersonArms` | Both |
| Slots in the graph | `DefaultSlot` + `UpperBody` | `DefaultSlot` only | Body: both. Arms: `DefaultSlot` only |
| Layered Blend per Bone | yes | **no** | body only |
| Montages per one-off action | 1 (body) | 1 (arms) | 2 (arms + body) |
| Character Class Defaults | Default Perspective = Third Person, toggle off | Default Perspective = First Person, toggle off | your choice, toggle on |
| Steps to follow | 1 to 4, 5A, 6 to 12, 13 to 19 | 1 to 3, 5B, 6 to 12, 13 to 19 | 1 to 12 (both 5A and 5B), 13 to 19 |

Mesh and camera placement for each column is in `Docs/Character/Character_Blueprint_Setup.md` (Steps 6, 6b, 6c).

## Design in one picture

Body Animation Blueprint (A, and the body of C):

```
Locomotion state machine (which DOMAIN)
  |- Ground  -> GroundSM: [ GroundPoses  <->  JumpEnd ]
  |               GroundPoses = Blend Poses by EGroundAnimState
  |- Fly     -> Blend Poses by EFlyAnimState
  |- Water   -> Blend Poses by EWaterAnimState

Locomotion -> cached pose -> Layered Blend per Bone (slot 'UpperBody', weight = UpperBodyBlendWeight)
                          -> Slot 'DefaultSlot' -> Output Pose
```

Arms Animation Blueprint (B, and the arms of C): the same `Locomotion` machine, with no layer:

```
Locomotion state machine (same domains, same states)  ->  Slot 'DefaultSlot'  ->  Output Pose
```

One-off actions never enter these graphs as states. They arrive as montages through the slots:

```
Gameplay (input, target, rules)
   -> ABaseCharacter::PlayActionMontages(ArmsMontage, BodyMontage)
        |- arms AnimationInstance : montage in 'DefaultSlot'
        |- body AnimationInstance : montage in 'UpperBody' or 'DefaultSlot'
```

The domain is a state machine because you can delete a whole domain from it. Inside a domain the pose is chosen by an enum with a **Blend Poses by** node, which needs no transitions and grows with the enum.

---

## Step 1 - Folders and assets

```
Content/Frameworks/Animation/
    Blueprints/        ABP_Character (body), ABP_FirstPersonArms (arms)
    Ground/            AS_Idle, AS_Walk, AS_Run, AS_JumpStart, AS_JumpLoop, AS_JumpEnd,
                       AS_Slide, AS_Dash, AS_Roll, AS_Mantle, BS_Crouch
    Fly/               AS_FlyIdle, AS_FlyMove, AS_Glide, AS_FlyDash, AS_FlyDodge
    Water/             AS_SwimSurfaceIdle, AS_SwimSurfaceMove, AS_SwimSurfaceFast,
                       AS_SwimToSurface, AS_SwimToUnderwater,
                       AS_SwimUnderIdle, AS_SwimUnderMove, AS_SwimUnderFast, AS_SwimUnderDash,
                       AS_SwimResurface
    Arms/              AS_Arms_Idle, AS_Arms_Walk, AS_Arms_Run, AS_Arms_JumpStart, AS_Arms_JumpLoop,
                       AS_Arms_JumpEnd ... (arms skeleton, only the states your game uses)
    Montages/          AM_Grab_Body, AM_Grab_Arms, AM_Inspect_Body, AM_Inspect_Arms ...
```

Ground, Fly and Water hold **body** animations (body skeleton). `Arms` holds the same states for the **arms** skeleton. Only create the folders and assets for the domains and configurations your game uses. A pure first-person game has no body animations at all. Montages are skeleton-specific, so every one-off action has one montage per skeleton it must appear on (Step 15).

Loop settings you will apply later (body and arms alike):

| Loop on | Loop off (one-shot) |
|---|---|
| Idle, Walk, Run, JumpLoop, Slide, Crouch, all Fly loops, all swim loops, Resurface | JumpStart, JumpEnd, Dash, Roll, Mantle, FlyDash, FlyDodge, SwimToSurface, SwimToUnderwater, SwimUnderDash |

`BS_Crouch` is a **1D Blend Space** on an axis named `Speed`, range 0 to 250: crouch idle at 0, crouch walk at 200.

## Step 2 - Create the Animation Blueprints

Do this once per Animation Blueprint your configuration needs (`ABP_Character` for the body skeleton, `ABP_FirstPersonArms` for the arms skeleton).

1. In `Blueprints`, right-click > **Animation > Animation Blueprint**.
2. **Parent Class:** search **AnimationInstance** and select it. Use the same parent for the arms.
3. **Target Skeleton:** the skeleton of that mesh. Name it `ABP_Character` or `ABP_FirstPersonArms`.
4. Open it and leave the **Event Graph** empty. The C++ class does all updates.

## Step 3 - Configure the class

Toolbar **Class Settings**: **Use Multi Threaded Animation Update** = on (default), in both Animation Blueprints.

Toolbar **Class Defaults**, category **Animation Framework**:

| Category | Property | `ABP_Character` (body) | `ABP_FirstPersonArms` (arms) |
|---|---|---|---|
| Domains | Enable Ground Domain | Match the character's movement component | Same as the body |
| Domains | Enable Fly Domain | Match the character's movement component | Same as the body |
| Domains | Enable Water Domain | Match the character's movement component | Same as the body |
| Root Motion | Enable Root Motion | off (turn on only for root-motion montages, Step 19) | off |
| Thresholds | Moving Speed Threshold | 3 | 3 |
| Thresholds | Run Speed Threshold | 300 (used only when no state machine exists) | 300 |
| Thresholds | Landed Flag Duration | 0.15 | 0.15 |
| Thresholds | Require Movement Input To Move | on | on |
| Layers | Upper Body Layer Allowed | on | **off** (the arms have no layer) |
| Layers | Suppress Upper Body During Actions | on | **off** |
| Layers | Upper Body Blend Interp Speed | 12 | not used |

The arms Animation Blueprint must use the **same domain toggles as the body**, because both read the same character state. `UpperBodyBlendWeight` is still computed for the arms, and simply is not read by the graph.

In **My Blueprint**, click the gear icon and tick **Show Inherited Variables**. `Domain`, `GroundState`, `FlyState`, `WaterState`, `bJustLanded`, `bIsRising` and the rest now appear. You will drag them into the graphs below.

## Step 4 - Create the animation slots

Slots belong to the **skeleton**, not to the Animation Blueprint.

**Body skeleton (A, C):**

1. **Window > Anim Slot Manager** (in the Animation Blueprint editor).
2. In `DefaultGroup` keep `DefaultSlot` and click **Add Slot**, name it `UpperBody`.
3. Save the skeleton if prompted.

**Arms skeleton (B, C):** nothing to add. `DefaultSlot` already exists in `DefaultGroup`, and it is the only slot the arms use. Do not create `UpperBody` for arms.

If the arms and the body share one skeleton asset, the `UpperBody` slot exists on both. That is harmless: the arms graph simply never references it.

## Step 5 - Build the top of the AnimGraph

### 5A - Body (configurations A and C)

Open **AnimGraph** of `ABP_Character` and create this chain:

```
[Locomotion state machine] -> [Save cached pose "LocomotionPose"]

[Use cached pose "LocomotionPose"] -> Base Pose        [Layered blend per bone] -> [Slot 'DefaultSlot'] -> [Output Pose]
[Use cached pose "LocomotionPose"] -> [Slot 'UpperBody'] -> Blend Poses 0 -^
                        UpperBodyBlendWeight ---------------> Blend Weights 0 -^
```

1. Right-click > **State Machines > Add New State Machine**, name it `Locomotion`.
2. Drag from its output pin, add **Save cached pose**, name it `LocomotionPose`.
3. Add **Use cached pose 'LocomotionPose'** twice.
4. Add a **Slot** node: *Slot Name* = `UpperBody`, source = a cached pose.
5. Add **Layered blend per bone**:
   - *Base Pose* = a cached pose.
   - *Blend Poses 0* = the `UpperBody` slot output.
   - *Blend Weights 0* = variable **UpperBodyBlendWeight** (drag it from My Blueprint).
   - In the node's Details, *Layer Setup > 0 > Branch Filters*: add one entry, **Bone Name** = `spine_01` (UE5 mannequin; use your first spine bone), **Blend Depth** = `0`.
6. Add a second **Slot** node: *Slot Name* = `DefaultSlot`. Connect the layered blend output to its source, and its output to **Output Pose**.

With no montage playing, both slots pass the locomotion pose straight through.

### 5B - Arms (configurations B and C)

Open **AnimGraph** of `ABP_FirstPersonArms` and create this chain:

```
[Locomotion state machine] -> [Slot 'DefaultSlot'] -> [Output Pose]
```

1. Right-click > **State Machines > Add New State Machine**, name it `Locomotion`.
2. Add a **Slot** node: *Slot Name* = `DefaultSlot`. Connect the state machine output to its source and its output to **Output Pose**.

That is the whole top of the arms graph. There is no cached pose, no `UpperBody` slot and no Layered Blend per Bone, because the arms are already the upper body and there is no lower body to keep running (the reasoning is in Step 14.3). With no montage playing, `DefaultSlot` passes the locomotion pose straight through.

## Step 6 - The `Locomotion` state machine (domains)

Build `Locomotion` in **each** Animation Blueprint. The structure is identical for the body and the arms; only the animation assets differ.

Double-click `Locomotion`. Right-click > **Add State** three times: `Ground`, `Fly`, `Water`. Connect **Entry** to the state you want at spawn: `Ground` for normal games, `Fly` for a flight-only game.

Create transitions by dragging from a state edge to another. Open each transition's rule graph and build:

```
Equal (Enum)  [ Domain  ==  <target domain> ]   ->  Can Enter Transition
```

Drag **Domain** in from My Blueprint, pull off its pin, search `==`, and pick the domain value on the second pin.

| From | To | Rule | Blend |
|---|---|---|---|
| Ground | Fly | `Domain == Fly` | 0.20 |
| Fly | Ground | `Domain == Ground` | 0.20 |
| Ground | Water | `Domain == Water` | 0.25 |
| Water | Ground | `Domain == Ground` | 0.25 |
| Fly | Water | `Domain == Water` | 0.25 |
| Water | Fly | `Domain == Fly` | 0.25 |

Create only the transitions between states you keep.

## Step 7 - `Ground` state

### 7.1 Landing state machine

1. Double-click `Ground`. Add a **State Machine** node named `GroundSM` and connect it to **Result**.
2. In `GroundSM` create two states, `GroundPoses` (entry) and `JumpEnd`.

| From | To | Rule | Blend |
|---|---|---|---|
| GroundPoses | JumpEnd | `bJustLanded` | 0.05 |
| JumpEnd | GroundPoses | `Time Remaining` of the JumpEnd player is at most 0.1 | 0.15 |

`JumpEnd` contains one **Sequence Player** with `AS_JumpEnd`, loop off, wired to Result. To make the time rule, right-click in the rule graph, search **Time Remaining**, and choose the entry for that sequence player; compare it with `<=` and `0.1`.

`AS_JumpEnd` should be longer than about 0.3 s so the landing state ends after `bJustLanded` has gone false. If it is shorter, lower **Landed Flag Duration**.

### 7.2 The pose selector

Open `GroundPoses`. Right-click, search **Blend Poses by**, and choose the entry for **EGroundAnimState**. Connect its output to **Result**, and drag **GroundState** into its **Active Enum Value** pin.

In the node's Details:

- Tick **Reset Child On Activation** so one-shot animations always restart from the beginning.
- Set the **Blend Time** for each pose:

| Pose pin | Content | Blend Time |
|---|---|---|
| Idle | Sequence Player `AS_Idle` | 0.20 |
| Walk | Sequence Player `AS_Walk` | 0.20 |
| Run | Sequence Player `AS_Run` | 0.20 |
| Jump | State machine `JumpSM` (7.3) | 0.10 |
| Slide | Sequence Player `AS_Slide` | 0.15 |
| Crouch | Blend Space Player `BS_Crouch`, `Speed` = **GroundSpeed** | 0.20 |
| Dash | Sequence Player `AS_Dash` | 0.08 |
| Roll | Sequence Player `AS_Roll` | 0.08 |
| Mantle | Sequence Player `AS_Mantle` | 0.10 |

Give `AS_Walk` and `AS_Run` the same sync group so the feet stay together when switching: in each Sequence Player's Details, **Sync** > *Group Name* = `Locomotion`, *Group Role* = `Can Be Leader`.

Any pose you have no animation for (for example Slide or Mantle if the game never triggers them) should still be wired to the `AS_Idle` player. It is never shown unless that state is requested, and this way a request cannot produce a reference pose.

### 7.3 `JumpSM` (the Jump pose)

1. Create a new **State Machine** named `JumpSM` and wire it to the **Jump** pin.
2. Inside, create `JumpStart` (`AS_JumpStart`, loop off) and `JumpLoop` (`AS_JumpLoop`, loop on). Connect **Entry** to `JumpStart`.
3. Transition `JumpStart -> JumpLoop`, blend 0.15, rule: `NOT bIsRising` **OR** `Time Remaining <= 0.1` (Time Remaining for the JumpStart player).

Jumping plays JumpStart then JumpLoop. Walking off a ledge (`bIsRising` false) goes straight to JumpLoop.

## Step 8 - `Fly` state

1. Double-click `Fly`. Add **Blend Poses by** for **EFlyAnimState**, wire it to **Result**, and connect **FlyState** to its **Active Enum Value**.
2. Tick **Reset Child On Activation**.

| Pose pin | Content | Blend Time |
|---|---|---|
| Idle | Sequence Player `AS_FlyIdle` | 0.25 |
| Move | Sequence Player `AS_FlyMove` | 0.25 |
| Glide | Sequence Player `AS_Glide` | 0.25 |
| Dash | Sequence Player `AS_FlyDash` | 0.08 |
| Dodge | Sequence Player `AS_FlyDodge` | 0.08 |

## Step 9 - `Water` state

1. Double-click `Water`. Add **Blend Poses by** for **EWaterAnimState**, wire it to **Result**, and connect **WaterState** to its **Active Enum Value**.
2. Tick **Reset Child On Activation**.

| Pose pin | Content | Blend Time |
|---|---|---|
| SurfaceIdle | `AS_SwimSurfaceIdle` | 0.25 |
| SurfaceMove | `AS_SwimSurfaceMove` | 0.25 |
| SurfaceFastSwim | `AS_SwimSurfaceFast` | 0.20 |
| TransitionToSurface | `AS_SwimToSurface` | 0.15 |
| TransitionToUnderwater | `AS_SwimToUnderwater` | 0.15 |
| UnderwaterIdle | `AS_SwimUnderIdle` | 0.25 |
| UnderwaterMove | `AS_SwimUnderMove` | 0.25 |
| UnderwaterFastSwim | `AS_SwimUnderFast` | 0.20 |
| UnderwaterDash | `AS_SwimUnderDash` | 0.08 |
| Resurfacing | `AS_SwimResurface` | 0.25 |

## Step 10 - Assign the Animation Blueprints

1. Compile and save each Animation Blueprint. The compile log must be clean: the graph reads only inherited member variables, so there are no thread-safety warnings.
2. Open `BP_BaseCharacter` and assign them by configuration:

| Config | `Mesh` (body) | `FirstPersonMesh` (arms) | Character Class Defaults |
|---|---|---|---|
| **A. Full body** | Skeletal Mesh Asset = body. **Animation Mode** = *Use Animation Blueprint*, **Anim Class** = `ABP_Character` | no mesh asset | Default Perspective = Third Person, Allow Perspective Toggle = off |
| **B. Pure first person** | **no Skeletal Mesh Asset** (leave it empty) | arms mesh, **Anim Class** = `ABP_FirstPersonArms` | Default Perspective = First Person, Allow Perspective Toggle = off |
| **C. Hybrid** | body mesh, **Anim Class** = `ABP_Character` | arms mesh, **Anim Class** = `ABP_FirstPersonArms` | your default view, Allow Perspective Toggle = on |

Notes:

- Both Animation Blueprints read the same variables, because both meshes belong to the same character. The arms Blueprint never needs the body Blueprint, and the reverse.
- In configuration B there is no body Animation Blueprint, and nothing breaks: `Mesh` has no mesh asset, and every character call that touches the body (Step 16) skips it silently.
- `ABP_Character` is also the Animation Blueprint of any mesh a head-socket camera rides on (`Character_Blueprint_Setup.md`, Step 6c). That mesh keeps animating while it is hidden from its owner.
- The mesh placement, eye point and camera profiles of each configuration are in `Character_Blueprint_Setup.md` (Steps 6, 6b, 6c).

## Step 11 - Verify

Play in the editor. In the Animation Blueprint select your character in the **Debug Object** dropdown to see states light up, and run `ShowDebug Animation` for the active values.

| Test | Expected |
|---|---|
| Stand still | `Domain = Ground`, `GroundState = Idle` |
| Walk, hold Sprint | `Walk`, then `Run` |
| Crouch | `Crouch` |
| Jump | `Jump` (JumpStart, then JumpLoop), landing plays JumpEnd |
| Dash / Roll | `Dash` / `Roll` state, then back |
| Press Fly (or press Jump again while airborne) | `Domain = Fly`, `FlyState = Idle`; move without Sprint: `Move`; move while holding Sprint: `Glide` (stand still and it returns to `Idle`) |
| Dash / Dodge while flying | `Dash` / `Dodge` |
| Enter the water pool (no key needed) | `Domain = Water`, `SurfaceIdle`; move while holding Sprint: `SurfaceFastSwim` |
| Dive | `TransitionToUnderwater` for about 0.6 s, then `Underwater*` |
| Rise back up | `Resurfacing`, then `TransitionToSurface`, then `Surface*` |
| Animation Blueprint editor preview | Ground / Idle, no errors (no pawn exists there) |

The same table applies to the arms: open `ABP_FirstPersonArms`, pick the character in **Debug Object** and walk through it. In configuration B the arms Blueprint is the only one, so the visible check is the arms pose changing between Idle, Walk, Run, Jump and landing, with no body in view.

Multiplayer: repeat with two clients. Every player must see the other's states. If a remote character stays in `Idle`, see the State Machine documents.

## Step 12 - Turning domains off

Do both parts for each unused domain, in **each** Animation Blueprint (body and arms). The Class Defaults toggle guarantees the domain is never entered; deleting the state keeps the graph clean.

| Game | Toggles off (Class Defaults) | Delete in `Locomotion` | Delete in `GroundSM` | Entry connects to |
|---|---|---|---|---|
| Ground only | Fly, Water | `Fly`, `Water` | (none) | `Ground` |
| Ground + Water | Fly | `Fly` | (none) | `Ground` |
| Ground + Flight | Water | `Water` | (none) | `Ground` |
| **Flight only** | **Ground, Water** | `Ground`, `Water` | (state is deleted with Ground) | `Fly` |
| Water only | Ground, Fly | `Ground`, `Fly` | (deleted with Ground) | `Water` |

Deleting a state removes its transitions. Nothing else references the deleted parts. The variables still exist in C++ and are simply unused, so no C++ change is ever needed.

**Flight-only checklist:**

1. Movement component: *Enable Ground Domain* off, *Enable Fly Domain* on.
2. Animation Blueprint: *Enable Ground Domain* off, *Enable Water Domain* off, *Enable Fly Domain* on.
3. `Locomotion`: delete `Ground` and `Water`; Entry to `Fly`.
4. Compile. It must compile without errors.

## Step 13 - One-off actions: where they live

A one-off action is something that *happens* (grab, inspect, press a button, flinch from an explosion) as opposed to something the character *is doing continuously* (walking, flying). This step settles who owns what. Steps 14 to 18 build it.

### 13.1 The rule

```
INTENT  lives in gameplay    input, target, rules, cooldown
POSE    lives in the ABP     slots only, no interaction logic
BRIDGE  is the montage       ABaseCharacter::PlayActionMontages
```

### 13.2 Who does what

| Concern | Lives in | Why |
|---|---|---|
| Deciding *when* (input, target, range, rules) | Gameplay code: the project character class, or an interaction component of the project (in the test setup, the Event Graph of `BP_BaseCharacter`) | This is game logic. The framework does not own it; promote it into a framework only after it repeats in several projects. |
| Playing, stopping and replicating the montage | `ABaseCharacter::PlayActionMontages`, `StopActionMontages`, `IsPlayingActionMontage`, which call `UAnimationInstance::PlayActionMontage` | One call reaches the arms and the body, with network forwarding. |
| Slots and the layer | The Animation Blueprint graph (Steps 4 and 5) | The graph only declares *where* a montage can show. |
| Moves that change *how the character moves* (Dash, Roll, Slide, Mantle) | State machine action tags through `RequestAction` | They need a locomotion pose and movement coupling. |
| Reading what the character is doing (to allow or refuse an action) | State machine queries: `HasActiveAction`, `IsInDomain`, `IsInState` | Read-only. |
| Timing inside the action (the frame where the hand closes) | Montage notifies | The timing lives with the animation asset. |

### 13.3 Why an interaction is not a state machine action

`RequestAction` and the state machine are for movement. Three facts from the code make them wrong for interactions:

1. `BeginAction` rejects any tag that is not under the active domain's state root (`Frameworks.State.Ground`, `.Fly`, `.Water`). An interaction would have to pretend to be a Ground, Fly and Water state at once.
2. `MapStateTag` only knows the tags in its tables. An unknown tag is not an error: the state is derived from motion, so the pose ignores your action and the Animation Blueprint would need a new enum value and a new pose per interaction.
3. While an action is running, `bActionActive` is true and the upper-body layer fades out (`Suppress Upper Body During Actions`). An `UpperBody` montage started that way would be hidden by the framework itself.

Use the state machine only to **read**: it tells you when a Dash, Roll or Mantle owns the pose, so you can refuse or cancel an interaction.

### 13.4 Montage, state or stance?

| The action ... | Use |
|---|---|
| happens on top of whatever movement is going on and has its own length (grab, press, inspect, drink, reload, hit reaction) | a **montage**, through `PlayActionMontages` |
| replaces how the character moves (Dash, Roll, Slide, Mantle, swim transitions) | a **state machine action** (`RequestAction`), with its pose in the Animation Blueprint |
| lasts until something ends it (carrying a crate, holding a torch) | a **stance**: a variable the Animation Blueprint reads (add it in a subclass, `Animation_Architecture_Overview.md`, section 12). Use montages only for picking up and putting down. |

### 13.5 What the Animation Blueprint never contains

No interaction variables, no casts to the character, no input events, no notify handlers, no "is interacting" flag. If you find yourself adding one, the logic belongs in gameplay (13.2).

---

## Step 14 - Slots, override and blend

### 14.1 Which slot

| Action | Full body (A, body of C) | Arms only (B, arms of C) |
|---|---|---|
| Grab, press a button, inspect with one hand, drink, reload (legs may keep moving) | `UpperBody` | `DefaultSlot` |
| Pick up from the floor, open a heavy door, any action that needs legs or the whole body still | `DefaultSlot` | `DefaultSlot` |
| Hit reaction, flinch | `UpperBody` for a light hit, `DefaultSlot` for a heavy one. Or author it additive (below). | `DefaultSlot`, ideally additive |

Hybrid (C): one action is a **pair**. The arms montage goes in the arms `DefaultSlot`; the body montage goes in `UpperBody` or `DefaultSlot` as in the full-body column.

`UpperBody` and `DefaultSlot` share `DefaultGroup`, so a new montage in either one interrupts the previous one. One action plays at a time.

### 14.2 How a montage meets locomotion

| Situation | What you see |
|---|---|
| Body, `UpperBody` montage | Legs keep the locomotion pose. From `spine_01` up the montage replaces it (weight 1). While a Dash, Roll, Mantle or water transition runs, the layer fades out, so the montage is hidden until that action ends or you cancel it (Step 17.3). |
| Body, `DefaultSlot` montage | The whole body is the montage. The locomotion pose keeps running underneath and returns at blend-out. |
| Arms, `DefaultSlot` montage | The arms are the montage. The locomotion sway is hidden until blend-out. |
| Any slot, additive montage | The montage is added on top of the locomotion pose instead of replacing it. Author the asset as additive (the sequence's **Additive Anim Type** = *Local Space*, with a base pose). Use it for flinches and small reactions. |

Blend in and out are the montage asset's **Blend In** and **Blend Out** times. No state machine transition is involved, so nothing to wire.

### 14.3 Arms only: the direct answers

- **Do I need an `UpperBody` slot or a Layered Blend per Bone?** No. The arms *are* the upper body, and there is no lower body to keep running. Leave them out (Step 5B).
- **Where do grab and interact animations play?** On `DefaultSlot`, as montages. They replace the arms pose for their length and blend back to the locomotion arms.
- **Should they be states in `Locomotion`?** No (13.3, 13.4). A state per action multiplied by the locomotion states would never end, and tags would have to carry something the movement does not know about.
- **When would I add a second arms slot?** Only if two arm animations must play at the same time (a held-item pose plus a finger gesture over it). Add a slot and a Layered Blend per Bone from the clavicles at that point, not before.

---

## Step 15 - Author the montages

Create one montage per skeleton per action. For a grab in a hybrid character: `AM_Grab_Arms` (arms skeleton) and `AM_Grab_Body` (body skeleton).

1. Right-click the animation > **Create > Create AnimMontage**.
2. In the montage's slot track choose the slot from 14.1: `DefaultSlot` for arms, `UpperBody` or `DefaultSlot` for the body.
3. **Blend In** 0.15 to 0.2, **Blend Out** 0.2, **Enable Auto Blend Out** on (the montage ends by itself).
4. Make the arms and body montages **the same length**, with the **same section names** if you use sections (`Start`, `Hold`, `End`). They are played at the same rate, and `PlayActionMontages` returns the longest duration.
5. Timing for gameplay: right-click the notify track > **Add Notify > Play Montage Notify**, name it `GrabCommit`, and place it on the frame where the hand closes. **Put it in one montage per action only**: the arms montage when the character has an arms mesh, otherwise the body montage. Two copies fire twice.
6. Cosmetic notifies (sounds, particles) go in the montage of the mesh that is seen. Arms notifies play on the owner only, by design.
7. Leave root motion off in the montage asset unless Step 19 applies.

A mesh that has no montage for an action simply passes null (a pure first-person character passes only the arms montage).

---

## Step 16 - Trigger an action from gameplay

### 16.1 The runtime flow

```
1. Input pressed                      (owner)
2. Gate: is it allowed right now?     (Step 17.1)
3. Find and validate the target       (project logic)
4. PlayActionMontages(Arms, Body)     plays at once on the owner, forwarded by the server
5. Montage notify GrabCommit          (owner) apply the result: attach, pick up, open (Step 17.2)
6. Montage blends out                 locomotion returns, the gate opens by itself
7. Cancel path: a Dash / Roll / Mantle starts -> StopActionMontages, no commit
```

### 16.2 Test setup in `BP_BaseCharacter` (no C++ change in your project)

The framework already contains everything. This recipe adds the trigger to the test Blueprint so you can see it work and copy the pattern.

**Variables** (type *Anim Montage*, instance editable):

| Variable | Value |
|---|---|
| `GrabMontageArms` | `AM_Grab_Arms` (leave empty in configuration A) |
| `GrabMontageBody` | `AM_Grab_Body` (leave empty in configuration B) |

**Event Graph, setup (Begin Play):**

1. `Event BeginPlay` > from the `State Machine` component, **Bind Event to On Action Started** > custom event `CancelInteraction` > **Stop Action Montages** (*Blend Out Time* `0.15`).
2. `First Person Mesh` > **Get Anim Instance** > **Cast To AnimationInstance** > **Bind Event to On Play Montage Notify Begin** > custom event `OnMontageNotify`.
3. Repeat 2 with `Mesh` and bind it to the same `OnMontageNotify` event. A mesh without an Animation Blueprint fails the cast, which is fine.

**Event Graph, the trigger (test key):**

1. `E` (Keyboard Events, Pressed) > **Branch** on the AND of:
   - **Is Locally Controlled**
   - NOT **Is Playing Action Montage**
   - NOT **Has Active Action** (from `State Machine`)
   - **Is In Domain** (from `State Machine`, *Domain Tag* = `Frameworks.Domain.Ground`)
2. True > **Play Action Montages**: *First Person Montage* = `GrabMontageArms`, *Body Montage* = `GrabMontageBody`, *Play Rate* `1`, *Start Section* `None`.
3. Optional: **Print String** with the return value. `0` means nothing started; read `LogFrameworksAnimation` (slot missing or skeleton mismatch).

**Event Graph, the commit:**

1. `OnMontageNotify` > **Branch**: **Is Locally Controlled** AND *Notify Name* == `GrabCommit`.
2. True > **Print String** `Grab committed`. This is the place where your project applies the result (attach the object, add it to the inventory, open the door). In a networked game this is also where the owner sends the server request (Step 17.2).

The Keyboard `E` event is only the test input. In a project, bind an Enhanced Input action in the project character (16.3).

### 16.3 The same thing in a project character (C++)

Interaction input belongs to the project, as the Character documents describe: bind your own action in the project's `SetupPlayerInputComponent` override, after `Super`. The handler is the whole trigger:

```cpp
// A<Project>Character.h
UPROPERTY(EditDefaultsOnly, Category = "Interaction")
TObjectPtr<UAnimMontage> GrabMontageArms;     // null in a full-body game

UPROPERTY(EditDefaultsOnly, Category = "Interaction")
TObjectPtr<UAnimMontage> GrabMontageBody;     // null in a pure first-person game

// A<Project>Character.cpp  (needs #include "StateMachine/StateMachineComponent.h")
void A<Project>Character::Input_Interact(const FInputActionValue& Value)
{
	if (!IsLocallyControlled() || IsPlayingActionMontage())
	{
		return;
	}
	if (!StateMachine || StateMachine->HasActiveAction()
		|| !StateMachine->IsInDomain(TAG_Frameworks_Domain_Ground.GetTag()))
	{
		return;
	}

	// PROJECT GAME LOGIC: find and validate the target here, store it for the commit.

	PlayActionMontages(GrabMontageArms, GrabMontageBody);
}
```

The cancel and the commit use the same bindings as in 16.2 (`StateMachine->OnActionStarted` and the `OnPlayMontageNotifyBegin` delegate of the mesh Animation Instances).

### 16.4 The three typical actions

| Action | Montage layout | Trigger and end |
|---|---|---|
| **Grab** (take an object) | One short montage per skeleton, `GrabCommit` notify in the arms montage (body montage in a full-body game) | Press; commit at the notify; ends by itself |
| **Inspect** (hold an object up) | Sections `Start` (plays once), `Hold` (linked to itself in the Sections panel so it loops), `End` | Press plays it. Press again, or release, calls **Stop Action Montages** (blend out 0.25). |
| **Environment reaction** (flinch from an explosion) | Additive or `UpperBody` montage, no commit notify | No input at all: server code calls `PlayActionMontages` (Step 17.5) |

---

## Step 17 - Stay in sync with player intent

### 17.1 Gate before you play

Check these in order, on the owning machine, before every one-off action. The first four are the same checks as in 16.2.

| Check | Why |
|---|---|
| `IsLocallyControlled` | Only the owner turns input into actions. Remote copies mirror the result. |
| NOT `IsPlayingActionMontage` | Derived from the animation itself, so it can never get stuck. Do not keep a separate "is interacting" flag. |
| NOT `State Machine > HasActiveAction` | A Dash, Roll, Mantle or water transition owns the pose right now. |
| `State Machine > IsInDomain (Ground)` | The montages are authored for walking. Fly and Water need their own montages and their own gate. |
| A valid target in range (project) | Game logic. |

### 17.2 Two moments: start, then commit

- **Press** decides *whether* to start. It validates and plays the animation. Nothing in the world has changed yet.
- **The `GrabCommit` notify** decides *when the result lands*: the object attaches to the hand on the frame where the hand closes.

If the montage is interrupted before the notify (a Dash cancels it), the notify never fires and nothing is committed. That is the behavior you want, and it needs no cleanup code.

In a networked game the owner sends the server a request from the commit event (an RPC of your project, carrying the target). The server validates it again (range, still available) and applies it. The animation is cosmetic; the server owns the result.

### 17.3 Cancel

Bind the state machine's `On Action Started` to `Stop Action Montages` (16.2, step 1). Any movement action that starts then ends the interaction on every machine, the arms and the body together. This is the recommended policy because an `UpperBody` montage is hidden by Dash, Roll and Mantle anyway, and would otherwise reappear half-finished when the action ends. Jump and Crouch do not start actions, so they do not cancel; change the policy here if your game wants that.

### 17.4 Who plays what (multiplayer)

| Machine | Arms montage | Body montage |
|---|---|---|
| Owning client | plays at once (no wait for the server) | plays at once (hidden from the owner in first person) |
| Server | does not play for a remote player's pawn; plays for the host's own pawn | plays |
| Other clients | never played (the arms are owner-only) | plays |
| Simulated proxy calling the function | ignored | ignored |

- The server forwards with reliable multicasts and skips the owner, who already played. `PlayRate` and blend-out time are clamped on the server.
- A client that was not relevant when the action started does not replay it. Keep one-off montages short; anything that lasts is a stance (13.4), not a montage.

### 17.5 Actions started by the server

Hit reactions, environment events and AI actions start on the server. Call `PlayActionMontages` there (for example behind a **Has Authority** branch). The server plays the body montage and forwards both montages to every machine, **the owner included**, so the owner sees the arms react.

### 17.6 Do not

- Do not play montages from the Animation Blueprint, and do not add interaction variables to it.
- Do not put `GrabCommit` in both the arms and the body montage.
- Do not call `PlayActionMontages` on a character you do not own or are not the server for.
- Do not make a one-off action a state machine action (13.3).

---

## Step 18 - Test checklist

Use `BP_BaseCharacter` from 16.2 with placeholder montages. Run each line in every configuration you ship.

| Test | Expected |
|---|---|
| Stand still, press `E` | A: body `UpperBody` montage plays. B: arms montage plays. C: arms in first person, body in third person. Locomotion returns after blend-out. `Grab committed` prints once. |
| Walk or run, press `E` | A or C body: legs keep moving, upper body is the montage. B or arms: the arms are the montage and the walk sway returns afterwards. |
| Press `E` twice quickly | The second press is ignored (`IsPlayingActionMontage`). |
| Press `E`, then Dash or Roll before the commit frame | The montage stops on both meshes. `Grab committed` does not print. |
| Hybrid: press `E` once | `Grab committed` prints once, even though two montages played (the notify exists in one montage only). |
| Hybrid: press `E`, then `V` (switch view) | The action continues in the other view without a pop. |
| Configuration B: Output Log | No warning about the body mesh. |
| Missing slot test: rename the slot in the montage | `PlayActionMontages` returns `0` and `LogFrameworksAnimation` names the montage. |
| Two clients: client 1 presses `E` | Client 2 sees client 1's **body** montage. Client 2 never sees client 1's arms. |
| Listen server or server code plays a reaction on client 1 | Client 1 sees its own arms react, and client 2 sees client 1's body react. |

Nothing in Steps 13 to 18 was compiled or run in the editor when they were written. Run this table once and note any difference before you copy the setup into a project.

---

## Step 19 - Root motion (optional)

- **Class Defaults > Root Motion > Enable Root Motion** off: root motion is discarded (default; correct for velocity-driven Dash and Roll).
- On: root motion is applied in the mode you pick (default *Root Motion From Montages Only*). Use it for attack lunges or climbs made as montages. Also enable root motion on the montage asset.
- Root motion moves the capsule only through the character's body `Mesh`, so use it in the **body** montage. An arms-only character (configuration B) has no body Animation Blueprint, and an arms montage never moves the capsule; give such actions a movement impulse from the movement component instead.
- Play a root-motion montage with `PlayActionMontages` from the owning client or the server, so the body montage runs on both. Test it with network emulation.

## Troubleshooting

| Symptom | Likely cause |
|---|---|
| Character is in a T-pose | No **Anim Class** on `Mesh`, or a pose pin of the selected state is unwired. |
| A state shows the reference pose | The pin for that enum value has no player. Wire `AS_Idle` to it. |
| Character always in Ground / Idle | Class Defaults toggles disagree with the character (a domain is off), or the mesh has no pawn owner. |
| Compile warning about thread safety | A node in the graph calls a Blueprint function that is not thread-safe. Use only inherited variables and `Has Active Tag`. |
| Jump start never plays | `JumpSM` entry is not `JumpStart`, or `Reset Child On Activation` is off. |
| Landing plays twice or not at all | `AS_JumpEnd` is shorter than 0.3 s. Lower **Landed Flag Duration**. |
| Feet slide between Walk and Run | Sync group is missing on the two players. |
| Upper-body montage plays on the whole body | The Layered Blend branch filter bone is wrong, or the montage uses `DefaultSlot`. |
| Upper-body montage does nothing | `UpperBodyBlendWeight` is not wired into *Blend Weights 0*, or **Upper Body Layer Allowed** is off. |
| Montage does not play | The montage's slot name is not in the graph, or the montage belongs to a different skeleton. Check the log for `LogFrameworksAnimation`. |
| Character slides while dashing or rolling | Root motion is on for those sequences while the movement component also moves the character. Turn root motion off. |
| Remote players animate wrongly | The state machine is not replicating. Check the State Machine setup document. |
| Arms are in a T-pose or reference pose in first person | No **Anim Class** on `FirstPersonMesh`, a pose pin of the selected state is unwired, or the arms Animation Blueprint was made for another skeleton. |
| Arms Animation Blueprint compiles but `UpperBody` errors appear | The arms graph references the `UpperBody` slot. Remove the slot, the cached poses and the Layered Blend (Step 5B). |
| `PlayActionMontages` returns 0 | Both montages are null, the slot named in the montage is not in that Animation Blueprint, the montage belongs to another skeleton, or the mesh has no Animation Blueprint. `LogFrameworksAnimation` and `LogBaseCharacter` give the reason. |
| The body animates but the arms do nothing (or the reverse) | The matching montage variable is empty, or you called the function on a machine that does not own the character (the arms play only on the owner). |
| Warning: the Animation Blueprint "does not derive from AnimationInstance" | That mesh's Animation Blueprint has another parent class. Re-parent it to **AnimationInstance** (Step 2). |
| `Grab committed` prints twice | `GrabCommit` is in both montages, or two bindings call the same handler. Keep it in one montage (Step 15). |
| The action looks cut off or pops when a Dash starts | Expected: a locomotion action fades the upper-body layer and `CancelInteraction` stops the montage (Step 17.3). |
| Other players do not see the action | `BodyMontage` is null, `Mesh` has no Animation Blueprint, or the call was made on a simulated proxy. |
| The owner does not see a hit reaction started by the server | The server code called the function on a different character, or `FirstPersonMontage` is null. |

