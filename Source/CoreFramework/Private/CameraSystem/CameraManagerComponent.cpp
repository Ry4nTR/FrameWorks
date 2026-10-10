#include "CameraSystem/CameraManagerComponent.h"

#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Curves/CurveFloat.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogCameraManager, Log, All);

namespace
{
	FCameraRigState MakeRigState(const FCameraProfile& Profile)
	{
		FCameraRigState State;
		State.FieldOfView = Profile.FieldOfView;
		State.ArmLength = Profile.ArmLength;
		State.SocketOffset = Profile.SocketOffset;
		State.TargetOffset = Profile.TargetOffset;
		return State;
	}
}

UCameraManagerComponent::UCameraManagerComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	bWantsInitializeComponent = true;
	SetIsReplicatedByDefault(false);

	// Built-in profiles so the component works out of the box. Override them in a Blueprint or a data layer.
	FCameraProfile ThirdPerson;
	ThirdPerson.ArmLength = 300.f;
	ThirdPerson.MinArmLength = 120.f;
	ThirdPerson.MaxArmLength = 500.f;
	ThirdPerson.SocketOffset = FVector(0.0, 40.0, 20.0);
	ThirdPerson.TargetOffset = FVector(0.0, 0.0, 60.0);
	CameraProfiles.Add(FName(TEXT("ThirdPerson")), ThirdPerson);

	FCameraProfile FirstPerson;
	FirstPerson.ArmLength = 0.f;
	FirstPerson.bAllowZoom = false;
	FirstPerson.bDoCollisionTest = false;
	FirstPerson.TargetOffset = FVector(0.0, 0.0, 64.0);
	FirstPerson.bFirstPerson = true;
	CameraProfiles.Add(FName(TEXT("FirstPerson")), FirstPerson);

	FCameraProfile Aim;
	Aim.FieldOfView = 70.f;
	Aim.ArmLength = 140.f;
	Aim.bAllowZoom = false;
	Aim.SocketOffset = FVector(0.0, 55.0, 25.0);
	Aim.TargetOffset = FVector(0.0, 0.0, 60.0);
	Aim.LookSensitivityScale = 0.6f;
	Aim.BlendInTime = 0.15f;
	CameraProfiles.Add(FName(TEXT("Aim")), Aim);

	DefaultCameraName = FName(TEXT("ThirdPerson"));
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void UCameraManagerComponent::InitializeComponent()
{
	Super::InitializeComponent();

	AActor* Owner = GetOwner();
	if (!Owner || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	CreateRig();

	if (APawn* OwnerPawn = Cast<APawn>(Owner))
	{
		OwnerPawn->ReceiveControllerChangedDelegate.AddDynamic(this, &UCameraManagerComponent::HandleControllerChanged);
	}

	ActivateInitialCamera();
}

void UCameraManagerComponent::BeginPlay()
{
	Super::BeginPlay();

	// Covers a pawn that already has a controller by the time play begins.
	ApplyViewPitchLimits();
}

void UCameraManagerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ExternalBlendTimer);
	}

	Super::EndPlay(EndPlayReason);
}

