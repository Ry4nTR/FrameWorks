// CoreFramework UI System
#include "UISystem/UIFrameSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "CommonInputSubsystem.h"
#include "CommonInputTypeEnum.h"
#include "Engine/AssetManager.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "UISystem/BaseActivatableWidget.h"
#include "UISystem/BaseRootLayoutWidget.h"
#include "UISystem/UIBindingHub.h"
#include "UISystem/UIConfigDataAsset.h"
#include "UISystem/UIFrameSettings.h"
#include "UISystem/UIGameplayTags.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

DEFINE_LOG_CATEGORY(LogCoreUI);

// ---------------------------------------------------------------------------------------------------------------------
// Lookup
// ---------------------------------------------------------------------------------------------------------------------

UUIFrameSubsystem* UUIFrameSubsystem::GetFromWidget(const UUserWidget* Widget)
{
	if (!Widget)
	{
		return nullptr;
	}
	if (ULocalPlayer* LocalPlayer = Widget->GetOwningLocalPlayer())
	{
		return LocalPlayer->GetSubsystem<UUIFrameSubsystem>();
	}
	return nullptr;
}

UUIFrameSubsystem* UUIFrameSubsystem::GetFromController(const APlayerController* Controller)
{
	if (!Controller)
	{
		return nullptr;
	}
	if (ULocalPlayer* LocalPlayer = Controller->GetLocalPlayer())
	{
		return LocalPlayer->GetSubsystem<UUIFrameSubsystem>();
	}
	return nullptr;
}

// ---------------------------------------------------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------------------------------------------------

void UUIFrameSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	BindingHub = NewObject<UUIBindingHub>(this);
}

void UUIFrameSubsystem::Deinitialize()
{
	TeardownRoot();
	ResidentHandles.Reset();
	BindingHub = nullptr;
	Super::Deinitialize();
}

void UUIFrameSubsystem::EnsureConfigLoaded()
{
	if (Config)
	{
		return;
	}

	// The config is a tiny data-only asset (soft references only), so a synchronous load is cheap and happens once.
	Config = UUIFrameSettings::Get()->DefaultUIConfig.LoadSynchronous();
	if (!Config)
	{
		UE_LOG(LogCoreUI, Error, TEXT("No UI config assigned. Set Project Settings > CoreFramework > CoreFramework UI > Default UI Config."));
		return;
	}

	for (const FUIScreenDefinition& Definition : Config->Screens)
	{
		RegisterScreen(Definition);
	}

	for (const FUIHUDElementDescriptor& Descriptor : Config->HUDElements)
	{
		RegisterHUDElement(Descriptor);
	}

	for (const FUIMenuTabSetBinding& Binding : Config->MenuTabSets)
	{
		if (const UUITabSetDataAsset* TabSet = Binding.TabSet.LoadSynchronous())
		{
			for (const FUITabDescriptor& Tab : TabSet->Tabs)
			{
				RegisterMenuTab(Binding.MenuTag, Tab);
			}
		}
	}
}

void UUIFrameSubsystem::InitializeForPlayerController(APlayerController* Controller, const TArray<FGameplayTag>& StartupScreens, const UObject* Owner)
{
	if (!Controller || !Controller->IsLocalController() || Controller->GetLocalPlayer() != GetLocalPlayer())
	{
		return;
	}

	EnsureConfigLoaded();

	if (ActiveController.Get() != Controller)
	{
		TeardownRoot();
		ActiveController = Controller;
		BindingHub->Start(Controller, UUIFrameSettings::Get()->BindingPollIntervalSeconds);
	}
	RootOwner = Owner;

	// Requests are queued until the root layout exists, so the order of StartupScreens is preserved.
	for (const FGameplayTag& ScreenTag : StartupScreens)
	{
		PushScreenAsync(ScreenTag);
	}

	if (!RootLayout && !bRootLoading)
	{
		BeginRootLoad(Controller);
	}
}

