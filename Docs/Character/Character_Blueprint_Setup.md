# Character Framework - Blueprint Setup

Step-by-step editor setup for `ABaseCharacter`. Follow the steps in order. The Animation Blueprint for the meshes is covered by the Animation documents (Stage 3), so leave the Anim Class empty for now or use any Animation Blueprint that fits your skeleton.

Prerequisites: the four framework folders and `FrameWorks.Build.cs` (with `EnhancedInput` and `GameplayTags`) are in the project, and it compiles.

> **Game projects.** This document sets up the framework character itself (`BP_BaseCharacter`). In a game project, create the character Blueprint from the project's own class (`A<Project>Character : ABaseCharacter`), not from `BaseCharacter`, and set the same Class Defaults on it. See `Docs/Integration/Project_Integration_Guide.md`, sections 2.1 and 5.1.

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
| `IA_Dash` | Digital (bool) |
| `IA_Dodge` | Digital (bool) |
| `IA_PerspectiveToggle` | Digital (bool) |

Leave the **Triggers** list of every action empty. That gives the "fires every frame while held" behavior that `Jump` and `Crouch` need, and the `Started` event that the other actions use.

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
| Actions > Dash | `IA_Dash` |
| Actions > Dodge | `IA_Dodge` |
| Actions > Perspective Toggle | `IA_PerspectiveToggle` |

4. Right-click the asset > **Asset Actions > Validate**. It must report success. Errors name any missing context, Move or Look.

**Migrating from the earlier version:** the `Swim` entry no longer exists. After you compile, open `DA_InputConfig_Default` and confirm it opens without it. Delete `IA_Swim` and remove its `C` mapping from `IMC_Character`. Nothing new has to be added to the Input Config. Swimming works through Water Volumes (see `Movement_Blueprint_Setup.md`, Step 6).

Leave an action empty to remove that feature from the game. For a ground-only game, clear Fly and any Dash or Dodge you do not use. There is nothing to clear for swimming: it has no input and works through Water Volumes placed in the level.

## Step 6 - Create the character Blueprint

