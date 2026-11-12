// CoreFramework UI System
#include "UISystem/Foundation/BaseActivatableWidget.h"

#include "Animation/WidgetAnimation.h"
#include "Components/Widget.h"
#include "Widgets/CommonActivatableWidgetContainer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Input/CommonUIInputTypes.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "UISystem/Manager/UIFrameSubsystem.h"
#include "AudioSystem/Core/AudioTypes.h"
#include "AudioSystem/Manager/AudioFrameSubsystem.h"

UBaseActivatableWidget::UBaseActivatableWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

TOptional<FUIInputConfig> UBaseActivatableWidget::GetDesiredInputConfig() const
{
	switch (InputMode)
	{
	case EUIInputMode::GameOnly:
		return FUIInputConfig(ECommonInputMode::Game, GameMouseCaptureMode);
	case EUIInputMode::UIOnly:
		return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
	case EUIInputMode::GameAndUI:
		return FUIInputConfig(ECommonInputMode::All, GameMouseCaptureMode);
	case EUIInputMode::Default:
	default:
		return TOptional<FUIInputConfig>();
	}
}

void UBaseActivatableWidget::SetScreenContext(FGameplayTag InScreenTag, FGameplayTag InLayerTag)
{
	ScreenTag = InScreenTag;
	LayerTag = InLayerTag;
	bScreenOpen = true;

	// Containers pool their widgets: a reused instance must start clean.
	bCovered = false;
	bOpenCuePlayed = false;
}

bool UBaseActivatableWidget::IsBelowAnotherScreenOnLayer() const
{
	const UUIFrameSubsystem* Subsystem = GetUIFrameSubsystem();
	const UCommonActivatableWidgetContainerBase* Layer = Subsystem ? Subsystem->GetLayer(LayerTag) : nullptr;
	if (!Layer)
	{
		return false;
	}

	// A stack adds the new widget to its list BEFORE it deactivates the one below, and removes a closing widget only
	// after its deactivation. So while deactivating: a widget above us = we are being covered; none = we are closing.
	const TArray<UCommonActivatableWidget*>& Widgets = Layer->GetWidgetList();
	const int32 Index = Widgets.IndexOfByKey(this);
	return Index != INDEX_NONE && Index < Widgets.Num() - 1;
}

void UBaseActivatableWidget::CloseScreen()
{
	if (UUIFrameSubsystem* Subsystem = GetUIFrameSubsystem())
	{
		Subsystem->PopWidget(this);
	}
	else
	{
		DeactivateWidget();
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------------------------------------------------

void UBaseActivatableWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (bAutoRegisterAsBinder)
	{
		if (UUIFrameSubsystem* Subsystem = GetUIFrameSubsystem())
		{
			Subsystem->RegisterBinder(this);
		}
	}
}

void UBaseActivatableWidget::NativeDestruct()
{
	if (UUIFrameSubsystem* Subsystem = GetUIFrameSubsystem())
	{
		Subsystem->UnregisterBinder(this);
	}

	// Safety net: the engine deactivates before destruct, but input suppression and pause are
	// reference counted on the controller, so make sure they can never leak.
	// Also the end of a screen that was closed while covered (ClearLayer, PopScreen of a lower screen).
	ReleaseActiveSideEffects();
	bScreenOpen = false;
	bCovered = false;

	Super::NativeDestruct();
}

void UBaseActivatableWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	// The open sound plays once per push. CommonUI also re-activates a screen that was covered by another one on the
	// same stack; that must not play the open sound again.
	if (bScreenOpen && !bOpenCuePlayed && (ActivatedCue.IsValid() || bUseDefaultScreenCues))
	{
		if (UAudioFrameSubsystem* Audio = UAudioFrameSubsystem::Get(this))
		{
			Audio->PlayUIEvent(EAudioUIEvent::ScreenOpen, ActivatedCue);
		}
	}
	bOpenCuePlayed = true;
	bCovered = false;

	// Idempotent: effects kept while covered are not applied twice.
	ApplyActiveSideEffects();

	if (Anim_Activate)
	{
		PlayAnimation(Anim_Activate);
	}

	ScheduleDefaultFocus();
}

void UBaseActivatableWidget::NativeOnDeactivated()
{
	if (bScreenOpen && IsBelowAnotherScreenOnLayer())
	{
		// Covered by a newer screen of the same stack: still open. Pause and suppression stay unless the class opts out.
		bCovered = true;
		if (!bKeepActiveEffectsWhileCovered)
		{
			ReleaseActiveSideEffects();
		}
	}
	else
	{
		// Really closing.
		ReleaseActiveSideEffects();

		if (bScreenOpen && (DeactivatedCue.IsValid() || bUseDefaultScreenCues))
		{
			if (UAudioFrameSubsystem* Audio = UAudioFrameSubsystem::Get(this))
			{
				Audio->PlayUIEvent(EAudioUIEvent::ScreenClose, DeactivatedCue);
			}
		}
		bScreenOpen = false;
		bCovered = false;
	}

	if (Anim_Deactivate)
	{
		PlayAnimation(Anim_Deactivate);
	}

	Super::NativeOnDeactivated();
}

