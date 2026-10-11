// CoreFramework UI System
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UISystem/BaseActivatableWidget.h"
#include "UISystem/UITypes.h"
#include "BaseMenuWidget.generated.h"

class UBaseTabButton;
class UBaseTabWidget;
class UCommonButtonBase;
class UCommonTabListWidgetBase;
class UUITabSetDataAsset;
class UWidgetSwitcher;
struct FStreamableHandle;

/**
 * Base class for Main Menu, Pause Menu and Settings screens.
 *
 * Works with or without tabs. When the Blueprint contains a Common Tab List named TabList and a
 * Widget Switcher named TabSwitcher, the menu builds its tabs from data:
 *   DefaultTabSet (this widget)  <  UUIConfigDataAsset::MenuTabSets  <  UUIFrameSubsystem::RegisterMenuTab
 * Later sources override earlier ones by TabId, so a project can replace or disable (bEnabled = false)
 * a framework tab without touching framework assets. Tab content classes are async loaded.
 */
UCLASS(Abstract, Blueprintable)
class COREFRAMEWORK_API UBaseMenuWidget : public UBaseActivatableWidget
{
	GENERATED_BODY()

public:
	UBaseMenuWidget(const FObjectInitializer& ObjectInitializer);

	/** Collects descriptors from all sources, async loads the tab widgets and rebuilds the tab bar. */
	UFUNCTION(BlueprintCallable, Category = "Base UI|Menu")
	void RebuildTabs();

	UFUNCTION(BlueprintCallable, Category = "Base UI|Menu")
	bool SelectTab(FName TabId);

	UFUNCTION(BlueprintPure, Category = "Base UI|Menu")
	FName GetActiveTabId() const { return ActiveTabId; }

	UFUNCTION(BlueprintPure, Category = "Base UI|Menu")
	UBaseTabWidget* GetTabWidget(FName TabId) const;

	/** Applies every tab that reports pending changes. Wire to an Apply button. */
	UFUNCTION(BlueprintCallable, Category = "Base UI|Menu")
	void ApplyAllTabs();

	/** Reverts every tab that reports pending changes. Wire to a Revert / Cancel button. */
	UFUNCTION(BlueprintCallable, Category = "Base UI|Menu")
	void RevertAllTabs();

	UFUNCTION(BlueprintPure, Category = "Base UI|Menu")
	bool HasPendingChangesInAnyTab() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;
	virtual UWidget* ResolveDefaultFocusWidget() const override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Base UI|Menu", meta = (DisplayName = "On Tabs Built"))
	void BP_OnTabsBuilt();

	UFUNCTION(BlueprintImplementableEvent, Category = "Base UI|Menu", meta = (DisplayName = "On Active Tab Changed"))
	void BP_OnActiveTabChanged(FName TabId);

	/** Id used to receive tabs registered through the config asset or the subsystem. Leave empty for menus without extensions. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base UI|Menu", meta = (Categories = "CoreFramework.UI.Screen"))
	FGameplayTag MenuTag;

	/** Tabs that ship with this menu Blueprint. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base UI|Menu")
	TObjectPtr<UUITabSetDataAsset> DefaultTabSet;

	/** Button Blueprint (child of UBaseTabButton) instantiated for each tab. Required when tabs are used. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base UI|Menu")
	TSubclassOf<UBaseTabButton> TabButtonClass;

	/** Optional Common Tab List named exactly TabList. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTabListWidgetBase> TabList;

	/** Optional Widget Switcher (or Common Animated Switcher) named exactly TabSwitcher. Hosts the tab content. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidgetSwitcher> TabSwitcher;

private:
	UFUNCTION()
	void HandleTabSelected(FName TabId);

	void HandleMenuTabsChanged(FGameplayTag ChangedMenuTag);

	void CollectDescriptors(TArray<FUITabDescriptor>& OutDescriptors) const;
	void BuildTabs(const TArray<FUITabDescriptor>& Descriptors);
	void ClearTabs();
	void ActivateTabContent(FName TabId);

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UBaseTabWidget>> TabContents;

	TSharedPtr<FStreamableHandle> TabLoadHandle;
	FDelegateHandle TabsChangedHandle;
	FName ActiveTabId = NAME_None;
	uint32 TabBuildGeneration = 0;
	bool bTabsBuilt = false;
	bool bTabsLoading = false;
};
