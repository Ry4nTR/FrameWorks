// CoreFramework UI System
#include "UISystem/HUD/BaseHUDWidget.h"

#include "Components/PanelWidget.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "UISystem/Foundation/BaseUserWidget.h"
#include "UISystem/Manager/UIFrameSubsystem.h"
#include "UISystem/Core/UIGameplayTags.h"

UBaseHUDWidget::UBaseHUDWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// The HUD lives on the Game layer and lets the game keep all input.
	InputMode = EUIInputMode::GameOnly;
}

void UBaseHUDWidget::NativeConstruct()
{
	// Super runs the Blueprint "Event Construct", where custom slots are registered.
	Super::NativeConstruct();

	if (DefaultElementSlot)
	{
		RegisterElementSlot(FUITags::HUDSlotDefault(), DefaultElementSlot);
	}

	if (UUIFrameSubsystem* Subsystem = GetUIFrameSubsystem())
	{
		ElementRegisteredHandle = Subsystem->OnHUDElementRegistered.AddUObject(this, &UBaseHUDWidget::HandleHUDElementRegistered);
		ElementUnregisteredHandle = Subsystem->OnHUDElementUnregistered.AddUObject(this, &UBaseHUDWidget::HandleHUDElementUnregistered);
	}

	SpawnAllRegisteredElements();
}

void UBaseHUDWidget::NativeDestruct()
{
	if (UUIFrameSubsystem* Subsystem = GetUIFrameSubsystem())
	{
		Subsystem->OnHUDElementRegistered.Remove(ElementRegisteredHandle);
		Subsystem->OnHUDElementUnregistered.Remove(ElementUnregisteredHandle);
	}
	ElementRegisteredHandle.Reset();
	ElementUnregisteredHandle.Reset();

	for (TPair<FName, TSharedPtr<FStreamableHandle>>& Pending : PendingElementLoads)
	{
		if (Pending.Value.IsValid())
		{
			Pending.Value->CancelHandle();
		}
	}
	PendingElementLoads.Reset();

	for (TPair<FName, TObjectPtr<UBaseUserWidget>>& Pair : SpawnedElements)
	{
		if (Pair.Value)
		{
			Pair.Value->RemoveFromParent();
		}
	}
	SpawnedElements.Reset();
	ElementSlots.Reset();

	Super::NativeDestruct();
}

void UBaseHUDWidget::RegisterElementSlot(FGameplayTag SlotTag, UPanelWidget* Container)
{
	if (!SlotTag.IsValid() || !Container)
	{
		return;
	}

	ElementSlots.Add(SlotTag, Container);

	// Elements registered before this slot existed can be placed now.
	if (const UUIFrameSubsystem* Subsystem = GetUIFrameSubsystem())
	{
		for (const FUIHUDElementDescriptor& Descriptor : Subsystem->GetHUDElements())
		{
			if (Descriptor.SlotTag == SlotTag)
			{
				SpawnElement(Descriptor);
			}
		}
	}
}

void UBaseHUDWidget::UnregisterElementSlot(FGameplayTag SlotTag)
{
	ElementSlots.Remove(SlotTag);
}

UBaseUserWidget* UBaseHUDWidget::FindElement(FName ElementId) const
{
	const TObjectPtr<UBaseUserWidget>* Found = SpawnedElements.Find(ElementId);
	return Found ? Found->Get() : nullptr;
}

void UBaseHUDWidget::HandleHUDElementRegistered(const FUIHUDElementDescriptor& Descriptor)
{
	SpawnElement(Descriptor);
}

void UBaseHUDWidget::HandleHUDElementUnregistered(FName ElementId)
{
	RemoveElement(ElementId);
}

void UBaseHUDWidget::SpawnAllRegisteredElements()
{
	if (const UUIFrameSubsystem* Subsystem = GetUIFrameSubsystem())
	{
		for (const FUIHUDElementDescriptor& Descriptor : Subsystem->GetHUDElements())
		{
			SpawnElement(Descriptor);
		}
	}
}

void UBaseHUDWidget::SpawnElement(const FUIHUDElementDescriptor& Descriptor)
{
	if (Descriptor.ElementId.IsNone() || Descriptor.WidgetClass.IsNull())
	{
		return;
	}
	if (SpawnedElements.Contains(Descriptor.ElementId) || PendingElementLoads.Contains(Descriptor.ElementId))
	{
		return;
	}
	if (!ElementSlots.Contains(Descriptor.SlotTag))
	{
		// Not an error: the slot may be registered later, RegisterElementSlot retries.
		return;
	}

	if (Descriptor.WidgetClass.Get())
	{
		CreateElement(Descriptor);
		return;
	}

	TSharedPtr<FStreamableHandle> Handle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		Descriptor.WidgetClass.ToSoftObjectPath(),
		FStreamableDelegate::CreateWeakLambda(this, [this, Descriptor]()
		{
			PendingElementLoads.Remove(Descriptor.ElementId);

			// The element may have been unregistered while it was loading.
			const UUIFrameSubsystem* Subsystem = GetUIFrameSubsystem();
			if (Subsystem && Subsystem->IsHUDElementRegistered(Descriptor.ElementId))
			{
				CreateElement(Descriptor);
			}
		}));

	if (Handle.IsValid() && !Handle->HasLoadCompleted())
	{
		PendingElementLoads.Add(Descriptor.ElementId, Handle);
	}
}

void UBaseHUDWidget::CreateElement(const FUIHUDElementDescriptor& Descriptor)
{
	UPanelWidget* Container = ElementSlots.FindRef(Descriptor.SlotTag);
	UClass* LoadedClass = Descriptor.WidgetClass.Get();
	if (!Container || !LoadedClass || SpawnedElements.Contains(Descriptor.ElementId))
	{
		return;
	}

	UBaseUserWidget* Element = CreateWidget<UBaseUserWidget>(GetOwningPlayer(), TSubclassOf<UBaseUserWidget>(LoadedClass));
	if (!Element)
	{
		return;
	}

	Container->AddChild(Element);
	SpawnedElements.Add(Descriptor.ElementId, Element);
	BP_OnElementAdded(Descriptor.ElementId, Element);
}

void UBaseHUDWidget::RemoveElement(FName ElementId)
{
	if (TSharedPtr<FStreamableHandle>* Pending = PendingElementLoads.Find(ElementId))
	{
		if (Pending->IsValid())
		{
			(*Pending)->CancelHandle();
		}
		PendingElementLoads.Remove(ElementId);
	}

	TObjectPtr<UBaseUserWidget> Element;
	if (SpawnedElements.RemoveAndCopyValue(ElementId, Element))
	{
		if (Element)
		{
			Element->RemoveFromParent();
		}
		BP_OnElementRemoved(ElementId);
	}
}
