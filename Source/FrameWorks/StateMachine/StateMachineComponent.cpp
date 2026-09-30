// StateMachineComponent.cpp
// State Machine Framework - Gameplay Tag driven state store with change broadcasting.

#include "Frameworks/StateMachine/StateMachineComponent.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

// ----------------------------------------------------------------------
// Native tag definitions
// ----------------------------------------------------------------------

UE_DEFINE_GAMEPLAY_TAG(TAG_Frameworks_Domain_Ground, "Frameworks.Domain.Ground");
UE_DEFINE_GAMEPLAY_TAG(TAG_Frameworks_Domain_Fly, "Frameworks.Domain.Fly");
UE_DEFINE_GAMEPLAY_TAG(TAG_Frameworks_Domain_Water, "Frameworks.Domain.Water");

UE_DEFINE_GAMEPLAY_TAG(TAG_Frameworks_State_Ground, "Frameworks.State.Ground");
UE_DEFINE_GAMEPLAY_TAG(TAG_Frameworks_State_Fly, "Frameworks.State.Fly");
UE_DEFINE_GAMEPLAY_TAG(TAG_Frameworks_State_Water, "Frameworks.State.Water");

UE_DEFINE_GAMEPLAY_TAG(TAG_Frameworks_State_Ground_Idle, "Frameworks.State.Ground.Idle");
UE_DEFINE_GAMEPLAY_TAG(TAG_Frameworks_State_Ground_Walk, "Frameworks.State.Ground.Walk");
UE_DEFINE_GAMEPLAY_TAG(TAG_Frameworks_State_Ground_Run, "Frameworks.State.Ground.Run");
UE_DEFINE_GAMEPLAY_TAG(TAG_Frameworks_State_Ground_Jump, "Frameworks.State.Ground.Jump");
UE_DEFINE_GAMEPLAY_TAG(TAG_Frameworks_State_Ground_Slide, "Frameworks.State.Ground.Slide");
UE_DEFINE_GAMEPLAY_TAG(TAG_Frameworks_State_Ground_Crouch, "Frameworks.State.Ground.Crouch");
UE_DEFINE_GAMEPLAY_TAG(TAG_Frameworks_State_Ground_Dash, "Frameworks.State.Ground.Dash");
UE_DEFINE_GAMEPLAY_TAG(TAG_Frameworks_State_Ground_Roll, "Frameworks.State.Ground.Roll");
UE_DEFINE_GAMEPLAY_TAG(TAG_Frameworks_State_Ground_Mantle, "Frameworks.State.Ground.Mantle");

UE_DEFINE_GAMEPLAY_TAG(TAG_Frameworks_State_Fly_Idle, "Frameworks.State.Fly.Idle");
UE_DEFINE_GAMEPLAY_TAG(TAG_Frameworks_State_Fly_Move, "Frameworks.State.Fly.Move");
UE_DEFINE_GAMEPLAY_TAG(TAG_Frameworks_State_Fly_Glide, "Frameworks.State.Fly.Glide");
UE_DEFINE_GAMEPLAY_TAG(TAG_Frameworks_State_Fly_Dash, "Frameworks.State.Fly.Dash");
UE_DEFINE_GAMEPLAY_TAG(TAG_Frameworks_State_Fly_Dodge, "Frameworks.State.Fly.Dodge");

UE_DEFINE_GAMEPLAY_TAG(TAG_Frameworks_State_Water_Surface_Idle, "Frameworks.State.Water.Surface.Idle");
UE_DEFINE_GAMEPLAY_TAG(TAG_Frameworks_State_Water_Surface_Move, "Frameworks.State.Water.Surface.Move");
UE_DEFINE_GAMEPLAY_TAG(TAG_Frameworks_State_Water_Surface_FastSwim, "Frameworks.State.Water.Surface.FastSwim");
UE_DEFINE_GAMEPLAY_TAG(TAG_Frameworks_State_Water_Transition_ToSurface, "Frameworks.State.Water.Transition.ToSurface");
UE_DEFINE_GAMEPLAY_TAG(TAG_Frameworks_State_Water_Transition_ToUnderwater, "Frameworks.State.Water.Transition.ToUnderwater");
UE_DEFINE_GAMEPLAY_TAG(TAG_Frameworks_State_Water_Underwater_Idle, "Frameworks.State.Water.Underwater.Idle");
UE_DEFINE_GAMEPLAY_TAG(TAG_Frameworks_State_Water_Underwater_Move, "Frameworks.State.Water.Underwater.Move");
UE_DEFINE_GAMEPLAY_TAG(TAG_Frameworks_State_Water_Underwater_FastSwim, "Frameworks.State.Water.Underwater.FastSwim");
UE_DEFINE_GAMEPLAY_TAG(TAG_Frameworks_State_Water_Underwater_Dash, "Frameworks.State.Water.Underwater.Dash");
UE_DEFINE_GAMEPLAY_TAG(TAG_Frameworks_State_Water_Resurfacing, "Frameworks.State.Water.Resurfacing");

