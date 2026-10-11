// CoreFramework UI System
#pragma once

#include "CoreMinimal.h"
#include "UISystem/BaseUserWidget.h"
#include "UISystem/UITypes.h"
#include "BaseTabWidget.generated.h"

class UWidget;

/**
 * Content of one tab (Settings category, Main Menu page...). Created and hosted by UBaseMenuWidget.
 *
 * Game-specific tabs are Blueprint or C++ children of this class; register them through a
 * UUITabSetDataAsset or UUIFrameSubsystem::RegisterMenuTab. Settings tabs override
 * HasPendingChanges / ApplyChanges / RevertChanges so the menu's Apply and Revert buttons work uniformly.
 */
UCLASS(Abstract, Blueprintable)
class COREFRAMEWORK_API UBaseTabWidget : public UBaseUserWidget
{
	GENERATED_BODY()

public:
	UBaseTabWidget(const FObjectInitializer& ObjectInitializer);

	/** Called once by the hosting menu right after creation. */
	void InitializeTab(const FUITabDescriptor& InDescriptor);

	/** Called by the hosting menu when this tab becomes / stops being the visible tab. */
	void NotifyTabSelected();
	void NotifyTabDeselected();

	UFUNCTION(BlueprintPure, Category = "Base UI|Tab")
	FName GetTabId() const { return Descriptor.TabId; }

	UFUNCTION(BlueprintPure, Category = "Base UI|Tab")
	FText GetTabDisplayName() const { return Descriptor.DisplayName; }

	UFUNCTION(BlueprintPure, Category = "Base UI|Tab")
	bool IsTabSelected() const { return bTabSelected; }

	bool ShouldFocusContentOnSelect() const { return bFocusContentOnSelect; }

	/** Moves keyboard / gamepad focus into the tab content (DefaultFocusWidget). */
	UFUNCTION(BlueprintCallable, Category = "Base UI|Tab")
	void FocusTabContent();

	/** True when the tab holds edits that were not applied yet. */
	UFUNCTION(BlueprintNativeEvent, Category = "Base UI|Tab")
	bool HasPendingChanges() const;

	/** Commit pending edits. */
	UFUNCTION(BlueprintNativeEvent, Category = "Base UI|Tab")
	void ApplyChanges();

	/** Discard pending edits and refresh the displayed values. */
	UFUNCTION(BlueprintNativeEvent, Category = "Base UI|Tab")
	void RevertChanges();

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Base UI|Tab", meta = (DisplayName = "On Tab Initialized"))
	void BP_OnTabInitialized(FName TabId, const FText& DisplayName);

	UFUNCTION(BlueprintImplementableEvent, Category = "Base UI|Tab", meta = (DisplayName = "On Tab Selected"))
	void BP_OnTabSelected();

	UFUNCTION(BlueprintImplementableEvent, Category = "Base UI|Tab", meta = (DisplayName = "On Tab Deselected"))
	void BP_OnTabDeselected();

	/** Optional widget named exactly DefaultFocusWidget. Focus target for FocusTabContent. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> DefaultFocusWidget;

	/** Move focus into the content when the tab is selected (mouse users). Leave off so gamepad bumpers keep focus on the tab bar. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base UI|Tab")
	bool bFocusContentOnSelect = false;

private:
	FUITabDescriptor Descriptor;
	bool bTabSelected = false;
};
