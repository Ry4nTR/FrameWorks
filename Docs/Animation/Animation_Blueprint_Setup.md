# Animation Framework - Blueprint Setup

Step-by-step construction of the Animation Blueprint in the UE 5.8 editor. Every rule and pose selector below reads a variable that `UAnimationInstance` already computes, so the graph needs no Event Graph logic.

Prerequisites: the four frameworks compile, `BP_BaseCharacter` exists, and you have the animation sequences listed in Step 1.

Design in one picture:

```
Locomotion state machine (which DOMAIN)
  |- Ground  -> GroundSM: [ GroundPoses  <->  JumpEnd ]
  |               GroundPoses = Blend Poses by EGroundAnimState
  |- Fly     -> Blend Poses by EFlyAnimState
  |- Water   -> Blend Poses by EWaterAnimState

Locomotion -> cached pose -> Layered Blend per Bone (upper-body slot, weight = UpperBodyBlendWeight)
                          -> Slot 'DefaultSlot' -> Output Pose
```

The domain is a state machine because you can delete a whole domain from it. Inside a domain the pose is chosen by an enum with a **Blend Poses by** node, which needs no transitions and grows with the enum.

---

## Step 1 - Folders and assets

```
Content/Frameworks/Animation/
    Blueprints/        ABP_Character, ABP_FirstPersonArms
    Ground/            AS_Idle, AS_Walk, AS_Run, AS_JumpStart, AS_JumpLoop, AS_JumpEnd,
                       AS_Slide, AS_Dash, AS_Roll, AS_Mantle, BS_Crouch
    Fly/               AS_FlyIdle, AS_FlyMove, AS_Glide, AS_FlyDash, AS_FlyDodge
    Water/             AS_SwimSurfaceIdle, AS_SwimSurfaceMove, AS_SwimSurfaceFast,
                       AS_SwimToSurface, AS_SwimToUnderwater,
                       AS_SwimUnderIdle, AS_SwimUnderMove, AS_SwimUnderFast, AS_SwimUnderDash,
                       AS_SwimResurface
    Montages/          AM_* (attack, interact, hit reaction ...)
```

Only create the folders and assets for the domains your game uses. Loop settings you will apply later:

| Loop on | Loop off (one-shot) |
|---|---|
| Idle, Walk, Run, JumpLoop, Slide, Crouch, all Fly loops, all swim loops, Resurface | JumpStart, JumpEnd, Dash, Roll, Mantle, FlyDash, FlyDodge, SwimToSurface, SwimToUnderwater, SwimUnderDash |

`BS_Crouch` is a **1D Blend Space** on an axis named `Speed`, range 0 to 250: crouch idle at 0, crouch walk at 200.

## Step 2 - Create the Animation Blueprint

1. In `Blueprints`, right-click > **Animation > Animation Blueprint**.
2. **Parent Class:** search **AnimationInstance** and select it.
3. **Target Skeleton:** your character skeleton. Name it `ABP_Character`.
4. Open it and leave the **Event Graph** empty. The C++ class does all updates.

## Step 3 - Configure the class

Toolbar **Class Settings**:

| Setting | Value |
|---|---|
| Use Multi Threaded Animation Update | on (default) |

Toolbar **Class Defaults**, category **Animation Framework**:

| Category | Property | Value |
|---|---|---|
| Domains | Enable Ground Domain | Match the character's movement component |
| Domains | Enable Fly Domain | Match the character's movement component |
| Domains | Enable Water Domain | Match the character's movement component |
| Root Motion | Enable Root Motion | off (turn on only for root-motion montages) |
| Thresholds | Moving Speed Threshold | 3 |
| Thresholds | Run Speed Threshold | 300 (used only when no state machine exists) |
| Thresholds | Landed Flag Duration | 0.15 |
| Thresholds | Require Movement Input To Move | on |
| Layers | Upper Body Layer Allowed | on |
| Layers | Suppress Upper Body During Actions | on |
| Layers | Upper Body Blend Interp Speed | 12 |