void UUIFrameSubsystem::ReleasePlayerController(APlayerController* Controller, const UObject* Owner)
{
	if (ActiveController.Get() == Controller && (!Owner || RootOwner.Get() == Owner))
	{
		TeardownRoot();
	}
}

void UUIFrameSubsystem::BeginRootLoad(APlayerController* Controller)
{
	if (!Config || Config->RootLayoutClass.IsNull())
	{
		UE_LOG(LogCoreUI, Error, TEXT("UI config has no Root Layout Class."));
		return;
	}

	bRootLoading = true;
	const TWeakObjectPtr<APlayerController> WeakController = Controller;
	const uint32 Session = SessionId;

	RootClassHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		Config->RootLayoutClass.ToSoftObjectPath(),
		FStreamableDelegate::CreateWeakLambda(this, [this, WeakController, Session]()
		{
			if (Session == SessionId)
			{
				OnRootClassLoaded(WeakController.Get());
			}
		}));
}

void UUIFrameSubsystem::OnRootClassLoaded(APlayerController* Controller)
{
	bRootLoading = false;

	if (!Controller || ActiveController.Get() != Controller || RootLayout)
	{
		return;
	}

	UClass* RootClass = Config ? Config->RootLayoutClass.Get() : nullptr;
	if (!RootClass)
	{
		UE_LOG(LogCoreUI, Error, TEXT("Root layout class failed to load."));
		return;
	}

	RootLayout = CreateWidget<UBaseRootLayoutWidget>(Controller, TSubclassOf<UBaseRootLayoutWidget>(RootClass));
	if (!RootLayout)
	{
		UE_LOG(LogCoreUI, Error, TEXT("Failed to create the root layout widget."));
		return;
	}

	RootLayout->AddToPlayerScreen(UUIFrameSettings::Get()->RootLayoutZOrder);
	FlushDeferredPushes();
}

void UUIFrameSubsystem::FlushDeferredPushes()
{
	TArray<FDeferredPush> Queued = MoveTemp(DeferredPushes);
	DeferredPushes.Reset();
	for (FDeferredPush& Entry : Queued)
	{
		PushScreenAsync(Entry.ScreenTag, MoveTemp(Entry.Callback));
	}
}

void UUIFrameSubsystem::TeardownRoot()
{
	++SessionId;

	DeferredPushes.Reset();
	PendingPushes.Reset();
	OpenScreens.Reset();

	if (RootClassHandle.IsValid())
	{
		RootClassHandle->CancelHandle();
		RootClassHandle.Reset();
	}
	bRootLoading = false;

	if (RootLayout)
	{
		// Deactivate screens first so input suppression and pause state are released cleanly.
		RootLayout->ClearAllLayers();
		RootLayout->RemoveFromParent();
		RootLayout = nullptr;
	}

	if (BindingHub)
	{
		BindingHub->Stop();
	}

	ActiveController.Reset();
	RootOwner.Reset();
}

// ---------------------------------------------------------------------------------------------------------------------
// Screens
// ---------------------------------------------------------------------------------------------------------------------

void UUIFrameSubsystem::PushScreen(FGameplayTag ScreenTag)
{
	PushScreenAsync(ScreenTag, nullptr);
}

