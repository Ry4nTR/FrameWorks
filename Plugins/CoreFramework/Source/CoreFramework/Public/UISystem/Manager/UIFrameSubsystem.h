// CoreFramework UI System
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "UISystem/Core/UITypes.h"
#include "UIFrameSubsystem.generated.h"

class APawn;
class APlayerController;
class APlayerState;
class UBaseActivatableWidget;
class UBaseRootLayoutWidget;
class UCommonActivatableWidgetContainerBase;
class UUIBindingHub;
class UUIConfigDataAsset;
class UUserWidget;
struct FStreamableHandle;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnUIScreenPushed, FGameplayTag, ScreenTag, UBaseActivatableWidget*, Widget);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnUIHUDElementRegistered, const FUIHUDElementDescriptor&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnUIHUDElementUnregistered, FName);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnUIMenuTabsChanged, FGameplayTag);

/** Completion callback of an async screen push. Receives null when the push failed. */
using FUIScreenPushedCallback = TFunction<void(UBaseActivatableWidget*)>;

/**
 * Central manager of the UI of one local player (split-screen safe: one instance per ULocalPlayer).
 *
 * Responsibilities
 *  - Owns the root layout widget and its layers (Game, Menu, Modal, Overlay + project layers).
 *  - Pushes / pops screens by gameplay tag; loads their widget classes asynchronously from soft references.
 *  - Keeps the registries that make the system extensible without touching framework code:
 *    screens, HUD elements, menu tabs. Filled from UUIConfigDataAsset and from runtime Register* calls.
 *  - Owns the binding hub that connects widgets to the local controller / player state / pawn.
 *
 * Input mode and focus are not set here: every screen declares them (UBaseActivatableWidget) and CommonUI applies
 * the configuration of the topmost active screen.
 *
 * Bootstrap: ABaseHUD (or any project HUD) calls InitializeForPlayerController in BeginPlay.
 */