In **My Blueprint**, click the gear icon and tick **Show Inherited Variables**. `Domain`, `GroundState`, `FlyState`, `WaterState`, `bJustLanded`, `bIsRising` and the rest now appear. You will drag them into the graphs below.

## Step 4 - Create the animation slots

1. **Window > Anim Slot Manager** (in the Animation Blueprint editor).
2. In `DefaultGroup` keep `DefaultSlot` and click **Add Slot**, name it `UpperBody`.
3. Save the skeleton if prompted.

## Step 5 - Build the top of the AnimGraph

Open **AnimGraph** and create this chain:

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

## Step 6 - The `Locomotion` state machine (domains)

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

## Step 10 - Assign the Animation Blueprint

1. Compile and save `ABP_Character`. The compile log must be clean: the graph reads only inherited member variables, so there are no thread-safety warnings.
2. Open `BP_BaseCharacter`, select **Mesh**, set **Animation Mode** = *Use Animation Blueprint* and **Anim Class** = `ABP_Character`.
3. **First-person arms:** create `ABP_FirstPersonArms` the same way (parent **AnimationInstance**, skeleton of the arms mesh). Build the same `Locomotion` structure with your arms animations; you can omit the slot and layered blend nodes if the arms never play montages. Assign it as the **Anim Class** of `FirstPersonMesh`.

The arms Animation Blueprint reads the same variables, because its owner is the same character.

## Step 11 - Verify

Play in the editor. In the Animation Blueprint select your character in the **Debug Object** dropdown to see states light up, and run `ShowDebug Animation` for the active values.

| Test | Expected |
|---|---|
| Stand still | `Domain = Ground`, `GroundState = Idle` |
| Walk, hold Sprint | `Walk`, then `Run` |
| Crouch | `Crouch` |
| Jump | `Jump` (JumpStart, then JumpLoop), landing plays JumpEnd |
| Dash / Roll | `Dash` / `Roll` state, then back |
| Press Fly | `Domain = Fly`, `FlyState = Idle`; move: `Move`; Sprint: `Glide` |
| Dash / Dodge while flying | `Dash` / `Dodge` |
| Enter the water pool | `Domain = Water`, `SurfaceIdle` |
| Dive | `TransitionToUnderwater` for about 0.6 s, then `Underwater*` |
| Rise back up | `Resurfacing`, then `TransitionToSurface`, then `Surface*` |
| Animation Blueprint editor preview | Ground / Idle, no errors (no pawn exists there) |

Multiplayer: repeat with two clients. Every player must see the other's states. If a remote character stays in `Idle`, see the State Machine documents.

## Step 12 - Turning domains off

Do both parts for each unused domain. The Class Defaults toggle guarantees the domain is never entered; deleting the state keeps the graph clean.

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

## Step 13 - One-off actions with montages

1. Create a montage (`AM_Attack`) from your animation. In the montage's slot track choose **UpperBody** for actions the legs should ignore, or **DefaultSlot** for full-body actions.
2. Play it from gameplay code, on the locally controlled character:
   - **Blueprint:** Get the mesh's Anim Instance > Cast To your Animation Blueprint class (or `AnimationInstance`) > **Play Action Montage**. Bind **On Action Montage Ended** in Begin Play.
   - **C++:** `Cast<UAnimationInstance>(GetMesh()->GetAnimInstance())->PlayActionMontage(AttackMontage);`
3. Keep attack or interact state on the character, not in the Animation Blueprint, and clear it in the end callback.
4. Use **Animation Notifies** inside the montage for gameplay timing (hit window, spawn). For notifies that drive gameplay set the notify's trigger to *Branching Point*.

While an action state (Dash, Roll, Mantle, water transition) is active, the upper-body layer fades out automatically, so upper-body montages do not fight the full-body pose. Turn that off with *Suppress Upper Body During Actions* in Class Defaults.

## Step 14 - Root motion (optional)

- **Class Defaults > Root Motion > Enable Root Motion** off: root motion is discarded (default; correct for velocity-driven Dash and Roll).
- On: root motion is applied in the mode you pick (default *Root Motion From Montages Only*). Use it for attack lunges or climbs made as montages. Also enable root motion on the montage asset.

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
