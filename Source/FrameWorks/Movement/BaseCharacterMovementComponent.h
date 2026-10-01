// BaseCharacterMovementComponent.h
// Movement Framework - Ground / Fly / Water locomotion on top of UCharacterMovementComponent.
//
// DOMAINS (derived from the engine movement mode, never stored separately)
//   Ground  MOVE_Walking, MOVE_NavWalking, MOVE_Falling
//   Fly     MOVE_Flying    entered with the Fly toggle, or by pressing Jump while airborne
//   Water   MOVE_Swimming  entered and left automatically through a PhysicsVolume with bWaterVolume enabled
//
// CONTROL MODES (Fly and Water only; Sprint is the switch)
//   Standard         Move is planar and follows the camera YAW only. Camera pitch never steers.
//                    Vertical movement comes from dedicated Up / Down buttons. The body stays upright.
//   Camera Steering  Sprint held. Move follows the full camera orientation including pitch (look up + forward = climb).
//                    The Up / Down buttons are ignored. The body may tilt toward the movement vector.
//   On the ground there is a single mode: planar movement along the camera yaw.
//
// PREDICTION
//   Sprint, Fly, Dash and Dodge travel in the four custom compressed flags of the saved move, so client and
//   server execute them on the same move and replay them correctly after a correction. The control mode is a
//   pure function of that state, so client and server always agree on it.
//
// INDEPENDENCE
//   This component knows nothing about the StateMachine or Animation frameworks. It exposes facts
//   (GetMovementDomain, GetControlMode, GetWaterZone ...), input helpers (GetMoveBasisRotation,
//   AcceptsVerticalButtons) and the OnActionStarted event. ABaseCharacter connects them to the rest.
//
// PORTING: copy this folder, keep FRAMEWORKS_API (or rename it to your module macro).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "BaseCharacterMovementComponent.generated.h"

UENUM(BlueprintType)
enum class EMovementDomain : uint8
{
	Ground	UMETA(DisplayName = "Ground"),
	Fly		UMETA(DisplayName = "Fly"),
	Water	UMETA(DisplayName = "Water")
};

/** How Move and vertical input are interpreted. Only meaningful in the Fly and Water domains. */
UENUM(BlueprintType)
enum class EDomainControlMode : uint8
{
	/** Planar movement along the camera yaw, dedicated Up / Down buttons. */
	Standard		UMETA(DisplayName = "Standard"),
	/** Sprint held: full 3D movement along the camera, vertical buttons ignored. */
	CameraSteering	UMETA(DisplayName = "Camera Steering")
};

UENUM(BlueprintType)
enum class EWaterZone : uint8
{
	None		UMETA(DisplayName = "None"),
	Surface		UMETA(DisplayName = "Surface"),
	Underwater	UMETA(DisplayName = "Underwater")
};

UENUM(BlueprintType)
enum class EMovementAction : uint8
{
	Dash	UMETA(DisplayName = "Dash"),
	Dodge	UMETA(DisplayName = "Dodge")
};

/** Tuning for one burst action in one domain. */
USTRUCT(BlueprintType)
struct FRAMEWORKS_API FMovementActionSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	bool bEnabled = true;

	/** Speed (cm/s) the character is launched at along the action direction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action", meta = (ClampMin = "0.0", Units = "cm/s"))
	float Speed = 1200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action", meta = (ClampMin = "0.0", Units = "s"))
	float Cooldown = 1.0f;

	/** How long the matching animation state stays active. Reported through OnActionStarted. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action", meta = (ClampMin = "0.0", Units = "s"))
	float StateDuration = 0.4f;
};

/** Fired once per executed action (not during move replays), on the owning client and on the server. */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnMovementActionStarted, EMovementAction /*Action*/, EMovementDomain /*Domain*/, float /*StateDuration*/);

