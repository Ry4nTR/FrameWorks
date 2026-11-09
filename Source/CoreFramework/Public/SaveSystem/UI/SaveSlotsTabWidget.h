// CoreFramework Save System - UI
#pragma once

#include "CoreMinimal.h"
#include "SaveSystem/Core/SaveTypes.h"
#include "UISystem/Menu/Tabs/BaseTabWidget.h"
#include "SaveSlotsTabWidget.generated.h"

class UCommonButtonBase;
class UPanelWidget;
class USaveSlotEntryWidget;
class USaveSubsystem;
class UTextBlock;
class UWidget;

/** What the slot list offers. */
UENUM(BlueprintType)
enum class ESaveSlotsMode : uint8
{
	/** Main menu "Continue / Load Game": pick a slot and load it. */
	Load,
	/** Pause menu "Save Game": pick a slot (or a new one) and save into it. Autosaves are hidden. */
	Save,
	/** One page for both. */
	SaveAndLoad
};

/** The action waiting for a confirmation. */
UENUM(BlueprintType)
enum class ESaveSlotsAction : uint8
{
	/** Save into an existing slot (overwrite). */
	Overwrite,
	Load,
	Delete
};

/**
 * The classic "choose which save to load, overwrite or delete" page, as a tab of UBaseMenuWidget.
 *
 * Use
 *   1. Create a Blueprint child (the plugin ships an unstyled WBP_SaveSlotsTab; duplicate it for a game).
 *   2. Add the widgets below, named exactly. SlotListPanel is required, the rest is optional.
 *   3. Set Entry Class (a child of USaveSlotEntryWidget) and Mode.
 *   4. Register the Blueprint as a tab of the main menu or pause menu (UITabSetDataAsset, or RegisterMenuTab).
 *
 *   SlotListPanel   Panel Widget (Vertical Box, Scroll Box ...)  rows are created inside it. REQUIRED.
 *   SaveButton      Common Button   saves into the selected slot
 *   NewSlotButton   Common Button   saves into a new slot
 *   LoadButton      Common Button   loads the selected slot
 *   DeleteButton    Common Button   deletes the selected slot
 *   StatusText      Text Block      last error, or empty
 *   BusyIndicator   any widget      visible while the save system works
 *   EmptyListWidget any widget      visible when there are no slots
 *
 * Buttons that the mode does not allow are collapsed, and the others are enabled only when they make sense.
 *
 * Game specific behaviour (choose the world to leave, show a "Are you sure?" popup, travel to the saved map, show a
 * thumbnail) is added by a child class: override the Blueprint events below, or the C++ virtuals. Only what the game
 * needs; the base already does the rest.
 */
UCLASS(Abstract, Blueprintable)
class COREFRAMEWORK_API USaveSlotsTabWidget : public UBaseTabWidget
{
	GENERATED_BODY()

public:
	USaveSlotsTabWidget(const FObjectInitializer& ObjectInitializer);

	virtual void NotifyTabSelected() override;

	/** Asks the save system for the slot list and rebuilds the rows. Done automatically when the tab is selected. */
	UFUNCTION(BlueprintCallable, Category = "Save|UI")
	void RefreshSlots();

	UFUNCTION(BlueprintCallable, Category = "Save|UI")
	void SelectSlot(const FString& SlotName);

	UFUNCTION(BlueprintPure, Category = "Save|UI")
	bool HasSelection() const { return !SelectedSlotName.IsEmpty(); }

	UFUNCTION(BlueprintPure, Category = "Save|UI")
	FSaveSlotInfo GetSelectedSlot() const;

	// ---- Actions (wired to the buttons; call them from custom buttons too) -------------------------------

	UFUNCTION(BlueprintCallable, Category = "Save|UI")
	void RequestSaveSelected();

	UFUNCTION(BlueprintCallable, Category = "Save|UI")
	void RequestSaveToNewSlot();

	UFUNCTION(BlueprintCallable, Category = "Save|UI")
	void RequestLoadSelected();

	UFUNCTION(BlueprintCallable, Category = "Save|UI")
	void RequestDeleteSelected();

	/** Runs the action that waits for a confirmation. Call it from your popup's Yes button. */
	UFUNCTION(BlueprintCallable, Category = "Save|UI")
	void ConfirmPendingAction();

	/** Drops the action that waits for a confirmation. Call it from your popup's No button. */
	UFUNCTION(BlueprintCallable, Category = "Save|UI")
	void CancelPendingAction();

