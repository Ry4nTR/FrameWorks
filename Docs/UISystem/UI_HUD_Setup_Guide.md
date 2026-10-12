# CoreFramework UI & HUD System — Setup & Implementation Guide (v2)

**Document:** `UI_HUD_Setup_Guide.md`
**Companion:** `UI_HUD_System_Architecture.md` (design, diagrams, full headers)
**Engine target:** Unreal Engine 5.5 – 5.8
**Written for:** someone who is still learning Widget Blueprints (UMG) and CommonUI. Every editor action says **where** to click, **what** to type, and **why**.

> **Compile note.** The C++ was reviewed against the UE 5.5–5.8 CommonUI APIs but not compiled by its author. If your first build reports a signature mismatch, `UI_HUD_System_Architecture.md` → *Appendix A* names every engine call the system uses and the one-line fix.
> **Editor note.** Menu names in the editor move slightly between engine versions. When a path in this guide does not match what you see, use the **search box** of the panel (Project Settings, Details, Palette, Content Browser all have one). The property names stay the same.

---

## Contents

* **Read this first** — rules, editor vocabulary, how "named widgets" work, which parent class to pick
* **What lives where** — plugin vs. game project
* **Step 1 — Framework C++ (plugin)**
* **Step 2 — Plugin base assets (plugin)** — the only Blueprint the plugin ships
* **Step 3 — Game project foundation** — folders, CommonUI prerequisites, tags, buttons, config, HUD classes
* **Step 4 — Build your game's UI from the base parents** — HUD, Main Menu, Pause Menu, Settings with tabs, Popup
* **Step 5 — Bind gameplay data** — HP / Mana example
* **Step 6 — Input and navigation, tests, troubleshooting**
* **Final checklist**

---

# 0. Read this first

## 0.1 The three rules of this framework

1. **The plugin is infrastructure only.** It contains code that does the work (input modes, layers, loading, binding) and empty parent classes.
2. **The plugin contains no designed UI.** No Main Menu, no HUD layout, no buttons with artwork, no styles.
3. **Your game project owns every screen.** You create `WBP_MainMenu`, `WBP_HUD`, `WBP_Settings`… in your game project and choose the plugin's classes as their **parents**.

*Why?* Every game looks different. If the plugin shipped a menu you would spend your time deleting or fighting it. With this split, updating the plugin never changes how your game looks.

## 0.2 Editor vocabulary

| Word | What it is | Where to find it |
|---|---|---|
| **Content Browser** | The file explorer of the editor. All your assets live here. | Bottom of the editor (`Ctrl+Space` opens the drawer). |
| **Widget Blueprint (WBP)** | A piece of UI (a screen, a button, a health bar) designed visually. | Content Browser → right-click → *User Interface → Widget Blueprint*. |
| **Parent class** | The class a Blueprint is built on. It gives the Blueprint its behavior. You pick it when you create the Blueprint. | The *Pick Parent Class* window. |
| **Designer** | The mode of the widget editor where you drag and drop the layout. | Top-right of the widget editor: the **Designer** button. |
| **Graph** | The mode where you place Blueprint nodes (logic). | Next to Designer: the **Graph** button. |
| **Palette** | List of widgets you can drag into your layout. Has a search box. | Designer, top-left. |
| **Hierarchy** | The tree of all widgets of your screen (a widget inside another is indented). | Designer, left, below the Palette. |
| **Details panel** | Properties of whatever is selected. Has a search box. | Right side. |
| **Class Defaults** | Properties of the **Blueprint itself** (not of a widget placed inside). This is where "Input Mode", "Is Back Handler", "Menu Tag" are. | Click **Graph**, then **Class Defaults** in the toolbar. The Details panel now shows the Blueprint's own properties. |
| **Compile** | Applies your changes and checks for errors. | Top-left toolbar. Compile (and Save) after every change. A green check means OK. |
| **Slot** | How a widget sits inside its parent (padding, alignment, size). Shown in Details under **Slot** when a child is selected. | Details panel. |
| **Visibility** | Whether a widget is drawn and whether it blocks mouse clicks. | Details → *Behavior → Visibility*. |
| **Gameplay Tag** | A named id like `UI.Screen.MainMenu`. We use tags to say "open *this* screen". | Project Settings → Gameplay Tags. |
| **Data Asset** | A file that stores settings (no logic). `DA_UIConfig` is one. | Content Browser → right-click → *Miscellaneous → Data Asset*. |
| **Soft reference** | A reference that does **not** load the thing until needed. All screens are soft references, so opening the game stays fast. | Automatic. |

## 0.3 Naming prefixes (use them everywhere)

| Prefix | Means | Example |
|---|---|---|
| `WBP_` | Widget Blueprint | `WBP_MainMenu` |
| `BP_` | Normal Blueprint (actors, styles, input data) | `BP_HUD_Gameplay` |
| `DA_` | Data Asset | `DA_UIConfig` |
| `DT_` | Data Table | `DT_UI_Actions` |

## 0.4 "Named widgets": the one concept you must understand

Many framework parent classes look for widgets **by exact name** inside your Blueprint. If a widget with that name and the right type exists, the C++ code can use it. If it does not exist, nothing breaks, the feature is just off (all bindings are optional).

How to rename a widget: in the **Hierarchy**, click it once, press **F2**, type the name, press **Enter**. Also tick **Is Variable** (checkbox at the top of the Details panel, next to the name) to be safe.

*Why names?* The C++ parent cannot know what you called your widgets, so we agree on names. It is the standard way C++ and UMG cooperate (`BindWidget`).

| Parent class | Named widgets it looks for (all optional) | What the name is for |
|---|---|---|
| `BaseRootLayoutWidget` | `GameLayerStack`, `MenuLayerStack`, `ModalLayerStack`, `OverlayLayerStack` | The four layers. |
| `BaseActivatableWidget` (and its children) | `DefaultFocusWidget` | Widget that gets keyboard / gamepad focus when the screen opens. |
| `BaseHUDWidget` | `DefaultElementSlot` | A panel where HUD elements are placed by default. |
| `BaseMenuWidget` | `TabList`, `TabSwitcher`, `DefaultFocusWidget` | The tab bar, the area where tab pages are shown, the focus target. |
| `BaseTabWidget` | `DefaultFocusWidget` | Focus target inside a tab page. |
| `BaseButton` | `LabelText` | The text of the button. |

Animations are also found by name: `Anim_Show` / `Anim_Hide` (`BaseUserWidget`), `Anim_Activate` / `Anim_Deactivate` (screens). To create one: Designer → **Animations** panel (bottom) → **+ Animation** → name it exactly.

## 0.5 Which parent class do I pick?

| I am building… | Parent class | Why |
|---|---|---|
| The gameplay HUD | `BaseHUDWidget` | Game input mode, HUD element slots. |
| Main menu, pause menu, settings (any menu) | `BaseMenuWidget` | UI input mode, optional tab bar. |
| A popup, a toast, any other screen on a layer | `BaseActivatableWidget` | Gets activation, input mode, focus, back handling. |
| A tab page inside a menu | `BaseTabWidget` | Gets selected / deselected events and the Apply / Revert contract. |
| A HUD element (health bar, minimap) or any small piece | `BaseUserWidget` | Can bind itself to the player's pawn. |
| A button | `BaseButton` | Has the label text feature. |
| The class that starts the UI (a HUD actor) | `BaseHUD` | Starts and stops the UI for the local player. |

There are **no** empty wrapper Blueprints such as `WBP_BaseHUD`. You use the C++ class directly as the parent.

---

# 1. What lives where

