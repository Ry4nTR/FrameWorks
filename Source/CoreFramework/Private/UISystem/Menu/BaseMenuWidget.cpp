// CoreFramework UI System
#include "UISystem/Menu/BaseMenuWidget.h"

#include "CommonTabListWidgetBase.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "UISystem/Menu/Tabs/BaseTabWidget.h"
#include "UISystem/Data/UIConfigDataAsset.h"
#include "UISystem/Foundation/BaseButton.h"
#include "UISystem/Manager/UIFrameSubsystem.h"

UBaseMenuWidget::UBaseMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InputMode = EUIInputMode::UIOnly;
}

void UBaseMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (TabList)
	{
		TabList->OnTabSelected.AddUniqueDynamic(this, &UBaseMenuWidget::HandleTabSelected);
	}

	// Tabs registered at runtime (plugins, game features) rebuild an open menu.
	if (UUIFrameSubsystem* Subsystem = GetUIFrameSubsystem())
	{
		TabsChangedHandle = Subsystem->OnMenuTabsChanged.AddUObject(this, &UBaseMenuWidget::HandleMenuTabsChanged);
	}
}

void UBaseMenuWidget::NativeDestruct()
{
	if (UUIFrameSubsystem* Subsystem = GetUIFrameSubsystem())
	{
		Subsystem->OnMenuTabsChanged.Remove(TabsChangedHandle);
	}
	TabsChangedHandle.Reset();

	if (TabList)
	{
		TabList->OnTabSelected.RemoveDynamic(this, &UBaseMenuWidget::HandleTabSelected);
	}

	// Invalidate any in-flight tab load and drop the content.
	++TabBuildGeneration;
	if (TabLoadHandle.IsValid())
	{
		TabLoadHandle->CancelHandle();
		TabLoadHandle.Reset();
	}
	bTabsLoading = false;
	ClearTabs();

	Super::NativeDestruct();
}

void UBaseMenuWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	if (!bTabsBuilt && !bTabsLoading)
	{
		RebuildTabs();
	}
}

UWidget* UBaseMenuWidget::ResolveDefaultFocusWidget() const
{
	if (UWidget* Explicit = Super::ResolveDefaultFocusWidget())
	{
		return Explicit;
	}

	// Tabbed menus: land on the button of the active tab so D-pad / arrow keys work immediately.
	if (TabList && !ActiveTabId.IsNone())
	{
		return TabList->GetTabButtonBaseByID(ActiveTabId);
	}
	return nullptr;
}

// ---------------------------------------------------------------------------------------------------------------------
// Tab data
// ---------------------------------------------------------------------------------------------------------------------

void UBaseMenuWidget::CollectDescriptors(TArray<FUITabDescriptor>& OutDescriptors) const
{
	OutDescriptors.Reset();
	TMap<FName, int32> IndexById;

	// Later sources override earlier ones by TabId.
	auto Merge = [&OutDescriptors, &IndexById](const FUITabDescriptor& Descriptor)
	{
		if (Descriptor.TabId.IsNone())
		{
			return;
		}
		if (const int32* ExistingIndex = IndexById.Find(Descriptor.TabId))
		{
			OutDescriptors[*ExistingIndex] = Descriptor;
		}
		else
		{
			IndexById.Add(Descriptor.TabId, OutDescriptors.Add(Descriptor));
		}
	};

	if (DefaultTabSet)
	{
		for (const FUITabDescriptor& Descriptor : DefaultTabSet->Tabs)
		{
			Merge(Descriptor);
		}
	}

	if (MenuTag.IsValid())
	{
		if (const UUIFrameSubsystem* Subsystem = GetUIFrameSubsystem())
		{
			for (const FUITabDescriptor& Descriptor : Subsystem->GetMenuTabs(MenuTag))
			{
				Merge(Descriptor);
			}
		}
	}

	OutDescriptors.RemoveAll([](const FUITabDescriptor& Descriptor) { return !Descriptor.bEnabled; });
	OutDescriptors.StableSort([](const FUITabDescriptor& A, const FUITabDescriptor& B) { return A.SortOrder < B.SortOrder; });
}

