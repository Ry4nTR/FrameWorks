// CoreFramework UI System - Settings
#include "UISystem/Settings/GraphicsSettingsTabWidget.h"

#include "GameFramework/GameUserSettings.h"
#include "Kismet/KismetSystemLibrary.h"
#include "UISystem/Settings/OptionSettingRowWidget.h"
#include "UISystem/Settings/SliderSettingRowWidget.h"
#include "UISystem/Settings/ToggleSettingRowWidget.h"

#define LOCTEXT_NAMESPACE "CoreFrameworkSettings"

namespace
{
	/** The scalability groups, in the order of the row properties of the tab. */
	namespace QualityGroup
	{
		enum Type : int32
		{
			ViewDistance = 0,
			Shadows,
			GlobalIllumination,
			Reflections,
			AntiAliasing,
			Textures,
			Effects,
			PostProcessing,
			Foliage,
			Shading
		};
	}

	int32 GetGroupQuality(const UGameUserSettings& Settings, int32 Group)
	{
		switch (Group)
		{
		case QualityGroup::ViewDistance:       return Settings.GetViewDistanceQuality();
		case QualityGroup::Shadows:            return Settings.GetShadowQuality();
		case QualityGroup::GlobalIllumination: return Settings.GetGlobalIlluminationQuality();
		case QualityGroup::Reflections:        return Settings.GetReflectionQuality();
		case QualityGroup::AntiAliasing:       return Settings.GetAntiAliasingQuality();
		case QualityGroup::Textures:           return Settings.GetTextureQuality();
		case QualityGroup::Effects:            return Settings.GetVisualEffectQuality();
		case QualityGroup::PostProcessing:     return Settings.GetPostProcessingQuality();
		case QualityGroup::Foliage:            return Settings.GetFoliageQuality();
		case QualityGroup::Shading:            return Settings.GetShadingQuality();
		default:                 return 0;
		}
	}

	void SetGroupQuality(UGameUserSettings& Settings, int32 Group, int32 Level)
	{
		switch (Group)
		{
		case QualityGroup::ViewDistance:       Settings.SetViewDistanceQuality(Level); break;
		case QualityGroup::Shadows:            Settings.SetShadowQuality(Level); break;
		case QualityGroup::GlobalIllumination: Settings.SetGlobalIlluminationQuality(Level); break;
		case QualityGroup::Reflections:        Settings.SetReflectionQuality(Level); break;
		case QualityGroup::AntiAliasing:       Settings.SetAntiAliasingQuality(Level); break;
		case QualityGroup::Textures:           Settings.SetTextureQuality(Level); break;
		case QualityGroup::Effects:            Settings.SetVisualEffectQuality(Level); break;
		case QualityGroup::PostProcessing:     Settings.SetPostProcessingQuality(Level); break;
		case QualityGroup::Foliage:            Settings.SetFoliageQuality(Level); break;
		case QualityGroup::Shading:            Settings.SetShadingQuality(Level); break;
		default: break;
		}
	}

	FText ResolutionText(const FIntPoint& Resolution)
	{
		return FText::Format(LOCTEXT("ResolutionFormat", "{0} x {1}"), FText::AsNumber(Resolution.X, &FNumberFormattingOptions::DefaultNoGrouping()), FText::AsNumber(Resolution.Y, &FNumberFormattingOptions::DefaultNoGrouping()));
	}
}

bool UGraphicsSettingsTabWidget::FGraphicsState::operator==(const FGraphicsState& Other) const
{
	if (WindowMode != Other.WindowMode || Resolution != Other.Resolution || bVSync != Other.bVSync
		|| !FMath::IsNearlyEqual(FrameRateLimit, Other.FrameRateLimit) || !FMath::IsNearlyEqual(ResolutionScale, Other.ResolutionScale))
	{
		return false;
	}
	for (int32 Group = 0; Group < NumQualityGroups; ++Group)
	{
		if (Quality[Group] != Other.Quality[Group])
		{
			return false;
		}
	}
	return true;
}

