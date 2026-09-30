# Character Framework - Blueprint Setup

Step-by-step editor setup for `ABaseCharacter`. Follow the steps in order. The Animation Blueprint for the meshes is covered by the Animation documents (Stage 3), so leave the Anim Class empty for now or use any Animation Blueprint that fits your skeleton.

Prerequisites: the four framework folders and `FrameWorks.Build.cs` (with `EnhancedInput` and `GameplayTags`) are in the project, and it compiles.

---

## Step 1 - Check project input settings

*Project Settings > Engine > Input > Default Classes*

| Setting | Value |
|---|---|
| Default Player Input Class | `EnhancedPlayerInput` |
| Default Input Component Class | `EnhancedInputComponent` |

New UE5 projects already use these. If the character does not react to input later, this is the first thing to verify.

## Step 2 - Create the folders

```
Content/Frameworks/Character/
    Input/            IA_* assets and IMC_Character
    Blueprints/       BP_BaseCharacter
    Data/             DA_InputConfig_Default
```

## Step 3 - Create the Input Actions

In `Input`, right-click > **Input > Input Action** for each row.

| Asset | Value Type |
|---|---|
| `IA_Move` | Axis2D (Vector2D) |
| `IA_Look` | Axis2D (Vector2D) |
| `IA_Jump` | Digital (bool) |
| `IA_Crouch` | Digital (bool) |
| `IA_Sprint` | Digital (bool) |
| `IA_Fly` | Digital (bool) |
| `IA_Swim` | Digital (bool) |
| `IA_Dash` | Digital (bool) |
| `IA_Dodge` | Digital (bool) |
| `IA_PerspectiveToggle` | Digital (bool) |

Leave the **Triggers** list of every action empty. That gives the "fires every frame while held" behavior that `Jump`, `Crouch` and `Swim` need, and the `Started` event that the other actions use.

## Step 4 - Create the Mapping Context

Right-click > **Input > Input Mapping Context**, name it `IMC_Character`, then add the mappings.

| Action | Key | Modifiers on that key |
|---|---|---|
| `IA_Move` | `W` | Swizzle Input Axis Values (Order: YXZ) |
| `IA_Move` | `S` | Swizzle Input Axis Values (YXZ), then Negate |
| `IA_Move` | `A` | Negate |
| `IA_Move` | `D` | (none) |
| `IA_Look` | Mouse XY 2D-Axis | Negate with **X unticked, Y ticked** |
| `IA_Jump` | `Space Bar` | (none) |
| `IA_Crouch` | `Left Ctrl` | (none) |
| `IA_Sprint` | `Left Shift` | (none) |
| `IA_Fly` | `F` | (none) |
| `IA_Swim` | `C` | (none) |
| `IA_Dash` | `Q` | (none) |
| `IA_Dodge` | `E` | (none) |
| `IA_PerspectiveToggle` | `V` | (none) |

Gamepad is added the same way. For the left stick on `IA_Move` use *Gamepad Left Thumbstick 2D-Axis* with no modifiers, and for the right stick on `IA_Look` use *Gamepad Right Thumbstick 2D-Axis* with the same Y-only Negate.

## Step 5 - Create the Input Config data asset

1. In `Data`, right-click > **Miscellaneous > Data Asset**.
2. In the class picker choose **InputConfigData**. Name it `DA_InputConfig_Default`.
3. Open it and fill in:

| Field | Value |
|---|---|
| Default Mapping Context | `IMC_Character` |
| Mapping Priority | `0` |
| Actions > Move | `IA_Move` |
| Actions > Look | `IA_Look` |
| Actions > Jump | `IA_Jump` |
| Actions > Crouch | `IA_Crouch` |
| Actions > Sprint | `IA_Sprint` |
| Actions > Fly | `IA_Fly` |
| Actions > Swim | `IA_Swim` |
| Actions > Dash | `IA_Dash` |
| Actions > Dodge | `IA_Dodge` |
| Actions > Perspective Toggle | `IA_PerspectiveToggle` |

4. Right-click the asset > **Asset Actions > Validate**. It must report success. Errors name any missing context, Move or Look.

Leave an action empty to remove that feature from the game. For a ground-only game, clear Fly, Swim, and the water and flight actions you do not use.

## Step 6 - Create the character Blueprint

1. In `Blueprints`, right-click > **Blueprint Class**, search for **BaseCharacter**, name the asset `BP_BaseCharacter`.
2. Open it. The **Components** panel already contains everything below (nothing to add):

   `CapsuleComponent`, `Mesh`, `FirstPersonCamera`, `FirstPersonMesh`, `ThirdPersonSpringArm`, `ThirdPersonCamera`, `StateMachine`, `CharacterMovement`.

3. Select **Mesh** (third-person body):
   - **Skeletal Mesh Asset:** your character mesh.
   - **Animation Mode:** Use Animation Blueprint. **Anim Class:** your ABP (from Stage 3) or leave empty for now.
   - The C++ constructor places the mesh at `Z = -88` with `Yaw = -90`, which fits the UE5 mannequin. Adjust only if your mesh pivot differs.
