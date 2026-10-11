// CoreFramework UI System
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "UISystem/UITypes.h"
#include "UIConfigDataAsset.generated.h"

class UBaseRootLayoutWidget;

/** A reusable list of tabs. Create one per menu (or one per feature) in the Content Browser. */
UCLASS(BlueprintType)
class COREFRAMEWORK_API UUITabSetDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tabs", meta = (TitleProperty = "TabId"))
	TArray<FUITabDescriptor> Tabs;
};

/** Attaches a tab set to a menu id so a child project can extend a framework menu without editing it. */
USTRUCT(BlueprintType)
struct COREFRAMEWORK_API FUIMenuTabSetBinding
{
	GENERATED_BODY()

	/** Must match UBaseMenuWidget::MenuTag of the menu that should receive the tabs. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tabs", meta = (Categories = "CoreFramework.UI.Screen"))
	FGameplayTag MenuTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tabs")
	TSoftObjectPtr<UUITabSetDataAsset> TabSet;
};

/**
 * The single data-driven entry point of the UI system. One asset per project, assigned in
 * Project Settings > CoreFramework > CoreFramework UI. Everything is soft-referenced.
 */
UCLASS(BlueprintType)
class COREFRAMEWORK_API UUIConfigDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Root layout widget Blueprint (child of UBaseRootLayoutWidget) holding the layer stacks. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Root")
	TSoftClassPtr<UBaseRootLayoutWidget> RootLayoutClass;

	/** Every screen the project can push by tag. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Screens", meta = (TitleProperty = "ScreenTag"))
	TArray<FUIScreenDefinition> Screens;

	/** Widgets injected into HUD slots. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HUD", meta = (TitleProperty = "ElementId"))
	TArray<FUIHUDElementDescriptor> HUDElements;

	/** Tab sets attached to menus by tag. Several entries may target the same menu; their tabs are merged. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Menus", meta = (TitleProperty = "MenuTag"))
	TArray<FUIMenuTabSetBinding> MenuTabSets;
};
