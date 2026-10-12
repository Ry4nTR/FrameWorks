// CoreFramework UI System
#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "GameplayTagContainer.h"
#include "BaseRootLayoutWidget.generated.h"

class UCommonActivatableWidgetContainerBase;
class UCommonActivatableWidgetStack;

/**
 * Root of the whole UI. One instance per local player, owned by UUIFrameSubsystem and added to the player screen.
 * The Blueprint child (WBP_BaseRootLayout) is an Overlay containing one Common Activatable Widget Stack per layer.
 *
 * Four stacks are discovered by name (GameLayerStack, MenuLayerStack, ModalLayerStack, OverlayLayerStack).
 * Any number of extra layers can be added with RegisterLayer from the Blueprint, using your own layer tags.
 * Overlay order in the designer decides draw order: put Game first and Overlay last.
 */
UCLASS(Abstract, Blueprintable)
class COREFRAMEWORK_API UBaseRootLayoutWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Base UI|Layers")
	void RegisterLayer(FGameplayTag LayerTag, UCommonActivatableWidgetContainerBase* LayerContainer);

	UFUNCTION(BlueprintCallable, Category = "Base UI|Layers")
	UCommonActivatableWidgetContainerBase* FindLayer(FGameplayTag LayerTag) const;

	/** Removes every widget from every layer (used on teardown). */
	UFUNCTION(BlueprintCallable, Category = "Base UI|Layers")
	void ClearAllLayers();

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UCommonActivatableWidgetStack> GameLayerStack;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UCommonActivatableWidgetStack> MenuLayerStack;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UCommonActivatableWidgetStack> ModalLayerStack;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UCommonActivatableWidgetStack> OverlayLayerStack;

private:
	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<UCommonActivatableWidgetContainerBase>> Layers;
};
