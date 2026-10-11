// CoreFramework UI System
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UISystem/BaseActivatableWidget.h"
#include "UISystem/UITypes.h"
#include "BaseHUDWidget.generated.h"

class UBaseUserWidget;
class UPanelWidget;
struct FStreamableHandle;

/**
 * Master container for gameplay overlays. Pushed on the Game layer, input mode GameOnly.
 *
 * Binding: because it is an activatable binder it is told about the local controller, player state and pawn
 * (override OnUIPawnBound etc. in C++ or Blueprint). Child HUD elements derive from UBaseUserWidget and bind
 * themselves, so the HUD never has to know what a health bar is.
 *
 * Extension: designers or game code inject widgets into named slots with FUIHUDElementDescriptor
 * (UUIConfigDataAsset::HUDElements or UUIFrameSubsystem::RegisterHUDElement). Elements are async loaded and
 * created on demand, also when registered while the HUD is already on screen.
 */
UCLASS(Abstract, Blueprintable)
class COREFRAMEWORK_API UBaseHUDWidget : public UBaseActivatableWidget
{
	GENERATED_BODY()

public:
	UBaseHUDWidget(const FObjectInitializer& ObjectInitializer);

	/** Register a panel as the container for a slot tag. Call from Event Construct for every custom slot. */
	UFUNCTION(BlueprintCallable, Category = "Base UI|HUD")
	void RegisterElementSlot(FGameplayTag SlotTag, UPanelWidget* Container);

	UFUNCTION(BlueprintCallable, Category = "Base UI|HUD")
	void UnregisterElementSlot(FGameplayTag SlotTag);

	UFUNCTION(BlueprintPure, Category = "Base UI|HUD")
	UBaseUserWidget* FindElement(FName ElementId) const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Base UI|HUD", meta = (DisplayName = "On HUD Element Added"))
	void BP_OnElementAdded(FName ElementId, UBaseUserWidget* Element);

	UFUNCTION(BlueprintImplementableEvent, Category = "Base UI|HUD", meta = (DisplayName = "On HUD Element Removed"))
	void BP_OnElementRemoved(FName ElementId);

	/** Optional panel named exactly DefaultElementSlot. Registered under the HUDSlot.Default tag. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> DefaultElementSlot;

private:
	void HandleHUDElementRegistered(const FUIHUDElementDescriptor& Descriptor);
	void HandleHUDElementUnregistered(FName ElementId);

	void SpawnAllRegisteredElements();
	void SpawnElement(const FUIHUDElementDescriptor& Descriptor);
	void CreateElement(const FUIHUDElementDescriptor& Descriptor);
	void RemoveElement(FName ElementId);

	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<UPanelWidget>> ElementSlots;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UBaseUserWidget>> SpawnedElements;

	TMap<FName, TSharedPtr<FStreamableHandle>> PendingElementLoads;

	FDelegateHandle ElementRegisteredHandle;
	FDelegateHandle ElementUnregisteredHandle;
};
