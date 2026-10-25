// BaseCharacter.cpp
// Character Framework - Enhanced Input character with first/third person perspective and framework wiring.

#include "Character/BaseCharacter.h"

#include "Camera/CameraComponent.h"
#include "CameraSystem/CameraManagerComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Character/InputConfigData.h"
#include "Movement/BaseCharacterMovementComponent.h"
#include "StateMachine/StateMachineComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "InputMappingContext.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY_STATIC(LogBaseCharacter, Log, All);

namespace
{
	FGameplayTag DomainTagFor(EMovementDomain Domain)
	{
		switch (Domain)
		{
		case EMovementDomain::Fly:
			return TAG_Frameworks_Domain_Fly.GetTag();
		case EMovementDomain::Water:
			return TAG_Frameworks_Domain_Water.GetTag();
		default:
			return TAG_Frameworks_Domain_Ground.GetTag();
		}
	}

	/** Maps a movement action to the tag the animation layer understands. Invalid tag = no state for that combination. */
	FGameplayTag ActionTagFor(EMovementAction Action, EMovementDomain Domain)
	{
		const bool bDash = (Action == EMovementAction::Dash);
		switch (Domain)
		{
		case EMovementDomain::Ground:
			return bDash ? TAG_Frameworks_State_Ground_Dash.GetTag() : TAG_Frameworks_State_Ground_Roll.GetTag();
		case EMovementDomain::Fly:
			return bDash ? TAG_Frameworks_State_Fly_Dash.GetTag() : TAG_Frameworks_State_Fly_Dodge.GetTag();
		case EMovementDomain::Water:
			return bDash ? TAG_Frameworks_State_Water_Underwater_Dash.GetTag() : FGameplayTag();
		default:
			return FGameplayTag();
		}
	}
}

// ----------------------------------------------------------------------
// Construction
// ----------------------------------------------------------------------

ABaseCharacter::ABaseCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UBaseCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
	, LastWaterZone(EWaterZone::None)
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	// Standard character capsule (matches the default ACharacter physics setup).
	GetCapsuleComponent()->InitCapsuleSize(34.0f, 88.0f);

	// Rotation is owned by the movement component (see SetFaceControlRotation and the control modes).
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Third-person mesh: everyone sees it, the local player only sees it in third person.
	USkeletalMeshComponent* BodyMesh = GetMesh();
	BodyMesh->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, -88.0f), FRotator(0.0f, -90.0f, 0.0f));
	BodyMesh->bCastHiddenShadow = true;
	// The first-person camera rides on this mesh, so its pose must update even while it is hidden from the owner.
	BodyMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	// Camera rig: the manager creates the spring arm and the camera at runtime and owns every profile.
	CameraManager = CreateDefaultSubobject<UCameraManagerComponent>(TEXT("CameraManager"));

	// First person: the camera hangs on FirstPersonCameraAnchor, a point relative to the capsule, so it needs no body
	// mesh and no socket. To follow the animated head of a visible body instead, set Attach Socket Name on this
	// profile (CameraManager > Camera Profiles); PostInitializeComponents then attaches the rig to the body mesh.
	FCameraProfile FirstPersonProfile;
	FirstPersonProfile.ArmLength = 0.0f;
	FirstPersonProfile.TargetOffset = FVector::ZeroVector;
	FirstPersonProfile.SocketOffset = FVector::ZeroVector;
	FirstPersonProfile.AttachOffset = FVector::ZeroVector;
	FirstPersonProfile.bAllowZoom = false;
	FirstPersonProfile.bDoCollisionTest = false;
	FirstPersonProfile.bFirstPerson = true;
	FirstPersonProfile.bAttachToSocketParent = true;
	CameraManager->CameraProfiles.Add(FirstPersonCameraName, FirstPersonProfile);

	// Third person: spring arm on the capsule, raised to head height.
	FCameraProfile ThirdPersonProfile;
	ThirdPersonProfile.ArmLength = 350.0f;
	ThirdPersonProfile.TargetOffset = FVector(0.0f, 0.0f, 60.0f);
	ThirdPersonProfile.SocketOffset = FVector(0.0f, 0.0f, 30.0f);
	ThirdPersonProfile.bDoCollisionTest = true;
	CameraManager->CameraProfiles.Add(ThirdPersonCameraName, ThirdPersonProfile);
	CameraManager->DefaultCameraName = ThirdPersonCameraName;

	// Eye point of the first-person view: a plain scene component under the capsule, placed in the Blueprint viewport.
	FirstPersonCameraAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("FirstPersonCameraAnchor"));
	FirstPersonCameraAnchor->SetupAttachment(GetRootComponent());
	FirstPersonCameraAnchor->SetRelativeLocation(FVector(0.0f, 0.0f, 64.0f));

	// First-person arms follow the managed camera (attached in PostInitializeComponents) and are only rendered for the owner.
	// At design time they are a child of the eye point, so their transform is relative to where the camera will sit.
	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FirstPersonMesh"));
	FirstPersonMesh->SetupAttachment(FirstPersonCameraAnchor);
	FirstPersonMesh->SetOnlyOwnerSee(true);
	FirstPersonMesh->SetOwnerNoSee(false);
	FirstPersonMesh->CastShadow = false;
	FirstPersonMesh->bCastDynamicShadow = false;
	FirstPersonMesh->bReceivesDecals = false;
	FirstPersonMesh->SetCollisionProfileName(TEXT("NoCollision"));
	FirstPersonMesh->SetGenerateOverlapEvents(false);
	FirstPersonMesh->SetHiddenInGame(true);

	// State machine (replicates its own properties).
	StateMachine = CreateDefaultSubobject<UStateMachineComponent>(TEXT("StateMachine"));
}

void ABaseCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if (UBaseCharacterMovementComponent* Movement = GetBaseMovement())
	{
		Movement->OnActionStarted.AddUObject(this, &ABaseCharacter::HandleMovementActionStarted);

		// Order each frame: movement -> state machine update (this actor) -> animation (meshes).
		AddTickPrerequisiteComponent(Movement);

		if (StateMachine)
		{
			StateMachine->ConfigureDomains(Movement->bEnableGroundDomain, Movement->bEnableFlyDomain, Movement->bEnableWaterDomain);
		}
	}

	if (CameraManager)
	{
		// Decides whether the first-person profile hangs on the anchor or on a socket of the body mesh.
		ConfigureFirstPersonCameraAttachment();

		// The managed camera exists once the manager is initialized (never on a dedicated server).
		// The arms keep their relative transform: relative to the anchor at design time, relative to the camera at
		// runtime. At rest the camera sits at the anchor, so both describe the same place.
		if (FirstPersonMesh)
		{
			if (UCameraComponent* ManagedCamera = CameraManager->GetCameraComponent())
			{
				FirstPersonMesh->AttachToComponent(ManagedCamera, FAttachmentTransformRules::KeepRelativeTransform);
			}
		}
	}

	if (GetMesh())
	{
		GetMesh()->AddTickPrerequisiteActor(this);
	}
	if (FirstPersonMesh)
	{
		FirstPersonMesh->AddTickPrerequisiteActor(this);
	}
}

void ABaseCharacter::BeginPlay()
{
	Super::BeginPlay();

	ActivePerspective = DefaultPerspective;
	ApplyMovementRotationPolicy();
	RefreshPerspectiveVisuals();
}

void ABaseCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateStateMachine();
}

void ABaseCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// The owner applies its own perspective immediately; everyone else learns it from the server.
	DOREPLIFETIME_CONDITION(ABaseCharacter, ActivePerspective, COND_SkipOwner);
}

UBaseCharacterMovementComponent* ABaseCharacter::GetBaseMovement() const
{
	return Cast<UBaseCharacterMovementComponent>(GetCharacterMovement());
}

bool ABaseCharacter::IsGroundDomain() const
{
	const UBaseCharacterMovementComponent* Movement = GetBaseMovement();
	return !Movement || Movement->GetMovementDomain() == EMovementDomain::Ground;
}

// ----------------------------------------------------------------------
// Possession and input setup
// ----------------------------------------------------------------------

void ABaseCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// Covers the listen-server host, where PawnClientRestart is not the first local hook.
	RefreshPerspectiveVisuals();
}

void ABaseCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();

	AddInputMappingContext();
	RefreshPerspectiveVisuals();
}

