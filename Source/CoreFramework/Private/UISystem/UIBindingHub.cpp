// CoreFramework UI System
#include "UISystem/UIBindingHub.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "UISystem/BaseUIBinderInterface.h"
#include "UISystem/UITypes.h"

void UUIBindingHub::Start(APlayerController* InController, float InPollIntervalSeconds)
{
	Stop();

	if (!InController)
	{
		return;
	}

	Controller = InController;
	TimerWorld = InController->GetWorld();
	InController->OnPossessedPawnChanged.AddDynamic(this, &UUIBindingHub::HandlePossessedPawnChanged);

	bControllerBound = true;
	ForEachBinder([InController](UObject* Binder)
	{
		IBaseUIBinderInterface::Execute_OnUIControllerBound(Binder, InController);
	});

	Refresh();

	if (InPollIntervalSeconds > 0.f)
	{
		if (UWorld* World = TimerWorld.Get())
		{
			World->GetTimerManager().SetTimer(PollHandle, this, &UUIBindingHub::Refresh, FMath::Max(0.05f, InPollIntervalSeconds), true);
		}
	}
}

void UUIBindingHub::Stop()
{
	if (UWorld* World = TimerWorld.Get())
	{
		World->GetTimerManager().ClearTimer(PollHandle);
	}
	TimerWorld.Reset();

	APlayerController* PC = Controller.Get();
	if (PC)
	{
		PC->OnPossessedPawnChanged.RemoveDynamic(this, &UUIBindingHub::HandlePossessedPawnChanged);
	}

	// Mirror of Refresh: every Bound gets exactly one Unbound, innermost first.
	if (bPlayerStateBound)
	{
		APlayerState* OldState = PlayerState.Get();
		bPlayerStateBound = false;
		ForEachBinder([OldState](UObject* Binder) { IBaseUIBinderInterface::Execute_OnUIPlayerStateUnbound(Binder, OldState); });
	}
	if (bPawnBound)
	{
		APawn* OldPawn = Pawn.Get();
		bPawnBound = false;
		ForEachBinder([OldPawn](UObject* Binder) { IBaseUIBinderInterface::Execute_OnUIPawnUnbound(Binder, OldPawn); });
	}
	if (bControllerBound)
	{
		bControllerBound = false;
		ForEachBinder([PC](UObject* Binder) { IBaseUIBinderInterface::Execute_OnUIControllerUnbound(Binder, PC); });
	}

	Controller.Reset();
	Pawn.Reset();
	PlayerState.Reset();
}

void UUIBindingHub::Refresh()
{
	APlayerController* PC = Controller.Get();
	if (!PC)
	{
		if (bControllerBound)
		{
			// The controller was destroyed (travel, disconnect). Release everything.
			Stop();
		}
		return;
	}

	// ---- Player state -----------------------------------------------------------------------
	{
		APlayerState* NewState = PC->GetPlayerState<APlayerState>();
		APlayerState* OldState = PlayerState.Get();
		// A destroyed old object leaves OldState null while bPlayerStateBound is still true: that counts as a change.
		const bool bSame = (OldState == NewState) && (bPlayerStateBound == (OldState != nullptr));
		if (!bSame)
		{
			if (bPlayerStateBound)
			{
				ForEachBinder([OldState](UObject* Binder) { IBaseUIBinderInterface::Execute_OnUIPlayerStateUnbound(Binder, OldState); });
			}
			PlayerState = NewState;
			bPlayerStateBound = (NewState != nullptr);
			if (NewState)
			{
				ForEachBinder([NewState](UObject* Binder) { IBaseUIBinderInterface::Execute_OnUIPlayerStateBound(Binder, NewState); });
			}
		}
	}

	// ---- Pawn -------------------------------------------------------------------------------
	{
		APawn* NewPawn = PC->GetPawn();
		APawn* OldPawn = Pawn.Get();
		const bool bSame = (OldPawn == NewPawn) && (bPawnBound == (OldPawn != nullptr));
		if (!bSame)
		{
			if (bPawnBound)
			{
				ForEachBinder([OldPawn](UObject* Binder) { IBaseUIBinderInterface::Execute_OnUIPawnUnbound(Binder, OldPawn); });
			}
			Pawn = NewPawn;
			bPawnBound = (NewPawn != nullptr);
			if (NewPawn)
			{
				ForEachBinder([NewPawn](UObject* Binder) { IBaseUIBinderInterface::Execute_OnUIPawnBound(Binder, NewPawn); });
			}
		}
	}
}

void UUIBindingHub::HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	Refresh();
}

void UUIBindingHub::RegisterBinder(UObject* Binder)
{
	if (!Binder || !Binder->GetClass()->ImplementsInterface(UBaseUIBinderInterface::StaticClass()))
	{
		return;
	}

	const bool bAlreadyRegistered = Binders.ContainsByPredicate([Binder](const TWeakObjectPtr<UObject>& Entry)
	{
		return Entry.Get() == Binder;
	});
	if (bAlreadyRegistered)
	{
		return;
	}

	Binders.Add(Binder);

	// Late join: bring the new binder up to date with the current context.
	if (bControllerBound)
	{
		IBaseUIBinderInterface::Execute_OnUIControllerBound(Binder, Controller.Get());
	}
	if (bPlayerStateBound && PlayerState.IsValid())
	{
		IBaseUIBinderInterface::Execute_OnUIPlayerStateBound(Binder, PlayerState.Get());
	}
	if (bPawnBound && Pawn.IsValid())
	{
		IBaseUIBinderInterface::Execute_OnUIPawnBound(Binder, Pawn.Get());
	}
}

void UUIBindingHub::UnregisterBinder(UObject* Binder)
{
	if (!Binder)
	{
		return;
	}

	const int32 RemovedCount = Binders.RemoveAll([Binder](const TWeakObjectPtr<UObject>& Entry)
	{
		return Entry.Get() == Binder;
	});
	if (RemovedCount == 0 || !Binder->GetClass()->ImplementsInterface(UBaseUIBinderInterface::StaticClass()))
	{
		return;
	}

	// Symmetry: a binder that leaves while bound gets its Unbound calls so it can drop subscriptions.
	if (bPawnBound)
	{
		IBaseUIBinderInterface::Execute_OnUIPawnUnbound(Binder, Pawn.Get());
	}
	if (bPlayerStateBound)
	{
		IBaseUIBinderInterface::Execute_OnUIPlayerStateUnbound(Binder, PlayerState.Get());
	}
	if (bControllerBound)
	{
		IBaseUIBinderInterface::Execute_OnUIControllerUnbound(Binder, Controller.Get());
	}
}

void UUIBindingHub::ForEachBinder(TFunctionRef<void(UObject*)> Callback)
{
	Binders.RemoveAll([](const TWeakObjectPtr<UObject>& Entry) { return !Entry.IsValid(); });

	// Iterate a snapshot: binders may register or unregister while handling a callback.
	const TArray<TWeakObjectPtr<UObject>> Snapshot = Binders;
	for (const TWeakObjectPtr<UObject>& Entry : Snapshot)
	{
		if (UObject* Binder = Entry.Get())
		{
			Callback(Binder);
		}
	}
}
