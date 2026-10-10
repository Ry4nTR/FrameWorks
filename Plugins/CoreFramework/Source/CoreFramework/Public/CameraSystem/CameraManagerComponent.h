#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraTypes.h"
#include "Components/ActorComponent.h"
#include "CameraManagerComponent.generated.h"

class AController;
class APawn;
class APlayerController;
class UCameraComponent;
class UCurveFloat;
class USceneComponent;
class USpringArmComponent;

/**
 * A named camera setup. Profiles are data: a new game adds entries to the
 * component's CameraProfiles map and never needs new camera code.
 */
USTRUCT(BlueprintType)
struct COREFRAMEWORK_API FCameraProfile
{
	GENERATED_BODY()

	/** Horizontal field of view in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (ClampMin = "5.0", ClampMax = "170.0"))
	float FieldOfView = 90.f;

	/** Spring arm length. 0 places the camera on the arm origin (first person). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (ClampMin = "0.0"))
	float ArmLength = 300.f;

	/** Offset applied at the end of the arm, in camera space (shoulder offset). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	FVector SocketOffset = FVector::ZeroVector;

	/** Offset applied at the arm origin, in world space (head height). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	FVector TargetOffset = FVector::ZeroVector;

	/** Allows AddZoomInput to change the arm length while this profile is active. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zoom")
	bool bAllowZoom = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zoom", meta = (EditCondition = "bAllowZoom", ClampMin = "0.0"))
	float MinArmLength = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zoom", meta = (EditCondition = "bAllowZoom", ClampMin = "0.0"))
	float MaxArmLength = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behavior")
	bool bDoCollisionTest = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behavior")
	bool bEnableCameraLag = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behavior", meta = (EditCondition = "bEnableCameraLag", ClampMin = "0.0"))
	float CameraLagSpeed = 10.f;

	/** Pitch limits applied to the owning player's camera manager while this profile is active. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behavior", meta = (ClampMin = "-89.9", ClampMax = "0.0"))
	float ViewPitchMin = -89.9f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behavior", meta = (ClampMin = "0.0", ClampMax = "89.9"))
	float ViewPitchMax = 89.9f;

	/** Multiplies look input while active (use below 1 for aiming). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behavior", meta = (ClampMin = "0.0"))
	float LookSensitivityScale = 1.f;

	/** Default time, in seconds, to blend into this profile. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behavior", meta = (ClampMin = "0.0"))
	float BlendInTime = 0.25f;

	/** Lets the owner react (for example swap visible meshes) through UCameraManagerComponent::IsFirstPerson. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behavior")
	bool bFirstPerson = false;

	/**
	 * Socket the rig attaches to while this profile is active (for example a head socket for first person).
	 * Needs a socket parent set through UCameraManagerComponent::SetSocketParent. None attaches to the owner's root.
	 * Switching between profiles with different attach points is always an instant cut.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behavior")
	FName AttachSocketName = NAME_None;
};

/** The values the component interpolates on the owned rig. Internal, not reflected. */
struct FCameraRigState
{
	float FieldOfView = 90.f;
	float ArmLength = 300.f;
	FVector SocketOffset = FVector::ZeroVector;
	FVector TargetOffset = FVector::ZeroVector;

	static FCameraRigState Lerp(const FCameraRigState& A, const FCameraRigState& B, float T)
	{
		FCameraRigState Result;
		Result.FieldOfView = FMath::Lerp(A.FieldOfView, B.FieldOfView, T);
		Result.ArmLength = FMath::Lerp(A.ArmLength, B.ArmLength, T);
		Result.SocketOffset = FMath::Lerp(A.SocketOffset, B.SocketOffset, T);
		Result.TargetOffset = FMath::Lerp(A.TargetOffset, B.TargetOffset, T);
		return Result;
	}
};

/** Fired when the active camera profile changes, at the start of the blend. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCameraChanged, FName, PreviousCamera, FName, NewCamera);

/** Fired when a profile blend or an external view target blend has finished. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCameraBlendFinished);

/**
 * Owns the camera rig (spring arm + camera) for its owning actor and everything
 * that happens to it: switching between named profiles, FOV changes, zoom, look
 * input, and blending to and from external camera actors.
 *
 * The component depends only on the Engine module. It knows nothing about any
 * character class, so it can be attached to any pawn. Look input requires a pawn
 * owner; every other feature works on any actor with a root component.
 *
 * The rig is created at runtime and is skipped on dedicated servers, so every
 * rig-driven function is a no-op there.
 */
UCLASS(ClassGroup = (Camera), meta = (BlueprintSpawnableComponent))
class COREFRAMEWORK_API UCameraManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCameraManagerComponent();

	// ---- Profile switching on the owned rig ----

	/**
	 * Blends the owned rig to the named profile.
	 * @param BlendTime Seconds to blend. A negative value uses the profile's BlendInTime.
	 * @return False if the profile does not exist or there is no rig.
	 */
	UFUNCTION(BlueprintCallable, Category = "Camera Manager|Switching")
	bool SwitchToCamera(FName CameraName, float BlendTime = -1.f);

	/** Switches back to the camera that was active before the current one. */
	UFUNCTION(BlueprintCallable, Category = "Camera Manager|Switching")
	bool RevertToPreviousCamera(float BlendTime = -1.f);

	/** Adds or replaces a profile at runtime. Re-applies it if it is the active one. */
	UFUNCTION(BlueprintCallable, Category = "Camera Manager|Switching")
	void RegisterCameraProfile(FName CameraName, const FCameraProfile& Profile);