UCLASS()
class FRAMEWORKS_API UBaseCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UBaseCharacterMovementComponent();

	//~ Begin UActorComponent / UMovementComponent Interface
	virtual void BeginPlay() override;
	virtual float GetMaxSpeed() const override;
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;
	virtual void PhysicsRotation(float DeltaTime) override;
	//~ End UActorComponent / UMovementComponent Interface

	//~ Begin UCharacterMovementComponent Interface
	virtual class FNetworkPredictionData_Client* GetPredictionData_Client() const override;
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
	//~ End UCharacterMovementComponent Interface

	// ------------------------------------------------------------------
	// Domain toggles
	// ------------------------------------------------------------------

	/** Untick for a flight-only game. The character then starts in flight and cannot land. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Domains")
	bool bEnableGroundDomain = true;

	/** Untick for games without flight. Fly toggles and double-jump takeoff are ignored. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Domains")
	bool bEnableFlyDomain = true;

	/** Untick for games without swimming. The character can no longer enter MOVE_Swimming. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Domains")
	bool bEnableWaterDomain = true;

	// ------------------------------------------------------------------
	// Ground
	// ------------------------------------------------------------------

	/** Speed (cm/s) while sprinting on the ground or in the air. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Ground", meta = (ClampMin = "0.0", Units = "cm/s"))
	float SprintSpeed = 700.0f;

	// ------------------------------------------------------------------
	// Fly
	// ------------------------------------------------------------------

	/**
	 * Camera Steering only (Sprint held).
	 * True: the body turns to face the flight vector, pitch included, and levels out when there is no input.
	 * False: the body stays upright and turns with the camera yaw.
	 * Standard mode always keeps the body upright.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Fly")
	bool bOrientRotationToFlight = true;

	/** Pitch and roll turn rate (degrees/s). Applied to RotationRate so the body can tilt in flight and level out on landing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Fly", meta = (ClampMin = "0.0"))
	float FlightPitchRollRate = 240.0f;

	/** Interp speed at which the body levels out in flight (Standard mode, or Camera Steering without input). 0 = instant. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Fly", meta = (ClampMin = "0.0"))
	float FlightUprightInterpSpeed = 4.0f;

	/** Sprint while flying uses GlideSpeed (and the Glide animation state) instead of MaxFlySpeed. Camera Steering works either way. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Fly")
	bool bEnableGlide = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Fly", meta = (ClampMin = "0.0", Units = "cm/s"))
	float GlideSpeed = 1400.0f;

	/** Optional downward acceleration (cm/s^2) while gliding. 0 = pure camera steering. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Fly", meta = (ClampMin = "0.0"))
	float GlideSinkAcceleration = 0.0f;

	// ------------------------------------------------------------------
	// Water
	// ------------------------------------------------------------------

	/** Speed (cm/s) while sprinting in water. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Water", meta = (ClampMin = "0.0", Units = "cm/s"))
	float FastSwimSpeed = 600.0f;

	/**
	 * Camera Steering only (Sprint held).
	 * True: the body turns to face the swim vector, pitch included. False: upright, yaw follows the camera.
	 * Standard mode always keeps the body upright.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Water")
	bool bOrientRotationToSwim = true;

	/** Interp speed at which the body levels out while swimming. 0 = instant. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Water", meta = (ClampMin = "0.0"))
	float SwimUprightInterpSpeed = 4.0f;

	/** Immersion (0..1) at or above which the character counts as underwater instead of at the surface. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Water", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float UnderwaterImmersionThreshold = 0.95f;

	/** Buoyancy used while floating. Above 1 the character rises to the surface. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Water", meta = (ClampMin = "0.0"))
	float SurfaceBuoyancy = 1.15f;

	/** Buoyancy used while actively diving (1 = neutral). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Water", meta = (ClampMin = "0.0"))
	float NeutralBuoyancy = 1.0f;

	/** If true the character floats back up when it stops diving. If false it hovers where it is. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Water")
	bool bAutoResurface = true;

	/** Downward input acceleration (cm/s^2) above which the character counts as diving. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Water", meta = (ClampMin = "0.0"))
	float DiveAccelerationThreshold = 100.0f;

	/** Upward speed (cm/s) above which an underwater character counts as resurfacing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Water", meta = (ClampMin = "0.0", Units = "cm/s"))
	float ResurfaceVerticalSpeed = 40.0f;

	// ------------------------------------------------------------------
	// Burst actions
	// ------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Actions")
	FMovementActionSettings GroundDash;

	/** Ground dodge = roll. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Actions")
	FMovementActionSettings GroundDodge;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Actions")
	FMovementActionSettings FlyDash;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Actions")
	FMovementActionSettings FlyDodge;

	/** Water only supports a dash, and only while underwater. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Actions")
	FMovementActionSettings UnderwaterDash;

	/** The server accepts an action this much (seconds) earlier than its own cooldown to absorb clock differences. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Actions", meta = (ClampMin = "0.0", Units = "s"))
	float ServerCooldownTolerance = 0.15f;

	// ------------------------------------------------------------------
	// Intent (call from input on the locally controlled character)
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Movement")
	void SetWantsToSprint(bool bWants);

	UFUNCTION(BlueprintCallable, Category = "Movement|Fly")
	void SetWantsToFly(bool bWants);

	UFUNCTION(BlueprintCallable, Category = "Movement|Fly")
	void ToggleFlying();

	/** Returns false when the action is unavailable in the current domain or still cooling down. */
	UFUNCTION(BlueprintCallable, Category = "Movement|Actions")
	bool RequestDash();

	UFUNCTION(BlueprintCallable, Category = "Movement|Actions")
	bool RequestDodge();

	/**
	 * True: the body follows the camera yaw (first-person style). False: it turns toward the movement direction.
	 * Applies to Ground, and to Fly / Water in Standard mode. Call on the server as well as the client.
	 */
	UFUNCTION(BlueprintCallable, Category = "Movement")
	void SetFaceControlRotation(bool bFace);

	// ------------------------------------------------------------------
	// Input routing helpers (used by ABaseCharacter, usable by AI and other input sources)
	// ------------------------------------------------------------------

	/** Standard or Camera Steering. Always Standard on the ground. */
	UFUNCTION(BlueprintPure, Category = "Movement|Input")
	EDomainControlMode GetControlMode() const;

	UFUNCTION(BlueprintPure, Category = "Movement|Input")
	bool IsCameraSteering() const;

	/**
	 * Rotation whose forward and right axes turn a Move input into a world direction.
	 * Yaw only on the ground and in Standard mode. Yaw and pitch in Camera Steering.
	 */
	UFUNCTION(BlueprintPure, Category = "Movement|Input")
	FRotator GetMoveBasisRotation(const FRotator& ControlRotation) const;

	/** True when the Up / Down buttons apply: Fly or Water in Standard mode. False on the ground and in Camera Steering. */
	UFUNCTION(BlueprintPure, Category = "Movement|Input")
	bool AcceptsVerticalButtons() const;

	// ------------------------------------------------------------------
	// Facts
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Movement")
	EMovementDomain GetMovementDomain() const;

	/** Surface or Underwater while swimming, otherwise None. Performs a volume trace: call once per frame. */
	UFUNCTION(BlueprintPure, Category = "Movement|Water")
	EWaterZone GetWaterZone() const;

	/** Pass the result of GetWaterZone to avoid a second trace. */
	UFUNCTION(BlueprintPure, Category = "Movement|Water")
	bool IsResurfacing(EWaterZone Zone) const;

	UFUNCTION(BlueprintPure, Category = "Movement|Water")
	float GetImmersionDepth() const;

	UFUNCTION(BlueprintPure, Category = "Movement")
	bool IsSprinting() const;

	/** Flying with Sprint held and glide enabled. */
	UFUNCTION(BlueprintPure, Category = "Movement|Fly")
	bool IsGliding() const;

	UFUNCTION(BlueprintPure, Category = "Movement|Fly")
	bool WantsToFly() const { return bWantsToFly; }

	UFUNCTION(BlueprintPure, Category = "Movement")
	bool HasMovementInput() const { return !Acceleration.IsNearlyZero(); }

	/** Broadcast when Dash or Dodge actually starts. Not fired while replaying saved moves. */
	FOnMovementActionStarted OnActionStarted;

	// ------------------------------------------------------------------
	// Prediction state (read and written by the saved move; do not set directly)
	// ------------------------------------------------------------------

	uint8 bWantsToSprint : 1;
	uint8 bWantsToFly : 1;
	uint8 bWantsToDash : 1;
	uint8 bWantsToDodge : 1;

protected:
	virtual void PhysFlying(float deltaTime, int32 Iterations) override;
	virtual void PhysSwimming(float deltaTime, int32 Iterations) override;

private:
	void UpdateFlightMode();
	EMovementMode GetLandingMovementMode() const;
	void ExecuteAction(EMovementAction Action);
	FVector ComputeActionDirection(EMovementAction Action, EMovementDomain Domain) const;
	const FMovementActionSettings* GetActionSettings(EMovementAction Action, EMovementDomain Domain) const;
	bool IsReplayingSavedMoves() const;
	void RefreshRotationPolicy();

	bool bFaceControlRotation = false;
	double NextDashTime = 0.0;
	double NextDodgeTime = 0.0;
};
