// BaseCharacterMovementComponent.cpp
// Movement Framework - Ground / Fly / Water locomotion on top of UCharacterMovementComponent.

#include "Frameworks/Movement/BaseCharacterMovementComponent.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PhysicsVolume.h"

// ----------------------------------------------------------------------
// Saved move: carries Sprint / Fly / Dash / Dodge in the custom compressed flags
// ----------------------------------------------------------------------

class FSavedMove_BaseCharacter : public FSavedMove_Character
{
public:
	typedef FSavedMove_Character Super;

	uint8 bSavedWantsToSprint : 1;
	uint8 bSavedWantsToFly : 1;
	uint8 bSavedWantsToDash : 1;
	uint8 bSavedWantsToDodge : 1;

	FSavedMove_BaseCharacter()
		: bSavedWantsToSprint(false)
		, bSavedWantsToFly(false)
		, bSavedWantsToDash(false)
		, bSavedWantsToDodge(false)
	{
	}

	virtual void Clear() override
	{
		Super::Clear();
		bSavedWantsToSprint = false;
		bSavedWantsToFly = false;
		bSavedWantsToDash = false;
		bSavedWantsToDodge = false;
	}

	virtual uint8 GetCompressedFlags() const override
	{
		uint8 Result = Super::GetCompressedFlags();
		if (bSavedWantsToSprint) { Result |= FLAG_Custom_0; }
		if (bSavedWantsToFly)    { Result |= FLAG_Custom_1; }
		if (bSavedWantsToDash)   { Result |= FLAG_Custom_2; }
		if (bSavedWantsToDodge)  { Result |= FLAG_Custom_3; }
		return Result;
	}

	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const override
	{
		const FSavedMove_BaseCharacter* Other = static_cast<const FSavedMove_BaseCharacter*>(NewMove.Get());
		if (bSavedWantsToSprint != Other->bSavedWantsToSprint
			|| bSavedWantsToFly != Other->bSavedWantsToFly
			|| bSavedWantsToDash != Other->bSavedWantsToDash
			|| bSavedWantsToDodge != Other->bSavedWantsToDodge)
		{
			return false;
		}
		return Super::CanCombineWith(NewMove, InCharacter, MaxDelta);
	}

	virtual void SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData) override
	{
		Super::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);

		if (const UBaseCharacterMovementComponent* Movement = Cast<UBaseCharacterMovementComponent>(C->GetCharacterMovement()))
		{
			bSavedWantsToSprint = Movement->bWantsToSprint;
			bSavedWantsToFly = Movement->bWantsToFly;
			bSavedWantsToDash = Movement->bWantsToDash;
			bSavedWantsToDodge = Movement->bWantsToDodge;
		}
	}

	virtual void PrepMoveFor(ACharacter* C) override
	{
		Super::PrepMoveFor(C);

		if (UBaseCharacterMovementComponent* Movement = Cast<UBaseCharacterMovementComponent>(C->GetCharacterMovement()))
		{
			Movement->bWantsToSprint = bSavedWantsToSprint;
			Movement->bWantsToFly = bSavedWantsToFly;
			Movement->bWantsToDash = bSavedWantsToDash;
			Movement->bWantsToDodge = bSavedWantsToDodge;
		}
	}
};

class FNetworkPredictionData_Client_BaseCharacter : public FNetworkPredictionData_Client_Character
{
public:
	typedef FNetworkPredictionData_Client_Character Super;

	explicit FNetworkPredictionData_Client_BaseCharacter(const UCharacterMovementComponent& ClientMovement)
		: Super(ClientMovement)
	{
	}

	virtual FSavedMovePtr AllocateNewMove() override
	{
		return FSavedMovePtr(new FSavedMove_BaseCharacter());
	}
};

// ----------------------------------------------------------------------
// Lifecycle
// ----------------------------------------------------------------------

UBaseCharacterMovementComponent::UBaseCharacterMovementComponent()
{
	bWantsToSprint = false;
	bWantsToFly = false;
	bWantsToDash = false;
	bWantsToDodge = false;

	GetNavAgentPropertiesRef().bCanCrouch = true;

	MaxWalkSpeed = 400.0f;
	MaxWalkSpeedCrouched = 200.0f;
	MaxSwimSpeed = 300.0f;
	MaxFlySpeed = 800.0f;
	BrakingDecelerationFlying = 2048.0f;
	BrakingDecelerationSwimming = 512.0f;

	GroundDash.Speed = 1400.0f;
	GroundDash.Cooldown = 1.0f;
	GroundDash.StateDuration = 0.35f;

	GroundDodge.Speed = 900.0f;
	GroundDodge.Cooldown = 0.9f;
	GroundDodge.StateDuration = 0.6f;

	FlyDash.Speed = 2200.0f;
	FlyDash.Cooldown = 1.0f;
	FlyDash.StateDuration = 0.4f;

	FlyDodge.Speed = 1400.0f;
	FlyDodge.Cooldown = 0.8f;
	FlyDodge.StateDuration = 0.5f;

	UnderwaterDash.Speed = 1000.0f;
	UnderwaterDash.Cooldown = 1.0f;
	UnderwaterDash.StateDuration = 0.4f;
}

void UBaseCharacterMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!CharacterOwner)
	{
		return;
	}

	GetNavAgentPropertiesRef().bCanSwim = bEnableWaterDomain;

	// Pitch and roll need a non-zero rate, otherwise the body can neither tilt in flight nor level out on landing.
	RotationRate.Pitch = FMath::Max(RotationRate.Pitch, FlightPitchRollRate);
	RotationRate.Roll = FMath::Max(RotationRate.Roll, FlightPitchRollRate);

	// Flight-only game: start airborne and stay there.
	if (!bEnableGroundDomain && bEnableFlyDomain)
	{
		bWantsToFly = true;
		SetMovementMode(MOVE_Flying);
	}

	RefreshRotationPolicy();
}

FNetworkPredictionData_Client* UBaseCharacterMovementComponent::GetPredictionData_Client() const
{
	check(PawnOwner != nullptr);

	if (ClientPredictionData == nullptr)
	{
		UBaseCharacterMovementComponent* MutableThis = const_cast<UBaseCharacterMovementComponent*>(this);
		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_BaseCharacter(*this);
	}

	return ClientPredictionData;
}

void UBaseCharacterMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);

	bWantsToSprint = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
	bWantsToFly = (Flags & FSavedMove_Character::FLAG_Custom_1) != 0;
	bWantsToDash = (Flags & FSavedMove_Character::FLAG_Custom_2) != 0;
	bWantsToDodge = (Flags & FSavedMove_Character::FLAG_Custom_3) != 0;
}

// ----------------------------------------------------------------------
// Intent
// ----------------------------------------------------------------------

void UBaseCharacterMovementComponent::SetWantsToSprint(bool bWants)
{
	bWantsToSprint = bWants;
}

void UBaseCharacterMovementComponent::SetWantsToFly(bool bWants)
{
	if (!bEnableFlyDomain)
	{
		bWantsToFly = false;
		return;
	}

	// Without a ground domain there is nothing to land on, so flight cannot be switched off.
	bWantsToFly = bWants || !bEnableGroundDomain;
}

void UBaseCharacterMovementComponent::ToggleFlying()
{
	SetWantsToFly(!bWantsToFly);
}

bool UBaseCharacterMovementComponent::RequestDash()
{
	const EMovementDomain Domain = GetMovementDomain();
	const FMovementActionSettings* Settings = GetActionSettings(EMovementAction::Dash, Domain);
	const UWorld* World = GetWorld();

	if (!Settings || !Settings->bEnabled || !World)
	{
		return false;
	}
	if (Domain == EMovementDomain::Water && GetWaterZone() != EWaterZone::Underwater)
	{
		return false;
	}
	if (World->GetTimeSeconds() < NextDashTime)
	{
		return false;
	}

	bWantsToDash = true;
	return true;
}

bool UBaseCharacterMovementComponent::RequestDodge()
{
	const EMovementDomain Domain = GetMovementDomain();
	const FMovementActionSettings* Settings = GetActionSettings(EMovementAction::Dodge, Domain);
	const UWorld* World = GetWorld();

	if (!Settings || !Settings->bEnabled || !World)
	{
		return false;
	}
	if (World->GetTimeSeconds() < NextDodgeTime)
	{
		return false;
	}

	bWantsToDodge = true;
	return true;
}

void UBaseCharacterMovementComponent::SetFaceControlRotation(bool bFace)
{
	bFaceControlRotation = bFace;
	RefreshRotationPolicy();
}

// ----------------------------------------------------------------------
// Facts
// ----------------------------------------------------------------------

EMovementDomain UBaseCharacterMovementComponent::GetMovementDomain() const
{
	switch (MovementMode)
	{
	case MOVE_Flying:
		return EMovementDomain::Fly;
	case MOVE_Swimming:
		return EMovementDomain::Water;
	default:
		return EMovementDomain::Ground;
	}
}

float UBaseCharacterMovementComponent::GetImmersionDepth() const
{
	return ImmersionDepth();
}

EWaterZone UBaseCharacterMovementComponent::GetWaterZone() const
{
	if (MovementMode != MOVE_Swimming)
	{
		return EWaterZone::None;
	}

	return GetImmersionDepth() >= UnderwaterImmersionThreshold ? EWaterZone::Underwater : EWaterZone::Surface;
}

