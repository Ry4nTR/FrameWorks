// SaveableComponent.h
// CoreFramework Save System - make any actor saveable by adding one component.
//
// WHAT IT SAVES
//   - The owning actor's variables that are marked SaveGame ("Save Game" checkbox on a Blueprint variable,
//     UPROPERTY(SaveGame) in C++).
//   - The same for every other component of the actor that has SaveGame variables (health component, inventory
//     component ...), except components that are saveable on their own (they implement ISaveableInterface).
//   - Optionally the actor's transform.
//   Nothing else: references to other actors are stored as paths, so they are valid for placed actors only.
//
// HOW
//   1. Add "Saveable" to the actor (Blueprint: Add Component).
//   2. Mark the variables to keep as Save Game.
//   3. For an actor that is placed in a level, nothing more. For an actor that is spawned at runtime, set Save Key
//      to something stable (the automatic key uses the actor's name, which is random for spawned actors).
//   4. React to a restore with On Restored (collected pickup destroys itself, a door plays its open pose ...).
//
// SCOPE
//   World (default): part of the playthrough, restored by the authority only. Profile: part of the local player's profile.
//
// PORTING: copy the SaveSystem folder, keep COREFRAMEWORK_API (or rename it to your module macro).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SaveSystem/Core/SaveTypes.h"
#include "SaveSystem/Interface/SaveableInterface.h"
#include "SaveableComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSaveableSavingSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSaveableRestoredSignature, int32, SavedVersion);

UCLASS(ClassGroup = (Frameworks), meta = (BlueprintSpawnableComponent, DisplayName = "Saveable"))
class COREFRAMEWORK_API USaveableComponent : public UActorComponent, public ISaveableInterface
{
	GENERATED_BODY()

public:
	USaveableComponent();

	//~ Begin UActorComponent Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent Interface

	//~ Begin ISaveableInterface
	virtual void GetSaveDescriptor_Implementation(FSaveableDescriptor& OutDescriptor) const override;
	virtual void OnBeforeSave_Implementation() override;
	virtual void OnAfterLoad_Implementation(int32 SavedVersion) override;
	virtual bool SerializeSaveData(FArchive& Ar) override;
	//~ End ISaveableInterface

	/**
	 * Unique id of this actor in the save. Empty = automatic: "<level name>.<actor name>", right for actors placed
	 * in a level. Spawned actors must set it (for example "Pickup_" + a stable id).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	FString SaveKey;

	/** World: part of the playthrough. Profile: part of the local player's profile. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	ESaveScope Scope = ESaveScope::World;

	/** Version of this actor's data. Raise it when the meaning of a saved variable changes; On Restored receives the old one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save", meta = (ClampMin = "1"))
	int32 SaveVersion = 1;

	/** Profile scope only: which local player's profile stores this actor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save", meta = (ClampMin = "0", EditCondition = "Scope == ESaveScope::Profile"))
	int32 LocalPlayerIndex = 0;

	/** Also save and restore the actor's location, rotation and scale. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	bool bSaveTransform = true;

	/** Save the SaveGame variables of the owning actor itself. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	bool bSaveOwnerVariables = true;

	/** Save the SaveGame variables of the other components of the actor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	bool bSaveSiblingComponents = true;

	/** Fires just before the actor is captured for a save. Last chance to copy live state into SaveGame variables. */
	UPROPERTY(BlueprintAssignable, Category = "Save")
	FSaveableSavingSignature OnSaving;

	/** Fires after the actor was restored from a save. SavedVersion is the SaveVersion the data was written with. */
	UPROPERTY(BlueprintAssignable, Category = "Save")
	FSaveableRestoredSignature OnRestored;

	/** The key this component registers with (the explicit one, or the automatic one). */
	UFUNCTION(BlueprintPure, Category = "Save")
	FString GetEffectiveSaveKey() const;

private:
	/** Name used inside the blob for the owning actor. Components use their own name. */
	static const FString& OwnerPartName();
};
