# CoreFramework Submodule & Subtree Guide

This guide explains how to manage the `CoreFramework` plugin, export it as a clean submodule branch, and integrate/update it across Unreal Engine game projects using **VS Code Terminal (PowerShell)**.

## 1. Updating the Plugin inside the `FrameWorks` Repository

Whenever you edit, add, or fix C++ / Blueprints inside `Plugins/CoreFramework` within the main `FrameWorks` project, follow these steps:

### Step 1: Commit and push changes to `main`

Ensure all your current work in the main repository is tracked and saved.

```powershell
git add .
git commit -m "Describe your framework changes here"
git push origin main
```

### Step 2: Split the Plugin into its own Branch (Subtree)

Isolate the `Plugins/CoreFramework` directory history into a clean, separate branch named `coreframework-branch`.

```powershell
git subtree split --prefix=Plugins/CoreFramework -b coreframework-branch
```

### Step 3: Push the Isolated Branch

Push this new branch to your remote repository so it can be accessed by your game projects. **This is a crucial step before attempting to add the submodule elsewhere.**

```powershell
git push origin coreframework-branch
```

## 2. Integrating the Plugin into a Game Project

When setting up a new Unreal Engine game project and needing to include `CoreFramework`, run these commands in the VS Code PowerShell terminal:

### Step 1: Create the Plugins Folder & Add Submodule

Ensure the `Plugins` directory exists safely without throwing errors, then attach the `coreframework-branch`.

```powershell
if (!(Test-Path "Plugins")) { New-Item -ItemType Directory -Path "Plugins" }
git submodule add -b coreframework-branch https://github.com/Ry4nTR/FrameWorks.git Plugins/CoreFramework
```

### Step 2: Initialize and Commit

Initialize the submodule and commit it to your game project's repository.

```powershell
git submodule update --init --recursive
git commit -m "Added CoreFramework plugin submodule"
```

## 3. Pulling Updates into Your Game Project

When new features or fixes are pushed to the `coreframework-branch` from the main repository, pull those updates into your specific game project.

### Step 1: Update the Submodule

Navigate to the root of your game project and run the update command to pull the latest commits from the remote branch.

```powershell
git submodule update --remote Plugins/CoreFramework
```

### Step 2: Commit the New Pointer

Git tracks the specific commit hash of the submodule. You must commit this updated pointer to your game project's repository.

```powershell
git add Plugins/CoreFramework
git commit -m "Updated CoreFramework submodule to the latest version"
```

## 4. Troubleshooting & Error Solutions (VS Code PowerShell)

### Issue A: Submodule folder is empty / Files show as "Deleted (D)" in VS Code

**Symptom:** After successfully adding the submodule, the `Plugins/CoreFramework` folder has no files in it, and VS Code's Source Control panel shows hundreds of files marked with a red **D** (Deleted).
**Cause:** The submodule repository was registered in Git's index, but the working directory failed to populate (often due to cache from a previous failed submodule attempt). Git thinks you manually deleted the files.

**Solution:**
Force Git to restore the physical files from the current commit by navigating into the submodule and running a hard reset:

```powershell
# 1. Navigate into the submodule directory
cd Plugins/CoreFramework

# 2. Force Git to restore all missing files from the index
git reset --hard HEAD

# 3. Navigate back to the root of your game project
cd ../..
```

### Issue B: `fatal: A git directory for 'Plugins/CoreFramework' is found locally...`

**Symptom:** Git complains that a local Git directory already exists in `.git/modules/`.
**Cause:** A previous `git submodule add` command failed halfway through, leaving Git's internal module cache behind inside the hidden `.git` folder.

**Solution 1 (Use Force):**
The fastest way to bypass this error is to pass the `--force` flag:

```powershell
git submodule add --force -b coreframework-branch https://github.com/Ry4nTR/FrameWorks.git Plugins/CoreFramework
```

**Solution 2 (Full Cleanup in PowerShell):**
If you want to completely scrub the cached submodule state and try fresh, run these PowerShell commands:

```powershell
# 1. Delete the untracked Plugins folder
Remove-Item -Recurse -Force Plugins/CoreFramework -ErrorAction SilentlyContinue

# 2. Delete Git's cached module metadata
Remove-Item -Recurse -Force .git/modules/Plugins/CoreFramework -ErrorAction SilentlyContinue

# 3. Clear cached references
git rm --cached -f Plugins/CoreFramework 2>$null
```

### Issue C: `mkdir : An item with the specified name ... already exists.`

**Symptom:** Running `mkdir -p Plugins` throws a red error in PowerShell.
**Cause:** In PowerShell, `mkdir` is an alias for `New-Item`. Unlike Linux/Bash, PowerShell does not support `-p` and throws an error if the directory already exists.

**Solution:**
Use the PowerShell `Test-Path` check before creating the directory (as outlined in Step 2.1):

```powershell
if (!(Test-Path "Plugins")) { New-Item -ItemType Directory -Path "Plugins" }
```

### Issue D: `fatal: 'origin/coreframework-branch' is not a commit...`

**Symptom:** Git fails to fetch the branch during `git submodule add`.
**Cause:** The `coreframework-branch` branch has not been split and pushed from the main repository yet, or it was deleted remotely.

**Solution:**
Return to your **FrameWorks** project and run:

```powershell
git subtree split --prefix=Plugins/CoreFramework -b coreframework-branch
git push origin coreframework-branch
```