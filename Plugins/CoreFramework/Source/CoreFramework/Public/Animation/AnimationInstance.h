// AnimationInstance.h
// Animation Framework - thread-safe Anim Instance driven by the StateMachineComponent tags.
//
// THREADING CONTRACT
//   NativeInitializeAnimation        [Game thread]   Cache the Pawn, its movement component and StateMachineComponent.
//   NativeUpdateAnimation            [Game thread]   Copy raw values (velocity, tags, flags) into a snapshot.
//   NativeThreadSafeUpdateAnimation  [Worker thread] Derive every AnimGraph variable from the snapshot and the
//                                                    config properties ONLY. Never touch the Pawn or other UObjects.
//   The AnimGraph reads the variables below as plain Self variables, so it never casts or reaches into gameplay.
//
// DOMAIN TOGGLES
//   bEnableGroundDomain / bEnableFlyDomain / bEnableWaterDomain decide which domains the AnimGraph can ever be in.
//   A requested domain that is disabled is replaced by the first enabled one (Ground, Fly, Water) and the state is
//   derived from motion. A flight-only AnimBP therefore only needs the Fly branch wired up; the other branches are
//   never entered and may stay empty or be deleted.
//
// SOURCES OF TRUTH
//   With a StateMachineComponent on the Pawn the state comes from its Gameplay Tags. Without one, the domain and a
//   basic state (Idle / Walk / Run / Jump / Move ...) are derived from movement, so the AnimBP still animates.
//
// PORTING: copy this folder, keep FRAMEWORKS_API (or rename it to your module macro).
// Depends on: StateMachine framework (tag vocabulary). Needs the GameplayTags module.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "GameplayTagContainer.h"
#include "AnimationInstance.generated.h"

class APawn;
class UAnimMontage;
class UCharacterMovementComponent;
class UPawnMovementComponent;
class UStateMachineComponent;

DECLARE_LOG_CATEGORY_EXTERN(LogFrameworksAnimation, Log, All);

/** Top-level branch of the AnimGraph. */
UENUM(BlueprintType)
enum class EAnimationDomain : uint8
{
	Ground	UMETA(DisplayName = "Ground"),
	Fly		UMETA(DisplayName = "Fly"),
	Water	UMETA(DisplayName = "Water")
};

UENUM(BlueprintType)
enum class EGroundAnimState : uint8
{
	Idle, Walk, Run, Jump, Slide, Crouch, Dash, Roll, Mantle
};

UENUM(BlueprintType)
enum class EFlyAnimState : uint8
{
	Idle, Move, Glide, Dash, Dodge
};

UENUM(BlueprintType)
enum class EWaterAnimState : uint8
{
	SurfaceIdle,
	SurfaceMove,
	SurfaceFastSwim,
	TransitionToSurface,
	TransitionToUnderwater,
	UnderwaterIdle,
	UnderwaterMove,
	UnderwaterFastSwim,
	UnderwaterDash,
	Resurfacing
};

/**
 * Value copy of everything the worker thread needs. Written on the game thread, read on the worker thread.
 * Only value types: no UObject pointers.
 */
struct FAnimationSnapshot
{
	bool bHasOwner = false;
	FVector Velocity = FVector::ZeroVector;
	FRotator ActorRotation = FRotator::ZeroRotator;
	bool bHasMovementInput = false;
	bool bIsFalling = false;
	bool bIsSwimming = false;
	bool bIsFlying = false;
	bool bIsCrouching = false;

	bool bHasStateMachine = false;
	bool bHasAction = false;
	FGameplayTag DomainTag;
	FGameplayTag StateTag;
	FGameplayTagContainer ActiveTags;

	bool bUpperBodyLayerAllowed = true;
};

