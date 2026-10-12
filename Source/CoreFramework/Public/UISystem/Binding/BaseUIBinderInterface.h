// CoreFramework UI System
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "BaseUIBinderInterface.generated.h"

class APawn;
class APlayerController;
class APlayerState;

UINTERFACE(BlueprintType, MinimalAPI)
class UBaseUIBinderInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Observer contract between the local player's gameplay objects and any UI object.
 *
 * Implemented by UBaseUserWidget and UBaseActivatableWidget. Registered binders receive:
 *  - a "Bound" call for every object that is currently valid at registration time (late join is safe),
 *  - an "Unbound" then "Bound" pair whenever the pawn or player state changes (respawn, possess, unpossess),
 *  - "Unbound" calls when they unregister or when the local player context is torn down.
 *
 * Every Bound call is paired with exactly one Unbound call. Implementers should drop every
 * subscription they made in Bound inside the matching Unbound. The Unbound argument can be null
 * when the object was destroyed before the call; drop weak or cached subscriptions in that case.
 *
 * The interface carries no gameplay data. Game code defines its own components and delegates
 * and the widget subscribes to them inside OnUIPawnBound.
 */
class COREFRAMEWORK_API IBaseUIBinderInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, Category = "Base UI|Binding")
	void OnUIControllerBound(APlayerController* Controller);

	UFUNCTION(BlueprintNativeEvent, Category = "Base UI|Binding")
	void OnUIControllerUnbound(APlayerController* Controller);

	UFUNCTION(BlueprintNativeEvent, Category = "Base UI|Binding")
	void OnUIPlayerStateBound(APlayerState* PlayerState);

	UFUNCTION(BlueprintNativeEvent, Category = "Base UI|Binding")
	void OnUIPlayerStateUnbound(APlayerState* PlayerState);

	UFUNCTION(BlueprintNativeEvent, Category = "Base UI|Binding")
	void OnUIPawnBound(APawn* Pawn);

	UFUNCTION(BlueprintNativeEvent, Category = "Base UI|Binding")
	void OnUIPawnUnbound(APawn* Pawn);
};