// ----------------------------------------------------------------------
// Lifecycle
// ----------------------------------------------------------------------

UStateMachineComponent::UStateMachineComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bWantsInitializeComponent = true;
	SetIsReplicatedByDefault(true);
}

void UStateMachineComponent::InitializeComponent()
{
	Super::InitializeComponent();

	if (!ActiveDomain.IsValid())
	{
		ActiveDomain = PickFirstEnabledDomain();
		LocomotionState = GetDefaultStateForDomain(ActiveDomain);
	}

	RebuildActiveTags();
	LastNotifiedState = GetActiveState();
}

void UStateMachineComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ActionTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void UStateMachineComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// The owning client computes its own (predicted) values, so only other machines receive these.
	DOREPLIFETIME_CONDITION(UStateMachineComponent, ActiveDomain, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(UStateMachineComponent, LocomotionState, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(UStateMachineComponent, ActionReplication, COND_SkipOwner);
}

// ----------------------------------------------------------------------
// Commands
// ----------------------------------------------------------------------

void UStateMachineComponent::ConfigureDomains(bool bGround, bool bFly, bool bWater)
{
	bEnableGroundDomain = bGround;
	bEnableFlyDomain = bFly;
	bEnableWaterDomain = bWater;

	if (!IsDomainEnabled(ActiveDomain))
	{
		SetDomain(PickFirstEnabledDomain());
	}
}

bool UStateMachineComponent::SetDomain(FGameplayTag NewDomain)
{
	if (!NewDomain.IsValid() || !IsDomainEnabled(NewDomain))
	{
		return false;
	}

	if (NewDomain == ActiveDomain)
	{
		return true;
	}

	const FGameplayTag OldDomain = ActiveDomain;
	ActiveDomain = NewDomain;
	LocomotionState = GetDefaultStateForDomain(NewDomain);

	// An action from the previous domain cannot continue.
	if (ActionState.IsValid() && bValidateDomainMembership && !BelongsToDomain(ActionState, NewDomain))
	{
		EndAction();
	}

	RebuildActiveTags();
	OnDomainChanged.Broadcast(OldDomain, NewDomain);
	NotifyIfStateChanged();
	return true;
}

bool UStateMachineComponent::SetLocomotionState(FGameplayTag NewState)
{
	if (!NewState.IsValid())
	{
		return false;
	}

	if (NewState == LocomotionState)
	{
		return true;
	}

	if (bValidateDomainMembership && !BelongsToDomain(NewState, ActiveDomain))
	{
		return false;
	}

	LocomotionState = NewState;
	RebuildActiveTags();
	NotifyIfStateChanged();
	return true;
}

bool UStateMachineComponent::BeginAction(FGameplayTag ActionTag, float Duration)
{
	if (!ActionTag.IsValid())
	{
		return false;
	}

	if (bValidateDomainMembership && !BelongsToDomain(ActionTag, ActiveDomain))
	{
		return false;
	}

	if (ActionState.IsValid() && !bAllowActionInterrupt)
	{
		return false;
	}

	const float ResolvedDuration = (Duration >= 0.0f) ? Duration : ResolveActionDuration(ActionTag);

	if (HasOwnerAuthority())
	{
		ActionReplication.Tag = ActionTag;
		ActionReplication.Duration = ResolvedDuration;
		++ActionReplication.Sequence;
	}

	ApplyActionLocal(ActionTag, ResolvedDuration, /*bStartTimer=*/true);
	return true;
}

void UStateMachineComponent::EndAction()
{
	if (!ActionState.IsValid())
	{
		return;
	}

	if (HasOwnerAuthority())
	{
		ActionReplication.Tag = FGameplayTag();
		ActionReplication.Duration = 0.0f;
		++ActionReplication.Sequence;
	}

	ClearActionLocal();
}

// ----------------------------------------------------------------------
// Queries
// ----------------------------------------------------------------------

bool UStateMachineComponent::IsInState(FGameplayTag StateTag, bool bExact) const
{
	return bExact ? ActiveTags.HasTagExact(StateTag) : ActiveTags.HasTag(StateTag);
}

bool UStateMachineComponent::IsDomainEnabled(FGameplayTag DomainTag) const
{
	if (DomainTag == TAG_Frameworks_Domain_Ground.GetTag())
	{
		return bEnableGroundDomain;
	}
	if (DomainTag == TAG_Frameworks_Domain_Fly.GetTag())
	{
		return bEnableFlyDomain;
	}
	if (DomainTag == TAG_Frameworks_Domain_Water.GetTag())
	{
		return bEnableWaterDomain;
	}
	return false;
}

bool UStateMachineComponent::BelongsToDomain(FGameplayTag StateTag, FGameplayTag DomainTag) const
{
	const FGameplayTag Root = GetDomainStateRoot(DomainTag);
	return Root.IsValid() && StateTag.IsValid() && StateTag.MatchesTag(Root);
}

FGameplayTag UStateMachineComponent::GetDomainStateRoot(FGameplayTag DomainTag)
{
	if (DomainTag == TAG_Frameworks_Domain_Ground.GetTag())
	{
		return TAG_Frameworks_State_Ground.GetTag();
	}
	if (DomainTag == TAG_Frameworks_Domain_Fly.GetTag())
	{
		return TAG_Frameworks_State_Fly.GetTag();
	}
	if (DomainTag == TAG_Frameworks_Domain_Water.GetTag())
	{
		return TAG_Frameworks_State_Water.GetTag();
	}
	return FGameplayTag();
}

FGameplayTag UStateMachineComponent::GetDefaultStateForDomain(FGameplayTag DomainTag)
{
	if (DomainTag == TAG_Frameworks_Domain_Ground.GetTag())
	{
		return TAG_Frameworks_State_Ground_Idle.GetTag();
	}
	if (DomainTag == TAG_Frameworks_Domain_Fly.GetTag())
	{
		return TAG_Frameworks_State_Fly_Idle.GetTag();
	}
	if (DomainTag == TAG_Frameworks_Domain_Water.GetTag())
	{
		return TAG_Frameworks_State_Water_Surface_Idle.GetTag();
	}
	return FGameplayTag();
}

// ----------------------------------------------------------------------
// RepNotifies (simulated proxies)
// ----------------------------------------------------------------------

void UStateMachineComponent::OnRep_ActiveDomain(FGameplayTag OldDomain)
{
	RebuildActiveTags();
	OnDomainChanged.Broadcast(OldDomain, ActiveDomain);
	NotifyIfStateChanged();
}

void UStateMachineComponent::OnRep_LocomotionState(FGameplayTag OldState)
{
	RebuildActiveTags();
	NotifyIfStateChanged();
}

void UStateMachineComponent::OnRep_Action()
{
	// The server owns the action lifetime, so proxies do not run a timer.
	if (ActionReplication.Tag.IsValid())
	{
		ApplyActionLocal(ActionReplication.Tag, ActionReplication.Duration, /*bStartTimer=*/false);
	}
	else
	{
		ClearActionLocal();
	}
}

// ----------------------------------------------------------------------
// Internals
// ----------------------------------------------------------------------

bool UStateMachineComponent::HasOwnerAuthority() const
{
	const AActor* Owner = GetOwner();
	return Owner && Owner->HasAuthority();
}

FGameplayTag UStateMachineComponent::PickFirstEnabledDomain() const
{
	if (bEnableGroundDomain)
	{
		return TAG_Frameworks_Domain_Ground.GetTag();
	}
	if (bEnableFlyDomain)
	{
		return TAG_Frameworks_Domain_Fly.GetTag();
	}
	if (bEnableWaterDomain)
	{
		return TAG_Frameworks_Domain_Water.GetTag();
	}
	return TAG_Frameworks_Domain_Ground.GetTag();
}

float UStateMachineComponent::ResolveActionDuration(FGameplayTag ActionTag) const
{
	if (const float* Override = ActionDurationOverrides.Find(ActionTag))
	{
		return *Override;
	}
	return DefaultActionDuration;
}

void UStateMachineComponent::RebuildActiveTags()
{
	ActiveTags.Reset();

	if (ActiveDomain.IsValid())
	{
		ActiveTags.AddTag(ActiveDomain);
	}
	if (LocomotionState.IsValid())
	{
		ActiveTags.AddTag(LocomotionState);
	}
	if (ActionState.IsValid())
	{
		ActiveTags.AddTag(ActionState);
	}
}

void UStateMachineComponent::NotifyIfStateChanged()
{
	const FGameplayTag Current = GetActiveState();
	if (Current != LastNotifiedState)
	{
		const FGameplayTag Previous = LastNotifiedState;
		LastNotifiedState = Current;
		OnStateChanged.Broadcast(Previous, Current);
	}
}

void UStateMachineComponent::ApplyActionLocal(FGameplayTag ActionTag, float Duration, bool bStartTimer)
{
	ActionState = ActionTag;
	RebuildActiveTags();

	if (bStartTimer)
	{
		if (const UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(ActionTimerHandle, this, &UStateMachineComponent::HandleActionTimerElapsed,
				FMath::Max(Duration, UE_KINDA_SMALL_NUMBER), false);
		}
	}

	OnActionStarted.Broadcast(ActionTag, Duration);
	NotifyIfStateChanged();
}

void UStateMachineComponent::ClearActionLocal()
{
	if (!ActionState.IsValid())
	{
		return;
	}

	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ActionTimerHandle);
	}

	const FGameplayTag Ended = ActionState;
	ActionState = FGameplayTag();
	RebuildActiveTags();

	OnActionEnded.Broadcast(Ended);
	NotifyIfStateChanged();
}

void UStateMachineComponent::HandleActionTimerElapsed()
{
	EndAction();
}