UGraphicsSettingsTabWidget::UGraphicsSettingsTabWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	QualityLevelNames = {
		LOCTEXT("QualityLow", "Low"),
		LOCTEXT("QualityMedium", "Medium"),
		LOCTEXT("QualityHigh", "High"),
		LOCTEXT("QualityUltra", "Ultra") };
	CustomPresetName = LOCTEXT("QualityCustom", "Custom");
	WindowModeNames = {
		LOCTEXT("WindowFullscreen", "Fullscreen"),
		LOCTEXT("WindowBorderless", "Borderless"),
		LOCTEXT("WindowWindowed", "Windowed") };
	FrameRateLimits = { 30, 60, 90, 120, 144, 0 };
	UnlimitedFrameRateName = LOCTEXT("FrameRateUnlimited", "Unlimited");
}

// ---------------------------------------------------------------------------------------------------------------------
// Rows
// ---------------------------------------------------------------------------------------------------------------------

UOptionSettingRowWidget* UGraphicsSettingsTabWidget::GetQualityRow(int32 Group) const
{
	switch (Group)
	{
	case QualityGroup::ViewDistance:       return ViewDistanceRow;
	case QualityGroup::Shadows:            return ShadowsRow;
	case QualityGroup::GlobalIllumination: return GlobalIlluminationRow;
	case QualityGroup::Reflections:        return ReflectionsRow;
	case QualityGroup::AntiAliasing:       return AntiAliasingRow;
	case QualityGroup::Textures:           return TexturesRow;
	case QualityGroup::Effects:            return EffectsRow;
	case QualityGroup::PostProcessing:     return PostProcessingRow;
	case QualityGroup::Foliage:            return FoliageRow;
	case QualityGroup::Shading:            return ShadingRow;
	default:                 return nullptr;
	}
}

int32 UGraphicsSettingsTabWidget::GetMaxQualityLevel() const
{
	return FMath::Clamp(QualityLevelNames.Num(), 1, 5) - 1;
}

void UGraphicsSettingsTabWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	TArray<UOptionSettingRowWidget*> OptionRows = { QualityPresetRow, WindowModeRow, ResolutionRow, FrameRateLimitRow };
	for (int32 Group = 0; Group < NumQualityGroups; ++Group)
	{
		OptionRows.Add(GetQualityRow(Group));
	}
	for (UOptionSettingRowWidget* Row : OptionRows)
	{
		if (Row)
		{
			Row->OnSelectionChanged.AddUniqueDynamic(this, &UGraphicsSettingsTabWidget::HandleOptionChanged);
		}
	}
	if (VSyncRow)
	{
		VSyncRow->OnToggled.AddUniqueDynamic(this, &UGraphicsSettingsTabWidget::HandleToggleChanged);
	}
	if (ResolutionScaleRow)
	{
		ResolutionScaleRow->OnValueChanged.AddUniqueDynamic(this, &UGraphicsSettingsTabWidget::HandleSliderChanged);
	}
}

void UGraphicsSettingsTabWidget::NativeConstruct()
{
	Super::NativeConstruct();

	UKismetSystemLibrary::GetSupportedFullscreenResolutions(Resolutions);
	Resolutions.Sort([](const FIntPoint& A, const FIntPoint& B) { return A.X != B.X ? A.X > B.X : A.Y > B.Y; });

	CaptureFromEngine(Snapshot);
	Staged = Snapshot;
	RefreshRows();
}

void UGraphicsSettingsTabWidget::NotifyTabSelected()
{
	Super::NotifyTabSelected();

	// Nothing pending: show what the engine uses now (it may have changed elsewhere).
	if (Staged == Snapshot)
	{
		CaptureFromEngine(Snapshot);
		Staged = Snapshot;
		RefreshRows();
	}
}

void UGraphicsSettingsTabWidget::CaptureFromEngine(FGraphicsState& OutState) const
{
	const UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings();
	if (!Settings)
	{
		return;
	}

	OutState.WindowMode = static_cast<int32>(Settings->GetFullscreenMode());
	OutState.Resolution = Settings->GetScreenResolution();
	OutState.bVSync = Settings->IsVSyncEnabled();
	OutState.FrameRateLimit = Settings->GetFrameRateLimit();

	float ScaleNormalized = 0.0f, ScaleValue = 100.0f, MinScale = 0.0f, MaxScale = 100.0f;
	Settings->GetResolutionScaleInformationEx(ScaleNormalized, ScaleValue, MinScale, MaxScale);
	OutState.ResolutionScale = ScaleValue;

	const int32 MaxLevel = GetMaxQualityLevel();
	for (int32 Group = 0; Group < NumQualityGroups; ++Group)
	{
		OutState.Quality[Group] = FMath::Clamp(GetGroupQuality(*Settings, Group), 0, MaxLevel);
	}
}