```
PLUGIN  (Plugins/CoreFramework)                      GAME PROJECT  (<YourGame>)
──────────────────────────────                       ───────────────────────────────────────
C++ base classes (UISystem/...)        ──parents──►  WBP_HUD, WBP_MainMenu, WBP_PauseMenu,
Content/UI/Layout/WBP_BaseRootLayout                 WBP_Settings, WBP_Tab_*, WBP_Button_*, WBP_Popup_*
   (the ONLY Blueprint, 4 empty stacks)  ──used by─► DA_UIConfig  (points at WBP_BaseRootLayout)
                                                     BP_HUD_*  (parents: BaseHUD)
Nothing the player sees.                             Gameplay tags, styles, fonts, textures, input data
```

| Step | Where you work | Result |
|---|---|---|
| 1. Framework C++ | Plugin | The plugin compiles with the UI system |
| 2. Plugin base assets | Plugin | `WBP_BaseRootLayout` exists |
| 3. Game project foundation | Game | Folders, CommonUI settings, tags, buttons, `DA_UIConfig`, HUD classes |
| 4. Build your UI | Game | HUD, menus, settings, popup working |
| 5. Bind gameplay data | Game | A health bar that follows the player's pawn |
| 6. Input and tests | Game | Mouse, keyboard and gamepad verified |

Steps 1–2 are done **once** in the framework repository. Steps 3–6 are done **in every game project**.

---

# Step 1 — Framework C++ (plugin)

## 1.1 Target layout

The UI code lives in one parent folder with a subfolder per responsibility:

```
Plugins/CoreFramework/Source/CoreFramework/UISystem/
├─ Core/         UITypes.h, UIGameplayTags.h/.cpp, UIFrameSettings.h
├─ Data/         UIConfigDataAsset.h
├─ Manager/      UIFrameSubsystem.h/.cpp
├─ Binding/      BaseUIBinderInterface.h, UIBindingHub.h/.cpp
├─ Layout/       BaseRootLayoutWidget.h/.cpp
├─ Foundation/   BaseUserWidget.h/.cpp, BaseActivatableWidget.h/.cpp, BaseButton.h/.cpp
├─ HUD/          BaseHUD.h/.cpp, BaseHUDWidget.h/.cpp
└─ Menu/         BaseMenuWidget.h/.cpp
   └─ Tabs/      BaseTabWidget.h/.cpp
```

*Why subfolders?* You find a class by asking "what is its job?" (manager, binding, HUD, menu) instead of scrolling through 26 files.

## 1.2 Copy the files (and migrate from the previous version)

1. Close the editor and your IDE.
2. **Delete** the old flat folder `Plugins/CoreFramework/Source/CoreFramework/UISystem/` (the 15 headers and 11 sources of the first version).
3. Copy the new `UISystem` folder from this package to `Plugins/CoreFramework/Source/CoreFramework/`.
4. Right-click your `.uproject` → **Generate Visual Studio project files** (needed because files moved).

Where did each old file go?

| Old file (flat) | New location |
|---|---|
| `UITypes.h`, `UIGameplayTags.*`, `UIFrameSettings.h` | `Core/` |
| `UIConfigDataAsset.h` | `Data/` |
| `UIFrameSubsystem.*` | `Manager/` |
| `UIBindingHub.*`, `BaseUIBinderInterface.h` | `Binding/` |
| `BaseRootLayoutWidget.*` | `Layout/` |
| `BaseUserWidget.*`, `BaseActivatableWidget.*` | `Foundation/` |
| `BaseTabButton.*` | **removed** — replaced by `Foundation/BaseButton.*` |
| `BaseHUD.*`, `BaseHUDWidget.*` | `HUD/` |
| `BaseMenuWidget.*` | `Menu/` |
| `BaseTabWidget.*` | `Menu/Tabs/` |

Include paths changed accordingly: `#include "UISystem/Foundation/BaseUserWidget.h"`.

**Breaking changes from the first version** (only matters if you already used it):

| Change | What to do |
|---|---|
| `FUITags::ScreenHUD / ScreenMainMenu / ScreenPauseMenu / ScreenSettings` removed | Create your own screen tags (Step 3.5). |
| `UBaseTabButton` removed, `TabLabelText` renamed | Blueprints that used it: open → *Class Settings* → *Parent Class* = `BaseButton`; rename the text widget to `LabelText`. |
| `UIConfigDataAsset.MenuTabSets` removed | Assign a tab set on the menu Blueprint instead (Step 4.4). |
| Tag pickers no longer filter on `CoreFramework.UI.*` | Nothing; your own tags now appear in the pickers. |

## 1.3 Module dependencies

Open `Plugins/CoreFramework/Source/CoreFramework/CoreFramework.Build.cs`. Make sure these modules are in `PublicDependencyModuleNames` (keep the ones already there; see `Setup/CoreFramework.Build.cs.additions.txt`):

```csharp
"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput",
"GameplayTags", "UMG", "Slate", "SlateCore",
"CommonUI", "CommonInput", "DeveloperSettings"
```

*Why?* Each module is a code library. `CommonUI` gives the screen stacks and input routing, `GameplayTags` the ids, `UMG` the widget base class, `DeveloperSettings` the Project Settings page.

If your **game** C++ will include CoreFramework headers (Step 5 does) and the compiler cannot find `"UISystem/..."`, add this line in the same Build.cs:

```csharp
PublicIncludePaths.Add(ModuleDirectory);
```

## 1.4 Plugin dependencies

In `Plugins/CoreFramework/CoreFramework.uplugin` add (see `Setup/CoreFramework.uplugin.additions.txt`):

```json
"Plugins": [
	{ "Name": "CommonUI",      "Enabled": true },
	{ "Name": "EnhancedInput", "Enabled": true }
],
"CanContainContent": true
```

*Why?* Game projects that use CoreFramework then enable CommonUI automatically. `CanContainContent` lets the plugin hold the one Blueprint of Step 2.

## 1.5 Build and verify

1. Build the editor target.
2. In the editor check:
   * *Edit → Project Settings* → search **CoreFramework UI**: the settings page exists.
   * *Project Settings → Gameplay Tags → Manage Gameplay Tags…* → search `CoreFramework`: you see `CoreFramework.UI.Layer.Game / Menu / Modal / Overlay` and `CoreFramework.UI.HUDSlot.Default`.
   * Content Browser → right-click → *Miscellaneous → Data Asset*: the picker lists **UIConfigDataAsset** and **UITabSetDataAsset**.
   * Content Browser → right-click → *User Interface → Widget Blueprint* → **All Classes**: search `Base` and you see `BaseHUDWidget`, `BaseMenuWidget`, `BaseTabWidget`, `BaseUserWidget`, `BaseActivatableWidget`, `BaseRootLayoutWidget`, `BaseButton`.

## 1.6 Commit

```bash
git add Plugins/CoreFramework
git commit -m "UI system v2: responsibility folders, BaseButton"
```

(Publish the plugin branch after Step 2.)

---

# Step 2 — Plugin base assets (plugin)

## 2.1 The plugin ships exactly one Blueprint

`WBP_BaseRootLayout`: a widget made of **four empty layer stacks**, nothing else.

*Why does this one exist?* The C++ needs a widget with four stacks that have **exact names**. If every game rebuilt it by hand, one typo would break the whole UI. It is infrastructure: it has no colors, no text, no art. Every other widget is created in the game project from the C++ parents.

## 2.2 Make the plugin's Content folder visible

1. Open the **Content Browser**.
2. Click **Settings** (top-right of the Content Browser, gear icon) and tick **Show Plugin Content**.
3. In the folder tree on the left you now see **CoreFramework Content** under *Plugins* (this exists because of `"CanContainContent": true`).
4. Right-click **CoreFramework Content** → **New Folder** → name it `UI`. Inside it create the folder `Layout`.