void UUIFrameSubsystem::PushScreenAsync(FGameplayTag ScreenTag, FUIScreenPushedCallback OnComplete)
{
	const FUIScreenDefinition* Found = ScreenRegistry.Find(ScreenTag);
	if (!Found)
	{
		UE_LOG(LogCoreUI, Warning, TEXT("PushScreen: no screen registered for tag '%s'."), *ScreenTag.ToString());
		if (OnComplete)
		{
			OnComplete(nullptr);
		}
		return;
	}

	if (!RootLayout)
	{
		FDeferredPush& Entry = DeferredPushes.AddDefaulted_GetRef();
		Entry.ScreenTag = ScreenTag;
		Entry.Callback = MoveTemp(OnComplete);
		return;
	}

	// Copy: the registry can change while the class streams in.
	const FUIScreenDefinition Definition = *Found;

	if (Definition.WidgetClass.IsNull())
	{
		UE_LOG(LogCoreUI, Warning, TEXT("PushScreen: screen '%s' has no widget class."), *ScreenTag.ToString());
		if (OnComplete)
		{
			OnComplete(nullptr);
		}
		return;
	}

	if (!Definition.bAllowMultipleInstances)
	{
		if (UBaseActivatableWidget* Existing = GetScreenWidget(ScreenTag))
		{
			if (OnComplete)
			{
				OnComplete(Existing);
			}
			return;
		}

		if (FPendingPush* Pending = PendingPushes.Find(ScreenTag))
		{
			// Already streaming: join the request instead of loading twice.
			if (OnComplete)
			{
				Pending->Callbacks.Add(MoveTemp(OnComplete));
			}
			return;
		}
	}

	// Already in memory (preloaded or used before): no reason to wait.
	if (Definition.WidgetClass.Get())
	{
		UBaseActivatableWidget* Widget = FinishPush(Definition);
		if (OnComplete)
		{
			OnComplete(Widget);
		}
		return;
	}

	const bool bTracked = !Definition.bAllowMultipleInstances;
	if (bTracked)
	{
		FPendingPush& Pending = PendingPushes.Add(ScreenTag);
		if (OnComplete)
		{
			Pending.Callbacks.Add(OnComplete);
		}
	}

	const uint32 Session = SessionId;
	UAssetManager::GetStreamableManager().RequestAsyncLoad(
		Definition.WidgetClass.ToSoftObjectPath(),
		FStreamableDelegate::CreateWeakLambda(this, [this, Definition, bTracked, Session, OnComplete]()
		{
			if (Session != SessionId)
			{
				return;
			}

			TArray<FUIScreenPushedCallback> Callbacks;
			if (bTracked)
			{
				FPendingPush Pending;
				if (PendingPushes.RemoveAndCopyValue(Definition.ScreenTag, Pending))
				{
					Callbacks = MoveTemp(Pending.Callbacks);
				}
			}
			else if (OnComplete)
			{
				Callbacks.Add(OnComplete);
			}

			UBaseActivatableWidget* Widget = FinishPush(Definition);
			for (const FUIScreenPushedCallback& Callback : Callbacks)
			{
				if (Callback)
				{
					Callback(Widget);
				}
			}
		}),
		FStreamableManager::DefaultAsyncLoadPriority,
		/*bManageActiveHandle=*/true);
}

UBaseActivatableWidget* UUIFrameSubsystem::FinishPush(const FUIScreenDefinition& Definition)
{
	if (!RootLayout)
	{
		return nullptr;
	}

	UClass* WidgetClass = Definition.WidgetClass.Get();
	if (!WidgetClass)
	{
		UE_LOG(LogCoreUI, Warning, TEXT("PushScreen: widget class of '%s' failed to load."), *Definition.ScreenTag.ToString());
		return nullptr;
	}

	const FGameplayTag LayerTag = Definition.LayerTag.IsValid() ? Definition.LayerTag : FUITags::LayerMenu();
	UCommonActivatableWidgetContainerBase* Layer = RootLayout->FindLayer(LayerTag);
	if (!Layer)
	{
		UE_LOG(LogCoreUI, Warning, TEXT("PushScreen: layer '%s' is not registered in the root layout."), *LayerTag.ToString());
		return nullptr;
	}

	const FGameplayTag ScreenTag = Definition.ScreenTag;
	UBaseActivatableWidget* Widget = Layer->AddWidget<UBaseActivatableWidget>(
		WidgetClass,
		[ScreenTag, LayerTag](UBaseActivatableWidget& NewWidget)
		{
			// Runs before activation, so the widget knows its identity in NativeOnActivated.
			NewWidget.SetScreenContext(ScreenTag, LayerTag);
		});

	if (Widget)
	{
		OpenScreens.Add(ScreenTag, Widget);
		OnScreenPushed.Broadcast(ScreenTag, Widget);
	}
	return Widget;
}