void UBaseMenuWidget::HandleMenuTabsChanged(FGameplayTag ChangedMenuTag)
{
	if (MenuTag.IsValid() && ChangedMenuTag == MenuTag)
	{
		RebuildTabs();
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Build
// ---------------------------------------------------------------------------------------------------------------------

void UBaseMenuWidget::RebuildTabs()
{
	if (!TabList || !TabSwitcher)
	{
		return; // A menu without a tab bar is valid.
	}

	TArray<FUITabDescriptor> Descriptors;
	CollectDescriptors(Descriptors);

	// Newest request wins: anything still streaming for an older request is discarded.
	const uint32 Generation = ++TabBuildGeneration;
	if (TabLoadHandle.IsValid())
	{
		TabLoadHandle->CancelHandle();
		TabLoadHandle.Reset();
	}

	TArray<FSoftObjectPath> PathsToLoad;
	for (const FUITabDescriptor& Descriptor : Descriptors)
	{
		if (!Descriptor.TabWidgetClass.IsNull() && !Descriptor.TabWidgetClass.Get())
		{
			PathsToLoad.AddUnique(Descriptor.TabWidgetClass.ToSoftObjectPath());
		}
	}

	if (PathsToLoad.IsEmpty())
	{
		bTabsLoading = false;
		BuildTabs(Descriptors);
		return;
	}

	bTabsLoading = true;
	TabLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		PathsToLoad,
		FStreamableDelegate::CreateWeakLambda(this, [this, Generation, Descriptors]()
		{
			if (Generation != TabBuildGeneration)
			{
				return;
			}
			bTabsLoading = false;
			BuildTabs(Descriptors);
		}));
}

void UBaseMenuWidget::BuildTabs(const TArray<FUITabDescriptor>& Descriptors)
{
	ClearTabs();

	if (!TabList || !TabSwitcher)
	{
		return;
	}
	if (!TabButtonClass)
	{
		UE_LOG(LogCoreUI, Error, TEXT("%s: TabButtonClass is not set (use a Blueprint child of BaseButton), tabs cannot be created."), *GetName());
		return;
	}

	FName FirstTabId = NAME_None;
	for (const FUITabDescriptor& Descriptor : Descriptors)
	{
		UClass* ContentClass = Descriptor.TabWidgetClass.Get();
		if (!ContentClass)
		{
			UE_LOG(LogCoreUI, Warning, TEXT("%s: tab '%s' has no loadable widget class."), *GetName(), *Descriptor.TabId.ToString());
			continue;
		}

		UBaseTabWidget* Content = CreateWidget<UBaseTabWidget>(GetOwningPlayer(), TSubclassOf<UBaseTabWidget>(ContentClass));
		if (!Content)
		{
			continue;
		}

		Content->InitializeTab(Descriptor);
		TabSwitcher->AddChild(Content);
		TabContents.Add(Descriptor.TabId, Content);

		// Content switching is handled here (HandleTabSelected), so no content widget is passed to the tab list.
		TabList->RegisterTab(Descriptor.TabId, TabButtonClass, nullptr);
		if (UBaseButton* Button = Cast<UBaseButton>(TabList->GetTabButtonBaseByID(Descriptor.TabId)))
		{
			Button->SetButtonLabel(Descriptor.DisplayName);
		}

		if (FirstTabId.IsNone())
		{
			FirstTabId = Descriptor.TabId;
		}
	}

	bTabsBuilt = true;
	BP_OnTabsBuilt();

	if (!FirstTabId.IsNone())
	{
		SelectTab(FirstTabId);
	}

	if (IsActivated())
	{
		ScheduleDefaultFocus();
	}
}

void UBaseMenuWidget::ClearTabs()
{
	if (!ActiveTabId.IsNone())
	{
		if (const TObjectPtr<UBaseTabWidget>* Active = TabContents.Find(ActiveTabId))
		{
			if (*Active)
			{
				(*Active)->NotifyTabDeselected();
			}
		}
	}

	if (TabList)
	{
		TabList->RemoveAllTabs();
	}
	if (TabSwitcher)
	{
		TabSwitcher->ClearChildren();
	}

	TabContents.Reset();
	ActiveTabId = NAME_None;
	bTabsBuilt = false;
}

// ---------------------------------------------------------------------------------------------------------------------
// Selection
// ---------------------------------------------------------------------------------------------------------------------

bool UBaseMenuWidget::SelectTab(FName TabId)
{
	if (!TabList || !TabContents.Contains(TabId))
	{
		return false;
	}

	TabList->SelectTabByID(TabId);
	ActivateTabContent(TabId); // Idempotent: also covers the case where the tab was already selected.
	return true;
}

void UBaseMenuWidget::HandleTabSelected(FName TabId)
{
	ActivateTabContent(TabId);
}

void UBaseMenuWidget::ActivateTabContent(FName TabId)
{
	if (TabId == ActiveTabId)
	{
		return;
	}

	UBaseTabWidget* NewContent = GetTabWidget(TabId);
	if (!NewContent || !TabSwitcher)
	{
		return;
	}

	if (UBaseTabWidget* OldContent = GetTabWidget(ActiveTabId))
	{
		OldContent->NotifyTabDeselected();
	}

	ActiveTabId = TabId;
	TabSwitcher->SetActiveWidget(NewContent);
	NewContent->NotifyTabSelected();
	BP_OnActiveTabChanged(TabId);

	if (NewContent->ShouldFocusContentOnSelect())
	{
		NewContent->FocusTabContent();
	}
}

UBaseTabWidget* UBaseMenuWidget::GetTabWidget(FName TabId) const
{
	const TObjectPtr<UBaseTabWidget>* Found = TabContents.Find(TabId);
	return Found ? Found->Get() : nullptr;
}

// ---------------------------------------------------------------------------------------------------------------------
// Apply / revert
// ---------------------------------------------------------------------------------------------------------------------

void UBaseMenuWidget::ApplyAllTabs()
{
	for (const TPair<FName, TObjectPtr<UBaseTabWidget>>& Pair : TabContents)
	{
		if (UBaseTabWidget* Tab = Pair.Value)
		{
			if (Tab->HasPendingChanges())
			{
				Tab->ApplyChanges();
			}
		}
	}
}

void UBaseMenuWidget::RevertAllTabs()
{
	for (const TPair<FName, TObjectPtr<UBaseTabWidget>>& Pair : TabContents)
	{
		if (UBaseTabWidget* Tab = Pair.Value)
		{
			if (Tab->HasPendingChanges())
			{
				Tab->RevertChanges();
			}
		}
	}
}

bool UBaseMenuWidget::HasPendingChangesInAnyTab() const
{
	for (const TPair<FName, TObjectPtr<UBaseTabWidget>>& Pair : TabContents)
	{
		if (const UBaseTabWidget* Tab = Pair.Value)
		{
			if (Tab->HasPendingChanges())
			{
				return true;
			}
		}
	}
	return false;
}