void ABaseCharacter::AddInputMappingContext()
{
	if (!InputConfig || !InputConfig->DefaultMappingContext)
	{
		return;
	}

	const APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (!PlayerController)
	{
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
	{
		// Remove first so a respawn never stacks the same context twice.
		Subsystem->RemoveMappingContext(InputConfig->DefaultMappingContext);
		Subsystem->AddMappingContext(InputConfig->DefaultMappingContext, InputConfig->MappingPriority);
	}
}

void ABaseCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!Input || !InputConfig)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: no Enhanced Input component or InputConfig set. Input is disabled."), *GetNameSafe(this));
		return;
	}

	const FCharacterInputActions& Actions = InputConfig->Actions;

	if (Actions.Move)
	{
		Input->BindAction(Actions.Move, ETriggerEvent::Triggered, this, &ABaseCharacter::Input_Move);
	}
	if (Actions.Look)
	{
		Input->BindAction(Actions.Look, ETriggerEvent::Triggered, this, &ABaseCharacter::Input_Look);
	}
	if (Actions.Jump)
	{
		Input->BindAction(Actions.Jump, ETriggerEvent::Started, this, &ABaseCharacter::Input_JumpStarted);
		Input->BindAction(Actions.Jump, ETriggerEvent::Triggered, this, &ABaseCharacter::Input_JumpHeld);
		Input->BindAction(Actions.Jump, ETriggerEvent::Completed, this, &ABaseCharacter::Input_JumpEnded);
	}
	if (Actions.Crouch)
	{
		Input->BindAction(Actions.Crouch, ETriggerEvent::Started, this, &ABaseCharacter::Input_CrouchStarted);
		Input->BindAction(Actions.Crouch, ETriggerEvent::Triggered, this, &ABaseCharacter::Input_CrouchHeld);
		Input->BindAction(Actions.Crouch, ETriggerEvent::Completed, this, &ABaseCharacter::Input_CrouchEnded);
	}
	if (Actions.Sprint)
	{
		Input->BindAction(Actions.Sprint, ETriggerEvent::Started, this, &ABaseCharacter::Input_SprintStarted);
		Input->BindAction(Actions.Sprint, ETriggerEvent::Completed, this, &ABaseCharacter::Input_SprintEnded);
	}
	if (Actions.Fly)
	{
		Input->BindAction(Actions.Fly, ETriggerEvent::Started, this, &ABaseCharacter::Input_Fly);
	}
	if (Actions.Dash)
	{
		Input->BindAction(Actions.Dash, ETriggerEvent::Started, this, &ABaseCharacter::Input_Dash);
	}
	if (Actions.Dodge)
	{
		Input->BindAction(Actions.Dodge, ETriggerEvent::Started, this, &ABaseCharacter::Input_Dodge);
	}
	if (Actions.PerspectiveToggle)
	{
		Input->BindAction(Actions.PerspectiveToggle, ETriggerEvent::Started, this, &ABaseCharacter::Input_PerspectiveToggle);
	}
}

// ----------------------------------------------------------------------
// Input handlers
// ----------------------------------------------------------------------

void ABaseCharacter::Input_Move(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	if (!Controller || Axis.IsNearlyZero())
	{
		return;
	}

	// The movement component decides how the camera steers:
	//   Ground and Standard (Fly / Water): camera yaw only, so pitch never changes the direction.
	//   Camera Steering (Sprint held in Fly / Water): full camera orientation, pitch included.
	const FRotator ControlRotation = Controller->GetControlRotation();
	const UBaseCharacterMovementComponent* Movement = GetBaseMovement();
	const FRotator Basis = Movement ? Movement->GetMoveBasisRotation(ControlRotation) : FRotator(0.0f, ControlRotation.Yaw, 0.0f);
	const FRotationMatrix BasisMatrix(Basis);

	AddMovementInput(BasisMatrix.GetUnitAxis(EAxis::X), Axis.Y);
	AddMovementInput(BasisMatrix.GetUnitAxis(EAxis::Y), Axis.X);
}

void ABaseCharacter::Input_Look(const FInputActionValue& Value)
{
	if (CameraManager)
	{
		CameraManager->AddLookInput(Value.Get<FVector2D>());
	}
}

void ABaseCharacter::Input_JumpStarted(const FInputActionValue& Value)
{
	UBaseCharacterMovementComponent* Movement = GetBaseMovement();
	if (!Movement)
	{
		Jump();
		return;
	}

	// Fly and Water: Jump is the Up button and acts while held (Input_JumpHeld). Nothing to do on press.
	if (Movement->GetMovementDomain() != EMovementDomain::Ground)
	{
		return;
	}

	if (bDoubleJumpActivatesFlight && Movement->bEnableFlyDomain && Movement->IsFalling())
	{
		// Airborne and Jump pressed again: take off.
		Movement->SetWantsToFly(true);
	}
	else
	{
		// Grounded: standard jump. Also the fallback when flight is unavailable, so multi-jump setups keep working.
		Jump();
	}
}