	// ---- Adjustments ----

	/**
	 * Sets the field of view until the next profile switch.
	 * A blend time above zero re-times any blend already in progress.
	 */
	UFUNCTION(BlueprintCallable, Category = "Camera Manager|Adjustments")
	void SetFOV(float NewFOV, float BlendTime = 0.f);

	/** Positive values zoom in. Ignored when the active profile does not allow zoom. */
	UFUNCTION(BlueprintCallable, Category = "Camera Manager|Adjustments")
	void AddZoomInput(float Axis);

	/** Forwards look input to the owning pawn's controller, scaled by sensitivity and the active profile. */
	UFUNCTION(BlueprintCallable, Category = "Camera Manager|Adjustments")
	void AddLookInput(const FVector2D& LookAxis);

	/**
	 * Sets the component that carries the sockets named by FCameraProfile::AttachSocketName (usually the skeletal mesh).
	 * Re-applies the attachment of the active profile.
	 */
	UFUNCTION(BlueprintCallable, Category = "Camera Manager|Adjustments")
	void SetSocketParent(USceneComponent* NewSocketParent);

	// ---- External cameras (cinematics, other actors) ----

	/** Blends the owning player's view to another actor, using the engine's view target blend. Call on the owning client. */
	UFUNCTION(BlueprintCallable, Category = "Camera Manager|External")
	void BlendToTarget(AActor* NewViewTarget, float BlendTime = 1.f, TEnumAsByte<EViewTargetBlendFunction> BlendFunction = VTBlend_Cubic, float BlendExp = 2.f, bool bLockOutgoing = false);

	/** Blends the owning player's view back to the owner's own rig. */
	UFUNCTION(BlueprintCallable, Category = "Camera Manager|External")
	void BlendBackToOwner(float BlendTime = 1.f, TEnumAsByte<EViewTargetBlendFunction> BlendFunction = VTBlend_Cubic, float BlendExp = 2.f);

	// ---- Queries ----

	UFUNCTION(BlueprintPure, Category = "Camera Manager|Queries")
	FName GetActiveCameraName() const { return ActiveCameraName; }

	UFUNCTION(BlueprintPure, Category = "Camera Manager|Queries")
	bool IsFirstPerson() const;

	/** True while a profile blend is running on the owned rig. */
	UFUNCTION(BlueprintPure, Category = "Camera Manager|Queries")
	bool IsBlending() const { return bProfileBlending; }

	/** True when the owning player is currently viewing through this owner's rig. */
	UFUNCTION(BlueprintPure, Category = "Camera Manager|Queries")
	bool IsViewTargetOwner() const;

	UFUNCTION(BlueprintPure, Category = "Camera Manager|Queries")
	UCameraComponent* GetCameraComponent() const { return Camera; }

	UFUNCTION(BlueprintPure, Category = "Camera Manager|Queries")
	USpringArmComponent* GetSpringArmComponent() const { return SpringArm; }

	// ---- Events ----

	UPROPERTY(BlueprintAssignable, Category = "Camera Manager|Events")
	FOnCameraChanged OnCameraChanged;

	UPROPERTY(BlueprintAssignable, Category = "Camera Manager|Events")
	FOnCameraBlendFinished OnCameraBlendFinished;

	// ---- Configuration ----

	/** Every camera this component can switch to, by name. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera Manager|Config")
	TMap<FName, FCameraProfile> CameraProfiles;

	/** Profile applied instantly when the component initializes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera Manager|Config")
	FName DefaultCameraName;

	/** Horizontal and vertical look multipliers. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Manager|Config")
	FVector2D LookSensitivity = FVector2D(1.0, 1.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Manager|Config")
	bool bInvertPitch = false;

	/** Arm length change per unit of zoom input. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Manager|Config", meta = (ClampMin = "0.0"))
	float ZoomStep = 40.f;

	/** Smoothing speed for zoom changes. 0 applies them instantly. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Manager|Config", meta = (ClampMin = "0.0"))
	float ZoomInterpSpeed = 10.f;

	/** Optional curve (0 to 1 in, 0 to 1 out) shaping profile blends. Falls back to ease in/out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Manager|Config")
	TObjectPtr<UCurveFloat> BlendCurve;

	/** Exponent for the ease in/out fallback when no BlendCurve is set. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Manager|Config", meta = (ClampMin = "1.0"))
	float BlendEaseExponent = 2.f;

protected:
	virtual void InitializeComponent() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void CreateRig();
	void ActivateInitialCamera();
	void BeginBlend(const FCameraRigState& NewTarget, float BlendTime);
	void ApplyRigState(const FCameraRigState& State);
	void ApplyProfileFlags(const FCameraProfile& Profile);

	/** Attaches the spring arm where the profile wants it. Returns true when the attach point changed. */
	bool ApplyAttachment(const FCameraProfile& Profile);
	void ApplyViewPitchLimits();
	float EvaluateBlendAlpha(float Alpha) const;
	APlayerController* GetOwnerPlayerController() const;
	void BroadcastExternalBlendFinished();

	UFUNCTION()
	void HandleControllerChanged(APawn* Pawn, AController* OldController, AController* NewController);

	UPROPERTY(Transient)
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(Transient)
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> SocketParent;

	FName ActiveCameraName;
	FName PreviousCameraName;

	FCameraRigState Current;
	FCameraRigState Target;
	FCameraRigState BlendStart;

	float BlendElapsed = 0.f;
	float BlendDuration = 0.f;
	bool bProfileBlending = false;

	FTimerHandle ExternalBlendTimer;
};