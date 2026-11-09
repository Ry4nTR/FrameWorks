// CoreFramework Save System - UI
#include "SaveSystem/UI/SaveSlotsTabWidget.h"

#include "Blueprint/UserWidget.h"
#include "CommonButtonBase.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"
#include "SaveSystem/Library/SaveFunctionLibrary.h"
#include "SaveSystem/Manager/SaveSubsystem.h"
#include "SaveSystem/UI/SaveSlotEntryWidget.h"

USaveSlotsTabWidget::USaveSlotsTabWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

USaveSubsystem* USaveSlotsTabWidget::GetSaveSubsystem() const
{
	return USaveSubsystem::Get(this);
}

// ---------------------------------------------------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------------------------------------------------

void USaveSlotsTabWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (SaveButton)
	{
		SaveButton->OnClicked().AddUObject(this, &USaveSlotsTabWidget::HandleSaveClicked);
	}
	if (NewSlotButton)
	{
		NewSlotButton->OnClicked().AddUObject(this, &USaveSlotsTabWidget::HandleNewSlotClicked);
	}
	if (LoadButton)
	{
		LoadButton->OnClicked().AddUObject(this, &USaveSlotsTabWidget::HandleLoadClicked);
	}
	if (DeleteButton)
	{
		DeleteButton->OnClicked().AddUObject(this, &USaveSlotsTabWidget::HandleDeleteClicked);
	}

	if (USaveSubsystem* Subsystem = GetSaveSubsystem())
	{
		Subsystem->OnSlotListChanged.AddDynamic(this, &USaveSlotsTabWidget::HandleSlotListChanged);
		Subsystem->OnBusyChanged.AddDynamic(this, &USaveSlotsTabWidget::HandleBusyChanged);
		bBound = true;
	}

	SetStatus(FText::GetEmpty());
	UpdateControls();
	RefreshSlots();
}

void USaveSlotsTabWidget::NativeDestruct()
{
	if (SaveButton)
	{
		SaveButton->OnClicked().RemoveAll(this);
	}
	if (NewSlotButton)
	{
		NewSlotButton->OnClicked().RemoveAll(this);
	}
	if (LoadButton)
	{
		LoadButton->OnClicked().RemoveAll(this);
	}
	if (DeleteButton)
	{
		DeleteButton->OnClicked().RemoveAll(this);
	}

	if (bBound)
	{
		if (USaveSubsystem* Subsystem = GetSaveSubsystem())
		{
			Subsystem->OnSlotListChanged.RemoveDynamic(this, &USaveSlotsTabWidget::HandleSlotListChanged);
			Subsystem->OnBusyChanged.RemoveDynamic(this, &USaveSlotsTabWidget::HandleBusyChanged);
		}
		bBound = false;
	}

	Super::NativeDestruct();
}

void USaveSlotsTabWidget::NotifyTabSelected()
{
	Super::NotifyTabSelected();
	RefreshSlots();
}

// ---------------------------------------------------------------------------------------------------------------------
// List
// ---------------------------------------------------------------------------------------------------------------------

void USaveSlotsTabWidget::RefreshSlots()
{
	USaveSubsystem* Subsystem = GetSaveSubsystem();
	if (!Subsystem)
	{
		return; // Designer preview: no game instance.
	}

	// The rows are rebuilt by HandleSlotListChanged when the list arrives. A failure is shown in the status line.
	TWeakObjectPtr<USaveSlotsTabWidget> WeakThis(this);
	FSaveCallback Callback = [WeakThis](ESaveResult Result, const FSaveSlotInfo& Info)
	{
		if (USaveSlotsTabWidget* Tab = WeakThis.Get())
		{
			Tab->OnOperationDone(ESaveOperation::ListSlots, Result, Info);
		}
	};
	Subsystem->ListSlots(MoveTemp(Callback));
}

bool USaveSlotsTabWidget::IsAutosaveSlot(const FString& SlotName) const
{
	const USaveSubsystem* Subsystem = GetSaveSubsystem();
	if (!Subsystem)
	{
		return false;
	}
	for (const FSaveSlotInfo& Info : Subsystem->GetCachedSlots())
	{
		if (Info.SlotName == SlotName)
		{
			return Info.bIsAutosave;
		}
	}
	return false;
}

FSaveSlotInfo USaveSlotsTabWidget::GetSelectedSlot() const
{
	if (const USaveSubsystem* Subsystem = GetSaveSubsystem())
	{
		for (const FSaveSlotInfo& Info : Subsystem->GetCachedSlots())
		{
			if (Info.SlotName == SelectedSlotName)
			{
				return Info;
			}
		}
	}
	return FSaveSlotInfo();
}

void USaveSlotsTabWidget::SelectSlot(const FString& SlotName)
{
	if (SelectedSlotName == SlotName)
	{
		return;
	}

	SelectedSlotName = SlotName;
	for (USaveSlotEntryWidget* Row : Rows)
	{
		if (Row)
		{
			Row->SetSelected(Row->GetSlotInfo().SlotName == SelectedSlotName);
		}
	}

	UpdateControls();
	BP_OnSelectionChanged(SelectedSlotName);
}

