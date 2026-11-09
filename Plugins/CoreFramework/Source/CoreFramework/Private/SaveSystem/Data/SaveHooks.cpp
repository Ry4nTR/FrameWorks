// CoreFramework Save System - Data
#include "SaveSystem/Data/SaveHooks.h"

// Every hook does nothing by default: a game overrides only the ones it needs.

bool USaveHooks::CanSave_Implementation(const FString& SlotName)
{
	return true;
}

FSaveSlotInfo USaveHooks::BuildSlotInfo_Implementation(const FSaveSlotInfo& DefaultInfo)
{
	return DefaultInfo;
}

void USaveHooks::OnBeforeSave_Implementation(ESaveScope Scope, const FString& SlotName)
{
}

void USaveHooks::OnAfterLoad_Implementation(ESaveScope Scope, const FSaveSlotInfo& Info)
{
}
