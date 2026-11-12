// CoreFramework Input System - Manager
#include "InputSystem/Manager/InputFrameSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "CommonInputBaseTypes.h"
#include "CommonInputSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedActionKeyMapping.h"
#include "EnhancedInputSubsystems.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Misc/ConfigCacheIni.h"
#include "UserSettings/EnhancedInputUserSettings.h"
#include "InputSystem/Core/InputFrameSettings.h"
#include "KeyCaptureInputProcessor.h"

namespace
{
	/** GameUserSettings.ini section of local player 0. Other local players add their index. */
	const TCHAR* const GInputSection = TEXT("CoreFramework.Input");

	bool IsKeyForDevice(const FKey& Key, bool bGamepad)
	{
		return Key.IsValid() && Key.IsGamepadKey() == bGamepad;
	}

	uint8 SlotIndex(EPlayerMappableKeySlot Slot)
	{
		return static_cast<uint8>(Slot);
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Lookup and lifecycle
// ---------------------------------------------------------------------------------------------------------------------

UInputFrameSubsystem* UInputFrameSubsystem::GetFromLocalPlayer(const ULocalPlayer* LocalPlayer)
{
	return LocalPlayer ? LocalPlayer->GetSubsystem<UInputFrameSubsystem>() : nullptr;
}

UInputFrameSubsystem* UInputFrameSubsystem::Get(const UObject* ContextObject)
{
	if (!ContextObject)
	{
		return nullptr;
	}

	if (const ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(ContextObject))
	{
		return GetFromLocalPlayer(LocalPlayer);
	}
	if (const UUserWidget* Widget = Cast<UUserWidget>(ContextObject))
	{
		if (const ULocalPlayer* Owner = Widget->GetOwningLocalPlayer())
		{
			return GetFromLocalPlayer(Owner);
		}
	}
	if (const APlayerController* Controller = Cast<APlayerController>(ContextObject))
	{
		return GetFromLocalPlayer(Controller->GetLocalPlayer());
	}
	if (const APawn* Pawn = Cast<APawn>(ContextObject))
	{
		if (const APlayerController* Controller = Cast<APlayerController>(Pawn->GetController()))
		{
			return GetFromLocalPlayer(Controller->GetLocalPlayer());
		}
	}

	// World-space widgets, actors, components: the first local player of that world.
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(ContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GetFromLocalPlayer(GameInstance->GetFirstGamePlayer()) : nullptr;
}

void UInputFrameSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Both are local player subsystems too: make sure they exist before this one reads them.
	Collection.InitializeDependency<UCommonInputSubsystem>();
	Collection.InitializeDependency<UEnhancedInputLocalPlayerSubsystem>();

	LoadPreferences();

	if (UCommonInputSubsystem* CommonInput = GetCommonInput())
	{
		LastInputType = CommonInput->GetCurrentInputType();
		LastGamepadName = CommonInput->GetCurrentGamepadName();
		CommonInput->OnInputMethodChangedNative.AddUObject(this, &UInputFrameSubsystem::HandleInputMethodChanged);
	}

	ConnectEnhancedInput();
}

void UInputFrameSubsystem::Deinitialize()
{
	CancelKeyCapture();
	SavePreferences();

	if (UCommonInputSubsystem* CommonInput = GetCommonInput())
	{
		CommonInput->OnInputMethodChangedNative.RemoveAll(this);
	}
	if (UEnhancedInputUserSettings* Settings = BoundUserSettings.Get())
	{
		Settings->OnSettingsChanged.RemoveDynamic(this, &UInputFrameSubsystem::HandleUserSettingsChanged);
	}
	BoundUserSettings.Reset();

	if (const ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* EnhancedInput = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			EnhancedInput->ControlMappingsRebuiltDelegate.RemoveDynamic(this, &UInputFrameSubsystem::HandleControlMappingsRebuilt);
		}
	}

	Super::Deinitialize();
}

void UInputFrameSubsystem::PlayerControllerChanged(APlayerController* NewPlayerController)
{
	Super::PlayerControllerChanged(NewPlayerController);

	// Enhanced Input may create its user settings only now; the controller also needs the vibration preference.
	ConnectEnhancedInput();
	ApplyVibration();
}

UCommonInputSubsystem* UInputFrameSubsystem::GetCommonInput() const
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	return LocalPlayer ? LocalPlayer->GetSubsystem<UCommonInputSubsystem>() : nullptr;
}