## 2.3 Create `WBP_BaseRootLayout`

1. Right-click inside `CoreFramework Content/UI/Layout` → **User Interface → Widget Blueprint**.
2. A window *Pick Parent Class for User Widget* opens. Click **All Classes** to expand the list, type `BaseRootLayoutWidget` in its search box, click it, then click **Select**.
   *Why: the parent class is what gives the Blueprint its behavior. This one owns the layer registry.*
3. Name the asset `WBP_BaseRootLayout` and double-click it. The widget editor opens in **Designer** mode (check the Designer / Graph buttons at the top right).

### Build the hierarchy

4. Look at the **Hierarchy** panel (left). If you see a **Canvas Panel**, right-click it → **Replace With → Overlay**. If the Hierarchy is empty, drag **Overlay** from the Palette (search `Overlay`) into the Hierarchy panel.
   *Why Overlay: an Overlay stacks all its children on top of each other, each one filling the whole area. Children lower in the list are drawn **on top**. That is exactly what layers are (HUD at the back, popups in front). A Canvas Panel would need manual anchors for each child.*
5. In the **Palette** search box type `Activatable`. Drag **Common Activatable Widget Stack** onto the **Overlay** row in the Hierarchy (the row highlights when you can drop). Repeat until the Overlay has **four** stacks as children (they must be indented under Overlay; if one is not, drag it onto the Overlay row again).
6. Rename them, from top to bottom, using **F2** (see 0.4), and tick **Is Variable** on each:

   | Order in Hierarchy | Name | Layer it becomes |
   |---|---|---|
   | 1 (back) | `GameLayerStack` | Game / HUD |
   | 2 | `MenuLayerStack` | Menus |
   | 3 | `ModalLayerStack` | Popups |
   | 4 (front) | `OverlayLayerStack` | Notifications |

   *Why exactly these names: `UBaseRootLayoutWidget` registers a stack under each layer tag by looking for these four names.*

### Set the Details of each stack

Select each stack in the Hierarchy, then in the **Details** panel:

7. **Slot (Overlay Slot) → Horizontal Alignment = Fill** and **Vertical Alignment = Fill**.
   *Why: a stack must cover the whole screen, so screens inside it can place themselves relative to the screen edges.*
8. **Behavior → Visibility = Not Hit-Testable (Self Only)** (in the C++ and in some docs this is called *Self Hit Test Invisible*).
   *Why: a full-screen widget that is **Visible** catches every mouse click, even when empty. The HUD stack would then block every button below it. With "Not Hit-Testable (Self Only)" the stack itself is transparent to clicks while the screens **inside** it still receive them.*
9. In the Details search box type `Transition`. Set:

   | Stack | Transition Type | Transition Duration |
   |---|---|---|
   | `GameLayerStack` | None | — |
   | `MenuLayerStack` | Fade | `0.15` |
   | `ModalLayerStack` | Fade | `0.15` |
   | `OverlayLayerStack` | None | — |

   *Why: the stack plays the open / close transition for every screen pushed into it, so you design it once. The HUD must not fade each time a menu opens.*

10. Click **Compile**, then **Save**. The Compile button must show a green check.

## 2.4 Final plugin Content

```
Plugins/CoreFramework/Content/
└─ UI/
   └─ Layout/
      └─ WBP_BaseRootLayout
```

Nothing else belongs here. If you want a fifth layer in a game, **duplicate** this asset into the game (architecture document, section 7.4) instead of editing the plugin's.

## 2.5 Commit and publish

```bash
git add Plugins/CoreFramework
git commit -m "UI system v2: WBP_BaseRootLayout"
git subtree split --prefix=Plugins/CoreFramework -b plugin-only
git push origin plugin-only
```

(Adapt branch and path names to your repository.)

---

# Step 3 — Game project foundation (every game project)

Everything from here on happens in the **game project**, never inside the plugin.

## 3.1 Pull the plugin

```bash
git submodule add -b plugin-only https://github.com/Ry4nTR/FrameWorks.git Plugins/CoreFramework
git submodule update --init --recursive
```