/** Fired when a montage started through PlayActionMontage finishes or is interrupted. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FAnimationMontageEndedSignature, UAnimMontage*, Montage, bool, bInterrupted);

UCLASS()
class COREFRAMEWORK_API UAnimationInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	//~ Begin UAnimInstance Interface
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;
	//~ End UAnimInstance Interface

	// ------------------------------------------------------------------
	// One-off event animations (attack, interact, hit reaction ...)
	// ------------------------------------------------------------------

	/**
	 * Plays a montage. Its slot (for example UpperBody or DefaultSlot) must exist in the AnimGraph.
	 * @return Montage duration in seconds, or 0 if the montage could not be played.
	 */
	UFUNCTION(BlueprintCallable, Category = "Animation Framework|Montage")
	float PlayActionMontage(UAnimMontage* Montage, float PlayRate = 1.0f, FName StartSection = NAME_None);

	/** Stops the given montage, or every active montage when Montage is null. */
	UFUNCTION(BlueprintCallable, Category = "Animation Framework|Montage")
	void StopActionMontage(UAnimMontage* Montage = nullptr, float BlendOutTime = 0.2f);

	UPROPERTY(BlueprintAssignable, Category = "Animation Framework|Montage")
	FAnimationMontageEndedSignature OnActionMontageEnded;

	/** Turns the upper-body layer on or off at runtime (for example while climbing). The weight blends smoothly. */
	UFUNCTION(BlueprintCallable, Category = "Animation Framework|Layers")
	void SetUpperBodyLayerAllowed(bool bAllowed);

	// ------------------------------------------------------------------
	// Config - set in the AnimBP "Class Defaults"
	// ------------------------------------------------------------------

	/** Untick for games without ground movement (for example flight-only). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation Framework|Domains")
	bool bEnableGroundDomain = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation Framework|Domains")
	bool bEnableFlyDomain = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation Framework|Domains")
	bool bEnableWaterDomain = true;

	/** false: root motion is discarded. true: EnabledRootMotionMode is applied. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation Framework|Root Motion")
	bool bEnableRootMotion = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation Framework|Root Motion", meta = (EditCondition = "bEnableRootMotion"))
	TEnumAsByte<ERootMotionMode::Type> EnabledRootMotionMode = ERootMotionMode::RootMotionFromMontagesOnly;

	/** Speed (cm/s) above which the character counts as moving. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation Framework|Thresholds", meta = (ClampMin = "0.0", Units = "cm/s"))
	float MovingSpeedThreshold = 3.0f;

	/** Ground speed (cm/s) at which Walk becomes Run. Only used when no StateMachineComponent supplies the state. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation Framework|Thresholds", meta = (ClampMin = "0.0", Units = "cm/s"))
	float RunSpeedThreshold = 300.0f;

	/** Seconds bJustLanded stays true after touching the ground. Keep it shorter than the landing animation. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation Framework|Thresholds", meta = (ClampMin = "0.0", Units = "s"))
	float LandedFlagDuration = 0.15f;

	/** If true, bShouldMove also requires movement input (prevents skidding from looking like running). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation Framework|Thresholds")
	bool bRequireMovementInputToMove = true;

	/** Master switch for the upper-body layer. Also settable at runtime with SetUpperBodyLayerAllowed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Animation Framework|Layers")
	bool bUpperBodyLayerAllowed = true;

	/** Drop the upper-body layer while an action state (Dash, Roll, Mantle ...) is active so full-body motion stays clean. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation Framework|Layers")
	bool bSuppressUpperBodyDuringActions = true;

	/** Blend speed of UpperBodyBlendWeight. 0 = instant. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation Framework|Layers", meta = (ClampMin = "0.0"))
	float UpperBodyBlendInterpSpeed = 12.0f;

	// ------------------------------------------------------------------
	// Animation data - written by the thread-safe update, read by the AnimGraph
	// ------------------------------------------------------------------

	/** Active AnimGraph branch after the domain toggles were applied. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Animation Framework|State")
	EAnimationDomain Domain = EAnimationDomain::Ground;

	/** Valid while Domain is Ground. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Animation Framework|State")
	EGroundAnimState GroundState = EGroundAnimState::Idle;

	/** Valid while Domain is Fly. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Animation Framework|State")
	EFlyAnimState FlyState = EFlyAnimState::Idle;

	/** Valid while Domain is Water. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Animation Framework|State")
	EWaterAnimState WaterState = EWaterAnimState::SurfaceIdle;

	/** Effective gameplay tag (action if running, else locomotion). Invalid without a StateMachineComponent. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Animation Framework|State")
	FGameplayTag ActiveStateTag;

	/** Domain + locomotion + action tags. Use HasActiveTag in transition rules. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Animation Framework|State")
	FGameplayTagContainer ActiveTags;

	/** True while a timed action state (Dash, Roll, Mantle, water transition ...) is running. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Animation Framework|State")
	bool bActionActive = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Animation Framework|State")
	bool bIsUnderwater = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Animation Framework|State")
	bool bIsGliding = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Animation Framework|State")
	bool bIsCrouching = false;

	/** Ground only: falling and moving upward (jump start), as opposed to falling. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Animation Framework|State")
	bool bIsRising = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Animation Framework|State")
	bool bIsFalling = false;

	/** True for LandedFlagDuration seconds after landing on the ground. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Animation Framework|State")
	bool bJustLanded = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Animation Framework|Locomotion")
	FVector Velocity = FVector::ZeroVector;

	/** 3D speed (cm/s). Used in Fly and Water. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Animation Framework|Locomotion")
	float Speed = 0.0f;

	/** Horizontal speed (cm/s). Used on the ground. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Animation Framework|Locomotion")
	float GroundSpeed = 0.0f;

	/** Vertical velocity (cm/s), positive = up. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Animation Framework|Locomotion")
	float VerticalSpeed = 0.0f;

	/** Degrees (-180..180) between facing and velocity. 0 = forward, positive = right. For strafe blend spaces. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Animation Framework|Locomotion")
	float Direction = 0.0f;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Animation Framework|Locomotion")
	bool bHasMovementInput = false;

	/** Moving above MovingSpeedThreshold (Ground: horizontal speed, Fly and Water: 3D speed). */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Animation Framework|Locomotion")
	bool bShouldMove = false;

	/** Weight for the Layered Blend per Bone node (Blend Weights pin). 1 = upper-body layer fully applied. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Animation Framework|Layers")
	float UpperBodyBlendWeight = 1.0f;

	/** Thread-safe tag query for transition rules and blend logic. */
	UFUNCTION(BlueprintPure, Category = "Animation Framework|State", meta = (BlueprintThreadSafe))
	bool HasActiveTag(FGameplayTag Tag) const { return ActiveTags.HasTag(Tag); }