UEnhancedInputUserSettings* UInputFrameSubsystem::GetUserSettings() const
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	const UEnhancedInputLocalPlayerSubsystem* EnhancedInput = LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	return EnhancedInput ? EnhancedInput->GetUserSettings() : nullptr;
}

UEnhancedPlayerMappableKeyProfile* UInputFrameSubsystem::GetKeyProfile() const
{
	const UEnhancedInputUserSettings* Settings = GetUserSettings();
	return Settings ? Settings->GetActiveKeyProfile() : nullptr;
}

void UInputFrameSubsystem::ConnectEnhancedInput()
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* EnhancedInput = LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (!EnhancedInput)
	{
		return;
	}

	if (!bMappingsRebuiltBound)
	{
		// A context added or removed (gameplay starts, a vehicle is entered) changes which keys actions use.
		EnhancedInput->ControlMappingsRebuiltDelegate.AddUniqueDynamic(this, &UInputFrameSubsystem::HandleControlMappingsRebuilt);
		bMappingsRebuiltBound = true;
	}

	UEnhancedInputUserSettings* Settings = EnhancedInput->GetUserSettings();
	if (!Settings)
	{
		return; // User settings disabled, or not created yet (retried in PlayerControllerChanged).
	}

	if (BoundUserSettings.Get() != Settings)
	{
		if (UEnhancedInputUserSettings* Old = BoundUserSettings.Get())
		{
			Old->OnSettingsChanged.RemoveDynamic(this, &UInputFrameSubsystem::HandleUserSettingsChanged);
		}
		Settings->OnSettingsChanged.AddUniqueDynamic(this, &UInputFrameSubsystem::HandleUserSettingsChanged);
		BoundUserSettings = Settings;
		bContextsRegistered = false;
	}

	if (!bContextsRegistered)
	{
		bContextsRegistered = true;
		for (const TSoftObjectPtr<UInputMappingContext>& SoftContext : UInputFrameSettings::Get()->RemappableContexts)
		{
			if (const UInputMappingContext* Context = SoftContext.LoadSynchronous())
			{
				if (!Settings->IsMappingContextRegistered(Context))
				{
					Settings->RegisterInputMappingContext(Context);
				}
			}
			else if (!SoftContext.IsNull())
			{
				UE_LOG(LogCoreInput, Warning, TEXT("Remappable context '%s' could not be loaded."), *SoftContext.ToString());
			}
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Device
// ---------------------------------------------------------------------------------------------------------------------

ECommonInputType UInputFrameSubsystem::GetCurrentInputType() const
{
	const UCommonInputSubsystem* CommonInput = GetCommonInput();
	return CommonInput ? CommonInput->GetCurrentInputType() : ECommonInputType::MouseAndKeyboard;
}

bool UInputFrameSubsystem::IsUsingGamepad() const
{
	return GetCurrentInputType() == ECommonInputType::Gamepad;
}

FName UInputFrameSubsystem::GetCurrentGamepadName() const
{
	const UCommonInputSubsystem* CommonInput = GetCommonInput();
	return CommonInput ? CommonInput->GetCurrentGamepadName() : NAME_None;
}

void UInputFrameSubsystem::HandleInputMethodChanged(ECommonInputType NewInputType)
{
	// CommonUI also broadcasts when only the gamepad family changed: forward both cases, ignore exact repeats.
	const FName GamepadName = GetCurrentGamepadName();
	if (NewInputType == LastInputType && GamepadName == LastGamepadName)
	{
		return;
	}

	LastInputType = NewInputType;
	LastGamepadName = GamepadName;
	OnInputDeviceChanged.Broadcast(NewInputType, GamepadName);
}

// ---------------------------------------------------------------------------------------------------------------------
// Keys and glyphs
// ---------------------------------------------------------------------------------------------------------------------

FKey UInputFrameSubsystem::GetKeyForAction(const UInputAction* Action, bool bGamepad) const
{
	if (!Action)
	{
		return EKeys::Invalid;
	}

	// 1. The mappings applied to the player right now (player remaps included).
	if (const ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (const UEnhancedInputLocalPlayerSubsystem* EnhancedInput = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			for (const FKey& Key : EnhancedInput->QueryKeysMappedToAction(Action))
			{
				if (IsKeyForDevice(Key, bGamepad))
				{
					return Key;
				}
			}
		}
	}

	// 2. Player-mappable keys of registered contexts that are not applied (for example in the main menu).
	if (const UEnhancedPlayerMappableKeyProfile* Profile = GetKeyProfile())
	{
		for (const TPair<FName, FKeyMappingRow>& Pair : Profile->GetPlayerMappingRows())
		{
			for (const FPlayerKeyMapping& Mapping : Pair.Value.Mappings)
			{
				if (Mapping.GetAssociatedInputAction() == Action && IsKeyForDevice(Mapping.GetCurrentKey(), bGamepad))
				{
					return Mapping.GetCurrentKey();
				}
			}
		}
	}

	// 3. Defaults of the remappable contexts (user settings disabled).
	for (const TSoftObjectPtr<UInputMappingContext>& SoftContext : UInputFrameSettings::Get()->RemappableContexts)
	{
		if (const UInputMappingContext* Context = SoftContext.Get())
		{
			for (const FEnhancedActionKeyMapping& Mapping : Context->GetMappings())
			{
				if (Mapping.Action == Action && IsKeyForDevice(Mapping.Key, bGamepad))
				{
					return Mapping.Key;
				}
			}
		}
	}

	return EKeys::Invalid;
}

FKey UInputFrameSubsystem::GetKeyForActionOnCurrentDevice(const UInputAction* Action) const
{
	return GetKeyForAction(Action, IsUsingGamepad());
}

bool UInputFrameSubsystem::FindKeyGlyph(const FKey& Key, FName GamepadName, FSlateBrush& OutBrush)
{
	if (!Key.IsValid())
	{
		return false;
	}

	const UCommonInputPlatformSettings* Platform = UCommonInputPlatformSettings::Get();
	if (!Platform)
	{
		return false;
	}

	const ECommonInputType InputType = Key.IsGamepadKey() ? ECommonInputType::Gamepad : ECommonInputType::MouseAndKeyboard;
	const FName DefaultGamepad = Platform->GetDefaultGamepadName();
	const FName Gamepad = GamepadName.IsNone() ? DefaultGamepad : GamepadName;

	if (Platform->TryGetInputBrush(OutBrush, Key, InputType, Gamepad))
	{
		return true;
	}

	// A gamepad family without its own icon set falls back to the platform default set.
	return InputType == ECommonInputType::Gamepad && Gamepad != DefaultGamepad
		&& Platform->TryGetInputBrush(OutBrush, Key, InputType, DefaultGamepad);
}

bool UInputFrameSubsystem::GetKeyGlyph(const FKey& Key, FSlateBrush& OutBrush) const
{
	return FindKeyGlyph(Key, GetCurrentGamepadName(), OutBrush);
}

FText UInputFrameSubsystem::GetKeyDisplayText(const FKey& Key)
{
	return Key.IsValid() ? Key.GetDisplayName(/*bLongDisplayName=*/false) : FText::GetEmpty();
}

void UInputFrameSubsystem::HandleControlMappingsRebuilt()
{
	OnInputBindingsChanged.Broadcast();
}

void UInputFrameSubsystem::HandleUserSettingsChanged(UEnhancedInputUserSettings* Settings)
{
	OnInputBindingsChanged.Broadcast();
}

// ---------------------------------------------------------------------------------------------------------------------
// Preferences
// ---------------------------------------------------------------------------------------------------------------------

FString UInputFrameSubsystem::GetConfigSection() const
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	const int32 Index = LocalPlayer ? LocalPlayer->GetLocalPlayerIndex() : 0;
	return Index > 0 ? FString::Printf(TEXT("%s.%d"), GInputSection, Index) : FString(GInputSection);
}

void UInputFrameSubsystem::LoadPreferences()
{
	const UInputFrameSettings* Settings = UInputFrameSettings::Get();
	Preferences = Settings->DefaultPreferences;

	if (GConfig)
	{
		const FString Section = GetConfigSection();
		GConfig->GetFloat(*Section, TEXT("MouseSensitivity"), Preferences.MouseSensitivity, GGameUserSettingsIni);
		GConfig->GetFloat(*Section, TEXT("GamepadSensitivity"), Preferences.GamepadSensitivity, GGameUserSettingsIni);
		GConfig->GetBool(*Section, TEXT("InvertLookX"), Preferences.bInvertLookX, GGameUserSettingsIni);
		GConfig->GetBool(*Section, TEXT("InvertLookY"), Preferences.bInvertLookY, GGameUserSettingsIni);
		GConfig->GetBool(*Section, TEXT("AimAssist"), Preferences.bAimAssist, GGameUserSettingsIni);
		GConfig->GetBool(*Section, TEXT("Vibration"), Preferences.bVibration, GGameUserSettingsIni);
	}

	Preferences.MouseSensitivity = FMath::Clamp(Preferences.MouseSensitivity, Settings->MinSensitivity, Settings->MaxSensitivity);
	Preferences.GamepadSensitivity = FMath::Clamp(Preferences.GamepadSensitivity, Settings->MinSensitivity, Settings->MaxSensitivity);
}

void UInputFrameSubsystem::SetPreferences(const FInputPreferences& NewPreferences)
{
	const UInputFrameSettings* Settings = UInputFrameSettings::Get();

	FInputPreferences Clamped = NewPreferences;
	Clamped.MouseSensitivity = FMath::Clamp(Clamped.MouseSensitivity, Settings->MinSensitivity, Settings->MaxSensitivity);
	Clamped.GamepadSensitivity = FMath::Clamp(Clamped.GamepadSensitivity, Settings->MinSensitivity, Settings->MaxSensitivity);

	if (Clamped == Preferences)
	{
		return;
	}

	Preferences = Clamped;

	// In memory now (like the audio volumes), on disk with SavePreferences or when the local player goes away.
	if (GConfig)
	{
		const FString Section = GetConfigSection();
		GConfig->SetFloat(*Section, TEXT("MouseSensitivity"), Preferences.MouseSensitivity, GGameUserSettingsIni);
		GConfig->SetFloat(*Section, TEXT("GamepadSensitivity"), Preferences.GamepadSensitivity, GGameUserSettingsIni);
		GConfig->SetBool(*Section, TEXT("InvertLookX"), Preferences.bInvertLookX, GGameUserSettingsIni);
		GConfig->SetBool(*Section, TEXT("InvertLookY"), Preferences.bInvertLookY, GGameUserSettingsIni);
		GConfig->SetBool(*Section, TEXT("AimAssist"), Preferences.bAimAssist, GGameUserSettingsIni);
		GConfig->SetBool(*Section, TEXT("Vibration"), Preferences.bVibration, GGameUserSettingsIni);
	}

	ApplyVibration();
	OnPreferencesChanged.Broadcast(Preferences);
}

void UInputFrameSubsystem::SavePreferences()
{
	if (GConfig)
	{
		GConfig->Flush(false, GGameUserSettingsIni);
	}
}

void UInputFrameSubsystem::ResetPreferencesToDefaults()
{
	SetPreferences(UInputFrameSettings::Get()->DefaultPreferences);
}

FVector2D UInputFrameSubsystem::ApplyLookPreferences(FVector2D LookInput) const
{
	const float Sensitivity = IsUsingGamepad() ? Preferences.GamepadSensitivity : Preferences.MouseSensitivity;
	LookInput *= Sensitivity;
	if (Preferences.bInvertLookX)
	{
		LookInput.X = -LookInput.X;
	}
	if (Preferences.bInvertLookY)
	{
		LookInput.Y = -LookInput.Y;
	}
	return LookInput;
}

bool UInputFrameSubsystem::IsAimAssistActive() const
{
	return Preferences.bAimAssist && IsUsingGamepad();
}

void UInputFrameSubsystem::ApplyVibration() const
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (APlayerController* Controller = LocalPlayer ? LocalPlayer->GetPlayerController(nullptr) : nullptr)
	{
		Controller->bForceFeedbackEnabled = Preferences.bVibration;
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Key bindings
// ---------------------------------------------------------------------------------------------------------------------

bool UInputFrameSubsystem::AreKeyBindingsAvailable() const
{
	return GetKeyProfile() != nullptr;
}

const FPlayerKeyMapping* UInputFrameSubsystem::FindDeviceMapping(const FKeyMappingRow& Row, bool bGamepad)
{
	// The device of a mapping is the device of its DEFAULT key: a keyboard slot stays the keyboard slot even while
	// unbound. Lowest slot first when a device has several.
	const FPlayerKeyMapping* Best = nullptr;
	for (const FPlayerKeyMapping& Mapping : Row.Mappings)
	{
		const FKey& Reference = Mapping.GetDefaultKey().IsValid() ? Mapping.GetDefaultKey() : Mapping.GetCurrentKey();
		if (!IsKeyForDevice(Reference, bGamepad))
		{
			continue;
		}
		if (!Best || SlotIndex(Mapping.GetSlot()) < SlotIndex(Best->GetSlot()))
		{
			Best = &Mapping;
		}
	}
	return Best;
}

void UInputFrameSubsystem::FillBinding(FName MappingName, const FKeyMappingRow& Row, FInputFrameKeyBinding& OutBinding)
{
	OutBinding = FInputFrameKeyBinding();
	OutBinding.MappingName = MappingName;

	if (const FPlayerKeyMapping* Keyboard = FindDeviceMapping(Row, /*bGamepad=*/false))
	{
		OutBinding.bHasKeyboardBinding = true;
		OutBinding.KeyboardKey = Keyboard->GetCurrentKey();
		OutBinding.KeyboardDefaultKey = Keyboard->GetDefaultKey();
		OutBinding.DisplayName = Keyboard->GetDisplayName();
		OutBinding.Category = Keyboard->GetDisplayCategory();
	}
	if (const FPlayerKeyMapping* Gamepad = FindDeviceMapping(Row, /*bGamepad=*/true))
	{
		OutBinding.bHasGamepadBinding = true;
		OutBinding.GamepadKey = Gamepad->GetCurrentKey();
		OutBinding.GamepadDefaultKey = Gamepad->GetDefaultKey();
		if (OutBinding.DisplayName.IsEmpty())
		{
			OutBinding.DisplayName = Gamepad->GetDisplayName();
			OutBinding.Category = Gamepad->GetDisplayCategory();
		}
	}

	if (OutBinding.DisplayName.IsEmpty())
	{
		OutBinding.DisplayName = FText::FromName(MappingName);
	}
}

TArray<FName> UInputFrameSubsystem::GetOrderedMappingNames(const UEnhancedPlayerMappableKeyProfile& Profile) const
{
	const TMap<FName, FKeyMappingRow>& Rows = Profile.GetPlayerMappingRows();
	TArray<FName> Ordered;

	// Designer order first: the order of the contexts in Project Settings, then of their mappings.
	for (const TSoftObjectPtr<UInputMappingContext>& SoftContext : UInputFrameSettings::Get()->RemappableContexts)
	{
		if (const UInputMappingContext* Context = SoftContext.Get())
		{
			for (const FEnhancedActionKeyMapping& Mapping : Context->GetMappings())
			{
				if (!Mapping.IsPlayerMappable())
				{
					continue;
				}
				const FName Name = Mapping.GetMappingName();
				if (!Name.IsNone() && Rows.Contains(Name))
				{
					Ordered.AddUnique(Name);
				}
			}
		}
	}

	// Then anything registered by other code.
	for (const TPair<FName, FKeyMappingRow>& Pair : Rows)
	{
		if (Pair.Value.HasAnyMappings())
		{
			Ordered.AddUnique(Pair.Key);
		}
	}
	return Ordered;
}

TArray<FInputFrameKeyBinding> UInputFrameSubsystem::GetKeyBindings() const
{
	TArray<FInputFrameKeyBinding> Result;

	const UEnhancedPlayerMappableKeyProfile* Profile = GetKeyProfile();
	if (!Profile)
	{
		return Result;
	}

	for (const FName& Name : GetOrderedMappingNames(*Profile))
	{
		if (const FKeyMappingRow* Row = Profile->FindKeyMappingRow(Name))
		{
			FInputFrameKeyBinding Binding;
			FillBinding(Name, *Row, Binding);
			if (Binding.bHasKeyboardBinding || Binding.bHasGamepadBinding)
			{
				Result.Add(MoveTemp(Binding));
			}
		}
	}
	return Result;
}

bool UInputFrameSubsystem::GetKeyBinding(FName MappingName, FInputFrameKeyBinding& OutBinding) const
{
	const UEnhancedPlayerMappableKeyProfile* Profile = GetKeyProfile();
	const FKeyMappingRow* Row = Profile ? Profile->FindKeyMappingRow(MappingName) : nullptr;
	if (!Row)
	{
		return false;
	}
	FillBinding(MappingName, *Row, OutBinding);
	return true;
}

bool UInputFrameSubsystem::IsKeyAllowedForBinding(const FKey& Key, bool bGamepad) const
{
	return IsKeyForDevice(Key, bGamepad) && !Key.IsAnalog() && !UInputFrameSettings::Get()->ReservedKeys.Contains(Key);
}

EKeyRebindResult UInputFrameSubsystem::RebindKey(FName MappingName, bool bGamepad, FKey NewKey, FText& OutOtherAction)
{
	OutOtherAction = FText::GetEmpty();

	UEnhancedInputUserSettings* Settings = GetUserSettings();
	UEnhancedPlayerMappableKeyProfile* Profile = Settings ? Settings->GetActiveKeyProfile() : nullptr;
	const FKeyMappingRow* Row = Profile ? Profile->FindKeyMappingRow(MappingName) : nullptr;
	if (!Row)
	{
		return EKeyRebindResult::Failed;
	}

	const UInputFrameSettings* FrameSettings = UInputFrameSettings::Get();
	if (FrameSettings->ReservedKeys.Contains(NewKey))
	{
		return EKeyRebindResult::ReservedKey;
	}
	if (!IsKeyForDevice(NewKey, bGamepad) || NewKey.IsAnalog())
	{
		return EKeyRebindResult::WrongDevice;
	}

	const FPlayerKeyMapping* Target = FindDeviceMapping(*Row, bGamepad);
	if (!Target)
	{
		return EKeyRebindResult::NotRemappable;
	}

	const EPlayerMappableKeySlot TargetSlot = Target->GetSlot();
	const FKey OldKey = Target->GetCurrentKey();
	const FKey TargetDefaultKey = Target->GetDefaultKey();
	if (OldKey == NewKey)
	{
		return EKeyRebindResult::Unchanged;
	}

	// Other actions that use NewKey on this device. Actions that share the key BY DESIGN (same default key) are not a
	// conflict: two context-dependent uses of one button stay together.
	struct FConflict
	{
		FName MappingName;
		EPlayerMappableKeySlot Slot;
		FText DisplayName;
	};
	TArray<FConflict> Conflicts;
	{
		TArray<FName> NamesUsingKey;
		Profile->GetMappingNamesForKey(NewKey, NamesUsingKey);
		for (const FName& OtherName : NamesUsingKey)
		{
			if (OtherName == MappingName)
			{
				continue;
			}
			const FKeyMappingRow* OtherRow = Profile->FindKeyMappingRow(OtherName);
			if (!OtherRow)
			{
				continue;
			}
			for (const FPlayerKeyMapping& Other : OtherRow->Mappings)
			{
				if (Other.GetCurrentKey() == NewKey && Other.GetDefaultKey() != TargetDefaultKey)
				{
					Conflicts.Add({ OtherName, Other.GetSlot(), Other.GetDisplayName() });
				}
			}
		}
	}

	if (Conflicts.Num() > 0)
	{
		OutOtherAction = Conflicts[0].DisplayName;
		if (FrameSettings->ConflictPolicy == EKeyRebindConflictPolicy::Reject)
		{
			return EKeyRebindResult::Rejected;
		}
	}

	FGameplayTagContainer Failure;

	FMapPlayerKeyArgs Args;
	Args.MappingName = MappingName;
	Args.Slot = TargetSlot;
	Args.NewKey = NewKey;
	Settings->MapPlayerKey(Args, Failure);
	if (!Failure.IsEmpty())
	{
		UE_LOG(LogCoreInput, Warning, TEXT("RebindKey: Enhanced Input refused %s -> %s (%s)."), *MappingName.ToString(), *NewKey.ToString(), *Failure.ToStringSimple());
		return EKeyRebindResult::Failed;
	}

	if (FrameSettings->ConflictPolicy == EKeyRebindConflictPolicy::Swap || FrameSettings->ConflictPolicy == EKeyRebindConflictPolicy::Unbind)
	{
		const FKey ReplacementKey = FrameSettings->ConflictPolicy == EKeyRebindConflictPolicy::Swap ? OldKey : EKeys::Invalid;
		for (const FConflict& Conflict : Conflicts)
		{
			FMapPlayerKeyArgs OtherArgs;
			OtherArgs.MappingName = Conflict.MappingName;
			OtherArgs.Slot = Conflict.Slot;
			OtherArgs.NewKey = ReplacementKey;
			Failure.Reset();
			Settings->MapPlayerKey(OtherArgs, Failure);
		}
	}

	CommitKeyBindings();
	return EKeyRebindResult::Success;
}

void UInputFrameSubsystem::ResetKeyBinding(FName MappingName, bool bGamepad)
{
	UEnhancedInputUserSettings* Settings = GetUserSettings();
	UEnhancedPlayerMappableKeyProfile* Profile = Settings ? Settings->GetActiveKeyProfile() : nullptr;
	const FKeyMappingRow* Row = Profile ? Profile->FindKeyMappingRow(MappingName) : nullptr;
	const FPlayerKeyMapping* Mapping = Row ? FindDeviceMapping(*Row, bGamepad) : nullptr;
	if (!Mapping || Mapping->GetCurrentKey() == Mapping->GetDefaultKey())
	{
		return;
	}

	FMapPlayerKeyArgs Args;
	Args.MappingName = MappingName;
	Args.Slot = Mapping->GetSlot();
	Args.NewKey = Mapping->GetDefaultKey();

	FGameplayTagContainer Failure;
	Settings->MapPlayerKey(Args, Failure);
	CommitKeyBindings();
}

void UInputFrameSubsystem::ResetAllKeyBindings()
{
	if (UEnhancedPlayerMappableKeyProfile* Profile = GetKeyProfile())
	{
		Profile->ResetToDefault();
		CommitKeyBindings();

		// Resetting the profile does not broadcast the settings change by itself.
		OnInputBindingsChanged.Broadcast();
	}
}

void UInputFrameSubsystem::CommitKeyBindings()
{
	if (UEnhancedInputUserSettings* Settings = GetUserSettings())
	{
		Settings->ApplySettings();
		if (UInputFrameSettings::Get()->bSaveKeyBindingsOnChange)
		{
			Settings->AsyncSaveSettings();
		}
	}
}

void UInputFrameSubsystem::SaveKeyBindings()
{
	if (UEnhancedInputUserSettings* Settings = GetUserSettings())
	{
		Settings->SaveSettings();
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Key capture
// ---------------------------------------------------------------------------------------------------------------------

bool UInputFrameSubsystem::K2_BeginKeyCapture(bool bGamepad, FOnInputFrameKeyCaptured OnCaptured)
{
	return BeginKeyCapture(bGamepad, [OnCaptured](const FKey& Key, bool bCancelled)
	{
		OnCaptured.ExecuteIfBound(Key, bCancelled);
	});
}

bool UInputFrameSubsystem::BeginKeyCapture(bool bGamepad, FInputFrameKeyCaptureCallback OnCaptured)
{
	if (IsCapturingKey() || !FSlateApplication::IsInitialized())
	{
		return false;
	}

	// A finished processor may still be waiting for key releases: it removes itself, we just drop our reference.
	CaptureProcessor.Reset();
	CaptureCallback = MoveTemp(OnCaptured);

	const UInputFrameSettings* Settings = UInputFrameSettings::Get();
	TWeakObjectPtr<UInputFrameSubsystem> WeakThis(this);
	CaptureProcessor = MakeShared<FKeyCaptureInputProcessor>(
		bGamepad,
		Settings->KeyCaptureTimeout,
		Settings->CancelCaptureKeys,
		Settings->bAllowStickDirectionKeys,
		FKeyCaptureInputProcessor::FOnCaptureFinished::CreateLambda([WeakThis](const FKey& Key, bool bCancelled)
		{
			if (UInputFrameSubsystem* Subsystem = WeakThis.Get())
			{
				Subsystem->FinishKeyCapture(Key, bCancelled);
			}
		}));

	// Index 0: before every other pre-processor, CommonUI's included.
	FSlateApplication::Get().RegisterInputPreProcessor(CaptureProcessor, 0);
	return true;
}

void UInputFrameSubsystem::FinishKeyCapture(const FKey& Key, bool bCancelled)
{
	FInputFrameKeyCaptureCallback Callback = MoveTemp(CaptureCallback);
	CaptureCallback = nullptr;
	if (Callback)
	{
		Callback(Key, bCancelled);
	}
}

void UInputFrameSubsystem::CancelKeyCapture()
{
	if (CaptureProcessor.IsValid() && CaptureProcessor->IsCapturing())
	{
		CaptureProcessor->StopCapture();
		FinishKeyCapture(EKeys::Invalid, true);
	}
}

bool UInputFrameSubsystem::IsCapturingKey() const
{
	return CaptureProcessor.IsValid() && CaptureProcessor->IsCapturing();
}
