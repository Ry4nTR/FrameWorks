# CoreFramework Submodule & Subtree Guide

This guide explains how to manage the `CoreFramework` plugin, export it as a clean submodule branch, and integrate/update it across Unreal Engine game projects.

---

## 1. Updating the Plugin inside the `FrameWorks` Repository

Whenever you edit, add, or fix C++ / Blueprints inside `Plugins/CoreFramework` within the main `FrameWorks` project, follow these steps:

### Step 1: Commit and push changes to `main`
Ensure all your current work in the main repository is tracked and saved.
```bash
git add .
git commit -m "Describe your framework changes here"
git push origin main
```

### Step 2: Split the Plugin into its own Branch (Subtree)
Isolate the `Plugins/CoreFramework` directory history into a clean, separate branch (e.g., `coreframework-branch`).
```bash
git subtree split --prefix=Plugins/CoreFramework -b coreframework-branch
```

### Step 3: Push the Isolated Branch
Push this new branch to your remote repository so it can be accessed by your game projects.
```bash
git push origin coreframework-branch
```

---

## 2. Integrating the Plugin into a Game Project

When you are setting up a new Unreal Engine game project and need to include the `CoreFramework`, add it as a Git submodule.

### Step 1: Add the Submodule
Navigate to the root directory of your target game project and add the isolated branch into the `Plugins` folder.
```bash
mkdir -p Plugins
git submodule add -b coreframework-branch <your-repo-url.git> Plugins/CoreFramework
```

### Step 2: Initialize and Commit
Initialize the submodule and commit it to your game project's repository.
```bash
git submodule update --init --recursive
git add .gitmodules Plugins/CoreFramework
git commit -m "Added CoreFramework plugin as a submodule"
```

---

## 3. Pulling Updates into Your Game Project

When new features or fixes are pushed to the `CoreFramework` from the main repository, you will need to pull those updates into your specific game project.

### Step 1: Update the Submodule
Navigate to the root of your game project and run the update command to pull the latest commits from the remote branch.
```bash
git submodule update --remote Plugins/CoreFramework
```

### Step 2: Commit the New Pointer
Git tracks the specific commit hash of the submodule. You must commit this updated pointer to your game project's repository.
```bash
git add Plugins/CoreFramework
git commit -m "Updated CoreFramework submodule to the latest version"
```