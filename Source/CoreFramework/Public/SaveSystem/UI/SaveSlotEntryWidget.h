// CoreFramework Save System - UI
#pragma once

#include "CoreMinimal.h"
#include "SaveSystem/Core/SaveTypes.h"
#include "UISystem/Foundation/BaseUserWidget.h"
#include "SaveSlotEntryWidget.generated.h"

class UCommonButtonBase;
class UTextBlock;
class USaveSlotEntryWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSaveSlotEntrySelected, USaveSlotEntryWidget*, Entry);

/**
 * One row of a slot list (see USaveSlotsTabWidget). It shows the metadata of one slot and tells its list when the
 * player picks it.
 *
 * Build a Blueprint child and add any of these widgets, named exactly as below (all optional, add only what the
 * design needs):
 *   SlotNameText     Text Block   technical name of the slot
 *   DisplayNameText  Text Block   name shown to the player
 *   SavedAtText      Text Block   date and time of the save
 *   PlaytimeText     Text Block   time played, h:mm:ss
 *   MapText          Text Block   short name of the level
 *   SelectButton     Common Button  picking the row (any Base Button works)
 * Everything else (thumbnail, chapter label, difficulty icon) is game specific: use the Blueprint event
 * On Slot Info Set and read Info.CustomData (see USaveHooks::BuildSlotInfo).
 */
UCLASS(Abstract, Blueprintable)
class COREFRAMEWORK_API USaveSlotEntryWidget : public UBaseUserWidget
{
	GENERATED_BODY()

public:
	USaveSlotEntryWidget(const FObjectInitializer& ObjectInitializer);

	/** Shows a slot. Called by the list. */
	UFUNCTION(BlueprintCallable, Category = "Save|UI")
	void SetSlotInfo(const FSaveSlotInfo& InInfo);

	UFUNCTION(BlueprintPure, Category = "Save|UI")
	FSaveSlotInfo GetSlotInfo() const { return Info; }

	UFUNCTION(BlueprintPure, Category = "Save|UI")
	bool IsSelected() const { return bSelected; }

	/** Marks the row as the chosen one. Called by the list; fires On Selection Changed for the visual. */
	UFUNCTION(BlueprintCallable, Category = "Save|UI")
	void SetSelected(bool bInSelected);

	/** Tells the list that this row was picked. Wire any custom button of the Blueprint to it. */
	UFUNCTION(BlueprintCallable, Category = "Save|UI")
	void RequestSelect();

	/** Fires when the player picks this row. */
	UPROPERTY(BlueprintAssignable, Category = "Save|UI")
	FOnSaveSlotEntrySelected OnSelected;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** After SetSlotInfo filled the named widgets. Add game specific content here. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Save|UI", meta = (DisplayName = "On Slot Info Set"))
	void BP_OnSlotInfoSet(const FSaveSlotInfo& InInfo);

	/** Style the row as selected or not selected. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Save|UI", meta = (DisplayName = "On Selection Changed"))
	void BP_OnSelectionChanged(bool bInSelected);

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SlotNameText;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DisplayNameText;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SavedAtText;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PlaytimeText;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MapText;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UCommonButtonBase> SelectButton;

private:
	void HandleSelectClicked();

	FSaveSlotInfo Info;
	bool bSelected = false;
};