1. Right-click the `.uproject` → **Generate Visual Studio project files**, build, open the editor.
2. *Edit → Plugins* → search `CoreFramework`: it must be enabled (CommonUI and Enhanced Input switch on automatically through the plugin's dependency list). Restart if the editor asks.
3. If your game has C++ modules that include CoreFramework headers, add `"CoreFramework"`, `"GameplayTags"`, `"UMG"`, `"CommonUI"` to that module's `Build.cs` `PublicDependencyModuleNames`.

## 3.2 Create the Content folder structure

In the Content Browser: right-click → **New Folder**. Create this tree **before** creating assets, so nothing lands in the root folder.

```
Content/UI/
├─ Config/          DA_UIConfig, DA_Settings_Tabs
├─ Bootstrap/       BP_HUD_Gameplay, BP_HUD_MainMenu
├─ HUD/
│  └─ Elements/     WBP_HealthBar, WBP_ManaBar …
├─ Menus/
│  ├─ MainMenu/     WBP_MainMenu
│  ├─ PauseMenu/    WBP_PauseMenu
│  └─ Settings/     WBP_Settings
│     └─ Tabs/      WBP_Tab_Graphics, WBP_Tab_Audio, WBP_Tab_Controls
├─ Popups/          WBP_Popup_Confirm …
├─ Components/
│  ├─ Buttons/      WBP_Button_Primary, WBP_Button_Tab
│  │  └─ Styles/    BP_ButtonStyle_Primary, BP_ButtonStyle_Tab
│  └─ Text/         BP_TextStyle_Button …
└─ Input/           DT_UI_Actions, BP_CommonInputData
```

| Folder | Responsibility | Parent class of its Blueprints |
|---|---|---|
| `Config` | Data assets that tell the framework what exists | (data assets) |
| `Bootstrap` | Actors that start the UI per map type | `BaseHUD` |
| `HUD` / `HUD/Elements` | The gameplay HUD and the pieces shown in it | `BaseHUDWidget` / `BaseUserWidget` |
| `Menus/<Name>` | One folder per menu, with its tab pages in `Tabs` | `BaseMenuWidget` / `BaseTabWidget` |
| `Popups` | Dialogs and modal screens | `BaseActivatableWidget` |
| `Components` | Reusable building blocks (buttons, text styles) | `BaseButton`, style classes |
| `Input` | CommonUI input actions and input data | (data table, input data) |

*Why this split? One folder = one responsibility. When you look for "the pause menu" or "the style of my buttons", there is exactly one place to look. Adding a menu later means adding one folder, not searching a pile of 60 widgets.*

## 3.3 CommonUI prerequisites (once per project)

CommonUI does the input routing: which screen receives the keyboard, what Escape / gamepad B does, how focus moves. It needs three things from your project.

### A. The Game Viewport Client

1. *Edit → Project Settings*. In the search box type `viewport client`.
2. Set **Game Viewport Client Class** to **CommonGameViewportClient**.
3. Restart the editor when asked.

*Why: this class is the entry point through which CommonUI sees every input event before the game does. Without it screens open, but Back, tab shortcuts and device switching do nothing.*

### B. The UI actions table `DT_UI_Actions`

1. In `Content/UI/Input` right-click → **Miscellaneous → Data Table**.
2. In *Pick Row Structure* choose **CommonInputActionDataBase**, click **OK**, name it `DT_UI_Actions`, open it.
3. In the toolbar click **Add** four times. For each row select it in the table, type the **Row Name** in the Row Name field, then edit the **Row Editor** panel (bottom / right side of the Data Table editor):

   | Row Name | Display Name | Keyboard Input Type Info → Key | Default Gamepad Input Type Info → Key |
   |---|---|---|---|
   | `Confirm` | Confirm | `Enter` | `Gamepad Face Button Bottom` (A) |
   | `Back` | Back | `Escape` | `Gamepad Face Button Right` (B) |
   | `NextTab` | Next Tab | `E` | `Gamepad Right Shoulder` |
   | `PreviousTab` | Previous Tab | `Q` | `Gamepad Left Shoulder` |

   In a **Key** field you can click the small keyboard icon and press the key instead of searching.

*Why a table: UI actions are data, not code. Players (and you) can change keys later without touching Blueprints, and CommonUI uses the same rows to show the right button glyph for keyboard or gamepad.*

### C. The input data `BP_CommonInputData`

1. In `Content/UI/Input` right-click → **Blueprint Class** → expand **All Classes** → search `CommonUIInputData` → **Select**. Name it `BP_CommonInputData` and open it.
2. In its defaults set **Default Click Action**: Data Table = `DT_UI_Actions`, Row Name = `Confirm`. Set **Default Back Action**: Data Table = `DT_UI_Actions`, Row Name = `Back`. Save.
3. *Edit → Project Settings*, search `Common Input`. In **Common Input Settings** set **Input Data** to `BP_CommonInputData`.

*Why: when a screen has "Is Back Handler" ticked, CommonUI needs to know which action means "back". This is where you tell it.*

You can leave **Enable Enhanced Input Support** and the *Platform Input* lists as they are for now (Step 6 explains them).

## 3.4 Gameplay tags: the ids of your screens

1. *Edit → Project Settings → Gameplay Tags* (search `Gameplay Tags`) → click **Manage Gameplay Tags…**.
2. Click **Add New Gameplay Tag** (the + button). Type the **Name**, leave *Source* on `DefaultGameplayTags.ini`, click **Add New Tag**. Repeat for each tag:

   | Tag | Used for |
   |---|---|
   | `UI.Screen.HUD` | the gameplay HUD |
   | `UI.Screen.MainMenu` | main menu |
   | `UI.Screen.PauseMenu` | pause menu |
   | `UI.Screen.Settings` | settings menu |
   | `UI.Screen.ConfirmPopup` | confirmation popup |
   | `UI.HUDSlot.BottomLeft` | a HUD area for vitals (Step 4.1) |

*Why tags instead of class names: you say "open `UI.Screen.Settings`" and the framework looks up which Widget Blueprint that is in `DA_UIConfig`. You can swap the Blueprint later without changing any code or Blueprint that opens it.*

*The four **layer** tags (`CoreFramework.UI.Layer.Game`…) already exist, they come from the plugin.*

## 3.5 Reusable building blocks: text style, button style, buttons

Create these once; every menu reuses them.

### Text style `BP_TextStyle_Button`

1. In `Components/Text` right-click → **Blueprint Class** → **All Classes** → search `CommonTextStyle` → **Select**. Name it `BP_TextStyle_Button`.
2. Open it and set **Font** (family, size) and **Color**.

*Why: Common Text widgets read their font and color from a style asset. Change the asset and every text that uses it changes.*

### Button style `BP_ButtonStyle_Primary`

1. In `Components/Buttons/Styles` right-click → **Blueprint Class** → **All Classes** → search `CommonButtonStyle` → **Select**. Name it `BP_ButtonStyle_Primary` and open it.
2. Set these brushes (each is a group with **Draw As**, **Tint**, **Image**…). Use **Draw As = Box** (or *Rounded Box*) and pick a **Tint** color:

   | Property | When it is used |
   |---|---|
   | **Normal Base** | idle button |
   | **Normal Hovered** | mouse over, or focused with the gamepad |
   | **Normal Pressed** | while pressed |
   | **Disabled Base** | button disabled |
   | **Selected Base / Selected Hovered / Selected Pressed** | only for buttons that can be *selected* (tab buttons) |
   | **Min Width / Min Height** | smallest size, so short texts do not make tiny buttons |

*Why: this one asset gives every button all its visual states. You do not draw hover or pressed states yourself.*

3. Duplicate it (right-click → **Duplicate**) and name the copy `BP_ButtonStyle_Tab`. Make its **Selected Base** clearly different from **Normal Base** (for example a brighter color), so the active tab is visible.

### Button `WBP_Button_Primary`

1. In `Components/Buttons` right-click → **User Interface → Widget Blueprint**, parent class **BaseButton**, name `WBP_Button_Primary`, open it.
2. Hierarchy: if there is a Canvas Panel, right-click → **Replace With → Overlay**. Drag **Common Text** (Palette search `Common Text`) into the Overlay.
3. Rename that text widget to **`LabelText`** and tick **Is Variable**.
   *Why: `BaseButton` writes the button's label into the widget named `LabelText`.*
4. With `LabelText` selected, Details → **Slot (Overlay Slot)**: Horizontal Alignment = **Center**, Vertical Alignment = **Center**. Under **Style** choose `BP_TextStyle_Button`.
5. Click **Graph**, then **Class Defaults**. In the search box type `Style`. Set **Style** to `BP_ButtonStyle_Primary`.
   *Why: the button background and states come from this property.*
6. Compile, Save.

### Tab button `WBP_Button_Tab`

1. Right-click `WBP_Button_Primary` → **Duplicate** → `WBP_Button_Tab`.
2. Class Defaults: **Style** = `BP_ButtonStyle_Tab`. Search `Selectable` and tick **Selectable**.
   *Why: a tab button must remember "I am the active tab" (the Selected look). Normal buttons never stay selected.*
3. Compile, Save.

## 3.6 The config asset `DA_UIConfig`

1. In `Content/UI/Config` right-click → **Miscellaneous → Data Asset**. In *Pick Data Asset Class* choose **UIConfigDataAsset**. Name it `DA_UIConfig`, open it.
2. Set **Root Layout Class** to **WBP_BaseRootLayout** (the dropdown lists the plugin's asset because *Show Plugin Content* is on). Leave **Screens** and **HUD Elements** empty for now.
3. *Edit → Project Settings* → search `CoreFramework UI`. Set **Default UI Config** to `DA_UIConfig`.

*Why: the framework needs to know where your UI definition is. This setting is the single link between the plugin and your game's UI.*

## 3.7 HUD actors and Game Modes (this starts the UI)

The framework starts the UI from a **HUD actor**. A HUD actor exists only for players who actually see a screen (never on a dedicated server), which makes it the safe starting point in multiplayer.

1. In `Content/UI/Bootstrap` right-click → **Blueprint Class** → **All Classes** → `BaseHUD` → **Select**. Name it `BP_HUD_Gameplay`.
2. Open it → **Class Defaults** → search `Startup Screens`. Click **+**, choose `UI.Screen.HUD`.
3. Repeat to create `BP_HUD_MainMenu` with **Startup Screens = [`UI.Screen.MainMenu`]**.
4. Create the Game Modes (in a `Content/Game/GameModes` folder, or wherever your project keeps them):
   * `BP_GameMode_Gameplay` (parent *Game Mode Base*) → Class Defaults → **HUD Class** = `BP_HUD_Gameplay`.
   * `BP_GameMode_MainMenu` → **HUD Class** = `BP_HUD_MainMenu`, **Default Pawn Class** = *None* (a menu map needs no character).
5. In each map: *Window → World Settings* → **GameMode Override** = the matching Game Mode.
6. *Project Settings → Maps & Modes*: set the default map to your main menu map.

*Why separate HUD classes: the **Startup Screens** list is "what opens when this map starts". The main menu map opens the main menu, the gameplay map opens the HUD.*

*If you list several startup screens, put the HUD first: the most recently opened screen decides the input mode.*

---

# Step 4 — Build your game's UI from the base parents

The layouts below are **examples to teach the mechanics**. Your game will look different. What matters is the parent class, the named widgets and the settings, not the colors or positions.

## 4.1 The HUD: `WBP_HUD`

**Goal:** an empty gameplay HUD with two places where elements (health bar, minimap…) can be placed.

### Create it

1. In `Content/UI/HUD` right-click → **User Interface → Widget Blueprint** → **All Classes** → `BaseHUDWidget` → **Select**. Name it `WBP_HUD`, open it (Designer mode).

### Layout (Designer)

2. Make the root a **Canvas Panel** (if the Hierarchy is empty, drag **Canvas Panel** from the Palette; Palette → *Panel*).
   *Why: a Canvas Panel lets you pin things to screen corners with anchors, which is what a HUD needs.*
3. Drag an **Overlay** from the Palette onto the Canvas Panel row in the Hierarchy. Select it and in Details → **Slot (Canvas Panel Slot)**: click the **Anchors** dropdown and choose the preset that fills the whole screen (the bottom-right icon of the grid). Then set **Offset Left, Top, Right, Bottom** to `0`.
4. Rename the Overlay to **`DefaultElementSlot`** and tick **Is Variable**.
   *Why: the HUD class places HUD elements into the widget with this exact name when no other slot is requested.*
5. Drag a **Vertical Box** onto the Canvas Panel. Details → **Slot**: Anchors = **bottom-left** preset; **Alignment** = `X 0, Y 1`; **Position X** `40`, **Position Y** `-40`; tick **Size To Content**. Rename it `BottomLeftBox`.
   *Why: Alignment (0, 1) means "the bottom-left corner of this box sits on the anchor". With the anchor in the bottom-left corner of the screen, the box grows up and to the right and never leaves the screen.*

### Register the custom slot (Graph)

6. Click **Graph**. Right-click empty space, search **Event Construct**, add it.
7. Drag from its white arrow, search **Register Element Slot**, add it. Set **Slot Tag** = `UI.HUDSlot.BottomLeft`. Drag `BottomLeftBox` from the **Variables** list (My Blueprint panel) into the graph as a *Get* node and connect it to **Container**.
   *Why: the framework does not know your widgets by name. This node says "the tag `UI.HUDSlot.BottomLeft` means this box". Elements registered for that tag (Step 5) are placed here automatically.*

### Class Defaults

8. **Graph → Class Defaults**. In the search box type `Input Mode`: it is already **Game Only** (set by the parent class). Leave it. Search `Back`: **Is Back Handler** must be **off**.
   *Why Game Only: the HUD must not take the mouse away. Menus opened on top switch the input to UI automatically and the HUD gets it back when they close.*
9. Compile, Save.

## 4.2 The main menu: `WBP_MainMenu`

**Goal:** a menu with three buttons and keyboard / gamepad focus.

### Create it

1. In `Menus/MainMenu` create a Widget Blueprint with parent **BaseMenuWidget**, name `WBP_MainMenu`, open it.

### Layout (Designer)

2. Root **Canvas Panel**. Add a **Vertical Box**; Details → Slot: Anchors = **center**, Alignment `0.5, 0.5`, **Size To Content** on.
3. From the Palette, section **User Created** (search `Button_Primary`), drag `WBP_Button_Primary` into the Vertical Box **three times**.
4. Select the first button. In Details search `Label`. In **Button Label** type `Play`. Second: `Settings`. Third: `Quit`.
   *Why: `Button Label` is the property the `BaseButton` parent provides, so each placed button gets its own text without any Blueprint logic. The Designer preview shows it immediately.*
5. For each button, Details → **Slot (Vertical Box Slot) → Padding**: `0, 8` (space between buttons).
6. Rename the first button (**Play**) to **`DefaultFocusWidget`** and tick **Is Variable**.
   *Why this name exists: a keyboard or gamepad player needs something **already focused** when the menu opens, otherwise the arrow keys or D-pad do nothing. The menu focuses the widget named `DefaultFocusWidget` for you. (A mouse player is not affected.)*
7. Rename the other two to `SettingsButton` and `QuitButton` (so the Graph is readable).

### Button actions (Graph)

8. Select `DefaultFocusWidget` in the Hierarchy → Details → scroll to **Events** → click the green **+** next to **On Clicked**. The Graph opens with an event node. Drag from its arrow → search **Open Level (by Name)** → **Level Name** = your gameplay map name.
9. Select `SettingsButton` → **On Clicked +**. In the Graph, right-click → search **Get UI Frame Subsystem** → add. Drag from its blue output pin → search **Push Screen** → add and connect the white arrow. Set **Screen Tag** = `UI.Screen.Settings`.
   *Why this route: a screen asks the framework to open another screen by tag; it never references the other Blueprint directly. The framework loads it in the background (no freeze) and puts it on the right layer.*
10. Select `QuitButton` → **On Clicked +**. Add **Get Owning Player** → drag from its output pin → **Quit Game** (leave *Quit Preference* on **Quit**).

### Class Defaults

11. **Graph → Class Defaults**:
    * **Input Mode** = **UI Only** (default). *Why: a menu needs the cursor and must not move the character behind it.*
    * **Is Back Handler** = **off**. *Why: the main menu is the first screen, there is nothing to go back to.*
12. Compile, Save.

## 4.3 The pause menu: `WBP_PauseMenu`

1. In `Menus/PauseMenu` create a Widget Blueprint with parent **BaseMenuWidget**, name `WBP_PauseMenu`.
2. Layout like the main menu with three `WBP_Button_Primary`: **Resume** (rename it `DefaultFocusWidget`), **Settings**, **Quit to Menu**.
3. Graph:
   * **Resume → On Clicked →** node **Close Screen** (right-click → search `Close Screen`; its target is *self*).
     *Why: `Close Screen` asks the framework to remove this screen from its layer and CommonUI restores the previous input mode.*
   * **Settings → On Clicked →** Get UI Frame Subsystem → **Push Screen** (`UI.Screen.Settings`).
   * **Quit to Menu → On Clicked →** **Open Level (by Name)** with your main menu map.
4. **Class Defaults**:
   * **Is Back Handler** = **on**. *Why: Escape / gamepad B now closes the pause menu with no extra Blueprint logic (this uses the Back action from Step 3.3).*
   * Search `Pause`: **Pause Game While Active** = **on**. *Why: the game stops while the menu is open and resumes when it closes (single player and listen-server host).*
5. Compile, Save.

## 4.4 Settings with tabs: `WBP_Settings`, tab pages, `DA_Settings_Tabs`

This is the most complex example; each part has one job.

### A. The tab pages (one Blueprint per category)

1. In `Menus/Settings/Tabs` create a Widget Blueprint with parent **BaseTabWidget**, name `WBP_Tab_Graphics`.
2. Layout: root **Scroll Box** → **Vertical Box** with your controls (sliders, check boxes…). Rename the **first interactive control** to **`DefaultFocusWidget`**.
   *Why: when the player moves into the tab with the gamepad, focus lands on this control.*
3. **Graph**. In the **My Blueprint** panel hover **Functions** and click **Override**. Pick **Has Pending Changes**, then repeat for **Apply Changes** and **Revert Changes**.
   *Why: these three functions are the shared contract that lets one **Apply** button work for every tab.*
4. Minimal logic (works for any tab):
   * Create a Boolean variable `bDirty`. Whenever the player changes any control in this tab, set `bDirty = true`.
   * **Has Pending Changes** → return `bDirty`.
   * **Apply Changes** → write the control values to your settings storage, then set `bDirty = false`.
   * **Revert Changes** → reload the stored values into the controls, then set `bDirty = false`.
5. Compile, Save. Create `WBP_Tab_Audio` and `WBP_Tab_Controls` the same way (parent **BaseTabWidget**). What is *inside* each tab is your game's business; the framework only hosts it.

### B. The tab list asset `DA_Settings_Tabs`

1. In `Config` right-click → **Miscellaneous → Data Asset** → **UITabSetDataAsset** → name `DA_Settings_Tabs`, open it.
2. Click **+** next to **Tabs** three times and fill:

   | Tab Id | Display Name | Tab Widget Class | Sort Order | Enabled |
   |---|---|---|---|---|
   | `Graphics` | Graphics | `WBP_Tab_Graphics` | 0 | ✔ |
   | `Audio` | Audio | `WBP_Tab_Audio` | 10 | ✔ |
   | `Controls` | Controls | `WBP_Tab_Controls` | 20 | ✔ |

   *Why a data asset: adding a "Gameplay" tab later means adding a row, not changing the menu Blueprint. **Tab Id** is the tab's unique name, **Sort Order** decides the order (lower first).*

### C. The menu `WBP_Settings`

1. In `Menus/Settings` create a Widget Blueprint with parent **BaseMenuWidget**, name `WBP_Settings`.
2. Layout (Designer):

   ```
   Canvas Panel
   └─ Vertical Box   (Anchors: fill screen, offsets 0)
        ├─ Horizontal Box
        │    └─ Common Tab List            name: TabList
        ├─ Common Animated Switcher        name: TabSwitcher     (Slot → Size = Fill)
        └─ Horizontal Box
             ├─ WBP_Button_Primary   Button Label "Apply"
             ├─ WBP_Button_Primary   Button Label "Revert"
             └─ WBP_Button_Primary   Button Label "Back"
   ```

   * Palette search `Tab List` → drag **Common Tab List**; search `Animated Switcher` → drag **Common Animated Switcher**.
   * Rename them exactly `TabList` and `TabSwitcher`, tick **Is Variable**.
   * Select `TabSwitcher` → Details → **Slot (Vertical Box Slot) → Size** = **Fill**.
     *Why: the tab pages are shown inside the switcher; Fill gives it all the remaining height. The switcher shows one page at a time.*
3. Select `TabList`. In Details search `Tab Input`:
   * **Next Tab Input Action Data**: Data Table = `DT_UI_Actions`, Row Name = `NextTab`.
   * **Previous Tab Input Action Data**: Data Table = `DT_UI_Actions`, Row Name = `PreviousTab`.
   * Tick **Auto Listen For Input**.
   *Why: this gives you tab switching with the gamepad shoulder buttons and with `Q` / `E` for free.*
4. Name the three buttons `ApplyButton`, `RevertButton`, `BackButton`. Do **not** create a `DefaultFocusWidget` in this menu: when it does not exist, the menu focuses the button of the active tab by itself, which is where a gamepad player wants to start.
5. **Graph** — button events (select button → Details → Events → **On Clicked +**):
   * `ApplyButton` → node **Apply All Tabs**.
   * `RevertButton` → node **Revert All Tabs**.
   * `BackButton` → node **Close Screen**.
6. **Class Defaults**:

   | Property (search it) | Value | Why |
   |---|---|---|
   | **Is Back Handler** | on | Escape / gamepad B closes the settings. |
   | **Menu Tag** | `UI.Screen.Settings` | The id under which runtime code can add tabs to this menu later. |
   | **Tab Button Class** | `WBP_Button_Tab` | The button the menu creates for each tab. |
   | **Default Tab Set** | `DA_Settings_Tabs` | The list of tabs from B. |

7. Compile, Save.

*What happens at runtime: when the menu opens it reads the tab set, loads the tab page classes in the background, creates one `WBP_Button_Tab` per tab (label = Display Name), puts each page into `TabSwitcher`, and selects the first tab.*

## 4.5 A popup: `WBP_Popup_Confirm`

1. In `Popups` create a Widget Blueprint with parent **BaseActivatableWidget** (a popup is not a menu), name `WBP_Popup_Confirm`.
2. Layout: Overlay → **Border** (dark, semi-transparent, fills the screen) + centered panel with a text and two `WBP_Button_Primary` (**Yes**, **No**). Name **No** `DefaultFocusWidget`.
   *Why No: the safe choice has focus, so an accidental Enter does not confirm.*
3. Graph: **No → On Clicked → Close Screen**. **Yes →** your action, then **Close Screen**.
4. **Class Defaults**: **Is Modal** = on, **Is Back Handler** = on, **Input Mode** = UI Only.
   *Why Modal: while a modal is open, the screens below it do not react to input at all.*
5. Compile, Save.

## 4.6 Tell the framework which screens exist

Open `DA_UIConfig`, click **+** next to **Screens** once per row:

| Screen Tag | Widget Class | Layer Tag | Allow Multiple Instances | Preload And Keep Resident |
|---|---|---|---|---|
| `UI.Screen.HUD` | `WBP_HUD` | `CoreFramework.UI.Layer.Game` | off | on |
| `UI.Screen.MainMenu` | `WBP_MainMenu` | `CoreFramework.UI.Layer.Menu` | off | off |
| `UI.Screen.PauseMenu` | `WBP_PauseMenu` | `CoreFramework.UI.Layer.Menu` | off | on |
| `UI.Screen.Settings` | `WBP_Settings` | `CoreFramework.UI.Layer.Menu` | off | off |
| `UI.Screen.ConfirmPopup` | `WBP_Popup_Confirm` | `CoreFramework.UI.Layer.Modal` | off | off |

Column meanings:

* **Layer Tag** = on which layer stack the screen appears. A popup goes on **Modal** so it is drawn over menus.
* **Allow Multiple Instances off** = pressing the open button twice does not stack two copies.
* **Preload And Keep Resident** = loaded at start and kept in memory, so the *first* time the player opens it there is no hitch. Use it for screens that open often (HUD, pause menu), not for all.

Save `DA_UIConfig`.

## 4.7 Opening the pause menu from a key

The pause menu is opened by the **game** (the framework does not decide your keys).

**Blueprint way** (in your PlayerController or character, from your pause input action):

1. Inside a **widget** you can use **Get UI Frame Subsystem**. Inside a **PlayerController or character**, right-click → search **Get Local Player Subsystem**, set its **Class** to **UIFrameSubsystem** and connect the player controller (use **Get Player Controller** or *Self* in a PlayerController) to its input pin.
2. Drag from the subsystem output → **Push Screen** → **Screen Tag** = `UI.Screen.PauseMenu`.

**C++ way** (game module):

```cpp
#include "GameplayTagContainer.h"
#include "UISystem/Manager/UIFrameSubsystem.h"

void AMyPlayerController::HandlePauseAction()
{
	if (UUIFrameSubsystem* UI = UUIFrameSubsystem::GetFromController(this))
	{
		// Cache the tag in a member in real code; shown inline for clarity.
		UI->PushScreen(FGameplayTag::RequestGameplayTag(TEXT("UI.Screen.PauseMenu")));
	}
}
```

Closing is handled by the menu itself (Resume button, Escape, gamepad B). While a **UI Only** screen is open the game does not receive the pause key; that is intended.

## 4.8 Smoke test

1. Open the main menu map and press **Play**. The main menu appears, the cursor is visible, **Play** is focused (try the arrow keys).
2. Click **Settings**: the settings menu fades in over it, the first tab is selected. Press **Esc**: it closes.
3. Click **Play** → the gameplay map starts: HUD (empty) visible, cursor hidden, character controllable.
4. Trigger your pause action: the pause menu opens, the game stops, the cursor appears. **Esc** closes it and gameplay resumes.

If something fails, go to **Step 6.5 Troubleshooting** and check the **Output Log** (*Window → Output Log*, filter `LogCoreUI`): the framework prints clear messages about missing tags, classes or layers.

---

# Step 5 — Bind gameplay data (HP / Mana example)

**Goal:** a health bar that follows the local player's current pawn through spawn, death, respawn and possession changes. Everything below lives in the **game project**; nothing is added to the plugin.

*Why not "just read the character's health every frame in the widget"? Because the widget may exist before the character does (multiplayer join), and the character can change at any time (respawn). The framework tells your widget **when** a pawn appears and disappears, so the widget connects and disconnects at the right moment.*

## 5.1 The game's component and delegate

```cpp
// MyGame/HealthComponent.h
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HealthComponent.generated.h"

class UHealthComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnHealthChanged, UHealthComponent*, Component, float, NewHealth, float, MaxHealth);

UCLASS(ClassGroup = (Game), meta = (BlueprintSpawnableComponent))
class MYGAME_API UHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHealthComponent();

	UPROPERTY(BlueprintAssignable, Category = "Health")
	FOnHealthChanged OnHealthChanged;

	UFUNCTION(BlueprintPure, Category = "Health") float GetHealth() const { return Health; }
	UFUNCTION(BlueprintPure, Category = "Health") float GetMaxHealth() const { return MaxHealth; }

	/** Server only. */
	UFUNCTION(BlueprintCallable, Category = "Health")
	void SetHealth(float NewHealth);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UFUNCTION()
	void OnRep_Health();

	UPROPERTY(ReplicatedUsing = OnRep_Health)
	float Health = 100.f;

	UPROPERTY(EditDefaultsOnly, Replicated, Category = "Health")
	float MaxHealth = 100.f;
};
```

```cpp
// MyGame/HealthComponent.cpp
#include "HealthComponent.h"
#include "Net/UnrealNetwork.h"

UHealthComponent::UHealthComponent()
{
	SetIsReplicatedByDefault(true);
}

void UHealthComponent::SetHealth(float NewHealth)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	Health = FMath::Clamp(NewHealth, 0.f, MaxHealth);
	OnHealthChanged.Broadcast(this, Health, MaxHealth);   // server and listen-server host
}

void UHealthComponent::OnRep_Health()
{
	OnHealthChanged.Broadcast(this, Health, MaxHealth);   // clients
}

void UHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UHealthComponent, Health);
	DOREPLIFETIME(UHealthComponent, MaxHealth);
}
```

Add `UHealthComponent` to your character Blueprint (*Components → Add → Health Component*). A mana component follows the identical pattern. CoreFramework never learns that health exists.

## 5.2 The bound widget in C++

```cpp
// MyGame/HealthBarWidget.h
#pragma once

#include "CoreMinimal.h"
#include "UISystem/Foundation/BaseUserWidget.h"
#include "HealthBarWidget.generated.h"

class UHealthComponent;
class UProgressBar;

UCLASS(Abstract, Blueprintable)
class MYGAME_API UHealthBarWidget : public UBaseUserWidget
{
	GENERATED_BODY()

public:
	//~ IBaseUIBinderInterface
	virtual void OnUIPawnBound_Implementation(APawn* Pawn) override;
	virtual void OnUIPawnUnbound_Implementation(APawn* Pawn) override;

protected:
	/** Optional: a Progress Bar named exactly HealthBar. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> HealthBar;

	UFUNCTION(BlueprintImplementableEvent, Category = "Health", meta = (DisplayName = "On Health Changed"))
	void BP_OnHealthChanged(float Normalized, float Current, float Max);

private:
	UFUNCTION()
	void HandleHealthChanged(UHealthComponent* Component, float NewHealth, float MaxHealth);

	/** What we subscribed to. The Unbound pawn can already be destroyed, so never rely on the argument. */
	TWeakObjectPtr<UHealthComponent> BoundHealth;
};
```

```cpp
// MyGame/HealthBarWidget.cpp
#include "HealthBarWidget.h"
#include "Components/ProgressBar.h"
#include "GameFramework/Pawn.h"
#include "HealthComponent.h"

void UHealthBarWidget::OnUIPawnBound_Implementation(APawn* Pawn)
{
	UHealthComponent* Health = Pawn ? Pawn->FindComponentByClass<UHealthComponent>() : nullptr;
	if (!Health)
	{
		return;   // this pawn has no health: the widget stays idle
	}

	BoundHealth = Health;
	Health->OnHealthChanged.AddUniqueDynamic(this, &UHealthBarWidget::HandleHealthChanged);

	// Delegates only fire on change: push the current value once.
	HandleHealthChanged(Health, Health->GetHealth(), Health->GetMaxHealth());
}

void UHealthBarWidget::OnUIPawnUnbound_Implementation(APawn* Pawn)
{
	if (UHealthComponent* Health = BoundHealth.Get())
	{
		Health->OnHealthChanged.RemoveDynamic(this, &UHealthBarWidget::HandleHealthChanged);
	}
	BoundHealth.Reset();

	if (HealthBar)
	{
		HealthBar->SetPercent(0.f);
	}
}

void UHealthBarWidget::HandleHealthChanged(UHealthComponent* Component, float NewHealth, float MaxHealth)
{
	const float Normalized = MaxHealth > 0.f ? NewHealth / MaxHealth : 0.f;
	if (HealthBar)
	{
		HealthBar->SetPercent(Normalized);
	}
	BP_OnHealthChanged(Normalized, NewHealth, MaxHealth);
}
```

### Create `WBP_HealthBar`

1. In `HUD/Elements` create a Widget Blueprint with parent **HealthBarWidget** (your game class; search `HealthBar` in *All Classes*).
2. Layout: **Size Box** → **Progress Bar**. Rename the Progress Bar **`HealthBar`**, tick **Is Variable**. Details → **Percent** `1`, **Style → Fill Image → Tint** a color, **Background Image → Tint** a dark color.
3. Compile, Save.

## 5.3 The same widget in Blueprint only

1. Create `WBP_HealthBar` with parent **BaseUserWidget**. Add a Progress Bar `HealthBar`. Add a variable `BoundHealth` of type *Health Component (Object Reference)*.
2. **Graph**: right-click → search **On UI Pawn Bound** → add **Event On UI Pawn Bound**. (If it is not listed: *My Blueprint → Functions → Override → On UI Pawn Bound*.)
   * Drag *Pawn* → **Get Component by Class** (class = Health Component) → set `BoundHealth` → **Bind Event to On Health Changed** (custom event updating the bar) → call the update once with the current value.
3. Add **Event On UI Pawn Unbound**: if `BoundHealth` is valid → **Unbind Event from On Health Changed** → clear `BoundHealth` → set bar to 0.

*The event fires again on every respawn, so there is nothing else to write.*

## 5.4 Show it on the HUD

Open `DA_UIConfig` → **HUD Elements** → **+**:

| Element Id | Slot Tag | Widget Class |
|---|---|---|
| `Health` | `UI.HUDSlot.BottomLeft` | `WBP_HealthBar` |
| `Mana` | `UI.HUDSlot.BottomLeft` | `WBP_ManaBar` |

*What happens: the HUD finds the slot you registered in Step 4.1 (`BottomLeftBox`), loads `WBP_HealthBar` in the background and adds it there. The bar then binds itself to the pawn.*

**Optional runtime alternative** (when a feature activates later):

```cpp
FUIHUDElementDescriptor Element;
Element.ElementId = TEXT("Health");
Element.SlotTag = FGameplayTag::RequestGameplayTag(TEXT("UI.HUDSlot.BottomLeft"));
Element.WidgetClass = TSoftClassPtr<UBaseUserWidget>(FSoftObjectPath(TEXT("/Game/UI/HUD/Elements/WBP_HealthBar.WBP_HealthBar_C")));
UI->RegisterHUDElement(Element);       // a live HUD spawns it immediately
// UI->UnregisterHUDElement(TEXT("Health"));
```

## 5.5 Variants

* **Attributes on the PlayerState** (ability system, replicated stats): override `On UI Player State Bound / Unbound` instead of the pawn events; you get one bind per player state change.
* **Typed access in C++**: `GetBoundPawnAs<AMyCharacter>()` in any `UBaseUserWidget` / `UBaseActivatableWidget`.
* **Several widgets, one source**: each widget binds on its own; they do not know each other.

## 5.6 Verify

1. Play, damage the pawn (call `SetHealth` from a test key): the bar follows.
2. Respawn or possess another pawn: the bar resets and rebinds with no extra code.
3. Multiplayer (2 clients + listen server): each client's bar follows its own pawn; a late join shows the correct value immediately.

---

# Step 6 — Input and navigation, tests, troubleshooting

## 6.1 Per-screen input settings (what to set, and why)

Open each screen's **Class Defaults** and search the property name.

| Screen | Input Mode | Is Back Handler | Is Modal | Extra | Why |
|---|---|---|---|---|---|
| HUD | Game Only | off | off | — | The player keeps control; the cursor stays hidden. |
| Main menu | UI Only | off | off | `DefaultFocusWidget` on the first button | Cursor visible; nothing to go back to. |
| Pause menu | UI Only | **on** | off | **Pause Game While Active** on | Escape resumes; the game stops while paused. |
| Settings | UI Only | **on** | off | Tab list next / previous actions set | Escape goes back; shoulder buttons switch tabs. |
| Popup | UI Only | **on** | **on** | pushed on the Modal layer | Screens below stop reacting. |
| Quick inventory over live gameplay | Game And UI | on | off | **Ignore Look Input While Active** on | The mouse moves the UI cursor, not the camera. |
| Toast / notification | Keep Current | off | off | pushed on the Overlay layer | Must not steal input from the screen below. |

## 6.2 What happens automatically

| Event | Result |
|---|---|
| A UI Only screen opens | UI-only input, cursor visible, game input blocked |
| That screen closes | Input returns to the next screen below (HUD → Game Only, cursor hidden) |
| The player touches another device (mouse ↔ keyboard ↔ gamepad) | CommonUI switches cursor visibility and focus handling; `Is Using Gamepad` reflects it |
| Escape / gamepad B on a Back Handler screen | The topmost back handler closes |
| Shoulder buttons / `Q` `E` in Settings | Previous / next tab |

## 6.3 Optional: Enhanced Input for UI actions and button glyphs

* **Enhanced Input Support**: *Project Settings → Common Input Settings → Enable Enhanced Input Support*. Afterwards each row of `DT_UI_Actions` can reference an **Input Action** asset, so UI actions share the same rebinding source as gameplay. Skip this until you need rebinding of UI keys.
* **Button glyphs** (A / B / X / Y icons): in Common Input Settings → *Platform Input* add controller data assets per platform. Skip until you need icons.
* Gameplay input (`ABaseCharacter` / `UInputConfigData`) is **not** touched by any of this.

## 6.4 Test matrix

| # | Action | Expected |
|---|---|---|
| 1 | Open the pause menu with the keyboard | Cursor visible, Resume focused, camera does not turn |
| 2 | Move the mouse, click Settings | Settings opens on top, first tab selected |
| 3 | Press a gamepad button | Cursor hides, focus highlight appears on the focused button |
| 4 | D-pad down / up | Focus moves between buttons |
| 5 | Shoulder buttons in Settings | Tab changes |
| 6 | B / Escape | Settings closes, then pause menu closes, game input returns |
| 7 | Move the mouse again | Cursor reappears, hover works |
| 8 | Open a popup from a menu | Menu below ignores input until the popup closes |

## 6.5 Troubleshooting

Always start with *Window → Output Log*, filter `LogCoreUI`.

| Symptom | Cause | Fix |
|---|---|---|
| Nothing appears | No Default UI Config, no Root Layout Class, or the Game Mode has no HUD class | Step 3.6 and 3.7 |
| `no screen registered for tag` | Row missing in `DA_UIConfig → Screens`, or tag typo | Step 4.6 |
| `layer … is not registered` | Stack name typo in the root layout, or the Layer Tag is empty / wrong | Step 2.3 (exact names); Layer Tag in 4.6 |
| Back / Escape does nothing | **Is Back Handler** off, or no Default Back Action | Step 3.3 (C), Class Defaults |
| Gamepad cannot move focus | Nothing focusable, or no focus target | Use `WBP_Button_*` (focusable), name one widget `DefaultFocusWidget` |
| Mouse blocked by the HUD | A stack is **Visible** | Step 2.3, point 8 |
| Tab bar empty | `Tab Button Class` unset, `Default Tab Set` empty, or a tab page is not a `BaseTabWidget` | Step 4.4 (C), check `LogCoreUI` |
| Button shows no text | The text widget is not named `LabelText`, or **Button Label** is empty | Step 3.5 |
| HUD element missing | Its slot was never registered | Step 4.1 steps 6–7, or use `DefaultElementSlot` |
| My own tags are missing in the tag pickers | You are on the first version that filtered tags | Update the plugin (this version removed the filters) |
| Widget does not rebind after respawn | Missing unsubscription in `Unbound`, or **Auto Register As Binder** off | Section 5.2 pattern |
| Input stays blocked after closing a screen | A screen was destroyed without deactivating | Use **Close Screen**; the framework also releases suppression when a screen is destroyed |
| `WBP_BaseRootLayout` is not in the picker | **Show Plugin Content** is off | Step 2.2 |

---

# Final checklist

**Plugin (once)**
- [ ] `UISystem` subfolders in place, plugin builds (Step 1)
- [ ] `WBP_BaseRootLayout` created with 4 correctly named stacks, compiled, committed (Step 2)

**Game project**
- [ ] Plugin pulled, enabled, game builds (3.1)
- [ ] Content folder structure created (3.2)
- [ ] Game Viewport Client, `DT_UI_Actions`, `BP_CommonInputData`, Common Input Settings done (3.3)
- [ ] Gameplay tags created (3.4)
- [ ] Text style, button styles, `WBP_Button_Primary`, `WBP_Button_Tab` created (3.5)
- [ ] `DA_UIConfig` assigned in Project Settings (3.6)
- [ ] `BP_HUD_Gameplay`, `BP_HUD_MainMenu`, Game Modes, maps set (3.7)
- [ ] `WBP_HUD`, `WBP_MainMenu`, `WBP_PauseMenu`, `WBP_Settings` + tabs, `WBP_Popup_Confirm` built (4.1–4.5)
- [ ] All screens listed in `DA_UIConfig` (4.6)
- [ ] Smoke test passes (4.8)
- [ ] First gameplay widget bound through `On UI Pawn Bound` (Step 5)
- [ ] Test matrix passes with mouse, keyboard and gamepad (6.4)
