// CoreFramework Save System - UI
#pragma once

#include "CoreMinimal.h"
#include "UISystem/Menu/Tabs/BaseTabWidget.h"
#include "SaveProfileTabWidget.generated.h"

class USaveSubsystem;

/**
 * Base class for a settings page whose values are kept in the player's profile (difficulty, camera sensitivity,
 * subtitles, key preferences, accessibility ...). It plugs into the Apply / Revert buttons of UBaseMenuWidget.
 *
 * Use
 *   1. Create a Blueprint child (the page of a settings menu) and register it as a tab, as for any tab.
 *   2. When a control changes (a slider, a checkbox) call Stage Float / Bool / Int / String with a key of your choice.
 *      Nothing is saved yet: the value waits, and Has Pending Changes becomes true.
 *   3. Initialize each control with Get Float / Bool / Int / String: they return the staged value, else the saved
 *      one, else your default.
 *   4. The menu's Apply button commits the staged values to the profile and saves the profile; Revert drops them
 *      and fires On Values Reverted, where you set the controls back with the Get nodes.
 *   5. Anywhere else in the game read the values with USaveFunctionLibrary Get Profile Float / Bool / Int.
 *
 * Which profile: the one of the local player that owns the widget (split-screen safe). Set Local Player Index
 * Override to pin it.
 */
UCLASS(Abstract, Blueprintable)
class COREFRAMEWORK_API USaveProfileTabWidget : public UBaseTabWidget
{
	GENERATED_BODY()

public:
	USaveProfileTabWidget(const FObjectInitializer& ObjectInitializer);

	// ---- Stage a change ---------------------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Save|Profile")
	void StageFloat(FName Key, float Value);

	UFUNCTION(BlueprintCallable, Category = "Save|Profile")
	void StageInt(FName Key, int32 Value);

	UFUNCTION(BlueprintCallable, Category = "Save|Profile")
	void StageBool(FName Key, bool bValue);

	UFUNCTION(BlueprintCallable, Category = "Save|Profile")
	void StageString(FName Key, const FString& Value);

	// ---- Read (staged value, else saved value, else default) ----------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Save|Profile")
	float GetFloat(FName Key, float Default) const;

	UFUNCTION(BlueprintPure, Category = "Save|Profile")
	int32 GetInt(FName Key, int32 Default) const;

	UFUNCTION(BlueprintPure, Category = "Save|Profile")
	bool GetBool(FName Key, bool bDefault) const;

	UFUNCTION(BlueprintPure, Category = "Save|Profile")
	FString GetString(FName Key, const FString& Default) const;

	/** True if a value waits for Apply. */
	UFUNCTION(BlueprintPure, Category = "Save|Profile")
	bool HasStagedValue(FName Key) const { return Staged.Contains(Key); }

	/** Index of the local player whose profile this page edits. */
	UFUNCTION(BlueprintPure, Category = "Save|Profile")
	int32 GetProfileIndex() const;

	// ---- UBaseTabWidget contract ----------------------------------------------------------------------------

	virtual bool HasPendingChanges_Implementation() const override;
	virtual void ApplyChanges_Implementation() override;
	virtual void RevertChanges_Implementation() override;

protected:
	/** After the staged values were written to the profile (and the save was started). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Save|Profile", meta = (DisplayName = "On Values Applied"))
	void BP_OnValuesApplied();

	/** After the staged values were dropped. Set the controls back with the Get nodes. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Save|Profile", meta = (DisplayName = "On Values Reverted"))
	void BP_OnValuesReverted();

	/** Pins the profile to one local player. -1 (default) = the local player that owns this widget. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Save|Profile", meta = (ClampMin = "-1"))
	int32 LocalPlayerIndexOverride = -1;

	/** Write the profile file when Apply is pressed. Turn off to save later yourself (for example when the menu closes). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Save|Profile")
	bool bSaveProfileOnApply = true;

private:
	/** Staged values as text, like the profile store keeps them. */
	TMap<FName, FString> Staged;
};