UCLASS()
class COREFRAMEWORK_API UUIFrameSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	/** Finds the subsystem of the local player that owns the widget. Null for widgets without an owning local player. */
	static UUIFrameSubsystem* GetFromWidget(const UUserWidget* Widget);
	static UUIFrameSubsystem* GetFromController(const APlayerController* Controller);

	//~ Begin USubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem

	// ---- Bootstrap -------------------------------------------------------------------------------

	/**
	 * Starts the UI for a local controller: loads the config, creates the root layout, starts the binding hub
	 * and pushes the startup screens in order. Safe to call repeatedly for the same controller.
	 * @param Owner  Object that owns the UI lifetime (the HUD). Only that owner can release it again.
	 */
	void InitializeForPlayerController(APlayerController* Controller, const TArray<FGameplayTag>& StartupScreens, const UObject* Owner);

	/** Tears the UI down if the controller / owner pair matches the active one. */
	void ReleasePlayerController(APlayerController* Controller, const UObject* Owner);

	// ---- Screens ---------------------------------------------------------------------------------

	/** Async push by tag. Fire and forget; listen to OnScreenPushed if the widget is needed. */
	UFUNCTION(BlueprintCallable, Category = "Base UI|Screens")
	void PushScreen(FGameplayTag ScreenTag);

	/**
	 * Async push by tag with a completion callback. If the root layout is not ready yet the request is queued.
	 * If the class is already loaded the push completes synchronously.
	 */
	void PushScreenAsync(FGameplayTag ScreenTag, FUIScreenPushedCallback OnComplete = nullptr);

	/** Closes the open instance of a screen. Returns false when it was not open. */
	UFUNCTION(BlueprintCallable, Category = "Base UI|Screens")
	bool PopScreen(FGameplayTag ScreenTag);

	/** Closes one specific screen instance. */
	void PopWidget(UBaseActivatableWidget* Widget);

	/** Closes the topmost screen of a layer. */
	UFUNCTION(BlueprintCallable, Category = "Base UI|Screens")
	void PopTopOfLayer(FGameplayTag LayerTag);

	UFUNCTION(BlueprintCallable, Category = "Base UI|Screens")
	void ClearLayer(FGameplayTag LayerTag);

	UFUNCTION(BlueprintPure, Category = "Base UI|Screens")
	bool IsScreenOpen(FGameplayTag ScreenTag) const;

	UFUNCTION(BlueprintPure, Category = "Base UI|Screens")
	UBaseActivatableWidget* GetScreenWidget(FGameplayTag ScreenTag) const;

	/** Loads a screen class in the background and keeps it resident, so the first PushScreen cannot hitch. */
	UFUNCTION(BlueprintCallable, Category = "Base UI|Screens")
	void PreloadScreen(FGameplayTag ScreenTag);

	UPROPERTY(BlueprintAssignable, Category = "Base UI|Screens")
	FOnUIScreenPushed OnScreenPushed;

	// ---- Extension registries (runtime) ---------------------------------------------------------

	/** Add or replace a screen definition. */
	UFUNCTION(BlueprintCallable, Category = "Base UI|Extensions")
	void RegisterScreen(const FUIScreenDefinition& Definition);

	UFUNCTION(BlueprintCallable, Category = "Base UI|Extensions")
	void UnregisterScreen(FGameplayTag ScreenTag);

	/** Add or replace a HUD element. Live HUDs spawn it immediately. */
	UFUNCTION(BlueprintCallable, Category = "Base UI|Extensions")
	void RegisterHUDElement(const FUIHUDElementDescriptor& Descriptor);

	UFUNCTION(BlueprintCallable, Category = "Base UI|Extensions")
	void UnregisterHUDElement(FName ElementId);

	UFUNCTION(BlueprintPure, Category = "Base UI|Extensions")
	bool IsHUDElementRegistered(FName ElementId) const;

	TArray<FUIHUDElementDescriptor> GetHUDElements() const;

	/** Add or replace a tab of the menu identified by MenuTag. Open menus rebuild their tabs. */
	UFUNCTION(BlueprintCallable, Category = "Base UI|Extensions")
	void RegisterMenuTab(FGameplayTag MenuTag, const FUITabDescriptor& Tab);

	UFUNCTION(BlueprintCallable, Category = "Base UI|Extensions")
	void UnregisterMenuTab(FGameplayTag MenuTag, FName TabId);

	TArray<FUITabDescriptor> GetMenuTabs(FGameplayTag MenuTag) const;

	FOnUIHUDElementRegistered OnHUDElementRegistered;
	FOnUIHUDElementUnregistered OnHUDElementUnregistered;
	FOnUIMenuTabsChanged OnMenuTabsChanged;

	// ---- Binding ---------------------------------------------------------------------------------

	/** Registers an object implementing IBaseUIBinderInterface. Done automatically by the base widgets. */
	void RegisterBinder(UObject* Binder);
	void UnregisterBinder(UObject* Binder);

	UFUNCTION(BlueprintPure, Category = "Base UI|Binding")
	APlayerController* GetBoundController() const;

	UFUNCTION(BlueprintPure, Category = "Base UI|Binding")
	APawn* GetBoundPawn() const;

	UFUNCTION(BlueprintPure, Category = "Base UI|Binding")
	APlayerState* GetBoundPlayerState() const;

	// ---- Input -----------------------------------------------------------------------------------

	/** True while the last used input device of this local player is a gamepad. */
	UFUNCTION(BlueprintPure, Category = "Base UI|Input")
	bool IsUsingGamepad() const;

private:
	struct FPendingPush
	{
		TArray<FUIScreenPushedCallback> Callbacks;
	};

	struct FDeferredPush
	{
		FGameplayTag ScreenTag;
		FUIScreenPushedCallback Callback;
	};

	void EnsureConfigLoaded();
	void BeginRootLoad(APlayerController* Controller);
	void OnRootClassLoaded(APlayerController* Controller);
	void FlushDeferredPushes();
	void TeardownRoot();

	UBaseActivatableWidget* FinishPush(const FUIScreenDefinition& Definition);

	UPROPERTY(Transient)
	TObjectPtr<UUIBindingHub> BindingHub;

	UPROPERTY(Transient)
	TObjectPtr<UBaseRootLayoutWidget> RootLayout;

	UPROPERTY(Transient)
	TObjectPtr<UUIConfigDataAsset> Config;

	TWeakObjectPtr<APlayerController> ActiveController;
	TWeakObjectPtr<const UObject> RootOwner;

	TMap<FGameplayTag, FUIScreenDefinition> ScreenRegistry;
	TMap<FName, FUIHUDElementDescriptor> HUDElementRegistry;
	TMap<FGameplayTag, TArray<FUITabDescriptor>> MenuTabRegistry;

	TMap<FGameplayTag, TWeakObjectPtr<UBaseActivatableWidget>> OpenScreens;
	TMap<FGameplayTag, FPendingPush> PendingPushes;
	TMap<FGameplayTag, TSharedPtr<FStreamableHandle>> ResidentHandles;
	TArray<FDeferredPush> DeferredPushes;
	TSharedPtr<FStreamableHandle> RootClassHandle;

	/** Incremented on teardown so late async completions of an old session are ignored. */
	uint32 SessionId = 0;
	bool bRootLoading = false;
};
