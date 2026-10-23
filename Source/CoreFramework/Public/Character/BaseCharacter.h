// BaseCharacter.h
// Character Framework - Enhanced Input character with first/third person perspective and framework wiring.
//
// COMPOSITION ROOT
//   ABaseCharacter is the only class that knows all frameworks:
//     UBaseCharacterMovementComponent  movement facts, control modes and predicted actions
//     UCameraManagerComponent          camera rig, profiles, blending, FOV, zoom and look input
//     UStateMachineComponent           Gameplay Tag state store (this class is its driver)
//     UInputConfigData                 input actions
//   Data flow each frame:  Movement -> ABaseCharacter::UpdateStateMachine -> StateMachine -> AnimationInstance.
//   Remove the state machine or use another movement component and the character still runs; only the parts that
//   depend on the missing piece are skipped.
//
// INPUT ROUTING (the movement component decides, this class only forwards)
//   Look     Forwarded to the camera manager.
//   Move     Ground: planar along camera yaw.  Fly / Water, Standard: planar along camera yaw.
//            Fly / Water, Sprint held (Camera Steering): full 3D along the camera, pitch included.
//   Jump     Ground: jump.  Airborne (falling): take off into flight.  Fly / Water, Standard: ascend while held.
//   Crouch   Ground: crouch.  Fly / Water, Standard: descend while held.
//   Sprint   Ground: run.  Fly / Water: hold to switch to Camera Steering (and glide / fast swim speed).
//            Jump and Crouch are ignored as vertical buttons while Sprint is held in Fly or Water.
//   Fly      Toggles flight.
//   Swimming has no input: it starts and ends with the PhysicsVolume (bWaterVolume).
//
// COMPONENT RIG (all created in the constructor)
//   GetMesh()                Third-person body mesh. Optional: a pure first-person character leaves it empty.
//                            Replicated to everyone. Hidden from the local player in first person.
//   FirstPersonCameraAnchor  Eye point of the first-person view, a child of the capsule. Move it in the Blueprint
//                            viewport to place the first-person camera relative to the capsule.
//   FirstPersonMesh          First-person arms. Visible to the owning player only. A child of the anchor at design
//                            time, so it is placed relative to the eye point; at runtime it follows the managed camera
//                            with the same relative transform.
//   CameraManager            Owns the spring arm and the camera. The "FirstPerson" profile hangs the rig on the anchor
//                            (no mesh and no socket needed), or on the body mesh when the profile names a socket that
//                            exists (a visible body with an animated view). The "ThirdPerson" profile attaches to the
//                            capsule.
//
// PERSPECTIVE
//   The perspective (first or third person) is character state: it is replicated and it selects the rotation policy
//   of the movement component. This class maps it to a camera profile name and leaves everything else to the
//   camera manager. Other camera profiles (aim, cinematic) are driven through GetCameraManager().
//   A pure first-person game sets DefaultPerspective to FirstPerson and bAllowPerspectiveToggle to false.
//
// PORTING: copy this folder, keep COREFRAMEWORK_API (or rename it to your module macro).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameplayTagContainer.h"
#include "InputActionValue.h"
#include "BaseCharacter.generated.h"

class USceneComponent;
class USkeletalMeshComponent;
class UCameraManagerComponent;
class UInputConfigData;
class UStateMachineComponent;
class UBaseCharacterMovementComponent;
enum class EMovementAction : uint8;
enum class EMovementDomain : uint8;
enum class EWaterZone : uint8;

UENUM(BlueprintType)
enum class ECharacterPerspective : uint8
{
	FirstPerson	UMETA(DisplayName = "First Person"),
	ThirdPerson	UMETA(DisplayName = "Third Person")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCharacterPerspectiveChangedSignature, ECharacterPerspective, NewPerspective, ECharacterPerspective, OldPerspective);

