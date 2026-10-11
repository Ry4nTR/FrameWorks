// CoreFramework UI System
#include "UISystem/BaseRootLayoutWidget.h"

#include "UISystem/UIGameplayTags.h"
#include "UISystem/UITypes.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

void UBaseRootLayoutWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Bound widgets exist at this point. Register the four standard layers; project layers use RegisterLayer.
	RegisterLayer(FUITags::LayerGame(), GameLayerStack);
	RegisterLayer(FUITags::LayerMenu(), MenuLayerStack);
	RegisterLayer(FUITags::LayerModal(), ModalLayerStack);
	RegisterLayer(FUITags::LayerOverlay(), OverlayLayerStack);
}

void UBaseRootLayoutWidget::RegisterLayer(FGameplayTag LayerTag, UCommonActivatableWidgetContainerBase* LayerContainer)
{
	if (!LayerTag.IsValid() || !LayerContainer)
	{
		return;
	}

	Layers.Add(LayerTag, LayerContainer);
}

UCommonActivatableWidgetContainerBase* UBaseRootLayoutWidget::FindLayer(FGameplayTag LayerTag) const
{
	const TObjectPtr<UCommonActivatableWidgetContainerBase>* Found = Layers.Find(LayerTag);
	return Found ? Found->Get() : nullptr;
}

void UBaseRootLayoutWidget::ClearAllLayers()
{
	for (const TPair<FGameplayTag, TObjectPtr<UCommonActivatableWidgetContainerBase>>& Pair : Layers)
	{
		if (UCommonActivatableWidgetContainerBase* Layer = Pair.Value)
		{
			Layer->ClearWidgets();
		}
	}
}
