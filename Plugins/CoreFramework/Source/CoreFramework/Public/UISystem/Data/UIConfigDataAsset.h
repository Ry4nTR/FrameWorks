// CoreFramework UI System - Data
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "UISystem/Core/UITypes.h"
#include "UIConfigDataAsset.generated.h"

class UBaseRootLayoutWidget;

/** A reusable list of tabs. Create one per tabbed menu in the game project's Content Browser. */
UCLASS(BlueprintType)
class COREFRAMEWORK_API UUITabSetDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tabs", meta = (TitleProperty = "TabId"))
	TArray<FUITabDescriptor> Tabs;
};

/**
 * The single data-driven entry point of the UI system. One asset per game project, assigned in
 * Project Settings > CoreFramework > CoreFramework UI. Everything is soft-referenced.
 * The framework ships no content for it: the game fills every list.
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
};