1. In `Blueprints`, right-click > **Blueprint Class**, search for **BaseCharacter**, name the asset `BP_BaseCharacter`. (In a game project, create it from the project's own character class instead.)
2. Open it. The **Components** panel already contains everything below (nothing to add):

   `CapsuleComponent`, `Mesh`, `FirstPersonCameraAnchor`, `FirstPersonMesh` (a child of the anchor), `CameraManager`, `StateMachine`, `CharacterMovement`.

   The spring arm and the camera are not in the list: the `CameraManager` component creates them at runtime.

3. Select **Mesh** (third-person body, **optional**):
   - **Skeletal Mesh Asset:** your character mesh. Leave it empty for a pure first-person character.
   - **Animation Mode:** Use Animation Blueprint. **Anim Class:** your ABP (from Stage 3) or leave empty for now.
   - The C++ constructor places the mesh at `Z = -88` with `Yaw = -90`, which fits the UE5 mannequin. Adjust only if your mesh pivot differs.
4. Select **FirstPersonCameraAnchor** (the eye point). Move it in the viewport to where the first-person camera should sit. The default is 64 cm above the capsule center. It needs no mesh and no socket.
5. Select **FirstPersonMesh** (arms):
   - **Skeletal Mesh Asset:** your first-person arms mesh.
   - **Anim Class:** its own Animation Blueprint. It can use the same `AnimationInstance` parent class as the body.
   - Position it with **Location / Rotation** in the Details panel or the viewport. Its transform is relative to the eye point (the anchor), and at runtime to the camera, which sits at the same place.
6. Select **CameraManager** and open **Camera Manager > Config > Camera Profiles**. Tune the `ThirdPerson` profile (arm length, shoulder offset) and the `FirstPerson` profile (field of view, pitch limits, look scale, `Attach Offset`). Details: `Docs/CameraSystem/Camera_Blueprint_Setup.md`.
7. Open **Class Defaults** and set:

| Category | Property | Value |
|---|---|---|
| Character > Input | Input Config | `DA_InputConfig_Default` |
| Character > Input | Double Jump Activates Flight | on (pressing Jump while airborne starts flight; off keeps it a plain jump) |
| Character > Camera | Default Perspective | First Person or Third Person |
| Character > Camera | Allow Perspective Toggle | on (off locks the view) |
| Character > Camera | First Person Camera Name, Third Person Camera Name | the profile names, normally left as `FirstPerson` and `ThirdPerson` |
| Character > States | Moving Speed Threshold | `3` |
| Character > States | Water Transition Duration | `0.6` |

8. Select **CharacterMovement** and configure domains, speeds and actions using `Movement_Blueprint_Setup.md`.
9. Compile and save.

### Step 6b - Pure first-person character (no body mesh, no head socket)

Use this for a first-person game that only shows arms. Starting from Step 6:

1. **Class Defaults:** Default Perspective = First Person, Allow Perspective Toggle = off.
2. **Mesh:** leave the Skeletal Mesh Asset empty. Nothing needs a head bone.
3. **FirstPersonMesh:** assign the arms mesh and its Animation Blueprint.
4. **FirstPersonCameraAnchor:** set its Z to the eye height you want (UE first-person templates use about 60 to 64).
5. **FirstPersonMesh:** move it until the arms sit correctly in view (the template arms usually sit a little below and behind the eye point).

### Step 6c - First person with a visible body (head socket)

1. Assign the body mesh to **Mesh** with its Animation Blueprint.
2. **CameraManager > Camera Profiles > FirstPerson:** set **Attach Socket Name** to your head socket or bone (for example `head`) and set **Attach Offset** to `0, 0, 0`.
3. Play. If the socket does not exist on the mesh, the Output Log shows a warning from `LogBaseCharacter` and the camera uses the anchor instead.

## Step 7 - Use the character in the level

1. Create or open a Game Mode Blueprint. Set **Default Pawn Class** to `BP_BaseCharacter`.
2. In *World Settings*, set **GameMode Override** to it.
3. Make sure a **Player Start** exists.

In a game project, the GameMode is the project's own (a Blueprint child of `A<Project>GameplayGameMode`) with the character Blueprint as `Default Pawn Class`; see `Docs/Integration/Project_Integration_Guide.md`, section 2.2.

## Step 8 - Test single player

| Test | Expected result |
|---|---|
| Move with `WASD`, look with the mouse | Character moves along the camera, mouse look is not inverted |
| `Space` | Jump |
| `V` | View switches; first person hides the body and shows the arms |
| `V` again | Third person returns, arms hidden |
| Pure first person (Step 6b): press Play | Camera at eye height, arms visible, no body, `V` does nothing |
| `Left Shift` while moving | Faster movement (Run) |
| `Left Ctrl` | Crouch |
| `Space` again while in the air | Flight starts (if the Fly domain is enabled) |
| `F` | Toggles flight on the ground or in the air |
| Flying, W with the camera tilted up or down, no Shift | Stays level; `Space` ascends and `Left Ctrl` descends |
| Flying, hold `Left Shift` + W, camera tilted up or down | Climbs or dives along the camera; `Space` and `Left Ctrl` are ignored |
| Walk into a Water Volume | Swimming starts on its own (no key); the same Standard / Shift rules apply |
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
| Ascend or descend only nudges once | `IA_Jump` or `IA_Crouch` has a *Pressed* trigger. Remove it. |
| First-person camera is at the wrong height | Move `FirstPersonCameraAnchor`, or change `Attach Offset` on the `FirstPerson` camera profile. |
| Output Log warning "first-person profile ... names socket ... which the body mesh does not have" | `Attach Socket Name` on the `FirstPerson` profile does not exist on `Mesh`. Clear it (pure first person) or fix the name (visible body). The camera already uses the anchor meanwhile. |
| Arms are not where expected | `FirstPersonMesh` Location and Rotation are relative to the eye point and to the camera, not to the capsule. Move the arms, not the camera. |
| Both body and arms are visible in first person | The body is not the `Mesh` component of this character, or the arms mesh is missing its owner-only flag (it is set by default; check you did not override it). |
| You see no character at all | The view is first person and no mesh is assigned to `FirstPersonMesh`. Assign the arms mesh, or press `V` for third person. |
| Character is a T-pose | No Anim Class on `Mesh`. Stage 3 covers the Animation Blueprint. |
| Console shows repeated corrections in multiplayer | Movement values changed only on one side at runtime. Change values on the Blueprint defaults, not from client-only code. |