bool UUIFrameSubsystem::PopScreen(FGameplayTag ScreenTag)
{
	if (UBaseActivatableWidget* Widget = GetScreenWidget(ScreenTag))
	{
		PopWidget(Widget);
		return true;
	}
	return false;
}

void UUIFrameSubsystem::PopWidget(UBaseActivatableWidget* Widget)
{
	if (!Widget)
	{
		return;
	}

	UCommonActivatableWidgetContainerBase* Layer = RootLayout ? RootLayout->FindLayer(Widget->GetLayerTag()) : nullptr;
	if (Layer)
	{
		Layer->RemoveWidget(*Widget);
	}
	else
	{
		Widget->DeactivateWidget();
	}
}

void UUIFrameSubsystem::PopTopOfLayer(FGameplayTag LayerTag)
{
	if (!RootLayout)
	{
		return;
	}
	if (UCommonActivatableWidgetContainerBase* Layer = RootLayout->FindLayer(LayerTag))
	{
		if (UCommonActivatableWidget* Active = Layer->GetActiveWidget())
		{
			Layer->RemoveWidget(*Active);
		}
	}
}

void UUIFrameSubsystem::ClearLayer(FGameplayTag LayerTag)
{
	if (!RootLayout)
	{
		return;
	}
	if (UCommonActivatableWidgetContainerBase* Layer = RootLayout->FindLayer(LayerTag))
	{
		Layer->ClearWidgets();
	}
}

bool UUIFrameSubsystem::IsScreenOpen(FGameplayTag ScreenTag) const
{
	return GetScreenWidget(ScreenTag) != nullptr;
}

UBaseActivatableWidget* UUIFrameSubsystem::GetScreenWidget(FGameplayTag ScreenTag) const
{
	if (const TWeakObjectPtr<UBaseActivatableWidget>* Entry = OpenScreens.Find(ScreenTag))
	{
		UBaseActivatableWidget* Widget = Entry->Get();
		if (Widget && Widget->IsScreenOpen())
		{
			return Widget;
		}
	}
	return nullptr;
}

void UUIFrameSubsystem::PreloadScreen(FGameplayTag ScreenTag)
{
	const FUIScreenDefinition* Definition = ScreenRegistry.Find(ScreenTag);
	if (!Definition || Definition->WidgetClass.IsNull() || ResidentHandles.Contains(ScreenTag))
	{
		return;
	}

	// The returned handle is kept: while it lives the class stays in memory.
	TSharedPtr<FStreamableHandle> Handle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		Definition->WidgetClass.ToSoftObjectPath(),
		FStreamableDelegate(),
		FStreamableManager::DefaultAsyncLoadPriority,
		/*bManageActiveHandle=*/false);
	ResidentHandles.Add(ScreenTag, Handle);
}

// ---------------------------------------------------------------------------------------------------------------------
// Registries
// ---------------------------------------------------------------------------------------------------------------------

void UUIFrameSubsystem::RegisterScreen(const FUIScreenDefinition& Definition)
{
	if (!Definition.ScreenTag.IsValid())
	{
		UE_LOG(LogCoreUI, Warning, TEXT("RegisterScreen: definition without a ScreenTag ignored."));
		return;
	}

	ScreenRegistry.Add(Definition.ScreenTag, Definition);
	if (Definition.bPreloadAndKeepResident)
	{
		PreloadScreen(Definition.ScreenTag);
	}
}

void UUIFrameSubsystem::UnregisterScreen(FGameplayTag ScreenTag)
{
	ScreenRegistry.Remove(ScreenTag);
	ResidentHandles.Remove(ScreenTag);
}