protected:
	/** [GAME THREAD] Copies raw data into OutSnapshot. Override (call Super) to add project inputs. Null-safe. */
	virtual void GatherSnapshot(FAnimationSnapshot& OutSnapshot, float DeltaSeconds);

	/** [THREAD SAFE] Computes every AnimGraph variable from the snapshot. Only touch members of this object. */
	virtual void ComputeAnimationData(const FAnimationSnapshot& InSnapshot, float DeltaSeconds);

private:
	void CacheOwnerReferences();
	void HandleActionMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	EAnimationDomain ResolveEnabledDomain(EAnimationDomain Requested) const;
	bool IsDomainEnabled(EAnimationDomain Candidate) const;
	bool MapStateTag(const FGameplayTag& StateTag);
	void DeriveKinematicState(const FAnimationSnapshot& InSnapshot);

	/** Game thread writes, worker thread reads. The animation system never runs both at once. */
	FAnimationSnapshot Snapshot;

	/** Worker-thread only. */
	float LandedTimeRemaining = 0.0f;
	bool bWasFalling = false;

	TWeakObjectPtr<APawn> CachedPawn;
	TWeakObjectPtr<UPawnMovementComponent> CachedMovement;
	TWeakObjectPtr<UCharacterMovementComponent> CachedCharacterMovement;
	TWeakObjectPtr<UStateMachineComponent> CachedStateMachine;
};