void USaveSlotsTabWidget::HandleEntrySelected(USaveSlotEntryWidget* Entry)
{
	if (Entry)
	{
		SelectSlot(Entry->GetSlotInfo().SlotName);
	}
}

void USaveSlotsTabWidget::HandleSlotListChanged()
{
	RebuildRows();
}

void USaveSlotsTabWidget::HandleBusyChanged(bool /*bBusy*/)
{
	UpdateControls();
}

void USaveSlotsTabWidget::RebuildRows()
{
	USaveSubsystem* Subsystem = GetSaveSubsystem();
	if (!Subsystem || !SlotListPanel)
	{
		return;
	}
	if (!EntryClass)
	{
		UE_LOG(LogCoreSave, Warning, TEXT("%s has no Entry Class: set it to a child of SaveSlotEntryWidget."), *GetName());
		return;
	}

	SlotListPanel->ClearChildren();
	Rows.Reset();

	bool bSelectionStillExists = false;
	for (const FSaveSlotInfo& Info : Subsystem->GetCachedSlots())
	{
		// The autosave is only for loading: never offered as a place to save into.
		if (Info.bIsAutosave && (!ModeAllowsLoad() || !bShowAutosave))
		{
			continue;
		}

		USaveSlotEntryWidget* Row = CreateWidget<USaveSlotEntryWidget>(this, EntryClass);
		if (!Row)
		{
			continue;
		}

		Row->SetSlotInfo(Info);
		Row->OnSelected.AddDynamic(this, &USaveSlotsTabWidget::HandleEntrySelected);

		const bool bIsSelected = (Info.SlotName == SelectedSlotName);
		bSelectionStillExists |= bIsSelected;
		Row->SetSelected(bIsSelected);

		SlotListPanel->AddChild(Row);
		Rows.Add(Row);
	}

	if (!bSelectionStillExists && !SelectedSlotName.IsEmpty())
	{
		SelectedSlotName.Reset();
		BP_OnSelectionChanged(SelectedSlotName);
	}

	UpdateControls();
	BP_OnSlotsRefreshed(Rows.Num());
}