4. Select **FirstPersonMesh** (arms):
   - **Skeletal Mesh Asset:** your first-person arms mesh.
   - **Anim Class:** its own Animation Blueprint. It can use the same `AnimationInstance` parent class as the body.
   - Move it in the viewport relative to `FirstPersonCamera` until the arms sit correctly in view.
5. Select **FirstPersonCamera**. It is attached to the `Mesh` socket named in *Class Defaults > First Person Camera Socket Name*. Add a small **Relative Location** offset if the camera needs to sit forward of the head bone.
6. Select **ThirdPersonSpringArm**. Tune *Target Arm Length*, *Socket Offset* and *Camera Lag* to taste.
7. Open **Class Defaults** and set:

| Category | Property | Value |
|---|---|---|
| Character > Input | Input Config | `DA_InputConfig_Default` |
| Character > Camera | Default Perspective | First Person or Third Person |
| Character > Camera | Allow Perspective Toggle | on (off locks the view) |
| Character > Camera | First Person Camera Socket Name | `head` (UE5 mannequin) or your head bone/socket |
| Character > States | Moving Speed Threshold | `3` |
| Character > States | Water Transition Duration | `0.6` |

8. Select **CharacterMovement** and configure domains, speeds and actions using `Movement_Blueprint_Setup.md`.
9. Compile and save.

## Step 7 - Use the character in the level

1. Create or open a Game Mode Blueprint. Set **Default Pawn Class** to `BP_BaseCharacter`.
2. In *World Settings*, set **GameMode Override** to it.
3. Make sure a **Player Start** exists.

## Step 8 - Test single player

| Test | Expected result |
|---|---|
| Move with `WASD`, look with the mouse | Character moves along the camera, mouse look is not inverted |
| `Space` | Jump |
| `V` | View switches; first person hides the body and shows the arms |
| `V` again | Third person returns, arms hidden |
| `Left Shift` while moving | Faster movement (Run) |
| `Left Ctrl` | Crouch |
| `F` | Takes off (if flight is enabled); `Space` ascends, `Left Ctrl` descends |
| `Q` / `E` | Dash / Dodge |

## Step 9 - Test multiplayer

1. *Play Settings*: **Number of Players** = 2, **Net Mode** = *Play As Listen Server* (then repeat with *Play As Client* and a dedicated server).
2. In each client window switch perspective with `V`, and check:
   - The other player's body is always visible to you.
   - The other player's arms are never visible to you.
   - Switching view on one client does not change what the other client sees of your own screen.
3. Open the console (`~`) and run `NetEmulation.PktLag 100`, then move, sprint, dash, fly and switch views. Movement should stay smooth. Run `p.NetShowCorrections 1` to draw every server correction. Repeated corrections while sprinting or dashing usually mean the movement values differ between client and server; see the Movement setup document.

## Using the framework from Blueprints

| Task | Nodes |
|---|---|
| Switch view from UI | Get Player Character > Cast To BP_BaseCharacter > **Set Perspective** (or **Toggle Perspective**) |
| React to a view change (HUD, FOV) | In the character: *Event BeginPlay* > **Assign On Perspective Changed**; or override the **On Perspective Changed** event in a child Blueprint |
| Read the current view | **Get Perspective** |
| Start a timed state such as Slide or Mantle | **Request Action** with a Gameplay Tag (for example `Frameworks.State.Ground.Slide`) and a duration in seconds |
| Listen for state changes | **Get State Machine** > Assign to **On State Changed**, **On Action Started**, **On Action Ended** |

## Troubleshooting

| Symptom | Likely cause |
|---|---|
| No input at all | Step 1 classes are wrong, `Input Config` is empty, or the mapping context asset is not set inside the data asset. Check the Output Log for the "Input is disabled" warning. |
| Look is inverted | The Y-only Negate modifier is missing on the mouse mapping. |
| `W` and `S` move sideways | The Swizzle Input Axis Values modifier is missing on the `Move` mappings. |
| Ascend or descend only nudges once | `IA_Jump`, `IA_Crouch` or `IA_Swim` has a *Pressed* trigger. Remove it. |
| First-person camera is on the floor | The `head` socket or bone is missing on the mesh. Change *First Person Camera Socket Name*. |
| Both body and arms are visible in first person | The body is not the `Mesh` component of this character, or the arms mesh is missing its owner-only flag (it is set by default; check you did not override it). |
| You see no character at all | The view is first person and no mesh is assigned to `FirstPersonMesh`. Assign the arms mesh, or press `V` for third person. |
| Character is a T-pose | No Anim Class on `Mesh`. Stage 3 covers the Animation Blueprint. |
| Console shows repeated corrections in multiplayer | Movement values changed only on one side at runtime. Change values on the Blueprint defaults, not from client-only code. |