bool UBaseCharacterMovementComponent::IsResurfacing(EWaterZone Zone) const
{
	return Zone == EWaterZone::Underwater && Velocity.Z > ResurfaceVerticalSpeed && Acceleration.Z >= 0.0f;
}

bool UBaseCharacterMovementComponent::IsSprinting() const
{
	return bWantsToSprint && !IsCrouching();
}

bool UBaseCharacterMovementComponent::IsGliding() const
{
	return bEnableGlide && MovementMode == MOVE_Flying && bWantsToSprint;
}

// ----------------------------------------------------------------------
// Speed
// ----------------------------------------------------------------------

float UBaseCharacterMovementComponent::GetMaxSpeed() const
{
	switch (MovementMode)
	{
	case MOVE_Walking:
	case MOVE_NavWalking:
		if (IsCrouching())
		{
			return MaxWalkSpeedCrouched;
		}
		return IsSprinting() ? SprintSpeed : MaxWalkSpeed;

	case MOVE_Falling:
		// Keep sprint momentum in the air.
		return IsSprinting() ? SprintSpeed : MaxWalkSpeed;

	case MOVE_Swimming:
		return IsSprinting() ? FastSwimSpeed : MaxSwimSpeed;

	case MOVE_Flying:
		return IsGliding() ? GlideSpeed : MaxFlySpeed;

	default:
		return Super::GetMaxSpeed();
	}
}

// ----------------------------------------------------------------------
// State updates
// ----------------------------------------------------------------------

void UBaseCharacterMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	UpdateFlightMode();

	// One-shot actions travel as flags; clear them once consumed so replays behave the same.
	if (bWantsToDash)
	{
		bWantsToDash = false;
		ExecuteAction(EMovementAction::Dash);
	}
	if (bWantsToDodge)
	{
		bWantsToDodge = false;
		ExecuteAction(EMovementAction::Dodge);
	}

	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
}

void UBaseCharacterMovementComponent::UpdateFlightMode()
{
	const bool bShouldFly = bEnableFlyDomain && (bWantsToFly || !bEnableGroundDomain);

	if (bShouldFly)
	{
		// Swimming keeps priority: flight resumes after leaving the water because bWantsToFly stays set.
		if (MovementMode == MOVE_Walking || MovementMode == MOVE_NavWalking || MovementMode == MOVE_Falling)
		{
			SetMovementMode(MOVE_Flying);
		}
	}
	else if (MovementMode == MOVE_Flying)
	{
		SetMovementMode(GetLandingMovementMode());
	}
}

EMovementMode UBaseCharacterMovementComponent::GetLandingMovementMode() const
{
	const APhysicsVolume* Volume = GetPhysicsVolume();
	if (bEnableWaterDomain && Volume && Volume->bWaterVolume)
	{
		return MOVE_Swimming;
	}
	return MOVE_Falling;
}

void UBaseCharacterMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);
	RefreshRotationPolicy();
}

// ----------------------------------------------------------------------
// Rotation
// ----------------------------------------------------------------------

void UBaseCharacterMovementComponent::RefreshRotationPolicy()
{
	if (!CharacterOwner)
	{
		return;
	}

	if (MovementMode == MOVE_Flying)
	{
		if (bOrientRotationToFlight)
		{
			// Face the flight vector. Pitch is allowed because the engine only forces vertical on ground and in the air.
			bOrientRotationToMovement = true;
			CharacterOwner->bUseControllerRotationYaw = false;
		}
		else
		{
			// Stay upright and turn with the camera.
			bOrientRotationToMovement = false;
			CharacterOwner->bUseControllerRotationYaw = true;
		}
	}
	else if (bFaceControlRotation)
	{
		bOrientRotationToMovement = false;
		CharacterOwner->bUseControllerRotationYaw = true;
	}
	else
	{
		bOrientRotationToMovement = true;
		CharacterOwner->bUseControllerRotationYaw = false;
	}
}

void UBaseCharacterMovementComponent::PhysicsRotation(float DeltaTime)
{
	Super::PhysicsRotation(DeltaTime);

	// Level out when flying without input. The engine leaves the rotation untouched in that case.
	if (MovementMode == MOVE_Flying && bOrientRotationToFlight && Acceleration.IsNearlyZero() && UpdatedComponent)
	{
		const FRotator Current = UpdatedComponent->GetComponentRotation();
		const FRotator Upright(0.0f, Current.Yaw, 0.0f);

		if (!Current.Equals(Upright, 0.1f))
		{
			const FRotator Target = FMath::RInterpTo(Current, Upright, DeltaTime, FlightUprightInterpSpeed);
			MoveUpdatedComponent(FVector::ZeroVector, Target, /*bSweep=*/false);
		}
	}
}