void USaveSlotsTabWidget::UpdateControls()
{
	const USaveSubsystem* Subsystem = GetSaveSubsystem();
	const bool bBusy = Subsystem && Subsystem->IsBusy();
	const bool bSelected = HasSelection();
	const bool bSelectedIsAutosave = bSelected && IsAutosaveSlot(SelectedSlotName);

	auto Configure = [](UCommonButtonBase* Button, bool bAllowedByMode, bool bEnabled)
	{
		if (Button)
		{
			Button->SetVisibility(bAllowedByMode ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
			Button->SetIsEnabled(bEnabled);
		}
	};

	Configure(SaveButton, ModeAllowsSave(), bSelected && !bSelectedIsAutosave && !bBusy);
	Configure(NewSlotButton, ModeAllowsSave(), !bBusy);
	Configure(LoadButton, ModeAllowsLoad(), bSelected && !bBusy);
	Configure(DeleteButton, true, bSelected && !bBusy);

	if (BusyIndicator)
	{
		BusyIndicator->SetVisibility(bBusy ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (EmptyListWidget)
	{
		EmptyListWidget->SetVisibility(Rows.Num() == 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void USaveSlotsTabWidget::SetStatus(const FText& Message)
{
	if (StatusText)
	{
		StatusText->SetText(Message);
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Actions
// ---------------------------------------------------------------------------------------------------------------------

void USaveSlotsTabWidget::RequestSaveSelected()
{
	if (!ModeAllowsSave() || !HasSelection() || IsAutosaveSlot(SelectedSlotName))
	{
		return;
	}
	BeginAction(ESaveSlotsAction::Overwrite, SelectedSlotName, bConfirmOverwrite);
}

void USaveSlotsTabWidget::RequestSaveToNewSlot()
{
	if (!ModeAllowsSave())
	{
		return;
	}
	// A new name cannot overwrite anything, so there is nothing to confirm.
	const FString NewName = USaveFunctionLibrary::MakeUniqueSlotName(this, NewSlotPrefix);
	ExecuteSave(NewName, NewName);
}

void USaveSlotsTabWidget::RequestLoadSelected()
{
	if (!ModeAllowsLoad() || !HasSelection())
	{
		return;
	}
	BeginAction(ESaveSlotsAction::Load, SelectedSlotName, bConfirmLoad);
}

void USaveSlotsTabWidget::RequestDeleteSelected()
{
	if (!HasSelection())
	{
		return;
	}
	BeginAction(ESaveSlotsAction::Delete, SelectedSlotName, bConfirmDelete);
}

void USaveSlotsTabWidget::BeginAction(ESaveSlotsAction Action, const FString& SlotName, bool bNeedsConfirmation)
{
	if (!bNeedsConfirmation)
	{
		ExecuteAction(Action, SlotName);
		return;
	}

	PendingAction = Action;
	PendingSlotName = SlotName;
	bHasPendingAction = true;

	FSaveSlotInfo Info;
	if (const USaveSubsystem* Subsystem = GetSaveSubsystem())
	{
		for (const FSaveSlotInfo& Candidate : Subsystem->GetCachedSlots())
		{
			if (Candidate.SlotName == SlotName)
			{
				Info = Candidate;
				break;
			}
		}
	}
	RequestConfirmation(Action, Info);
}

void USaveSlotsTabWidget::RequestConfirmation_Implementation(ESaveSlotsAction /*Action*/, const FSaveSlotInfo& /*Info*/)
{
	// No popup by default: confirm at once. A game overrides this to ask.
	ConfirmPendingAction();
}

void USaveSlotsTabWidget::ConfirmPendingAction()
{
	if (!bHasPendingAction)
	{
		return;
	}

	const ESaveSlotsAction Action = PendingAction;
	const FString SlotName = PendingSlotName;
	bHasPendingAction = false;
	PendingSlotName.Reset();
	ExecuteAction(Action, SlotName);
}

void USaveSlotsTabWidget::CancelPendingAction()
{
	bHasPendingAction = false;
	PendingSlotName.Reset();
}

void USaveSlotsTabWidget::ExecuteAction(ESaveSlotsAction Action, const FString& SlotName)
{
	USaveSubsystem* Subsystem = GetSaveSubsystem();
	if (!Subsystem)
	{
		return;
	}

	SetStatus(FText::GetEmpty());

	switch (Action)
	{
	case ESaveSlotsAction::Overwrite:
		ExecuteSave(SlotName, FString());
		break;

	case ESaveSlotsAction::Load:
		{
			TWeakObjectPtr<USaveSlotsTabWidget> WeakThis(this);
			FSaveCallback Callback = [WeakThis](ESaveResult Result, const FSaveSlotInfo& Info)
			{
				if (USaveSlotsTabWidget* Tab = WeakThis.Get())
				{
					Tab->OnOperationDone(ESaveOperation::LoadSlot, Result, Info);
				}
			};
			Subsystem->LoadSlot(SlotName, MoveTemp(Callback));
			break;
		}

	case ESaveSlotsAction::Delete:
		{
			TWeakObjectPtr<USaveSlotsTabWidget> WeakThis(this);
			FSaveCallback Callback = [WeakThis](ESaveResult Result, const FSaveSlotInfo& Info)
			{
				if (USaveSlotsTabWidget* Tab = WeakThis.Get())
				{
					Tab->OnOperationDone(ESaveOperation::DeleteSlot, Result, Info);
				}
			};
			Subsystem->DeleteSlot(SlotName, MoveTemp(Callback));
			break;
		}
	}
}

void USaveSlotsTabWidget::ExecuteSave(const FString& SlotName, const FString& DisplayName)
{
	USaveSubsystem* Subsystem = GetSaveSubsystem();
	if (!Subsystem)
	{
		return;
	}

	SetStatus(FText::GetEmpty());

	TWeakObjectPtr<USaveSlotsTabWidget> WeakThis(this);
	FSaveCallback Callback = [WeakThis](ESaveResult Result, const FSaveSlotInfo& Info)
	{
		if (USaveSlotsTabWidget* Tab = WeakThis.Get())
		{
			Tab->OnOperationDone(ESaveOperation::SaveSlot, Result, Info);
		}
	};
	Subsystem->SaveToSlot(SlotName, DisplayName, MoveTemp(Callback));
}

void USaveSlotsTabWidget::OnOperationDone(ESaveOperation Operation, ESaveResult Result, const FSaveSlotInfo& Info)
{
	if (Result != ESaveResult::Success)
	{
		HandleError(Operation, Result);
		return;
	}

	switch (Operation)
	{
	case ESaveOperation::SaveSlot:
		HandleSlotSaved(Info);
		break;
	case ESaveOperation::LoadSlot:
		HandleSlotLoaded(Info);
		break;
	case ESaveOperation::DeleteSlot:
		HandleSlotDeleted(Info);
		break;
	default:
		break;
	}
}

void USaveSlotsTabWidget::HandleSlotLoaded(const FSaveSlotInfo& Info)
{
	BP_OnSlotLoaded(Info);

	if (bOpenSavedMapOnLoad && !Info.MapName.IsEmpty())
	{
		UGameplayStatics::OpenLevel(this, FName(*Info.MapName));
	}
}

void USaveSlotsTabWidget::HandleSlotSaved(const FSaveSlotInfo& Info)
{
	// The list refreshes itself (OnSlotListChanged); keep the new slot selected.
	SelectSlot(Info.SlotName);
	BP_OnSlotSaved(Info);
}

void USaveSlotsTabWidget::HandleSlotDeleted(const FSaveSlotInfo& Info)
{
	BP_OnSlotDeleted(Info);
}

void USaveSlotsTabWidget::HandleError(ESaveOperation Operation, ESaveResult Result)
{
	const FText Message = USaveFunctionLibrary::ResultToText(Result);
	SetStatus(Message);
	BP_OnError(Operation, Result, Message);
}
