// CoreFramework Save System - UI
#include "SaveSystem/UI/SaveSlotEntryWidget.h"

#include "CommonButtonBase.h"
#include "Components/TextBlock.h"
#include "SaveSystem/Library/SaveFunctionLibrary.h"

USaveSlotEntryWidget::USaveSlotEntryWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USaveSlotEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (SelectButton)
	{
		SelectButton->OnClicked().AddUObject(this, &USaveSlotEntryWidget::HandleSelectClicked);
	}
}

void USaveSlotEntryWidget::NativeDestruct()
{
	if (SelectButton)
	{
		SelectButton->OnClicked().RemoveAll(this);
	}

	Super::NativeDestruct();
}

void USaveSlotEntryWidget::SetSlotInfo(const FSaveSlotInfo& InInfo)
{
	Info = InInfo;

	if (SlotNameText)
	{
		SlotNameText->SetText(FText::FromString(Info.SlotName));
	}
	if (DisplayNameText)
	{
		DisplayNameText->SetText(FText::FromString(Info.DisplayName));
	}
	if (SavedAtText)
	{
		SavedAtText->SetText(USaveFunctionLibrary::FormatSavedAt(Info.SavedAt));
	}
	if (PlaytimeText)
	{
		PlaytimeText->SetText(USaveFunctionLibrary::FormatPlaytime(Info.PlaytimeSeconds));
	}
	if (MapText)
	{
		MapText->SetText(FText::FromString(Info.MapName));
	}

	BP_OnSlotInfoSet(Info);
}

void USaveSlotEntryWidget::SetSelected(bool bInSelected)
{
	if (bSelected != bInSelected)
	{
		bSelected = bInSelected;
		BP_OnSelectionChanged(bSelected);
	}
}

void USaveSlotEntryWidget::RequestSelect()
{
	OnSelected.Broadcast(this);
}

void USaveSlotEntryWidget::HandleSelectClicked()
{
	RequestSelect();
}
