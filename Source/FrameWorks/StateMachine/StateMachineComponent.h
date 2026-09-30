// StateMachineComponent.h
// State Machine Framework - Gameplay Tag driven state store with change broadcasting.
//
// ROLE
//   Passive, replicated store for three layers of state, all expressed as native Gameplay Tags:
//     Domain      Frameworks.Domain.Ground | Fly | Water
//     Locomotion  one leaf under Frameworks.State.<Domain>   (Idle, Run, Fly.Glide, Water.Surface.Move ...)
//     Action      optional timed overlay (Dash, Roll, Mantle, Water.Transition.ToUnderwater ...)
//   The effective state (GetActiveState) is the Action when one is running, otherwise the Locomotion state.
//
//   The component never inspects movement. A driver (ABaseCharacter, or your own code) pushes states in.
//   That keeps it independent: it only depends on the GameplayTags module.
//
// NETWORK MODEL
//   Owner and server compute states locally. The server replicates to simulated proxies only (COND_SkipOwner),
//   so the owning client keeps its predicted values. Simulated proxies mirror the server through RepNotifies.
//
// PORTING: copy this folder, keep FRAMEWORKS_API (or rename it to your module macro).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "StateMachineComponent.generated.h"

// ----------------------------------------------------------------------
// Native tag vocabulary (defined in StateMachineComponent.cpp)
// ----------------------------------------------------------------------

// Domains
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Frameworks_Domain_Ground);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Frameworks_Domain_Fly);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Frameworks_Domain_Water);

// State roots (one per domain)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Frameworks_State_Ground);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Frameworks_State_Fly);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Frameworks_State_Water);

// Ground
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Frameworks_State_Ground_Idle);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Frameworks_State_Ground_Walk);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Frameworks_State_Ground_Run);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Frameworks_State_Ground_Jump);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Frameworks_State_Ground_Slide);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Frameworks_State_Ground_Crouch);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Frameworks_State_Ground_Dash);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Frameworks_State_Ground_Roll);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Frameworks_State_Ground_Mantle);

// Fly
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Frameworks_State_Fly_Idle);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Frameworks_State_Fly_Move);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Frameworks_State_Fly_Glide);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Frameworks_State_Fly_Dash);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Frameworks_State_Fly_Dodge);

// Water - surface
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Frameworks_State_Water_Surface_Idle);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Frameworks_State_Water_Surface_Move);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Frameworks_State_Water_Surface_FastSwim);
// Water - transitions between surface and underwater
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Frameworks_State_Water_Transition_ToSurface);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Frameworks_State_Water_Transition_ToUnderwater);
// Water - underwater
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Frameworks_State_Water_Underwater_Idle);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Frameworks_State_Water_Underwater_Move);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Frameworks_State_Water_Underwater_FastSwim);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Frameworks_State_Water_Underwater_Dash);
// Water - rising back to the surface
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Frameworks_State_Water_Resurfacing);

// ----------------------------------------------------------------------
// Delegates
// ----------------------------------------------------------------------

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FStateMachineDomainChangedSignature, FGameplayTag, OldDomain, FGameplayTag, NewDomain);
/** Fires when the effective state (action if running, else locomotion) changes. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FStateMachineStateChangedSignature, FGameplayTag, OldState, FGameplayTag, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FStateMachineActionStartedSignature, FGameplayTag, ActionTag, float, Duration);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FStateMachineActionEndedSignature, FGameplayTag, ActionTag);

/** Replicated description of the running action. Sequence changes on every start and end so repeats are never lost. */
USTRUCT()
struct FStateActionReplication
{
	GENERATED_BODY()

	UPROPERTY()
	FGameplayTag Tag;

	UPROPERTY()
	float Duration = 0.0f;

	UPROPERTY()
	uint8 Sequence = 0;
};

