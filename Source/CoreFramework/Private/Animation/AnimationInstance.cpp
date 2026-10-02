// AnimationInstance.cpp
// Animation Framework - thread-safe Anim Instance driven by the StateMachineComponent tags.

#include "Animation/AnimationInstance.h"

#include "Animation/AnimMontage.h"
#include "StateMachine/StateMachineComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"

DEFINE_LOG_CATEGORY(LogFrameworksAnimation);

namespace
{
	/** One row of a tag -> enum lookup table. */
	template <typename TState>
	struct TTagStateRow
	{
		const FNativeGameplayTag* Tag;
		TState State;
	};

	template <typename TState, size_t N>
	bool FindStateForTag(const FGameplayTag& InTag, const TTagStateRow<TState> (&Table)[N], TState& OutState)
	{
		for (const TTagStateRow<TState>& Row : Table)
		{
			if (InTag == Row.Tag->GetTag())
			{
				OutState = Row.State;
				return true;
			}
		}
		return false;
	}
}

// ----------------------------------------------------------------------
// Lifecycle
// ----------------------------------------------------------------------

void UAnimationInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	CacheOwnerReferences();

	// Root motion switch. IgnoreRootMotion extracts and discards it so the mesh never slides.
	if (bEnableRootMotion)
	{
		SetRootMotionMode(EnabledRootMotionMode);
	}
	else
	{
		SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
	}

	if (!CachedPawn.IsValid())
	{
		// Normal in the AnimBP editor preview. All values stay at their defaults.
		UE_LOG(LogFrameworksAnimation, Verbose, TEXT("%s: no Pawn owner found. Animation data stays at defaults."), *GetNameSafe(this));
	}
}

void UAnimationInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// The owner can appear late (possession, mesh re-attach) or swap movement components.
	if (!CachedPawn.IsValid() || !CachedMovement.IsValid())
	{
		CacheOwnerReferences();
	}

	GatherSnapshot(Snapshot, DeltaSeconds);
}

void UAnimationInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);

	ComputeAnimationData(Snapshot, DeltaSeconds);
}

void UAnimationInstance::SetUpperBodyLayerAllowed(bool bAllowed)
{
	// Copied into the snapshot on the next game-thread update, so the worker thread never races with this write.
	bUpperBodyLayerAllowed = bAllowed;
}

// ----------------------------------------------------------------------
// Game thread: gathering
// ----------------------------------------------------------------------

void UAnimationInstance::CacheOwnerReferences()
{
	CachedPawn.Reset();
	CachedMovement.Reset();
	CachedCharacterMovement.Reset();
	CachedStateMachine.Reset();

	APawn* Pawn = TryGetPawnOwner();
	if (!Pawn)
	{
		return;
	}

	CachedPawn = Pawn;
	CachedMovement = Pawn->GetMovementComponent();
	CachedStateMachine = Pawn->FindComponentByClass<UStateMachineComponent>();

	if (const ACharacter* Character = Cast<ACharacter>(Pawn))
	{
		CachedCharacterMovement = Character->GetCharacterMovement();
	}
}

void UAnimationInstance::GatherSnapshot(FAnimationSnapshot& OutSnapshot, float DeltaSeconds)
{
	OutSnapshot.bUpperBodyLayerAllowed = bUpperBodyLayerAllowed;

	const APawn* Pawn = CachedPawn.Get();
	if (!Pawn)
	{
		const bool bLayerAllowed = OutSnapshot.bUpperBodyLayerAllowed;
		OutSnapshot = FAnimationSnapshot();
		OutSnapshot.bUpperBodyLayerAllowed = bLayerAllowed;
		return;
	}

	OutSnapshot.bHasOwner = true;
	OutSnapshot.Velocity = Pawn->GetVelocity();
	OutSnapshot.ActorRotation = Pawn->GetActorRotation();

	// UPawnMovementComponent exposes these queries for every pawn movement type.
	const UPawnMovementComponent* Movement = CachedMovement.Get();
	OutSnapshot.bIsFalling = Movement && Movement->IsFalling();
	OutSnapshot.bIsSwimming = Movement && Movement->IsSwimming();
	OutSnapshot.bIsFlying = Movement && Movement->IsFlying();
	OutSnapshot.bIsCrouching = Movement && Movement->IsCrouching();

	if (const UCharacterMovementComponent* CharacterMovement = CachedCharacterMovement.Get())
	{
		OutSnapshot.bHasMovementInput = !CharacterMovement->GetCurrentAcceleration().IsNearlyZero();
	}
	else
	{
		OutSnapshot.bHasMovementInput = !Pawn->GetLastMovementInputVector().IsNearlyZero();
	}

	if (const UStateMachineComponent* StateMachine = CachedStateMachine.Get())
	{
		OutSnapshot.bHasStateMachine = true;
		OutSnapshot.bHasAction = StateMachine->HasActiveAction();
		OutSnapshot.DomainTag = StateMachine->GetActiveDomain();
		OutSnapshot.StateTag = StateMachine->GetActiveState();

		if (OutSnapshot.ActiveTags != StateMachine->GetActiveTags())
		{
			OutSnapshot.ActiveTags = StateMachine->GetActiveTags();
		}
	}
	else
	{
		OutSnapshot.bHasStateMachine = false;
		OutSnapshot.bHasAction = false;
		OutSnapshot.DomainTag = FGameplayTag();
		OutSnapshot.StateTag = FGameplayTag();
		OutSnapshot.ActiveTags.Reset();
	}
}