void UGraphicsSettingsTabWidget::PushToEngine(const FGraphicsState& State) const
{
	UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings();
	if (!Settings)
	{
		return;
	}

	Settings->SetFullscreenMode(static_cast<EWindowMode::Type>(State.WindowMode));
	if (State.Resolution.X > 0 && State.Resolution.Y > 0)
	{
		Settings->SetScreenResolution(State.Resolution);
	}
	Settings->SetVSyncEnabled(State.bVSync);
	Settings->SetFrameRateLimit(State.FrameRateLimit);
	Settings->SetResolutionScaleValueEx(State.ResolutionScale);
	for (int32 Group = 0; Group < NumQualityGroups; ++Group)
	{
		SetGroupQuality(*Settings, Group, State.Quality[Group]);
	}
}

int32 UGraphicsSettingsTabWidget::GetStagedPresetLevel() const
{
	const int32 First = Staged.Quality[0];
	for (int32 Group = 1; Group < NumQualityGroups; ++Group)
	{
		if (Staged.Quality[Group] != First)
		{
			return -1;
		}
	}
	return First;
}

void UGraphicsSettingsTabWidget::RefreshPresetRow()
{
	if (!QualityPresetRow)
	{
		return;
	}

	const int32 Level = GetStagedPresetLevel();
	TArray<FText> PresetOptions = QualityLevelNames;
	if (Level < 0)
	{
		// "Custom" exists only while the groups differ: it is shown, it cannot be chosen.
		PresetOptions.Add(CustomPresetName);
		QualityPresetRow->SetOptions(PresetOptions, PresetOptions.Num() - 1);
	}
	else
	{
		QualityPresetRow->SetOptions(PresetOptions, Level);
	}
}

void UGraphicsSettingsTabWidget::RefreshResolutionRowEnabled()
{
	if (ResolutionRow)
	{
		// Borderless always uses the desktop resolution.
		ResolutionRow->SetRowEnabled(Staged.WindowMode != static_cast<int32>(EWindowMode::WindowedFullscreen));
	}
}

void UGraphicsSettingsTabWidget::RefreshRows()
{
	TGuardValue<bool> Guard(bRefreshing, true);

	for (int32 Group = 0; Group < NumQualityGroups; ++Group)
	{
		if (UOptionSettingRowWidget* Row = GetQualityRow(Group))
		{
			Row->SetOptions(QualityLevelNames, Staged.Quality[Group]);
		}
	}
	RefreshPresetRow();

	if (WindowModeRow)
	{
		WindowModeRow->SetOptions(WindowModeNames, Staged.WindowMode);
	}

	if (ResolutionRow)
	{
		TArray<FText> ResolutionNames;
		int32 Selected = INDEX_NONE;
		for (int32 Index = 0; Index < Resolutions.Num(); ++Index)
		{
			ResolutionNames.Add(ResolutionText(Resolutions[Index]));
			if (Resolutions[Index] == Staged.Resolution)
			{
				Selected = Index;
			}
		}
		if (Selected == INDEX_NONE && Staged.Resolution.X > 0)
		{
			// The current resolution is not a fullscreen mode (a windowed size): keep it as a choice.
			Resolutions.Add(Staged.Resolution);
			ResolutionNames.Add(ResolutionText(Staged.Resolution));
			Selected = ResolutionNames.Num() - 1;
		}
		ResolutionRow->SetOptions(ResolutionNames, FMath::Max(Selected, 0));
		RefreshResolutionRowEnabled();
	}

	if (VSyncRow)
	{
		VSyncRow->SetIsOn(Staged.bVSync);
	}

	if (FrameRateLimitRow)
	{
		TArray<FText> RateNames;
		int32 Selected = INDEX_NONE;
		for (int32 Index = 0; Index < FrameRateLimits.Num(); ++Index)
		{
			const int32 Limit = FrameRateLimits[Index];
			RateNames.Add(Limit <= 0 ? UnlimitedFrameRateName : FText::AsNumber(Limit));
			if (FMath::IsNearlyEqual(static_cast<float>(FMath::Max(Limit, 0)), Staged.FrameRateLimit))
			{
				Selected = Index;
			}
		}
		FrameRateLimitRow->SetOptions(RateNames, FMath::Max(Selected, 0));
	}

	if (ResolutionScaleRow)
	{
		float ScaleNormalized = 0.0f, ScaleValue = 100.0f, MinScale = 50.0f, MaxScale = 100.0f;
		if (const UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings())
		{
			Settings->GetResolutionScaleInformationEx(ScaleNormalized, ScaleValue, MinScale, MaxScale);
		}
		ResolutionScaleRow->SetRange(MinScale, MaxScale, 5.0f);
		ResolutionScaleRow->SetValue(Staged.ResolutionScale);
	}

	BP_OnRowsRefreshed();
}

