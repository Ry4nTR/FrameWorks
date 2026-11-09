// CoreFramework Save System - internal helpers
#include "SaveSerialization.h"

#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "UObject/UnrealType.h"

namespace SaveSerialization
{
	bool HasSaveGameProperties(const UClass* Class)
	{
		if (!Class)
		{
			return false;
		}

		static TMap<TWeakObjectPtr<const UClass>, bool> Cache;
		if (const bool* Cached = Cache.Find(Class))
		{
			return *Cached;
		}

		bool bFound = false;
		for (TFieldIterator<FProperty> It(Class); It; ++It)
		{
			if (It->HasAnyPropertyFlags(CPF_SaveGame))
			{
				bFound = true;
				break;
			}
		}

		Cache.Add(Class, bFound);
		return bFound;
	}

	void WriteObject(UObject* Object, TArray<uint8>& OutBytes)
	{
		OutBytes.Reset();
		if (!Object)
		{
			return;
		}

		FMemoryWriter MemoryWriter(OutBytes, /*bIsPersistent=*/true);
		FObjectAndNameAsStringProxyArchive Ar(MemoryWriter, /*bInLoadIfFindFails=*/false);
		Ar.ArIsSaveGame = true; // Only variables marked SaveGame are written.
		Object->Serialize(Ar);
	}

	void ReadObject(UObject* Object, const TArray<uint8>& Bytes)
	{
		if (!Object || Bytes.Num() == 0)
		{
			return;
		}

		FMemoryReader MemoryReader(Bytes, /*bIsPersistent=*/true);
		FObjectAndNameAsStringProxyArchive Ar(MemoryReader, /*bInLoadIfFindFails=*/true);
		Ar.ArIsSaveGame = true;
		Object->Serialize(Ar);
	}
}