void ABaseCharacter::Input_JumpHeld(const FInputActionValue& Value)
{
	// Standard mode in Fly / Water: ascend straight up. Ignored on the ground and while Sprint is held.
	const UBaseCharacterMovementComponent* Movement = GetBaseMovement();
	if (Movement && Movement->AcceptsVerticalButtons())
	{
		AddMovementInput(FVector::UpVector, 1.0f);
	}
}

void ABaseCharacter::Input_JumpEnded(const FInputActionValue& Value)
{
	StopJumping();
}

void ABaseCharacter::Input_CrouchStarted(const FInputActionValue& Value)
{
	// Crouch exists on the ground only. In Fly / Water the same button is the Down button (Input_CrouchHeld).
	if (IsGroundDomain())
	{
		Crouch();
	}
}

void ABaseCharacter::Input_CrouchHeld(const FInputActionValue& Value)
{
	// Standard mode in Fly / Water: descend straight down. Ignored on the ground and while Sprint is held.
	const UBaseCharacterMovementComponent* Movement = GetBaseMovement();
	if (Movement && Movement->AcceptsVerticalButtons())
	{
		AddMovementInput(FVector::UpVector, -1.0f);
	}
}

void ABaseCharacter::Input_CrouchEnded(const FInputActionValue& Value)
{
	UnCrouch();
}

void ABaseCharacter::Input_SprintStarted(const FInputActionValue& Value)
{
	// On the ground: run. In Fly / Water: switch to Camera Steering with the fast speed.
	if (UBaseCharacterMovementComponent* Movement = GetBaseMovement())
	{
		Movement->SetWantsToSprint(true);
	}
}

void ABaseCharacter::Input_SprintEnded(const FInputActionValue& Value)
{
	// Back to walking, or back to Standard control in Fly / Water.
	if (UBaseCharacterMovementComponent* Movement = GetBaseMovement())
	{
		Movement->SetWantsToSprint(false);
	}
}

void ABaseCharacter::Input_Fly(const FInputActionValue& Value)
{
	if (UBaseCharacterMovementComponent* Movement = GetBaseMovement())
	{
		Movement->ToggleFlying();
	}
}

void ABaseCharacter::Input_Dash(const FInputActionValue& Value)
{
	if (UBaseCharacterMovementComponent* Movement = GetBaseMovement())
	{
		Movement->RequestDash();
	}
}

void ABaseCharacter::Input_Dodge(const FInputActionValue& Value)
{
	if (UBaseCharacterMovementComponent* Movement = GetBaseMovement())
	{
		Movement->RequestDodge();
	}
}

void ABaseCharacter::Input_PerspectiveToggle(const FInputActionValue& Value)
{
	TogglePerspective();
}

// ----------------------------------------------------------------------
// Perspective
// ----------------------------------------------------------------------

void ABaseCharacter::TogglePerspective()
{
	SetPerspective(ActivePerspective == ECharacterPerspective::FirstPerson ? ECharacterPerspective::ThirdPerson : ECharacterPerspective::FirstPerson);
}

void ABaseCharacter::SetPerspective(ECharacterPerspective NewPerspective)
{
	if (!bAllowPerspectiveToggle || !IsLocallyControlled() || NewPerspective == ActivePerspective)
	{
		return;
	}

	ApplyPerspective(NewPerspective);

	if (!HasAuthority())
	{
		ServerSetPerspective(NewPerspective);
	}
}

void ABaseCharacter::ServerSetPerspective_Implementation(ECharacterPerspective NewPerspective)
{
	if (NewPerspective != ActivePerspective)
	{
		ApplyPerspective(NewPerspective);
	}
}

void ABaseCharacter::ApplyPerspective(ECharacterPerspective NewPerspective)
{
	const ECharacterPerspective OldPerspective = ActivePerspective;
	ActivePerspective = NewPerspective;

	// The server must use the same rotation policy as the owning client, otherwise moves would be corrected.
	ApplyMovementRotationPolicy();
	RefreshPerspectiveVisuals();

	OnPerspectiveChanged.Broadcast(NewPerspective, OldPerspective);
	BP_OnPerspectiveChanged(NewPerspective, OldPerspective);
}