// ---------------------------------------------------------------------------------------------------------------------
// Player changes
// ---------------------------------------------------------------------------------------------------------------------

void UGraphicsSettingsTabWidget::HandleOptionChanged(UOptionSettingRowWidget* Row, int32 SelectedIndex)
{
	if (bRefreshing || !Row)
	{
		return;
	}

	if (Row == QualityPresetRow)
	{
		if (QualityLevelNames.IsValidIndex(SelectedIndex))
		{
			for (int32 Group = 0; Group < NumQualityGroups; ++Group)
			{
				Staged.Quality[Group] = SelectedIndex;
			}
			RefreshRows();
		}
		else
		{
			RefreshPresetRow(); // "Custom" was reached by navigation: show the real state again.
		}
		return;
	}

	for (int32 Group = 0; Group < NumQualityGroups; ++Group)
	{
		if (Row == GetQualityRow(Group))
		{
			Staged.Quality[Group] = SelectedIndex;
			TGuardValue<bool> Guard(bRefreshing, true);
			RefreshPresetRow(); // Custom, or a level when every group matches again.
			return;
		}
	}

	if (Row == WindowModeRow)
	{
		Staged.WindowMode = FMath::Clamp(SelectedIndex, 0, 2);
		if (Staged.WindowMode == static_cast<int32>(EWindowMode::WindowedFullscreen))
		{
			if (const UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings())
			{
				Staged.Resolution = Settings->GetDesktopResolution();
			}
		}
		RefreshResolutionRowEnabled();
	}
	else if (Row == ResolutionRow)
	{
		if (Resolutions.IsValidIndex(SelectedIndex))
		{
			Staged.Resolution = Resolutions[SelectedIndex];
		}
	}
	else if (Row == FrameRateLimitRow)
	{
		if (FrameRateLimits.IsValidIndex(SelectedIndex))
		{
			Staged.FrameRateLimit = static_cast<float>(FMath::Max(FrameRateLimits[SelectedIndex], 0));
		}
	}
}

void UGraphicsSettingsTabWidget::HandleToggleChanged(UToggleSettingRowWidget* Row, bool bIsOn)
{
	if (!bRefreshing && Row == VSyncRow)
	{
		Staged.bVSync = bIsOn;
	}
}

void UGraphicsSettingsTabWidget::HandleSliderChanged(USliderSettingRowWidget* Row, float Value)
{
	if (!bRefreshing && Row == ResolutionScaleRow)
	{
		Staged.ResolutionScale = Value;
	}
}

void UGraphicsSettingsTabWidget::StageAutoDetectedQuality()
{
	UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings();
	if (!Settings)
	{
		return;
	}

	// The benchmark writes its result into the settings object: read it, then put the object back as it was.
	Settings->RunHardwareBenchmark();
	const int32 MaxLevel = GetMaxQualityLevel();
	for (int32 Group = 0; Group < NumQualityGroups; ++Group)
	{
		Staged.Quality[Group] = FMath::Clamp(GetGroupQuality(*Settings, Group), 0, MaxLevel);
		SetGroupQuality(*Settings, Group, Snapshot.Quality[Group]);
	}
	RefreshRows();
}

// ---------------------------------------------------------------------------------------------------------------------
// Apply / revert
// ---------------------------------------------------------------------------------------------------------------------

bool UGraphicsSettingsTabWidget::HasPendingChanges_Implementation() const
{
	return !(Staged == Snapshot);
}

void UGraphicsSettingsTabWidget::ApplyChanges_Implementation()
{
	UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings();
	if (!Settings)
	{
		return;
	}

	PushToEngine(Staged);
	Settings->ApplySettings(/*bCheckForCommandLineOverrides=*/false); // Also writes GameUserSettings.ini.
	Snapshot = Staged;
}

void UGraphicsSettingsTabWidget::RevertChanges_Implementation()
{
	Staged = Snapshot;
	RefreshRows();
}

#undef LOCTEXT_NAMESPACE