void UBaseActivatableWidget::ApplyActiveSideEffects()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		SuppressedController = PC;

		// Ignore flags are counters on the controller: apply and release exactly once per activation.
		if (bIgnoreLookInputWhileActive && !bLookSuppressed)
		{
			PC->SetIgnoreLookInput(true);
			bLookSuppressed = true;
		}
		if (bIgnoreMoveInputWhileActive && !bMoveSuppressed)
		{
			PC->SetIgnoreMoveInput(true);
			bMoveSuppressed = true;
		}
	}

	if (bPauseGameWhileActive && !bPausedByThisWidget)
	{
		UGameplayStatics::SetGamePaused(this, true);
		bPausedByThisWidget = true;
	}
}

void UBaseActivatableWidget::ReleaseActiveSideEffects()
{
	if (APlayerController* PC = SuppressedController.Get())
	{
		if (bLookSuppressed)
		{
			PC->SetIgnoreLookInput(false);
		}
		if (bMoveSuppressed)
		{
			PC->SetIgnoreMoveInput(false);
		}
	}
	bLookSuppressed = false;
	bMoveSuppressed = false;
	SuppressedController.Reset();

	if (bPausedByThisWidget)
	{
		bPausedByThisWidget = false;
		UGameplayStatics::SetGamePaused(this, false);
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Focus
// ---------------------------------------------------------------------------------------------------------------------

UWidget* UBaseActivatableWidget::ResolveDefaultFocusWidget() const
{
	return DefaultFocusWidget;
}

void UBaseActivatableWidget::ScheduleDefaultFocus()
{
	// Deferred one tick: CommonUI applies the widget's own desired focus target during activation.
	// This fallback only acts when nothing inside the screen has focus afterwards.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			ApplyDefaultFocus();
		}));
	}
}

void UBaseActivatableWidget::ApplyDefaultFocus()
{
	if (!IsActivated() || HasFocusedDescendants())
	{
		return;
	}

	if (UWidget* Target = ResolveDefaultFocusWidget())
	{
		Target->SetFocus();
	}
}

void UBaseActivatableWidget::RequestDefaultFocus()
{
	if (UWidget* Target = ResolveDefaultFocusWidget())
	{
		Target->SetFocus();
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Access / input helpers
// ---------------------------------------------------------------------------------------------------------------------

UUIFrameSubsystem* UBaseActivatableWidget::GetUIFrameSubsystem() const
{
	return UUIFrameSubsystem::GetFromWidget(this);
}

APlayerController* UBaseActivatableWidget::GetBoundController() const
{
	const UUIFrameSubsystem* Subsystem = GetUIFrameSubsystem();
	return Subsystem ? Subsystem->GetBoundController() : nullptr;
}

APawn* UBaseActivatableWidget::GetBoundPawn() const
{
	const UUIFrameSubsystem* Subsystem = GetUIFrameSubsystem();
	return Subsystem ? Subsystem->GetBoundPawn() : nullptr;
}

APlayerState* UBaseActivatableWidget::GetBoundPlayerState() const
{
	const UUIFrameSubsystem* Subsystem = GetUIFrameSubsystem();
	return Subsystem ? Subsystem->GetBoundPlayerState() : nullptr;
}

bool UBaseActivatableWidget::IsUsingGamepad() const
{
	const UUIFrameSubsystem* Subsystem = GetUIFrameSubsystem();
	return Subsystem && Subsystem->IsUsingGamepad();
}

FUIActionBindingHandle UBaseActivatableWidget::BindUIActionNative(const FDataTableRowHandle& ActionRow, bool bShowInActionBar, const FSimpleDelegate& OnExecuted)
{
	FBindUIActionArgs BindArgs(ActionRow, bShowInActionBar, OnExecuted);
	return RegisterUIActionBinding(BindArgs);
}

void UBaseActivatableWidget::PlayUISound(FGameplayTag CueTag)
{
	if (UAudioFrameSubsystem* Audio = UAudioFrameSubsystem::Get(this))
	{
		Audio->PlayUICue(CueTag);
	}
}

void UBaseActivatableWidget::BindUIAction(const FDataTableRowHandle& ActionRow, bool bShowInActionBar, FBaseUIActionDelegate OnExecuted)
{
	BindUIActionNative(ActionRow, bShowInActionBar, FSimpleDelegate::CreateWeakLambda(this, [OnExecuted]()
	{
		OnExecuted.ExecuteIfBound();
	}));
}