// ----------------------------------------------------------------------
// Physics hooks
// ----------------------------------------------------------------------

void UBaseCharacterMovementComponent::PhysFlying(float deltaTime, int32 Iterations)
{
	if (IsGliding() && UpdatedComponent)
	{
		// Keep pushing forward and sink slowly. Input still steers because it is blended into the acceleration.
		const float MaxAccel = GetMaxAcceleration();
		const FVector Forward = UpdatedComponent->GetForwardVector();
		Acceleration = (Acceleration + Forward * MaxAccel).GetClampedToMaxSize(MaxAccel);
		Acceleration.Z -= GlideSinkAcceleration;
	}

	Super::PhysFlying(deltaTime, Iterations);
}

void UBaseCharacterMovementComponent::PhysSwimming(float deltaTime, int32 Iterations)
{
	// Diving is inferred from the downward input acceleration, which is identical on client and server.
	const bool bDiving = Acceleration.Z < -DiveAccelerationThreshold;
	Buoyancy = (bAutoResurface && !bDiving) ? SurfaceBuoyancy : NeutralBuoyancy;

	Super::PhysSwimming(deltaTime, Iterations);
}

// ----------------------------------------------------------------------
// Burst actions
// ----------------------------------------------------------------------

const FMovementActionSettings* UBaseCharacterMovementComponent::GetActionSettings(EMovementAction Action, EMovementDomain Domain) const
{
	switch (Domain)
	{
	case EMovementDomain::Ground:
		return Action == EMovementAction::Dash ? &GroundDash : &GroundDodge;
	case EMovementDomain::Fly:
		return Action == EMovementAction::Dash ? &FlyDash : &FlyDodge;
	case EMovementDomain::Water:
		return Action == EMovementAction::Dash ? &UnderwaterDash : nullptr;
	default:
		return nullptr;
	}
}

bool UBaseCharacterMovementComponent::IsReplayingSavedMoves() const
{
	if (CharacterOwner && CharacterOwner->GetLocalRole() == ROLE_AutonomousProxy)
	{
		if (const FNetworkPredictionData_Client_Character* ClientData = GetPredictionData_Client_Character())
		{
			return ClientData->bUpdatePosition;
		}
	}
	return false;
}

FVector UBaseCharacterMovementComponent::ComputeActionDirection(EMovementAction Action, EMovementDomain Domain) const
{
	FVector Input = Acceleration.GetSafeNormal();
	FVector Forward = UpdatedComponent->GetForwardVector();
	FVector Right = UpdatedComponent->GetRightVector();

	if (Domain == EMovementDomain::Ground)
	{
		Input = Input.GetSafeNormal2D();
		Forward = Forward.GetSafeNormal2D();
	}

	if (!Input.IsNearlyZero())
	{
		return Input;
	}

	if (Action == EMovementAction::Dash)
	{
		return Forward;
	}

	// Dodge without input: back-step on the ground, side-step in the air.
	return Domain == EMovementDomain::Fly ? Right : -Forward;
}

void UBaseCharacterMovementComponent::ExecuteAction(EMovementAction Action)
{
	const UWorld* World = GetWorld();
	if (!CharacterOwner || !UpdatedComponent || !World)
	{
		return;
	}

	const EMovementDomain Domain = GetMovementDomain();
	const FMovementActionSettings* Settings = GetActionSettings(Action, Domain);
	if (!Settings || !Settings->bEnabled)
	{
		return;
	}
	if (Domain == EMovementDomain::Water && GetWaterZone() != EWaterZone::Underwater)
	{
		return;
	}

	const bool bReplaying = IsReplayingSavedMoves();

	if (!bReplaying)
	{
		const double Now = World->GetTimeSeconds();
		double& NextAllowedTime = (Action == EMovementAction::Dash) ? NextDashTime : NextDodgeTime;

		// The server re-checks the cooldown for remote players. Clients already gated the request locally.
		if (CharacterOwner->HasAuthority() && !CharacterOwner->IsLocallyControlled()
			&& Now + ServerCooldownTolerance < NextAllowedTime)
		{
			return;
		}

		NextAllowedTime = Now + Settings->Cooldown;
	}

	const FVector Direction = ComputeActionDirection(Action, Domain);
	if (Direction.IsNearlyZero())
	{
		return;
	}

	// The velocity change is stateless, so replaying the same saved move reproduces it exactly.
	if (Domain == EMovementDomain::Ground)
	{
		Velocity.X = Direction.X * Settings->Speed;
		Velocity.Y = Direction.Y * Settings->Speed;
	}
	else
	{
		Velocity = Direction * Settings->Speed;
	}

	if (!bReplaying)
	{
		OnActionStarted.Broadcast(Action, Domain, Settings->StateDuration);
	}
}
