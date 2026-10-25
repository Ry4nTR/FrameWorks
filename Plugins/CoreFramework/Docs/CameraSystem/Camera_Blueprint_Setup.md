# Camera Framework - Blueprint Setup

Step-by-step editor setup for `UCameraManagerComponent`, with the first-person placement of `ABaseCharacter` in detail. Follow the steps in order. The character itself is set up with `Character_Blueprint_Setup.md`.

Prerequisites: the plugin compiles, and a character Blueprint exists (a child of `BaseCharacter`, or of your project's character class). In the Content Browser, turn on **Settings > Show Plugin Content** to see the plugin's assets.

---

## Step 1 - Know where the camera is

The camera rig (spring arm and camera) is created at runtime by the `CameraManager` component, so you will **not** see a camera in the Components panel or the viewport. Everything about it is data:

| What you want | Where you set it |
|---|---|
| Field of view, arm length, shoulder offset, zoom range, pitch limits | `CameraManager` > `Camera Profiles` > the profile |
| Eye position of a first-person character | `FirstPersonCameraAnchor` (a component you can move) and `Attach Offset` on the `FirstPerson` profile |
| Which view the character starts in | Class Defaults > `Default Perspective` |

## Step 2 - Edit the camera profiles

1. Open the character Blueprint. In **Components**, select **CameraManager**.
2. In **Details**, open **Camera Manager > Config > Camera Profiles**. Each entry is a name and a set of values.
3. The character creates two entries, `FirstPerson` and `ThirdPerson`. Expand one and edit it:

| Group | Property | What it does |
|---|---|---|
| Camera | Field Of View | Horizontal field of view in degrees |
| Camera | Arm Length | Distance behind the character. `0` is first person |
| Camera | Socket Offset | Shoulder offset, in camera space |
| Camera | Target Offset | Offset at the arm origin, in world space |
| Zoom | Allow Zoom, Min / Max Arm Length | Range of the zoom input |
| Behavior | Do Collision Test | Pulls the camera in front of walls |
| Behavior | View Pitch Min / Max | How far the player can look down and up |
| Behavior | Look Sensitivity Scale | Multiplies look input (below 1 for aiming) |
| Behavior | Blend In Time | Seconds to blend into this profile |
| Attachment | Attach Socket Name | Socket of a body mesh to hang the camera on (optional) |
| Attachment | Attach To Socket Parent | Hang the camera on the character's attach component instead of the capsule |
| Attachment | Attach Offset | Offset of the camera in the space of its attach point |

4. **Default Camera Name** (same panel) is the profile used at spawn. The character picks the profile by its perspective, so leave it alone.

## Step 3 - Pure first person (no body mesh, no head socket)

This is the setup of a first-person game that shows only arms.

1. Open the character Blueprint > **Class Defaults**. Set **Default Perspective** = First Person and **Allow Perspective Toggle** = off.
2. Select **Mesh**. Leave **Skeletal Mesh Asset** empty. Nothing needs a head bone.
3. Select **FirstPersonCameraAnchor**. In the viewport or **Details > Transform**, set its **Location** Z to the eye height you want. The default is `64`. This is the camera position, relative to the capsule, like the camera of the standard first-person templates.
4. Select **FirstPersonMesh**. Set **Skeletal Mesh Asset** to your arms and **Anim Class** to the arms Animation Blueprint. Move it with **Location** and **Rotation** until the arms look right. These values are relative to the eye point, and at runtime to the camera, which sits at the same place.
5. Select **CameraManager** > **Camera Profiles** > `FirstPerson`: set **Field Of View**, **View Pitch Min / Max** and **Look Sensitivity Scale** to taste. Leave **Attach Socket Name** empty.
6. Compile, save and press **Play**.

Tips:

- To lower the eye while crouching, add a second profile (Step 5) with a lower **Attach Offset** Z and switch to it from your crouch logic.
- Keep eye offsets mostly on Z. A forward offset (X) turns with the capsule.

## Step 4 - First person with a visible body

Use this when the player has a body mesh and the view should follow the animated head.

1. Assign the body to **Mesh** and its Animation Blueprint.
2. **Camera Profiles** > `FirstPerson`: set **Attach Socket Name** to the head socket or bone (for example `head`) and set **Attach Offset** to `0, 0, 0`.
3. Play. If the socket does not exist on the mesh, the Output Log shows a warning (`LogBaseCharacter`) and the character uses `FirstPersonCameraAnchor` instead. The camera never falls to the mesh origin.

## Step 5 - Add another camera (aim, crouch, vehicle)

1. In **Camera Profiles**, click **+** to add an entry. Name it, for example, `Aim`.
2. Set its values (for example Field Of View `70`, Arm Length `140`, Look Sensitivity Scale `0.6`, Blend In Time `0.15`).
3. Switch to it from any Blueprint that can reach the character:
   **Get Camera Manager** (on the character) > **Switch To Camera** (Camera Name = `Aim`, Blend Time = `-1` to use the profile's Blend In Time).
4. Go back with **Revert To Previous Camera**.

Profiles that attach to the same place blend smoothly. A profile with a different attach point (a socket against the capsule) is an instant cut.

## Step 6 - Field of view and zoom

| Task | Nodes |
|---|---|
| Change the field of view for a moment | **Set FOV** (New FOV, Blend Time). It lasts until the next profile switch |
| Zoom with the mouse wheel | Call **Add Zoom Input** (Axis) from the wheel action. It is ignored when the profile has Allow Zoom off |
| Forward look input yourself | **Add Look Input** (2D vector). The character already does this for its Look action |

## Step 7 - Cameras outside the character (cinematics)

1. Place a camera actor in the level.
2. On the owning client: **Blend To Target** (New View Target = the camera actor, Blend Time `1`).
3. Return with **Blend Back To Owner**.
4. Bind **On Camera Blend Finished** on the camera manager to run something when the blend ends.

## Step 8 - The camera manager on another pawn

1. Select the pawn Blueprint > **Add > Camera Manager**.
2. Fill **Camera Profiles** and set **Default Camera Name**.
3. If a profile attaches to a mesh or a placement component, call **Set Socket Parent** once (for example in **Event BeginPlay**).

## Step 9 - Test

| Test | Expected result |
|---|---|
| Play with `Default Perspective` = First Person (Step 3) | Camera at eye height, arms visible, no body, mouse look works, no warning in the Output Log |
| Move `FirstPersonCameraAnchor` up, play again | The camera and the arms move up together |
| Move `FirstPersonMesh`, play again | Only the arms move |
| Third person (`V` with the toggle on) | Camera behind the character, arms hidden |
| `Switch To Camera` with `Aim` | The view blends to the aim camera, look speed drops |
| Step 4 with a wrong socket name | One warning, camera still at eye height |

## Troubleshooting

| Symptom | Likely cause |
|---|---|
| First-person camera at the character's feet or inside the body | An old Blueprint that names a missing socket. Clear **Attach Socket Name** (Step 3) or fix the name (Step 4). The warning in the Output Log names it |
| Camera at the middle of the capsule | The `FirstPerson` profile has **Attach To Socket Parent** off and no socket, so it attaches to the capsule with a zero offset. Turn it on, or set **Attach Offset** Z |
| Arms in the wrong place | `FirstPersonMesh` is positioned relative to the eye point. Move the arms, not the camera |
| Arms shift when I change **Attach Offset** | The arms follow the camera. Adjust the arms transform after changing offsets |
| Look is slow in one view | **Look Sensitivity Scale** on that profile is below 1 |
| `Switch To Camera` does nothing | The profile name does not exist (a warning names it) or the code runs on a dedicated server (no rig) |
| Zoom does nothing | **Allow Zoom** is off on the active profile |
| No camera blend, only a cut | The two profiles attach to different points, or **Blend Time** is `0` |