void ABaseCharacter::OnRep_ActivePerspective(ECharacterPerspective OldPerspective)
{
	// Other players only need the notification; they always see the third-person mesh.
	OnPerspectiveChanged.Broadcast(ActivePerspective, OldPerspective);
	BP_OnPerspectiveChanged(ActivePerspective, OldPerspective);
}

void ABaseCharacter::ApplyMovementRotationPolicy()
{
	if (UBaseCharacterMovementComponent* Movement = GetBaseMovement())
	{
		Movement->SetFaceControlRotation(ActivePerspective == ECharacterPerspective::FirstPerson);
	}
}

void ABaseCharacter::RefreshPerspectiveVisuals()
{
	// Only the machine that owns the view shows first person. Everyone else keeps the third-person mesh.
	const bool bFirstPersonView = IsLocallyControlled() && ActivePerspective == ECharacterPerspective::FirstPerson;

	// Switching perspective is a cut, as before. The manager is a no-op on dedicated servers.
	if (CameraManager)
	{
		CameraManager->SwitchToCamera(bFirstPersonView ? FirstPersonCameraName : ThirdPersonCameraName, 0.0f);
	}
	if (FirstPersonMesh)
	{
		FirstPersonMesh->SetHiddenInGame(!bFirstPersonView);
	}
	if (USkeletalMeshComponent* BodyMesh = GetMesh())
	{
		BodyMesh->SetOwnerNoSee(bFirstPersonView);
	}
}

void ABaseCharacter::ConfigureFirstPersonCameraAttachment()
{
	if (!CameraManager)
	{
		return;
	}

	// Default: the camera hangs on the anchor, which sits relative to the capsule. No mesh and no socket are involved.
	USceneComponent* AttachComponent = FirstPersonCameraAnchor;

	if (const FCameraProfile* Profile = CameraManager->CameraProfiles.Find(FirstPersonCameraName))
	{
		FCameraProfile Resolved = *Profile;
		bool bProfileChanged = false;
		bool bUseBodySocket = false;

		// A first-person profile that names a socket (a visible body with an animated view) attaches to the body mesh,
		// but only when that socket really exists. A missing head bone must never move the camera to the floor.
		if (!Resolved.AttachSocketName.IsNone())
		{
			USkeletalMeshComponent* BodyMesh = GetMesh();
			if (BodyMesh && BodyMesh->DoesSocketExist(Resolved.AttachSocketName))
			{
				AttachComponent = BodyMesh;
				bUseBodySocket = true;
			}
			else
			{
				UE_LOG(LogBaseCharacter, Warning, TEXT("%s: first-person profile '%s' names socket '%s', which the body mesh does not have. The camera uses FirstPersonCameraAnchor instead."),
					*GetNameSafe(this), *FirstPersonCameraName.ToString(), *Resolved.AttachSocketName.ToString());
				Resolved.AttachSocketName = NAME_None;
				bProfileChanged = true;
			}
		}

		// Without a body socket the profile always hangs on the anchor. This also covers a Blueprint that saved its own
		// copy of the profile map before this option existed.
		if (!bUseBodySocket && !Resolved.bAttachToSocketParent)
		{
			Resolved.bAttachToSocketParent = true;
			bProfileChanged = true;
		}

		if (bProfileChanged)
		{
			CameraManager->RegisterCameraProfile(FirstPersonCameraName, Resolved);
		}
	}

	CameraManager->SetSocketParent(AttachComponent);
}

// ----------------------------------------------------------------------
// State machine driver
// ----------------------------------------------------------------------

bool ABaseCharacter::RequestAction(FGameplayTag ActionTag, float Duration)
{
	if (!StateMachine || !ActionTag.IsValid() || GetLocalRole() == ROLE_SimulatedProxy)
	{
		return false;
	}

	const bool bStarted = StateMachine->BeginAction(ActionTag, Duration);

	if (bStarted && !HasAuthority() && IsLocallyControlled())
	{
		ServerRequestAction(ActionTag, Duration);
	}

	return bStarted;
}

void ABaseCharacter::ServerRequestAction_Implementation(FGameplayTag ActionTag, float Duration)
{
	if (StateMachine)
	{
		// The state machine rejects tags that do not belong to the active domain.
		StateMachine->BeginAction(ActionTag, FMath::Clamp(Duration, -1.0f, 10.0f));
	}
}

