// CoreFramework Save System - Library
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SaveSystem/Core/SaveTypes.h"
#include "SaveFunctionLibrary.generated.h"

class USaveSubsystem;

/**
 * Blueprint (and C++) shortcuts to the save subsystem: they need only a world context object, so no subsystem node
 * has to be fetched first. Also the text helpers that slot lists need.
 */
UCLASS()
class COREFRAMEWORK_API USaveFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Save", meta = (WorldContext = "WorldContextObject", DisplayName = "Get Save Subsystem"))
	static USaveSubsystem* GetSaveSubsystem(const UObject* WorldContextObject);

	// ---- World slots ----------------------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Save|Slots", meta = (WorldContext = "WorldContextObject"))
	static ESaveResult StartNewGame(const UObject* WorldContextObject, const FString& SlotName);

	UFUNCTION(BlueprintCallable, Category = "Save|Slots", meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "DisplayName"))
	static void SaveToSlot(const UObject* WorldContextObject, const FString& SlotName, FSaveOperationDelegate OnComplete, const FString& DisplayName);

	UFUNCTION(BlueprintCallable, Category = "Save|Slots", meta = (WorldContext = "WorldContextObject"))
	static void QuickSave(const UObject* WorldContextObject, FSaveOperationDelegate OnComplete);

	UFUNCTION(BlueprintCallable, Category = "Save|Slots", meta = (WorldContext = "WorldContextObject"))
	static void AutoSave(const UObject* WorldContextObject, FSaveOperationDelegate OnComplete);

	UFUNCTION(BlueprintCallable, Category = "Save|Slots", meta = (WorldContext = "WorldContextObject"))
	static void LoadSlot(const UObject* WorldContextObject, const FString& SlotName, FSaveOperationDelegate OnComplete);

	UFUNCTION(BlueprintCallable, Category = "Save|Slots", meta = (WorldContext = "WorldContextObject"))
	static void DeleteSlot(const UObject* WorldContextObject, const FString& SlotName, FSaveOperationDelegate OnComplete);

	UFUNCTION(BlueprintCallable, Category = "Save|Slots", meta = (WorldContext = "WorldContextObject"))
	static void ListSlots(const UObject* WorldContextObject, FSaveOperationDelegate OnComplete);

	/** First name "<Prefix>1", "<Prefix>2" ... that is not in the cached slot list. Call ListSlots first. */
	UFUNCTION(BlueprintPure, Category = "Save|Slots", meta = (WorldContext = "WorldContextObject"))
	static FString MakeUniqueSlotName(const UObject* WorldContextObject, const FString& Prefix);

	UFUNCTION(BlueprintPure, Category = "Save|Slots")
	static bool IsValidSlotName(const FString& SlotName);

	// ---- Profile -------------------------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Save|Profile", meta = (WorldContext = "WorldContextObject"))
	static void SaveProfile(const UObject* WorldContextObject, FSaveOperationDelegate OnComplete, int32 LocalPlayerIndex = 0);

	UFUNCTION(BlueprintCallable, Category = "Save|Profile", meta = (WorldContext = "WorldContextObject"))
	static void LoadProfile(const UObject* WorldContextObject, FSaveOperationDelegate OnComplete, int32 LocalPlayerIndex = 0);

	UFUNCTION(BlueprintCallable, Category = "Save|Profile", meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "LocalPlayerIndex"))
	static void SetProfileFloat(const UObject* WorldContextObject, FName Key, float Value, int32 LocalPlayerIndex = 0);

	UFUNCTION(BlueprintPure, Category = "Save|Profile", meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "LocalPlayerIndex"))
	static float GetProfileFloat(const UObject* WorldContextObject, FName Key, float Default, int32 LocalPlayerIndex = 0);

	UFUNCTION(BlueprintCallable, Category = "Save|Profile", meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "LocalPlayerIndex"))
	static void SetProfileInt(const UObject* WorldContextObject, FName Key, int32 Value, int32 LocalPlayerIndex = 0);

	UFUNCTION(BlueprintPure, Category = "Save|Profile", meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "LocalPlayerIndex"))
	static int32 GetProfileInt(const UObject* WorldContextObject, FName Key, int32 Default, int32 LocalPlayerIndex = 0);

	UFUNCTION(BlueprintCallable, Category = "Save|Profile", meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "LocalPlayerIndex"))
	static void SetProfileBool(const UObject* WorldContextObject, FName Key, bool bValue, int32 LocalPlayerIndex = 0);

	UFUNCTION(BlueprintPure, Category = "Save|Profile", meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "LocalPlayerIndex"))
	static bool GetProfileBool(const UObject* WorldContextObject, FName Key, bool bDefault, int32 LocalPlayerIndex = 0);

	// ---- Saveable objects ----------------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Save|Objects", meta = (WorldContext = "WorldContextObject"))
	static void RegisterSaveable(const UObject* WorldContextObject, UObject* Saveable);

	UFUNCTION(BlueprintCallable, Category = "Save|Objects", meta = (WorldContext = "WorldContextObject"))
	static void UnregisterSaveable(const UObject* WorldContextObject, UObject* Saveable, bool bCaptureState = true);

	// ---- Text for UI ----------------------------------------------------------------------------------

	/** A sentence for an error, ready for a status line. Success gives an empty text. */
	UFUNCTION(BlueprintPure, Category = "Save|UI")
	static FText ResultToText(ESaveResult Result);

	/** "2:05:09" for 7509 seconds. */
	UFUNCTION(BlueprintPure, Category = "Save|UI")
	static FText FormatPlaytime(double Seconds);

	/** Date and time of a save in the player's culture. */
	UFUNCTION(BlueprintPure, Category = "Save|UI")
	static FText FormatSavedAt(const FDateTime& SavedAt);
};