	UFUNCTION(BlueprintPure, Category = "Save|UI")
	bool HasPendingAction() const { return bHasPendingAction; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// ---- Hooks for the game (Blueprint events, C++ virtuals) -----------------------------------------------

	/**
	 * An action needs a confirmation (see the Confirm flags). The default runs it at once. A game overrides this,
	 * shows its own popup, and calls Confirm Pending Action or Cancel Pending Action from the popup's buttons.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Save|UI")
	void RequestConfirmation(ESaveSlotsAction Action, const FSaveSlotInfo& Info);

	/**
	 * A slot finished loading. The default opens the saved map when Open Saved Map On Load is ticked, then fires the
	 * Blueprint event. Override to choose what happens (travel with options, show a loading screen, close the menu).
	 */
	virtual void HandleSlotLoaded(const FSaveSlotInfo& Info);
	virtual void HandleSlotSaved(const FSaveSlotInfo& Info);
	virtual void HandleSlotDeleted(const FSaveSlotInfo& Info);
	virtual void HandleError(ESaveOperation Operation, ESaveResult Result);

	UFUNCTION(BlueprintImplementableEvent, Category = "Save|UI", meta = (DisplayName = "On Slot Loaded"))
	void BP_OnSlotLoaded(const FSaveSlotInfo& Info);

	UFUNCTION(BlueprintImplementableEvent, Category = "Save|UI", meta = (DisplayName = "On Slot Saved"))
	void BP_OnSlotSaved(const FSaveSlotInfo& Info);

	UFUNCTION(BlueprintImplementableEvent, Category = "Save|UI", meta = (DisplayName = "On Slot Deleted"))
	void BP_OnSlotDeleted(const FSaveSlotInfo& Info);

	UFUNCTION(BlueprintImplementableEvent, Category = "Save|UI", meta = (DisplayName = "On Error"))
	void BP_OnError(ESaveOperation Operation, ESaveResult Result, const FText& Message);

	/** After the rows were rebuilt. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Save|UI", meta = (DisplayName = "On Slots Refreshed"))
	void BP_OnSlotsRefreshed(int32 SlotCount);

	/** The selected row changed (SlotName is empty when nothing is selected). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Save|UI", meta = (DisplayName = "On Selection Changed"))
	void BP_OnSelectionChanged(const FString& SlotName);

	// ---- Configuration ---------------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Save|UI")
	ESaveSlotsMode Mode = ESaveSlotsMode::SaveAndLoad;

	/** Row Blueprint (child of USaveSlotEntryWidget). Required. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Save|UI")
	TSubclassOf<USaveSlotEntryWidget> EntryClass;

	/** Show the autosave slot in Load mode. (Save mode never shows it.) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Save|UI")
	bool bShowAutosave = true;

	/** Ask before overwriting a slot. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Save|UI")
	bool bConfirmOverwrite = true;

	/** Ask before loading (useful in game, where unsaved progress would be lost). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Save|UI")
	bool bConfirmLoad = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Save|UI")
	bool bConfirmDelete = true;

	/** After a load, open the level the slot was saved in. For single player and listen-server hosts; multiplayer games override HandleSlotLoaded. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Save|UI")
	bool bOpenSavedMapOnLoad = false;

	/** Name prefix of slots created by the New Slot button ("Slot" gives Slot1, Slot2 ...). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Save|UI")
	FString NewSlotPrefix = TEXT("Slot");

	// ---- Widgets -----------------------------------------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidget))
	TObjectPtr<UPanelWidget> SlotListPanel;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UCommonButtonBase> SaveButton;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UCommonButtonBase> NewSlotButton;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UCommonButtonBase> LoadButton;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UCommonButtonBase> DeleteButton;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> BusyIndicator;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> EmptyListWidget;

private:
	UFUNCTION()
	void HandleSlotListChanged();

	UFUNCTION()
	void HandleBusyChanged(bool bBusy);

	UFUNCTION()
	void HandleEntrySelected(USaveSlotEntryWidget* Entry);

	void HandleSaveClicked() { RequestSaveSelected(); }
	void HandleNewSlotClicked() { RequestSaveToNewSlot(); }
	void HandleLoadClicked() { RequestLoadSelected(); }
	void HandleDeleteClicked() { RequestDeleteSelected(); }

	void RebuildRows();
	void UpdateControls();
	void SetStatus(const FText& Message);
	void BeginAction(ESaveSlotsAction Action, const FString& SlotName, bool bNeedsConfirmation);
	void ExecuteAction(ESaveSlotsAction Action, const FString& SlotName);
	void ExecuteSave(const FString& SlotName, const FString& DisplayName);
	void OnOperationDone(ESaveOperation Operation, ESaveResult Result, const FSaveSlotInfo& Info);
	bool ModeAllowsSave() const { return Mode != ESaveSlotsMode::Load; }
	bool ModeAllowsLoad() const { return Mode != ESaveSlotsMode::Save; }
	bool IsAutosaveSlot(const FString& SlotName) const;
	USaveSubsystem* GetSaveSubsystem() const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USaveSlotEntryWidget>> Rows;

	FString SelectedSlotName;

	ESaveSlotsAction PendingAction = ESaveSlotsAction::Load;
	FString PendingSlotName;
	bool bHasPendingAction = false;
	bool bBound = false;
};
