// BaseCharacter.h
// Character Framework - Enhanced Input character with first/third person rig and framework wiring.
//
// COMPOSITION ROOT
//   ABaseCharacter is the only class that knows all frameworks:
//     UBaseCharacterMovementComponent  movement facts, control modes and predicted actions
//     UStateMachineComponent           Gameplay Tag state store (this class is its driver)
//     UInputConfigData                 input actions
//   Data flow each frame:  Movement -> ABaseCharacter::UpdateStateMachine -> StateMachine -> AnimationInstance.
//   Remove the state machine or use another movement component and the character still runs; only the parts that
//   depend on the missing piece are skipped.
//
// INPUT ROUTING (the movement component decides, this class only forwards)
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
//   GetMesh()          Third-person mesh. Replicated to everyone. Hidden from the local player in first person.
//   FirstPersonMesh    First-person arms. Visible to the owning player only, attached to the first-person camera.
//   FirstPersonCamera  Attached to the third-person mesh at FirstPersonCameraSocketName.
//   ThirdPersonCamera  On a spring arm attached to the capsule.
//
// PORTING: copy this folder, keep FRAMEWORKS_API (or rename it to your module macro).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameplayTagContainer.h"
#include "InputActionValue.h"
#include "BaseCharacter.generated.h"

class UCameraComponent;
class USkeletalMeshComponent;
class USpringArmComponent;
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
class FRAMEWORKS_API ABaseCharacter : public ACharacter
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

	/** First-person arms. Owner-only. Give it its own Animation Blueprint based on the AnimationInstance class. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character|Components")
	TObjectPtr<USkeletalMeshComponent> FirstPersonMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character|Components")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character|Components")
	TObjectPtr<USpringArmComponent> ThirdPersonSpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character|Components")
	TObjectPtr<UCameraComponent> ThirdPersonCamera;

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

	/** Socket on the third-person mesh that carries the first-person camera. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character|Camera")
	FName FirstPersonCameraSocketName = TEXT("head");

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

	/** True on the ground (or when there is no framework movement component). */
	bool IsGroundDomain() const;

	/** Replicated to other players (the owner drives its own value). */
	UPROPERTY(ReplicatedUsing = OnRep_ActivePerspective)
	ECharacterPerspective ActivePerspective = ECharacterPerspective::ThirdPerson;

	/** Previous water zone, used to detect Surface <-> Underwater transitions. */
	EWaterZone LastWaterZone;
};
