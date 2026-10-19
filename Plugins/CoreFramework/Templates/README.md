# CoreFramework — Game Module Starter Kit

**Where this folder lives:** `Plugins/CoreFramework/Templates/`, next to `Docs/` and `Source/`. It is **not** inside `Source/`, so the plugin never compiles it.
**Companion:** `Docs/Integration/Project_Integration_Guide.md` (read its sections 1 and 2 first; section 3 uses this kit).
**Engine target:** Unreal Engine 5.8.

> **Status.** None of these files was compiled with Unreal Engine, and `Rename-Template.ps1` was not run. The C++ only passed a syntax-only check against hand-written stub headers. The files were written against the headers of the repository at commit `849ee25`. Expect to fix small things on the first build, and read `Rename-Template.ps1` once before you run it.

## What is inside

```
Templates/
├─ README.md                      this file
├─ MyGame.Build.cs                complete Build.cs for a NEW game module
├─ Rename-Template.ps1            copies Public/ and Private/ into your module and renames MyGame
├─ Public/                        headers   (includes are relative to this folder)
│  ├─ Core/
│  │  ├─ MyGameLog.h
│  │  ├─ MyGameMode.h
│  │  └─ MyGameInstance.h
│  └─ Player/
│     └─ MyPlayerController.h
└─ Private/                       sources   (same sub-folders as Public/)
   ├─ Core/
   │  ├─ MyGameLog.cpp
   │  ├─ MyGameMode.cpp
   │  └─ MyGameInstance.cpp
   └─ Player/
      └─ MyPlayerController.cpp
```

| File | What it is | Framework it connects to |
|---|---|---|
| `AMyGameMode` | Skeleton of the rules of one map type. Derives from `AGameModeBase`. | UI (HUD Class), Character (Default Pawn Class): set in a Blueprint child, explained in the header |
| `UMyGameInstance` | Skeleton of the session-long object. Derives from `UGameInstance`. | None directly. Explains why frameworks only ever add `UGameInstanceSubsystem` |
| `AMyPlayerController` | **Optional.** Pause key → `UUIFrameSubsystem`. | UI (`PushScreen` / `PopScreen`), Enhanced Input |
| `LogMyGame` | Log category used by the three classes | — |

Every place that you are expected to edit is marked `// PROJECT-SPECIFIC`.

## The rename: `MyGame` → your project name

The script applies these replacements to **file names and file contents**, in this order. Example with the project name `Atlas`:

| Template text | Becomes | Where it appears |
|---|---|---|
| `MyGameMode` | `AtlasGameMode` | class `AMyGameMode`, files `MyGameMode.h/.cpp` |
| `MyGameInstance` | `AtlasGameInstance` | class `UMyGameInstance`, files `MyGameInstance.h/.cpp` |
| `MyPlayerController` | `AtlasPlayerController` | class `AMyPlayerController`, files `MyPlayerController.h/.cpp` |
| `MYGAME` | `ATLAS` | export macro `MYGAME_API` → `ATLAS_API` |
| `MyGame` | `Atlas` | `LogMyGame`, file `MyGameLog.h`, Category names such as `MyGame|UI`, comments |

Rules for the project name: letters and digits only, starts with a letter, and it must be the **name of your game module** (the folder `Source/<Name>/` and the `<Name>.Build.cs`). The export macro of a module is always its upper-case name plus `_API`, so the name has to match.

## Install, in order

**1. Make sure the game module exists.** A C++ project has one (`Source/<Name>/<Name>.Build.cs`). A project created as Blueprint-only has none: in the editor use *Tools → New C++ Class*, pick any parent, finish the wizard, close the editor, then continue.

**2. Copy and rename.** Open PowerShell in `Plugins/CoreFramework/Templates` and run:

```powershell
.\Rename-Template.ps1 -ProjectName Atlas -ModuleDirectory ..\..\..\Source\Atlas
```

`..\..\..` is the project root (`Templates` → `CoreFramework` → `Plugins` → project root). The script creates `Source/Atlas/Public/Core/...` and `Source/Atlas/Private/...`, and stops with a message if a target file already exists.

**3. Build.cs.** Open `Source/Atlas/Atlas.Build.cs`. It must list these modules in `PublicDependencyModuleNames`: `Core`, `CoreUObject`, `Engine`, `InputCore`, `EnhancedInput`, `GameplayTags`, `CoreFramework`. The file `MyGame.Build.cs` in this folder is a complete example for a brand-new module (rename the class and file to your name). For an existing module, add the missing names to your own file.

**4. Enable the plugin in the `.uproject`.** The build cannot find the `CoreFramework` module otherwise:

```json
"Plugins": [
	{ "Name": "CoreFramework", "Enabled": true }
]
```

**5. Regenerate and build.** Right-click the `.uproject` → *Generate Visual Studio project files* (or the equivalent of your IDE), then build the `Development Editor` target. Open the editor.

**6. Check the result.** In the Content Browser: *Add → Blueprint Class → All Classes*, search `GameMode`. You should find `AtlasGameMode`. The same search with `PlayerController` finds `AtlasPlayerController`. If a class is missing, the build did not finish or the editor was opened before the build (close it and build from the IDE).

Continue with `Project_Integration_Guide.md`, section 3, step 8 onward.

## If your game module has no Public/ and Private/ folders

The kit commits to the `Public/` + `Private/` layout. A module created from an Epic template keeps its files directly in `Source/<Name>/`. In that case keep your old files where they are and create the two folders next to them: Unreal accepts both layouts in one module, and the kit's includes (`#include "Core/AtlasGameMode.h"`) work because `Public/` is on the include path automatically.

## What the kit does not contain

- No `BaseGameMode`, no `BaseGameInstance`, no game-specific rules: those belong to the game (guide, section 0.2).
- No Blueprint assets. Blueprint children (`BP_GameMode_*`, `BP_PlayerController`, `BP_Startup_*`) and the Input Action assets are created in the editor, step by step, in guide section 3.
- No compiled output and no tests.
