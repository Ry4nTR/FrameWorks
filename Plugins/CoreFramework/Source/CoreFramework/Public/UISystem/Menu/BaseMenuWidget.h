// CoreFramework UI System
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UISystem/Foundation/BaseActivatableWidget.h"
#include "UISystem/Core/UITypes.h"
#include "BaseMenuWidget.generated.h"

class UBaseButton;
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
 *   DefaultTabSet (assigned on this widget)  <  UUIFrameSubsystem::RegisterMenuTab (runtime, plugins / game features)
 * Later sources override earlier ones by TabId, so a project can replace or disable (bEnabled = false)
 * a tab provided by another source. Tab content classes are async loaded.
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

	/** Id used to receive tabs registered at runtime through the subsystem. Use the same tag you registered this screen with. Leave empty when no runtime tabs are expected. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base UI|Menu")
	FGameplayTag MenuTag;

	/** Tabs of this menu. Create a UITabSetDataAsset in the game project and assign it here. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base UI|Menu")
	TObjectPtr<UUITabSetDataAsset> DefaultTabSet;

	/** Button Blueprint (child of UBaseButton) instantiated for each tab. Required when tabs are used. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base UI|Menu")
	TSubclassOf<UBaseButton> TabButtonClass;

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