void UUIFrameSubsystem::RegisterHUDElement(const FUIHUDElementDescriptor& Descriptor)
{
	if (Descriptor.ElementId.IsNone())
	{
		UE_LOG(LogCoreUI, Warning, TEXT("RegisterHUDElement: descriptor without an ElementId ignored."));
		return;
	}

	// Replacing an element removes the old instance first so the HUD never shows two.
	if (HUDElementRegistry.Contains(Descriptor.ElementId))
	{
		OnHUDElementUnregistered.Broadcast(Descriptor.ElementId);
	}
	HUDElementRegistry.Add(Descriptor.ElementId, Descriptor);
	OnHUDElementRegistered.Broadcast(Descriptor);
}

void UUIFrameSubsystem::UnregisterHUDElement(FName ElementId)
{
	if (HUDElementRegistry.Remove(ElementId) > 0)
	{
		OnHUDElementUnregistered.Broadcast(ElementId);
	}
}

bool UUIFrameSubsystem::IsHUDElementRegistered(FName ElementId) const
{
	return HUDElementRegistry.Contains(ElementId);
}

TArray<FUIHUDElementDescriptor> UUIFrameSubsystem::GetHUDElements() const
{
	TArray<FUIHUDElementDescriptor> Result;
	HUDElementRegistry.GenerateValueArray(Result);
	return Result;
}

void UUIFrameSubsystem::RegisterMenuTab(FGameplayTag MenuTag, const FUITabDescriptor& Tab)
{
	if (!MenuTag.IsValid() || Tab.TabId.IsNone())
	{
		UE_LOG(LogCoreUI, Warning, TEXT("RegisterMenuTab: needs a valid MenuTag and TabId."));
		return;
	}

	TArray<FUITabDescriptor>& Tabs = MenuTabRegistry.FindOrAdd(MenuTag);
	const int32 ExistingIndex = Tabs.IndexOfByPredicate([&Tab](const FUITabDescriptor& Entry) { return Entry.TabId == Tab.TabId; });
	if (ExistingIndex != INDEX_NONE)
	{
		Tabs[ExistingIndex] = Tab;
	}
	else
	{
		Tabs.Add(Tab);
	}
	OnMenuTabsChanged.Broadcast(MenuTag);
}

void UUIFrameSubsystem::UnregisterMenuTab(FGameplayTag MenuTag, FName TabId)
{
	if (TArray<FUITabDescriptor>* Tabs = MenuTabRegistry.Find(MenuTag))
	{
		const int32 Removed = Tabs->RemoveAll([TabId](const FUITabDescriptor& Entry) { return Entry.TabId == TabId; });
		if (Removed > 0)
		{
			OnMenuTabsChanged.Broadcast(MenuTag);
		}
	}
}

TArray<FUITabDescriptor> UUIFrameSubsystem::GetMenuTabs(FGameplayTag MenuTag) const
{
	if (const TArray<FUITabDescriptor>* Tabs = MenuTabRegistry.Find(MenuTag))
	{
		return *Tabs;
	}
	return TArray<FUITabDescriptor>();
}

// ---------------------------------------------------------------------------------------------------------------------
// Binding and input
// ---------------------------------------------------------------------------------------------------------------------

void UUIFrameSubsystem::RegisterBinder(UObject* Binder)
{
	if (BindingHub)
	{
		BindingHub->RegisterBinder(Binder);
	}
}

void UUIFrameSubsystem::UnregisterBinder(UObject* Binder)
{
	if (BindingHub)
	{
		BindingHub->UnregisterBinder(Binder);
	}
}

APlayerController* UUIFrameSubsystem::GetBoundController() const
{
	return BindingHub ? BindingHub->GetController() : nullptr;
}

APawn* UUIFrameSubsystem::GetBoundPawn() const
{
	return BindingHub ? BindingHub->GetPawn() : nullptr;
}

APlayerState* UUIFrameSubsystem::GetBoundPlayerState() const
{
	return BindingHub ? BindingHub->GetPlayerState() : nullptr;
}

bool UUIFrameSubsystem::IsUsingGamepad() const
{
	if (const ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (const UCommonInputSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UCommonInputSubsystem>())
		{
			return InputSubsystem->GetCurrentInputType() == ECommonInputType::Gamepad;
		}
	}
	return false;
}