// ----------------------------------------------------------------------
// Thread-safe: derived data
// ----------------------------------------------------------------------

bool UAnimationInstance::IsDomainEnabled(EAnimationDomain Candidate) const
{
	switch (Candidate)
	{
	case EAnimationDomain::Ground:
		return bEnableGroundDomain;
	case EAnimationDomain::Fly:
		return bEnableFlyDomain;
	case EAnimationDomain::Water:
		return bEnableWaterDomain;
	default:
		return false;
	}
}

EAnimationDomain UAnimationInstance::ResolveEnabledDomain(EAnimationDomain Requested) const
{
	if (IsDomainEnabled(Requested))
	{
		return Requested;
	}

	// Disabled domain: fall back to the first enabled one so the AnimGraph always lands in a branch you wired up.
	if (bEnableGroundDomain)
	{
		return EAnimationDomain::Ground;
	}
	if (bEnableFlyDomain)
	{
		return EAnimationDomain::Fly;
	}
	if (bEnableWaterDomain)
	{
		return EAnimationDomain::Water;
	}

	// Every toggle is off: a configuration mistake, but still a valid pose source.
	return EAnimationDomain::Ground;
}

bool UAnimationInstance::MapStateTag(const FGameplayTag& StateTag)
{
	if (!StateTag.IsValid())
	{
		return false;
	}

	switch (Domain)
	{
	case EAnimationDomain::Ground:
		{
			static const TTagStateRow<EGroundAnimState> Table[] =
			{
				{ &TAG_Frameworks_State_Ground_Idle,   EGroundAnimState::Idle },
				{ &TAG_Frameworks_State_Ground_Walk,   EGroundAnimState::Walk },
				{ &TAG_Frameworks_State_Ground_Run,    EGroundAnimState::Run },
				{ &TAG_Frameworks_State_Ground_Jump,   EGroundAnimState::Jump },
				{ &TAG_Frameworks_State_Ground_Slide,  EGroundAnimState::Slide },
				{ &TAG_Frameworks_State_Ground_Crouch, EGroundAnimState::Crouch },
				{ &TAG_Frameworks_State_Ground_Dash,   EGroundAnimState::Dash },
				{ &TAG_Frameworks_State_Ground_Roll,   EGroundAnimState::Roll },
				{ &TAG_Frameworks_State_Ground_Mantle, EGroundAnimState::Mantle }
			};
			return FindStateForTag(StateTag, Table, GroundState);
		}

	case EAnimationDomain::Fly:
		{
			static const TTagStateRow<EFlyAnimState> Table[] =
			{
				{ &TAG_Frameworks_State_Fly_Idle,  EFlyAnimState::Idle },
				{ &TAG_Frameworks_State_Fly_Move,  EFlyAnimState::Move },
				{ &TAG_Frameworks_State_Fly_Glide, EFlyAnimState::Glide },
				{ &TAG_Frameworks_State_Fly_Dash,  EFlyAnimState::Dash },
				{ &TAG_Frameworks_State_Fly_Dodge, EFlyAnimState::Dodge }
			};
			return FindStateForTag(StateTag, Table, FlyState);
		}

	case EAnimationDomain::Water:
		{
			static const TTagStateRow<EWaterAnimState> Table[] =
			{
				{ &TAG_Frameworks_State_Water_Surface_Idle,           EWaterAnimState::SurfaceIdle },
				{ &TAG_Frameworks_State_Water_Surface_Move,           EWaterAnimState::SurfaceMove },
				{ &TAG_Frameworks_State_Water_Surface_FastSwim,       EWaterAnimState::SurfaceFastSwim },
				{ &TAG_Frameworks_State_Water_Transition_ToSurface,   EWaterAnimState::TransitionToSurface },
				{ &TAG_Frameworks_State_Water_Transition_ToUnderwater, EWaterAnimState::TransitionToUnderwater },
				{ &TAG_Frameworks_State_Water_Underwater_Idle,        EWaterAnimState::UnderwaterIdle },
				{ &TAG_Frameworks_State_Water_Underwater_Move,        EWaterAnimState::UnderwaterMove },
				{ &TAG_Frameworks_State_Water_Underwater_FastSwim,    EWaterAnimState::UnderwaterFastSwim },
				{ &TAG_Frameworks_State_Water_Underwater_Dash,        EWaterAnimState::UnderwaterDash },
				{ &TAG_Frameworks_State_Water_Resurfacing,            EWaterAnimState::Resurfacing }
			};
			return FindStateForTag(StateTag, Table, WaterState);
		}

	default:
		return false;
	}
}

