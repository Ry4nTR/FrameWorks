// CoreFramework UI System
#include "UISystem/BaseActivatableWidget.h"

#include "Animation/WidgetAnimation.h"
#include "Components/Widget.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Input/CommonUIInputTypes.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "UISystem/UIFrameSubsystem.h"

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
	ReleaseActiveSideEffects();
	bScreenOpen = false;

	Super::NativeDestruct();
}

void UBaseActivatableWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	ApplyActiveSideEffects();

	if (Anim_Activate)
	{
		PlayAnimation(Anim_Activate);
	}

	ScheduleDefaultFocus();
}

void UBaseActivatableWidget::NativeOnDeactivated()
{
	ReleaseActiveSideEffects();
	bScreenOpen = false;

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

void UBaseActivatableWidget::BindUIAction(const FDataTableRowHandle& ActionRow, bool bShowInActionBar, FBaseActivatableActionDelegate OnExecuted)
{
	BindUIActionNative(ActionRow, bShowInActionBar, FSimpleDelegate::CreateWeakLambda(this, [OnExecuted]()
	{
		OnExecuted.ExecuteIfBound();
	}));
}