UCLASS()
class COREFRAMEWORK_API ABaseCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ABaseCharacter(const FObjectInitializer& ObjectInitializer);

	//~ Begin AActor Interface
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~ End AActor Interface

	//~ Begin APawn Interface
	virtual void PawnClientRestart() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	//~ End APawn Interface

	// ------------------------------------------------------------------
	// Components
	// ------------------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character|Components")
	TObjectPtr<UStateMachineComponent> StateMachine;

	/**
	 * Eye point of the first-person view, a child of the capsule (default: 64 cm above the capsule center).
	 * Select it in the Components panel and move it to place the first-person camera relative to the capsule, as in the
	 * standard first-person templates. The camera rig attaches to it while the first-person profile is active, and
	 * FirstPersonMesh is its child. It needs no mesh and no socket. Fine adjustments per profile (for example a lower
	 * eye while crouching) use Attach Offset on the camera profile.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character|Components")
	TObjectPtr<USceneComponent> FirstPersonCameraAnchor;

	/**
	 * First-person arms. Owner-only. Give it its own Animation Blueprint based on the AnimationInstance class.
	 * Its Location, Rotation and Scale in the Components panel are relative to the eye point (FirstPersonCameraAnchor)
	 * and, at runtime, to the first-person camera, which sits at the same place.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character|Components")
	TObjectPtr<USkeletalMeshComponent> FirstPersonMesh;

	/** Owns the camera rig. Tune cameras in its CameraProfiles map. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character|Components")
	TObjectPtr<UCameraManagerComponent> CameraManager;

	// ------------------------------------------------------------------
	// Configuration
	// ------------------------------------------------------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character|Input")
	TObjectPtr<UInputConfigData> InputConfig;

	/**
	 * Pressing Jump while airborne (falling) starts flight. Ignored when the Fly domain is disabled,
	 * in which case the press is a normal jump again (for example for multi-jump games).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character|Input")
	bool bDoubleJumpActivatesFlight = true;

	/** Perspective used when the character spawns. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character|Camera")
	ECharacterPerspective DefaultPerspective = ECharacterPerspective::ThirdPerson;

	/** Set to false to lock the perspective (the toggle input then does nothing). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character|Camera")
	bool bAllowPerspectiveToggle = true;

	/**
	 * Camera profile used in first person. Its placement is data on that profile (CameraManager > Camera Profiles):
	 * Attach Offset relative to the anchor, an optional Attach Socket Name for a visible body, field of view and so on.
	 * With no socket named, the camera hangs on FirstPersonCameraAnchor and no head bone is needed.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character|Camera")
	FName FirstPersonCameraName = TEXT("FirstPerson");

	/** Camera profile used in third person, and by every machine that does not own the view. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character|Camera")
	FName ThirdPersonCameraName = TEXT("ThirdPerson");

	/** Horizontal speed (cm/s) above which the character counts as moving for state selection. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character|States", meta = (ClampMin = "0.0", Units = "cm/s"))
	float MovingSpeedThreshold = 3.0f;

	/** Length (seconds) of the Surface <-> Underwater transition state. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character|States", meta = (ClampMin = "0.0", Units = "s"))
	float WaterTransitionDuration = 0.6f;

	// ------------------------------------------------------------------
	// Perspective
	// ------------------------------------------------------------------

	/** Switches view. Call on the locally controlled character; the server is informed automatically. */
	UFUNCTION(BlueprintCallable, Category = "Character|Camera")
	void SetPerspective(ECharacterPerspective NewPerspective);

	UFUNCTION(BlueprintCallable, Category = "Character|Camera")
	void TogglePerspective();

	UFUNCTION(BlueprintPure, Category = "Character|Camera")
	ECharacterPerspective GetPerspective() const { return ActivePerspective; }

	/** Fires on every machine that learns about a perspective change. Bind UI, crosshair or audio here. */
	UPROPERTY(BlueprintAssignable, Category = "Character|Camera")
	FCharacterPerspectiveChangedSignature OnPerspectiveChanged;

	/** Blueprint hook for the same event. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Character|Camera", meta = (DisplayName = "On Perspective Changed"))
	void BP_OnPerspectiveChanged(ECharacterPerspective NewPerspective, ECharacterPerspective OldPerspective);

	// ------------------------------------------------------------------
	// State machine access
	// ------------------------------------------------------------------

	/**
	 * Starts a timed action state (Slide, Mantle, Attack ...) from gameplay code.
	 * Runs locally at once and is forwarded to the server. Duration below zero uses the state machine default.
	 */
	UFUNCTION(BlueprintCallable, Category = "Character|States")
	bool RequestAction(FGameplayTag ActionTag, float Duration = -1.0f);

	UFUNCTION(BlueprintPure, Category = "Character")
	UBaseCharacterMovementComponent* GetBaseMovement() const;

	UFUNCTION(BlueprintPure, Category = "Character")
	UStateMachineComponent* GetStateMachine() const { return StateMachine; }

	/** Entry point for aim, cinematic and any other camera request. */
	UFUNCTION(BlueprintPure, Category = "Character")
	UCameraManagerComponent* GetCameraManager() const { return CameraManager; }

protected:
	// Input handlers. Override to change behavior; each is bound only when its action is set in InputConfig.
	virtual void Input_Move(const FInputActionValue& Value);
	virtual void Input_Look(const FInputActionValue& Value);
	virtual void Input_JumpStarted(const FInputActionValue& Value);
	virtual void Input_JumpHeld(const FInputActionValue& Value);
	virtual void Input_JumpEnded(const FInputActionValue& Value);
	virtual void Input_CrouchStarted(const FInputActionValue& Value);
	virtual void Input_CrouchHeld(const FInputActionValue& Value);
	virtual void Input_CrouchEnded(const FInputActionValue& Value);
	virtual void Input_SprintStarted(const FInputActionValue& Value);
	virtual void Input_SprintEnded(const FInputActionValue& Value);
	virtual void Input_Fly(const FInputActionValue& Value);
	virtual void Input_Dash(const FInputActionValue& Value);
	virtual void Input_Dodge(const FInputActionValue& Value);
	virtual void Input_PerspectiveToggle(const FInputActionValue& Value);

	/** Pushes domain and locomotion state into the state machine. Runs on the server and the owning client. */
	virtual void UpdateStateMachine();

private:
	UFUNCTION(Server, Reliable)
	void ServerSetPerspective(ECharacterPerspective NewPerspective);

	UFUNCTION(Server, Reliable)
	void ServerRequestAction(FGameplayTag ActionTag, float Duration);

	UFUNCTION()
	void OnRep_ActivePerspective(ECharacterPerspective OldPerspective);

	void HandleMovementActionStarted(EMovementAction Action, EMovementDomain Domain, float Duration);
	void AddInputMappingContext();
	void ApplyPerspective(ECharacterPerspective NewPerspective);
	void ApplyMovementRotationPolicy();
	void RefreshPerspectiveVisuals();

	/** Chooses what the first-person profile attaches to: the body mesh socket it names, or FirstPersonCameraAnchor. */
	void ConfigureFirstPersonCameraAttachment();

	/** True on the ground (or when there is no framework movement component). */
	bool IsGroundDomain() const;

	/** Replicated to other players (the owner drives its own value). */
	UPROPERTY(ReplicatedUsing = OnRep_ActivePerspective)
	ECharacterPerspective ActivePerspective = ECharacterPerspective::ThirdPerson;

	/** Previous water zone, used to detect Surface <-> Underwater transitions. */
	EWaterZone LastWaterZone;
};