void UAnimationInstance::DeriveKinematicState(const FAnimationSnapshot& InSnapshot)
{
	// Used when there is no StateMachineComponent, or when a disabled domain was replaced by another one.
	switch (Domain)
	{
	case EAnimationDomain::Ground:
		if (bIsFalling)
		{
			GroundState = EGroundAnimState::Jump;
		}
		else if (InSnapshot.bIsCrouching)
		{
			GroundState = EGroundAnimState::Crouch;
		}
		else if (!bShouldMove)
		{
			GroundState = EGroundAnimState::Idle;
		}
		else
		{
			GroundState = (GroundSpeed >= RunSpeedThreshold) ? EGroundAnimState::Run : EGroundAnimState::Walk;
		}
		break;

	case EAnimationDomain::Fly:
		FlyState = bShouldMove ? EFlyAnimState::Move : EFlyAnimState::Idle;
		break;

	case EAnimationDomain::Water:
		WaterState = bShouldMove ? EWaterAnimState::SurfaceMove : EWaterAnimState::SurfaceIdle;
		break;
	}
}

void UAnimationInstance::ComputeAnimationData(const FAnimationSnapshot& InSnapshot, float DeltaSeconds)
{
	// ---- Velocity ----
	Velocity = InSnapshot.Velocity;
	Speed = Velocity.Size();
	GroundSpeed = Velocity.Size2D();
	VerticalSpeed = Velocity.Z;
	bHasMovementInput = InSnapshot.bHasMovementInput;

	// ---- Domain ----
	EAnimationDomain Requested = EAnimationDomain::Ground;
	if (InSnapshot.bHasStateMachine)
	{
		if (InSnapshot.DomainTag == TAG_Frameworks_Domain_Fly.GetTag())
		{
			Requested = EAnimationDomain::Fly;
		}
		else if (InSnapshot.DomainTag == TAG_Frameworks_Domain_Water.GetTag())
		{
			Requested = EAnimationDomain::Water;
		}
	}
	else if (InSnapshot.bIsSwimming)
	{
		Requested = EAnimationDomain::Water;
	}
	else if (InSnapshot.bIsFlying)
	{
		Requested = EAnimationDomain::Fly;
	}

	Domain = ResolveEnabledDomain(Requested);
	const bool bDomainSubstituted = (Domain != Requested);

	// ---- Movement intent ----
	const float MoveSpeed = (Domain == EAnimationDomain::Ground) ? GroundSpeed : Speed;
	bShouldMove = MoveSpeed > MovingSpeedThreshold && (!bRequireMovementInputToMove || bHasMovementInput);

	// ---- Airborne flags (Ground domain only) ----
	bIsFalling = (Domain == EAnimationDomain::Ground) && InSnapshot.bIsFalling;
	bIsRising = bIsFalling && VerticalSpeed > 0.0f;

	// ---- Landing latch ----
	// Held for a short time instead of one frame so a state machine transition cannot miss it.
	const bool bLandedThisUpdate = bWasFalling && !bIsFalling && Domain == EAnimationDomain::Ground;
	bWasFalling = bIsFalling;

	if (bLandedThisUpdate)
	{
		LandedTimeRemaining = LandedFlagDuration;
	}
	else if (Domain == EAnimationDomain::Ground)
	{
		LandedTimeRemaining = FMath::Max(0.0f, LandedTimeRemaining - DeltaSeconds);
	}
	else
	{
		LandedTimeRemaining = 0.0f;
	}
	bJustLanded = LandedTimeRemaining > 0.0f;

	// ---- State ----
	// Tags are the source of truth. A mapping miss (unknown tag, substituted domain, no component) falls back to motion.
	const bool bMapped = InSnapshot.bHasStateMachine && !bDomainSubstituted && MapStateTag(InSnapshot.StateTag);
	if (!bMapped)
	{
		DeriveKinematicState(InSnapshot);
	}

	if (ActiveStateTag != InSnapshot.StateTag)
	{
		ActiveStateTag = InSnapshot.StateTag;
	}
	if (ActiveTags != InSnapshot.ActiveTags)
	{
		ActiveTags = InSnapshot.ActiveTags;
	}
	bActionActive = InSnapshot.bHasAction;

	bIsCrouching = (Domain == EAnimationDomain::Ground) && (GroundState == EGroundAnimState::Crouch);
	bIsGliding = (Domain == EAnimationDomain::Fly) && (FlyState == EFlyAnimState::Glide);
	bIsUnderwater = (Domain == EAnimationDomain::Water)
		&& (WaterState == EWaterAnimState::UnderwaterIdle
			|| WaterState == EWaterAnimState::UnderwaterMove
			|| WaterState == EWaterAnimState::UnderwaterFastSwim
			|| WaterState == EWaterAnimState::UnderwaterDash
			|| WaterState == EWaterAnimState::Resurfacing);

	// ---- Direction (relative to actor facing) ----
	if (GroundSpeed > MovingSpeedThreshold)
	{
		Direction = FMath::FindDeltaAngleDegrees(InSnapshot.ActorRotation.Yaw, Velocity.Rotation().Yaw);
	}
	else
	{
		Direction = 0.0f;
	}

	// ---- Upper-body layer ----
	const bool bSuppressed = bSuppressUpperBodyDuringActions && InSnapshot.bHasAction;
	const float TargetWeight = (InSnapshot.bUpperBodyLayerAllowed && !bSuppressed) ? 1.0f : 0.0f;
	UpperBodyBlendWeight = FMath::FInterpTo(UpperBodyBlendWeight, TargetWeight, DeltaSeconds, UpperBodyBlendInterpSpeed);
}