UCLASS(ClassGroup = (Frameworks), meta = (BlueprintSpawnableComponent))
class FRAMEWORKS_API UStateMachineComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UStateMachineComponent();

	//~ Begin UActorComponent Interface
	virtual void InitializeComponent() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~ End UActorComponent Interface

	// ------------------------------------------------------------------
	// Configuration
	// ------------------------------------------------------------------

	/** Untick for games without ground movement (for example a flight-only game). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "State Machine|Domains")
	bool bEnableGroundDomain = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "State Machine|Domains")
	bool bEnableFlyDomain = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "State Machine|Domains")
	bool bEnableWaterDomain = true;

	/** Reject locomotion and action tags that do not belong to the active domain. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "State Machine|Rules")
	bool bValidateDomainMembership = true;

	/** If false, BeginAction fails while another action is running. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "State Machine|Rules")
	bool bAllowActionInterrupt = true;

	/** Duration (seconds) used when BeginAction is called without one and no override exists. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "State Machine|Rules", meta = (ClampMin = "0.0", Units = "s"))
	float DefaultActionDuration = 0.5f;

	/** Per-action duration overrides, keyed by action tag. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "State Machine|Rules")
	TMap<FGameplayTag, float> ActionDurationOverrides;

	// ------------------------------------------------------------------
	// Events
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "State Machine|Events")
	FStateMachineDomainChangedSignature OnDomainChanged;

	UPROPERTY(BlueprintAssignable, Category = "State Machine|Events")
	FStateMachineStateChangedSignature OnStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "State Machine|Events")
	FStateMachineActionStartedSignature OnActionStarted;

	UPROPERTY(BlueprintAssignable, Category = "State Machine|Events")
	FStateMachineActionEndedSignature OnActionEnded;

	// ------------------------------------------------------------------
	// Commands (call on the owning client and the server; simulated proxies mirror automatically)
	// ------------------------------------------------------------------

	/** Applies the enable flags in one call and moves to the first enabled domain if the current one was disabled. */
	UFUNCTION(BlueprintCallable, Category = "State Machine")
	void ConfigureDomains(bool bGround, bool bFly, bool bWater);

	/** Switches domain. Resets the locomotion state to the domain default and ends an action that no longer fits. */
	UFUNCTION(BlueprintCallable, Category = "State Machine")
	bool SetDomain(FGameplayTag NewDomain);

	/** Sets the locomotion state. Fails when the tag does not belong to the active domain (if validation is on). */
	UFUNCTION(BlueprintCallable, Category = "State Machine")
	bool SetLocomotionState(FGameplayTag NewState);

	/** Starts a timed action overlay. Duration below zero resolves through ActionDurationOverrides / DefaultActionDuration. */
	UFUNCTION(BlueprintCallable, Category = "State Machine")
	bool BeginAction(FGameplayTag ActionTag, float Duration = -1.0f);

	UFUNCTION(BlueprintCallable, Category = "State Machine")
	void EndAction();

	// ------------------------------------------------------------------
	// Queries
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "State Machine")
	FGameplayTag GetActiveDomain() const { return ActiveDomain; }

	UFUNCTION(BlueprintPure, Category = "State Machine")
	FGameplayTag GetLocomotionState() const { return LocomotionState; }

	UFUNCTION(BlueprintPure, Category = "State Machine")
	FGameplayTag GetActionState() const { return ActionState; }

	/** Action when one is running, otherwise the locomotion state. */
	UFUNCTION(BlueprintPure, Category = "State Machine")
	FGameplayTag GetActiveState() const { return ActionState.IsValid() ? ActionState : LocomotionState; }

	UFUNCTION(BlueprintPure, Category = "State Machine")
	bool HasActiveAction() const { return ActionState.IsValid(); }

	/** Domain + locomotion + action tags (with their parents). */
	UFUNCTION(BlueprintPure, Category = "State Machine")
	const FGameplayTagContainer& GetActiveTags() const { return ActiveTags; }

	/** Hierarchical match by default: Frameworks.State.Ground matches every Ground state. */
	UFUNCTION(BlueprintPure, Category = "State Machine")
	bool IsInState(FGameplayTag StateTag, bool bExact = false) const;

	UFUNCTION(BlueprintPure, Category = "State Machine")
	bool IsInDomain(FGameplayTag DomainTag) const { return ActiveDomain == DomainTag; }

	UFUNCTION(BlueprintPure, Category = "State Machine")
	bool IsDomainEnabled(FGameplayTag DomainTag) const;

	/** True if StateTag sits under the state root of DomainTag. */
	UFUNCTION(BlueprintPure, Category = "State Machine")
	bool BelongsToDomain(FGameplayTag StateTag, FGameplayTag DomainTag) const;

	UFUNCTION(BlueprintPure, Category = "State Machine")
	static FGameplayTag GetDomainStateRoot(FGameplayTag DomainTag);

	UFUNCTION(BlueprintPure, Category = "State Machine")
	static FGameplayTag GetDefaultStateForDomain(FGameplayTag DomainTag);

protected:
	UFUNCTION()
	void OnRep_ActiveDomain(FGameplayTag OldDomain);

	UFUNCTION()
	void OnRep_LocomotionState(FGameplayTag OldState);

	UFUNCTION()
	void OnRep_Action();

private:
	bool HasOwnerAuthority() const;
	FGameplayTag PickFirstEnabledDomain() const;
	float ResolveActionDuration(FGameplayTag ActionTag) const;

	void RebuildActiveTags();
	void NotifyIfStateChanged();
	void ApplyActionLocal(FGameplayTag ActionTag, float Duration, bool bStartTimer);
	void ClearActionLocal();
	void HandleActionTimerElapsed();

	UPROPERTY(ReplicatedUsing = OnRep_ActiveDomain)
	FGameplayTag ActiveDomain;

	UPROPERTY(ReplicatedUsing = OnRep_LocomotionState)
	FGameplayTag LocomotionState;

	UPROPERTY(ReplicatedUsing = OnRep_Action)
	FStateActionReplication ActionReplication;

	/** Local view of the running action (not replicated directly, mirrored from ActionReplication on proxies). */
	FGameplayTag ActionState;

	/** Derived: domain + locomotion + action. Rebuilt after every change on every machine. */
	FGameplayTagContainer ActiveTags;

	FGameplayTag LastNotifiedState;
	FTimerHandle ActionTimerHandle;
};
