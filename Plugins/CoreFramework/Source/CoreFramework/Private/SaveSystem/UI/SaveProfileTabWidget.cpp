// CoreFramework Save System - UI
#include "SaveSystem/UI/SaveProfileTabWidget.h"

#include "Engine/LocalPlayer.h"
#include "SaveSystem/Manager/SaveSubsystem.h"

USaveProfileTabWidget::USaveProfileTabWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

int32 USaveProfileTabWidget::GetProfileIndex() const
{
	if (LocalPlayerIndexOverride >= 0)
	{
		return LocalPlayerIndexOverride;
	}
	const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	return LocalPlayer ? LocalPlayer->GetLocalPlayerIndex() : 0;
}

void USaveProfileTabWidget::StageFloat(FName Key, float Value)
{
	Staged.Add(Key, FString::SanitizeFloat(Value));
}

void USaveProfileTabWidget::StageInt(FName Key, int32 Value)
{
	Staged.Add(Key, FString::FromInt(Value));
}

void USaveProfileTabWidget::StageBool(FName Key, bool bValue)
{
	Staged.Add(Key, bValue ? TEXT("1") : TEXT("0"));
}

void USaveProfileTabWidget::StageString(FName Key, const FString& Value)
{
	Staged.Add(Key, Value);
}

float USaveProfileTabWidget::GetFloat(FName Key, float Default) const
{
	if (const FString* Value = Staged.Find(Key))
	{
		return FCString::Atof(**Value);
	}
	const USaveSubsystem* Subsystem = USaveSubsystem::Get(this);
	return Subsystem ? Subsystem->GetProfileFloat(Key, Default, GetProfileIndex()) : Default;
}

int32 USaveProfileTabWidget::GetInt(FName Key, int32 Default) const
{
	if (const FString* Value = Staged.Find(Key))
	{
		return FCString::Atoi(**Value);
	}
	const USaveSubsystem* Subsystem = USaveSubsystem::Get(this);
	return Subsystem ? Subsystem->GetProfileInt(Key, Default, GetProfileIndex()) : Default;
}

bool USaveProfileTabWidget::GetBool(FName Key, bool bDefault) const
{
	if (const FString* Value = Staged.Find(Key))
	{
		return Value->ToBool();
	}
	const USaveSubsystem* Subsystem = USaveSubsystem::Get(this);
	return Subsystem ? Subsystem->GetProfileBool(Key, bDefault, GetProfileIndex()) : bDefault;
}

FString USaveProfileTabWidget::GetString(FName Key, const FString& Default) const
{
	if (const FString* Value = Staged.Find(Key))
	{
		return *Value;
	}
	const USaveSubsystem* Subsystem = USaveSubsystem::Get(this);
	return Subsystem ? Subsystem->GetProfileString(Key, Default, GetProfileIndex()) : Default;
}

bool USaveProfileTabWidget::HasPendingChanges_Implementation() const
{
	const USaveSubsystem* Subsystem = USaveSubsystem::Get(this);
	if (!Subsystem)
	{
		return Staged.Num() > 0;
	}

	// A staged value that equals the saved one is not a change.
	const int32 Index = GetProfileIndex();
	for (const TPair<FName, FString>& Pair : Staged)
	{
		if (!Subsystem->HasProfileValue(Pair.Key, Index) || Subsystem->GetProfileString(Pair.Key, FString(), Index) != Pair.Value)
		{
			return true;
		}
	}
	return false;
}

void USaveProfileTabWidget::ApplyChanges_Implementation()
{
	USaveSubsystem* Subsystem = USaveSubsystem::Get(this);
	if (!Subsystem || Staged.Num() == 0)
	{
		return;
	}

	const int32 Index = GetProfileIndex();
	for (const TPair<FName, FString>& Pair : Staged)
	{
		Subsystem->SetProfileString(Pair.Key, Pair.Value, Index);
	}
	Staged.Reset();

	if (bSaveProfileOnApply)
	{
		Subsystem->SaveProfile(Index);
	}
	BP_OnValuesApplied();
}

void USaveProfileTabWidget::RevertChanges_Implementation()
{
	Staged.Reset();
	BP_OnValuesReverted();
}