void ABaseCharacter::HandleMovementActionStarted(EMovementAction Action, EMovementDomain Domain, float Duration)
{
	if (!StateMachine)
	{
		return;
	}

	// The action may start on the same frame the domain changed.
	StateMachine->SetDomain(DomainTagFor(Domain));

	const FGameplayTag ActionTag = ActionTagFor(Action, Domain);
	if (ActionTag.IsValid())
	{
		StateMachine->BeginAction(ActionTag, Duration);
	}
}

void ABaseCharacter::UpdateStateMachine()
{
	UBaseCharacterMovementComponent* Movement = GetBaseMovement();

	// Simulated proxies mirror the server through replication.
	if (!StateMachine || !Movement || GetLocalRole() == ROLE_SimulatedProxy)
	{
		return;
	}

	const EMovementDomain Domain = Movement->GetMovementDomain();
	StateMachine->SetDomain(DomainTagFor(Domain));

	const FVector Velocity = GetVelocity();
	const float ThresholdSquared = FMath::Square(MovingSpeedThreshold);
	const bool bHasInput = Movement->HasMovementInput();
	const bool bSprinting = Movement->IsSprinting();

	FGameplayTag State;

	switch (Domain)
	{
	case EMovementDomain::Fly:
		{
			LastWaterZone = EWaterZone::None;

			// Glide is the Camera Steering flight (Sprint held) while actually moving. Standing still shows Idle.
			const bool bMoving = Velocity.SizeSquared() > ThresholdSquared;
			if (bMoving && Movement->IsGliding())
			{
				State = TAG_Frameworks_State_Fly_Glide.GetTag();
			}
			else
			{
				State = bMoving ? TAG_Frameworks_State_Fly_Move.GetTag() : TAG_Frameworks_State_Fly_Idle.GetTag();
			}
			break;
		}

	case EMovementDomain::Water:
		{
			const EWaterZone Zone = Movement->GetWaterZone();

			// Crossing the surface plays a short transition action on top of the locomotion state.
			if (LastWaterZone == EWaterZone::Surface && Zone == EWaterZone::Underwater)
			{
				StateMachine->BeginAction(TAG_Frameworks_State_Water_Transition_ToUnderwater.GetTag(), WaterTransitionDuration);
			}
			else if (LastWaterZone == EWaterZone::Underwater && Zone == EWaterZone::Surface)
			{
				StateMachine->BeginAction(TAG_Frameworks_State_Water_Transition_ToSurface.GetTag(), WaterTransitionDuration);
			}
			LastWaterZone = Zone;

			const bool bMoving = bHasInput && Velocity.SizeSquared() > ThresholdSquared;

			if (Zone == EWaterZone::Underwater)
			{
				if (Movement->IsResurfacing(Zone))
				{
					State = TAG_Frameworks_State_Water_Resurfacing.GetTag();
				}
				else if (!bMoving)
				{
					State = TAG_Frameworks_State_Water_Underwater_Idle.GetTag();
				}
				else
				{
					State = bSprinting ? TAG_Frameworks_State_Water_Underwater_FastSwim.GetTag() : TAG_Frameworks_State_Water_Underwater_Move.GetTag();
				}
			}
			else
			{
				if (!bMoving)
				{
					State = TAG_Frameworks_State_Water_Surface_Idle.GetTag();
				}
				else
				{
					State = bSprinting ? TAG_Frameworks_State_Water_Surface_FastSwim.GetTag() : TAG_Frameworks_State_Water_Surface_Move.GetTag();
				}
			}
			break;
		}

	default:
		{
			LastWaterZone = EWaterZone::None;

			const bool bMoving = bHasInput && Velocity.SizeSquared2D() > ThresholdSquared;

			if (Movement->IsFalling())
			{
				State = TAG_Frameworks_State_Ground_Jump.GetTag();
			}
			else if (Movement->IsCrouching())
			{
				State = TAG_Frameworks_State_Ground_Crouch.GetTag();
			}
			else if (!bMoving)
			{
				State = TAG_Frameworks_State_Ground_Idle.GetTag();
			}
			else
			{
				State = bSprinting ? TAG_Frameworks_State_Ground_Run.GetTag() : TAG_Frameworks_State_Ground_Walk.GetTag();
			}
			break;
		}
	}

	StateMachine->SetLocomotionState(State);
}