void UCameraManagerComponent::CreateRig()
{
	AActor* Owner = GetOwner();
	USceneComponent* AttachParent = Owner ? Owner->GetRootComponent() : nullptr;
	if (!AttachParent)
	{
		UE_LOG(LogCameraManager, Warning, TEXT("%s has no root component; camera rig not created."), *GetNameSafe(Owner));
		return;
	}

	SpringArm = NewObject<USpringArmComponent>(Owner, TEXT("CameraManager_SpringArm"));
	SpringArm->SetupAttachment(AttachParent);
	SpringArm->bUsePawnControlRotation = true;
	SpringArm->RegisterComponent();
	Owner->AddInstanceComponent(SpringArm);

	Camera = NewObject<UCameraComponent>(Owner, TEXT("CameraManager_Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;
	Camera->RegisterComponent();
	Camera->SetActive(true);
	Owner->AddInstanceComponent(Camera);
}

void UCameraManagerComponent::ActivateInitialCamera()
{
	const FCameraProfile* Profile = CameraProfiles.Find(DefaultCameraName);
	if (!Profile)
	{
		UE_LOG(LogCameraManager, Warning, TEXT("Default camera '%s' not found in CameraProfiles on %s."), *DefaultCameraName.ToString(), *GetNameSafe(GetOwner()));
		return;
	}

	ActiveCameraName = DefaultCameraName;
	Current = Target = BlendStart = MakeRigState(*Profile);
	ApplyAttachment(*Profile);
	ApplyProfileFlags(*Profile);
	ApplyRigState(Current);
}

void UCameraManagerComponent::HandleControllerChanged(APawn* Pawn, AController* OldController, AController* NewController)
{
	ApplyViewPitchLimits();
}

// ---------------------------------------------------------------------------
// Profile switching
// ---------------------------------------------------------------------------

bool UCameraManagerComponent::SwitchToCamera(FName CameraName, float BlendTime)
{
	if (!SpringArm || !Camera)
	{
		return false;
	}

	const FCameraProfile* Profile = CameraProfiles.Find(CameraName);
	if (!Profile)
	{
		UE_LOG(LogCameraManager, Warning, TEXT("SwitchToCamera: no profile named '%s' on %s."), *CameraName.ToString(), *GetNameSafe(GetOwner()));
		return false;
	}

	if (CameraName == ActiveCameraName)
	{
		return true;
	}

	const FName Previous = ActiveCameraName;
	PreviousCameraName = Previous;
	ActiveCameraName = CameraName;

	// A different attach point moves the camera by more than the blend can cover, so that switch is a cut.
	const bool bAttachmentChanged = ApplyAttachment(*Profile);
	const float EffectiveBlendTime = bAttachmentChanged ? 0.f : (BlendTime >= 0.f ? BlendTime : Profile->BlendInTime);

	ApplyProfileFlags(*Profile);
	ApplyViewPitchLimits();
	BeginBlend(MakeRigState(*Profile), EffectiveBlendTime);

	OnCameraChanged.Broadcast(Previous, CameraName);
	return true;
}

bool UCameraManagerComponent::RevertToPreviousCamera(float BlendTime)
{
	if (PreviousCameraName.IsNone())
	{
		return false;
	}

	return SwitchToCamera(PreviousCameraName, BlendTime);
}

void UCameraManagerComponent::RegisterCameraProfile(FName CameraName, const FCameraProfile& Profile)
{
	if (CameraName.IsNone())
	{
		return;
	}

	CameraProfiles.Add(CameraName, Profile);

	if (CameraName == ActiveCameraName && SpringArm && Camera)
	{
		const bool bAttachmentChanged = ApplyAttachment(Profile);
		ApplyProfileFlags(Profile);
		ApplyViewPitchLimits();
		BeginBlend(MakeRigState(Profile), bAttachmentChanged ? 0.f : Profile.BlendInTime);
	}
}

// ---------------------------------------------------------------------------
// Adjustments
// ---------------------------------------------------------------------------

void UCameraManagerComponent::SetFOV(float NewFOV, float BlendTime)
{
	if (!Camera)
	{
		return;
	}

	const float ClampedFOV = FMath::Clamp(NewFOV, 5.f, 170.f);

	if (BlendTime <= KINDA_SMALL_NUMBER)
	{
		// Change only the FOV channel so a profile blend in progress is not interrupted.
		Current.FieldOfView = ClampedFOV;
		Target.FieldOfView = ClampedFOV;
		BlendStart.FieldOfView = ClampedFOV;
		ApplyRigState(Current);
		return;
	}

	FCameraRigState NewTarget = Target;
	NewTarget.FieldOfView = ClampedFOV;
	BeginBlend(NewTarget, BlendTime);
}

void UCameraManagerComponent::AddZoomInput(float Axis)
{
	if (!SpringArm || FMath::IsNearlyZero(Axis))
	{
		return;
	}

	const FCameraProfile* Profile = CameraProfiles.Find(ActiveCameraName);
	if (!Profile || !Profile->bAllowZoom)
	{
		return;
	}

	Target.ArmLength = FMath::Clamp(Target.ArmLength - Axis * ZoomStep, Profile->MinArmLength, Profile->MaxArmLength);
	SetComponentTickEnabled(true);
}

void UCameraManagerComponent::AddLookInput(const FVector2D& LookAxis)
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn)
	{
		return;
	}

	const FCameraProfile* Profile = CameraProfiles.Find(ActiveCameraName);
	const float Scale = Profile ? Profile->LookSensitivityScale : 1.f;
	const float PitchSign = bInvertPitch ? -1.f : 1.f;

	OwnerPawn->AddControllerYawInput(static_cast<float>(LookAxis.X * LookSensitivity.X) * Scale);
	OwnerPawn->AddControllerPitchInput(static_cast<float>(LookAxis.Y * LookSensitivity.Y) * Scale * PitchSign);
}

