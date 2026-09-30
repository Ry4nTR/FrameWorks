// BaseCharacterMovementComponent.h
// Movement Framework - Ground / Fly / Water locomotion on top of UCharacterMovementComponent.
//
// DOMAINS (derived from the engine movement mode, never stored separately)
//   Ground  MOVE_Walking, MOVE_NavWalking, MOVE_Falling
//   Fly     MOVE_Flying
//   Water   MOVE_Swimming
//
// PREDICTION
//   Sprint, Fly, Dash and Dodge travel in the four custom compressed flags of the saved move, so client and
//   server execute them on the same move and replay them correctly after a correction.
//
// INDEPENDENCE
//   This component knows nothing about the StateMachine or Animation frameworks. It exposes facts
//   (GetMovementDomain, GetWaterZone, IsSprinting ...) and the OnActionStarted event; ABaseCharacter wires it up.
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

	/** Untick for games without flight. Fly requests are ignored. */
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
	 * True: the body turns to face the flight vector (pitch included) and levels out when idle.
	 * False: the body stays upright and only turns with the camera yaw.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Fly")
	bool bOrientRotationToFlight = true;

	/** Pitch and roll turn rate (degrees/s). Applied to RotationRate so the body can tilt in flight and level out on landing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Fly", meta = (ClampMin = "0.0"))
	float FlightPitchRollRate = 240.0f;

	/** How fast (interp speed) the body levels out when flying without input. Only used with bOrientRotationToFlight. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Fly", meta = (ClampMin = "0.0"))
	float FlightUprightInterpSpeed = 4.0f;

	/** Hold Sprint while flying to glide: fast forward flight with a slow sink. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Fly")
	bool bEnableGlide = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Fly", meta = (ClampMin = "0.0", Units = "cm/s"))
	float GlideSpeed = 1400.0f;

	/** Downward acceleration (cm/s^2) added while gliding. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Fly", meta = (ClampMin = "0.0"))
	float GlideSinkAcceleration = 250.0f;

	// ------------------------------------------------------------------
	// Water
	// ------------------------------------------------------------------

	/** Speed (cm/s) while sprinting in water. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Water", meta = (ClampMin = "0.0", Units = "cm/s"))
	float FastSwimSpeed = 600.0f;

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
	 * Applies to Ground and Water. Fly uses bOrientRotationToFlight. Call on the server as well as the client.
	 */
	UFUNCTION(BlueprintCallable, Category = "Movement")
	void SetFaceControlRotation(bool bFace);

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
