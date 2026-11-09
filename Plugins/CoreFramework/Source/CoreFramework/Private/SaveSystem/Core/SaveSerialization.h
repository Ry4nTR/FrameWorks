// CoreFramework Save System - internal helpers (not part of the public API)
#pragma once

#include "CoreMinimal.h"

class UObject;
class UClass;

namespace SaveSerialization
{
	/** True if the class has at least one variable marked SaveGame. Cached per class. */
	bool HasSaveGameProperties(const UClass* Class);

	/** Writes the SaveGame variables of Object. Object references are stored as paths, names as strings. */
	void WriteObject(UObject* Object, TArray<uint8>& OutBytes);

	/** Restores the SaveGame variables of Object from bytes written by WriteObject. Variables missing in the bytes keep their value. */
	void ReadObject(UObject* Object, const TArray<uint8>& Bytes);
}