void UCameraManagerComponent::SetSocketParent(USceneComponent* NewSocketParent)
{
	SocketParent = NewSocketParent;

	if (const FCameraProfile* Profile = CameraProfiles.Find(ActiveCameraName))
	{
		ApplyAttachment(*Profile);
	}
}

// ---------------------------------------------------------------------------
// External cameras
// ---------------------------------------------------------------------------

void UCameraManagerComponent::BlendToTarget(AActor* NewViewTarget, float BlendTime, TEnumAsByte<EViewTargetBlendFunction> BlendFunction, float BlendExp, bool bLockOutgoing)
{
	APlayerController* PlayerController = GetOwnerPlayerController();
	UWorld* World = GetWorld();
	if (!PlayerController || !NewViewTarget || !World)
	{
		return;
	}

	PlayerController->SetViewTargetWithBlend(NewViewTarget, BlendTime, BlendFunction, BlendExp, bLockOutgoing);

	World->GetTimerManager().ClearTimer(ExternalBlendTimer);
	if (BlendTime > KINDA_SMALL_NUMBER)
	{
		World->GetTimerManager().SetTimer(ExternalBlendTimer, this, &UCameraManagerComponent::BroadcastExternalBlendFinished, BlendTime, false);
	}
	else
	{
		BroadcastExternalBlendFinished();
	}
}

void UCameraManagerComponent::BlendBackToOwner(float BlendTime, TEnumAsByte<EViewTargetBlendFunction> BlendFunction, float BlendExp)
{
	BlendToTarget(GetOwner(), BlendTime, BlendFunction, BlendExp, false);
}

void UCameraManagerComponent::BroadcastExternalBlendFinished()
{
	OnCameraBlendFinished.Broadcast();
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------

bool UCameraManagerComponent::IsFirstPerson() const
{
	const FCameraProfile* Profile = CameraProfiles.Find(ActiveCameraName);
	return Profile && Profile->bFirstPerson;
}

bool UCameraManagerComponent::IsViewTargetOwner() const
{
	const APlayerController* PlayerController = GetOwnerPlayerController();
	return PlayerController && PlayerController->GetViewTarget() == GetOwner();
}

// ---------------------------------------------------------------------------
// Blending
// ---------------------------------------------------------------------------

void UCameraManagerComponent::BeginBlend(const FCameraRigState& NewTarget, float BlendTime)
{
	Target = NewTarget;

	if (BlendTime <= KINDA_SMALL_NUMBER)
	{
		Current = Target;
		BlendStart = Target;
		bProfileBlending = false;
		ApplyRigState(Current);
		OnCameraBlendFinished.Broadcast();
		return;
	}

	BlendStart = Current;
	BlendElapsed = 0.f;
	BlendDuration = BlendTime;
	bProfileBlending = true;
	SetComponentTickEnabled(true);
}

void UCameraManagerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	bool bBlendFinished = false;

	if (bProfileBlending)
	{
		BlendElapsed += DeltaTime;
		const float Alpha = FMath::Clamp(BlendElapsed / BlendDuration, 0.f, 1.f);
		Current = FCameraRigState::Lerp(BlendStart, Target, EvaluateBlendAlpha(Alpha));

		if (Alpha >= 1.f)
		{
			Current = Target;
			bProfileBlending = false;
			bBlendFinished = true;
		}
	}
	else if (!FMath::IsNearlyEqual(Current.ArmLength, Target.ArmLength, 0.05f))
	{
		Current.ArmLength = FMath::FInterpTo(Current.ArmLength, Target.ArmLength, DeltaTime, ZoomInterpSpeed);
	}
	else
	{
		Current.ArmLength = Target.ArmLength;
	}

	ApplyRigState(Current);

	const bool bZoomSettled = FMath::IsNearlyEqual(Current.ArmLength, Target.ArmLength, 0.05f);
	if (!bProfileBlending && bZoomSettled)
	{
		SetComponentTickEnabled(false);
	}

	// Broadcast last: a listener may start a new blend and re-enable ticking.
	if (bBlendFinished)
	{
		OnCameraBlendFinished.Broadcast();
	}
}

