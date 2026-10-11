// CoreFramework UI System
#include "UISystem/BaseUserWidget.h"

#include "Animation/WidgetAnimation.h"
#include "GameFramework/PlayerController.h"
#include "Input/CommonUIInputTypes.h"
#include "UISystem/UIFrameSubsystem.h"

UBaseUserWidget::UBaseUserWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UBaseUserWidget::NativeConstruct()
{
	// Super first: the Blueprint "Event Construct" runs inside it, so the widget is fully set up
	// before the hub replays the current controller / player state / pawn to it.
	Super::NativeConstruct();

	if (bAutoRegisterAsBinder)
	{
		if (UUIFrameSubsystem* Subsystem = GetUIFrameSubsystem())
		{
			Subsystem->RegisterBinder(this);
		}
	}
}

void UBaseUserWidget::NativeDestruct()
{
	// Unregistering dispatches the matching Unbound calls, so subscriptions are dropped before teardown.
	if (UUIFrameSubsystem* Subsystem = GetUIFrameSubsystem())
	{
		Subsystem->UnregisterBinder(this);
	}

	Super::NativeDestruct();
}

// ---------------------------------------------------------------------------------------------------------------------
// Show / Hide
// ---------------------------------------------------------------------------------------------------------------------

void UBaseUserWidget::ShowWidget(bool bAnimate)
{
	if (Anim_Hide)
	{
		UnbindAllFromAnimationFinished(Anim_Hide);
		if (IsAnimationPlaying(Anim_Hide))
		{
			StopAnimation(Anim_Hide);
		}
	}

	bIsShown = true;
	SetVisibility(ShownVisibility);

	if (bAnimate && Anim_Show)
	{
		PlayAnimation(Anim_Show);
	}

	BP_OnWidgetShown();
}

void UBaseUserWidget::HideWidget(bool bAnimate)
{
	bIsShown = false;

	if (bAnimate && Anim_Hide)
	{
		if (Anim_Show && IsAnimationPlaying(Anim_Show))
		{
			StopAnimation(Anim_Show);
		}

		UnbindAllFromAnimationFinished(Anim_Hide);
		FWidgetAnimationDynamicEvent FinishedEvent;
		FinishedEvent.BindDynamic(this, &UBaseUserWidget::HandleHideAnimationFinished);
		BindToAnimationFinished(Anim_Hide, FinishedEvent);
		PlayAnimation(Anim_Hide);
		return;
	}

	FinalizeHide();
}

void UBaseUserWidget::HandleHideAnimationFinished()
{
	// A Show call during the animation flips bIsShown back; do not collapse in that case.
	if (!bIsShown)
	{
		FinalizeHide();
	}
}

void UBaseUserWidget::FinalizeHide()
{
	SetVisibility(HiddenVisibility);
	BP_OnWidgetHidden();
}

// ---------------------------------------------------------------------------------------------------------------------
// Access
// ---------------------------------------------------------------------------------------------------------------------

UUIFrameSubsystem* UBaseUserWidget::GetUIFrameSubsystem() const
{
	return UUIFrameSubsystem::GetFromWidget(this);
}

APlayerController* UBaseUserWidget::GetBoundController() const
{
	const UUIFrameSubsystem* Subsystem = GetUIFrameSubsystem();
	return Subsystem ? Subsystem->GetBoundController() : nullptr;
}

APawn* UBaseUserWidget::GetBoundPawn() const
{
	const UUIFrameSubsystem* Subsystem = GetUIFrameSubsystem();
	return Subsystem ? Subsystem->GetBoundPawn() : nullptr;
}

APlayerState* UBaseUserWidget::GetBoundPlayerState() const
{
	const UUIFrameSubsystem* Subsystem = GetUIFrameSubsystem();
	return Subsystem ? Subsystem->GetBoundPlayerState() : nullptr;
}

bool UBaseUserWidget::IsUsingGamepad() const
{
	const UUIFrameSubsystem* Subsystem = GetUIFrameSubsystem();
	return Subsystem && Subsystem->IsUsingGamepad();
}

// ---------------------------------------------------------------------------------------------------------------------
// Input helpers
// ---------------------------------------------------------------------------------------------------------------------

FUIActionBindingHandle UBaseUserWidget::BindUIActionNative(const FDataTableRowHandle& ActionRow, bool bShowInActionBar, const FSimpleDelegate& OnExecuted)
{
	FBindUIActionArgs BindArgs(ActionRow, bShowInActionBar, OnExecuted);
	return RegisterUIActionBinding(BindArgs);
}

void UBaseUserWidget::BindUIAction(const FDataTableRowHandle& ActionRow, bool bShowInActionBar, FBaseUIActionDelegate OnExecuted)
{
	BindUIActionNative(ActionRow, bShowInActionBar, FSimpleDelegate::CreateWeakLambda(this, [OnExecuted]()
	{
		OnExecuted.ExecuteIfBound();
	}));
}
