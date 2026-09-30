// InputConfigData.h
// Character Framework - data-driven Enhanced Input configuration.
//
// One data asset holds the mapping context and every input action the BaseCharacter can bind.
// Leave an action empty and the character simply skips that binding, so a game without
// flying, swimming, dash or dodge needs no code changes.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "InputConfigData.generated.h"

class UInputAction;
class UInputMappingContext;

/** Every action the BaseCharacter knows how to bind. Empty entries are ignored. */
USTRUCT(BlueprintType)
struct FRAMEWORKS_API FCharacterInputActions
{
	GENERATED_BODY()

	/** Axis2D. X = right, Y = forward. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Locomotion")
	TObjectPtr<UInputAction> Move = nullptr;

	/** Axis2D. X = yaw, Y = pitch (negate Y in the mapping context for standard mouse look). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Locomotion")
	TObjectPtr<UInputAction> Look = nullptr;

	/** Ground: jump. Fly and Water: ascend while held. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Locomotion")
	TObjectPtr<UInputAction> Jump = nullptr;

	/** Ground: crouch while held. Fly and Water: descend while held. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Locomotion")
	TObjectPtr<UInputAction> Crouch = nullptr;

	/** Hold. Ground: run. Fly: glide. Water: fast swim. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Locomotion")
	TObjectPtr<UInputAction> Sprint = nullptr;

	/** Press to toggle flight on and off. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Domains")
	TObjectPtr<UInputAction> Fly = nullptr;

	/** Hold to dive while in the Water domain. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Domains")
	TObjectPtr<UInputAction> Swim = nullptr;

	/** Press. Burst of speed along the input direction (domain dependent). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Actions")
	TObjectPtr<UInputAction> Dash = nullptr;

	/** Press. Evasive move: roll on the ground, sidestep in flight. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Actions")
	TObjectPtr<UInputAction> Dodge = nullptr;

	/** Press. Switches between first-person and third-person view. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UInputAction> PerspectiveToggle = nullptr;
};

/** Data asset referenced by ABaseCharacter::InputConfig. */
UCLASS(BlueprintType)
class FRAMEWORKS_API UInputConfigData : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Mapping context added to the owning local player when the character is possessed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	int32 MappingPriority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (ShowOnlyInnerProperties))
	FCharacterInputActions Actions;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