float UCameraManagerComponent::EvaluateBlendAlpha(float Alpha) const
{
	if (BlendCurve)
	{
		return BlendCurve->GetFloatValue(Alpha);
	}

	return FMath::InterpEaseInOut(0.f, 1.f, Alpha, BlendEaseExponent);
}

// ---------------------------------------------------------------------------
// Applying state to the rig
// ---------------------------------------------------------------------------

void UCameraManagerComponent::ApplyRigState(const FCameraRigState& State)
{
	if (Camera)
	{
		Camera->SetFieldOfView(State.FieldOfView);
	}

	if (SpringArm)
	{
		SpringArm->TargetArmLength = State.ArmLength;
		SpringArm->SocketOffset = State.SocketOffset;
		SpringArm->TargetOffset = State.TargetOffset;
	}
}

void UCameraManagerComponent::ApplyProfileFlags(const FCameraProfile& Profile)
{
	if (!SpringArm)
	{
		return;
	}

	// Values that cannot be interpolated are applied immediately.
	SpringArm->bDoCollisionTest = Profile.bDoCollisionTest;
	SpringArm->bEnableCameraLag = Profile.bEnableCameraLag;
	SpringArm->CameraLagSpeed = Profile.CameraLagSpeed;
}

bool UCameraManagerComponent::ApplyAttachment(const FCameraProfile& Profile)
{
	AActor* Owner = GetOwner();
	if (!SpringArm || !Owner)
	{
		return false;
	}

	USceneComponent* DesiredParent = Owner->GetRootComponent();
	FName DesiredSocket = NAME_None;

	if (!Profile.AttachSocketName.IsNone() && SocketParent)
	{
		DesiredParent = SocketParent;
		DesiredSocket = Profile.AttachSocketName;
	}

	if (!DesiredParent || (SpringArm->GetAttachParent() == DesiredParent && SpringArm->GetAttachSocketName() == DesiredSocket))
	{
		return false;
	}

	SpringArm->AttachToComponent(DesiredParent, FAttachmentTransformRules::SnapToTargetNotIncludingScale, DesiredSocket);
	return true;
}

void UCameraManagerComponent::ApplyViewPitchLimits()
{
	const FCameraProfile* Profile = CameraProfiles.Find(ActiveCameraName);
	APlayerController* PlayerController = GetOwnerPlayerController();
	if (!Profile || !PlayerController || !PlayerController->PlayerCameraManager)
	{
		return;
	}

	PlayerController->PlayerCameraManager->ViewPitchMin = Profile->ViewPitchMin;
	PlayerController->PlayerCameraManager->ViewPitchMax = Profile->ViewPitchMax;
}

APlayerController* UCameraManagerComponent::GetOwnerPlayerController() const
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	return OwnerPawn ? Cast<APlayerController>(OwnerPawn->GetController()) : nullptr;
}