// CoreFramework UI System
#pragma once

#include "CoreMinimal.h"
#include "TimerManager.h"
#include "UObject/Object.h"
#include "UIBindingHub.generated.h"

class APawn;
class APlayerController;
class APlayerState;
class UWorld;

/**
 * Per-local-player observer hub. Tracks Controller -> PlayerState / Pawn and dispatches
 * IBaseUIBinderInterface calls to every registered binder.
 *
 * Change detection is two layered:
 *  1. Event: AController::OnPossessedPawnChanged for instant reaction.
 *  2. Safety net: a low-rate timer that compares the three pointers. This covers client replication
 *     ordering (pawn or player state arriving after the HUD exists) and any path that does not fire an event.
 *
 * Owned by UUIFrameSubsystem. Not meant to be used directly by game code.
 */
UCLASS()
class COREFRAMEWORK_API UUIBindingHub : public UObject
{
	GENERATED_BODY()

public:
	/** Begin tracking a local player controller. Replaces any previous controller. */
	void Start(APlayerController* InController, float InPollIntervalSeconds);

	/** Unbind everything (Unbound is dispatched for every Bound object) and stop tracking. */
	void Stop();

	void RegisterBinder(UObject* Binder);
	void UnregisterBinder(UObject* Binder);

	/** Re-evaluate controller, pawn and player state and dispatch changes. Cheap; safe to call at any time. */
	void Refresh();

	APlayerController* GetController() const { return Controller.Get(); }
	APawn* GetPawn() const { return Pawn.Get(); }
	APlayerState* GetPlayerState() const { return PlayerState.Get(); }

private:
	UFUNCTION()
	void HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);

	void ForEachBinder(TFunctionRef<void(UObject*)> Callback);

	TWeakObjectPtr<APlayerController> Controller;
	TWeakObjectPtr<APawn> Pawn;
	TWeakObjectPtr<APlayerState> PlayerState;
	TWeakObjectPtr<UWorld> TimerWorld;

	TArray<TWeakObjectPtr<UObject>> Binders;
	FTimerHandle PollHandle;

	bool bControllerBound = false;
	bool bPawnBound = false;
	bool bPlayerStateBound = false;
};
