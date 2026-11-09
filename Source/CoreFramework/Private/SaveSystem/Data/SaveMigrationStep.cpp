// CoreFramework Save System - Data
#include "SaveSystem/Data/SaveMigrationStep.h"

bool USaveMigrationStep::Migrate_Implementation(USaveSlotData* Data, ESaveScope Scope)
{
	// Nothing to change by default.
	return true;
}
