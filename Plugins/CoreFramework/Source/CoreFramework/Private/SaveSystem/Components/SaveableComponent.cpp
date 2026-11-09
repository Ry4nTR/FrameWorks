// SaveableComponent.cpp
// CoreFramework Save System - make any actor saveable by adding one component.

#include "SaveSystem/Components/SaveableComponent.h"

#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "SaveSystem/Manager/SaveSubsystem.h"
#include "../Core/SaveSerialization.h"

namespace
{
	/** Versions of the blob layout written by SerializeSaveData. */
	constexpr int32 BlobVersion = 1;
}

USaveableComponent::USaveableComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

const FString& USaveableComponent::OwnerPartName()
{
	static const FString Name(TEXT("@Actor"));
	return Name;
}

FString USaveableComponent::GetEffectiveSaveKey() const
{
	if (!SaveKey.IsEmpty())
	{
		return SaveKey;
	}

	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return FString();
	}

	// Placed actors keep their name between runs, so "<level>.<actor>" is stable.
	const ULevel* Level = Owner->GetLevel();
	const UObject* LevelOuter = Level ? Level->GetOuter() : nullptr;
	return FString::Printf(TEXT("%s.%s"), LevelOuter ? *LevelOuter->GetName() : TEXT("NoLevel"), *Owner->GetName());
}

void USaveableComponent::BeginPlay()
{
	Super::BeginPlay();

	if (USaveSubsystem* Subsystem = USaveSubsystem::Get(this))
	{
		Subsystem->RegisterSaveable(this);
	}
}

void USaveableComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (USaveSubsystem* Subsystem = USaveSubsystem::Get(this))
	{
		// On quit nothing will read the state again, so skip handing it back.
		const bool bCapture = EndPlayReason != EEndPlayReason::Quit && EndPlayReason != EEndPlayReason::EndPlayInEditor;
		Subsystem->UnregisterSaveable(this, bCapture);
	}

	Super::EndPlay(EndPlayReason);
}

void USaveableComponent::GetSaveDescriptor_Implementation(FSaveableDescriptor& OutDescriptor) const
{
	OutDescriptor.Key = GetEffectiveSaveKey();
	OutDescriptor.Scope = Scope;
	OutDescriptor.Version = SaveVersion;
	OutDescriptor.LocalPlayerIndex = LocalPlayerIndex;
}

void USaveableComponent::OnBeforeSave_Implementation()
{
	OnSaving.Broadcast();
}

void USaveableComponent::OnAfterLoad_Implementation(int32 SavedVersion)
{
	OnRestored.Broadcast(SavedVersion);
}

bool USaveableComponent::SerializeSaveData(FArchive& Ar)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return false;
	}

	// The objects whose SaveGame variables are written: the actor, then its components that have any.
	TArray<UObject*> Parts;
	if (bSaveOwnerVariables && SaveSerialization::HasSaveGameProperties(Owner->GetClass()))
	{
		Parts.Add(Owner);
	}
	if (bSaveSiblingComponents)
	{
		for (UActorComponent* Component : Owner->GetComponents())
		{
			if (Component && Component != this
				&& SaveSerialization::HasSaveGameProperties(Component->GetClass())
				&& !Component->GetClass()->ImplementsInterface(USaveableInterface::StaticClass()))
			{
				Parts.Add(Component);
			}
		}
	}

	if (Ar.IsSaving())
	{
		int32 Version = BlobVersion;
		Ar << Version;

		bool bHasTransform = bSaveTransform && Owner->GetRootComponent() != nullptr;
		Ar << bHasTransform;
		if (bHasTransform)
		{
			FTransform Transform = Owner->GetActorTransform();
			Ar << Transform;
		}

		int32 Count = Parts.Num();
		Ar << Count;
		for (UObject* Part : Parts)
		{
			FString Name = (Part == Owner) ? OwnerPartName() : Part->GetName();
			TArray<uint8> Bytes;
			SaveSerialization::WriteObject(Part, Bytes);
			Ar << Name;
			Ar << Bytes;
		}
		return true;
	}

	// Loading
	int32 Version = 0;
	Ar << Version;
	if (Version > BlobVersion)
	{
		return true; // Written by a newer layout: leave the actor as it is instead of misreading it.
	}

	bool bHasTransform = false;
	Ar << bHasTransform;
	if (bHasTransform)
	{
		FTransform Transform;
		Ar << Transform;
		if (bSaveTransform && Owner->GetRootComponent())
		{
			Owner->SetActorTransform(Transform, /*bSweep=*/false, /*OutSweepHitResult=*/nullptr, ETeleportType::TeleportPhysics);
		}
	}

	int32 Count = 0;
	Ar << Count;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		FString Name;
		TArray<uint8> Bytes;
		Ar << Name;
		Ar << Bytes;

		UObject* Target = nullptr;
		for (UObject* Part : Parts)
		{
			const FString PartName = (Part == Owner) ? OwnerPartName() : Part->GetName();
			if (PartName == Name)
			{
				Target = Part;
				break;
			}
		}
		if (Target)
		{
			SaveSerialization::ReadObject(Target, Bytes);
		}
		// A part that no longer exists (component removed in an update) is skipped on purpose.
	}
	return true;
}