// ----------------------------------------------------------------------
// Montages (one-off event animations)
// ----------------------------------------------------------------------

float UAnimationInstance::PlayActionMontage(UAnimMontage* Montage, float PlayRate, FName StartSection)
{
	if (!Montage)
	{
		UE_LOG(LogFrameworksAnimation, Warning, TEXT("%s: PlayActionMontage called with a null montage."), *GetNameSafe(this));
		return 0.0f;
	}

	const float Duration = Montage_Play(Montage, PlayRate);
	if (Duration <= 0.0f)
	{
		// Typical causes: slot missing in the AnimGraph, skeleton mismatch, invalid montage.
		UE_LOG(LogFrameworksAnimation, Warning, TEXT("%s: Montage_Play failed for %s."), *GetNameSafe(this), *GetNameSafe(Montage));
		return 0.0f;
	}

	if (StartSection != NAME_None)
	{
		Montage_JumpToSection(StartSection, Montage);
	}

	FOnMontageEnded EndDelegate;
	EndDelegate.BindUObject(this, &UAnimationInstance::HandleActionMontageEnded);
	Montage_SetEndDelegate(EndDelegate, Montage);

	return Duration;
}

void UAnimationInstance::StopActionMontage(UAnimMontage* Montage, float BlendOutTime)
{
	Montage_Stop(BlendOutTime, Montage);
}

void UAnimationInstance::HandleActionMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	OnActionMontageEnded.Broadcast(Montage, bInterrupted);
}
