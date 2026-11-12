// CoreFramework UI System - Settings
#pragma once

#include "CoreMinimal.h"
#include "UISystem/Menu/Tabs/BaseTabWidget.h"
#include "GraphicsSettingsTabWidget.generated.h"

class UOptionSettingRowWidget;
class USliderSettingRowWidget;
class UToggleSettingRowWidget;

/**
 * Graphics page of a settings menu, on the engine's UGameUserSettings (scalability groups, window mode, resolution,
 * V-Sync, frame rate limit, resolution scale). Changes wait for the menu's Apply button; Revert drops them.
 *
 * Quality preset
 *   QualityPresetRow offers Low / Medium / High / Ultra and sets every quality row at once. When the player changes one
 *   quality row so that they no longer match, the preset shows Custom by itself; when they match again it shows that
 *   level. "Custom" only exists while the rows differ (it cannot be picked).
 *
 * Rows are found by name and all optional: place only the ones the game wants.
 *   Option rows (UOptionSettingRowWidget):  QualityPresetRow, WindowModeRow, ResolutionRow, FrameRateLimitRow,
 *     ViewDistanceRow, ShadowsRow, GlobalIlluminationRow, ReflectionsRow, AntiAliasingRow, TexturesRow, EffectsRow,
 *     PostProcessingRow, FoliageRow, ShadingRow
 *   Toggle row (UToggleSettingRowWidget):   VSyncRow
 *   Slider row (USliderSettingRowWidget):   ResolutionScaleRow (screen percentage)
 * Texts (quality names, window modes, frame rates) are Class Defaults, so they can be translated or renamed.
 *
 * Display changes (window mode, resolution) apply on Apply without a "keep these settings?" countdown.
 */
UCLASS(Abstract, Blueprintable)
class COREFRAMEWORK_API UGraphicsSettingsTabWidget : public UBaseTabWidget
{
	GENERATED_BODY()

public:
	UGraphicsSettingsTabWidget(const FObjectInitializer& ObjectInitializer);

	//~ Begin UBaseTabWidget
	virtual void NotifyTabSelected() override;
	virtual bool HasPendingChanges_Implementation() const override;
	virtual void ApplyChanges_Implementation() override;
	virtual void RevertChanges_Implementation() override;
	//~ End UBaseTabWidget

	/** Stages the engine's recommended quality for this machine (hardware benchmark). Apply to keep it. */
	UFUNCTION(BlueprintCallable, Category = "Graphics Settings")
	void StageAutoDetectedQuality();

	/** -1 = Custom (the quality groups differ), else 0..3 (Low..Ultra) of the staged values. */
	UFUNCTION(BlueprintPure, Category = "Graphics Settings")
	int32 GetStagedPresetLevel() const;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;

	/** After the rows show the staged values (open, preset change, revert). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Graphics Settings", meta = (DisplayName = "On Rows Refreshed"))
	void BP_OnRowsRefreshed();

	// ---- Texts (Class Defaults) ---------------------------------------------------------------------

	/** Names of scalability levels 0..3. Their count is the number of levels offered (max 5 with Cinematic). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Graphics Settings|Texts")
	TArray<FText> QualityLevelNames;

	/** Shown by the preset row while the quality rows differ. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Graphics Settings|Texts")
	FText CustomPresetName;

	/** Names in the engine's EWindowMode order: Fullscreen, Windowed Fullscreen, Windowed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Graphics Settings|Texts")
	TArray<FText> WindowModeNames;

	/** Frame rate limits offered; 0 = unlimited. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Graphics Settings")
	TArray<int32> FrameRateLimits;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Graphics Settings|Texts")
	FText UnlimitedFrameRateName;

	// ---- Rows (exact names, optional) -----------------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UOptionSettingRowWidget> QualityPresetRow;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UOptionSettingRowWidget> WindowModeRow;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UOptionSettingRowWidget> ResolutionRow;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UToggleSettingRowWidget> VSyncRow;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UOptionSettingRowWidget> FrameRateLimitRow;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<USliderSettingRowWidget> ResolutionScaleRow;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UOptionSettingRowWidget> ViewDistanceRow;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UOptionSettingRowWidget> ShadowsRow;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UOptionSettingRowWidget> GlobalIlluminationRow;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UOptionSettingRowWidget> ReflectionsRow;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UOptionSettingRowWidget> AntiAliasingRow;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UOptionSettingRowWidget> TexturesRow;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UOptionSettingRowWidget> EffectsRow;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UOptionSettingRowWidget> PostProcessingRow;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UOptionSettingRowWidget> FoliageRow;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UOptionSettingRowWidget> ShadingRow;

private:
	/** The scalability groups, in the order of the rows above. */
	static constexpr int32 NumQualityGroups = 10;

	struct FGraphicsState
	{
		int32 WindowMode = 1;
		FIntPoint Resolution = FIntPoint::ZeroValue;
		bool bVSync = false;
		float FrameRateLimit = 0.0f;
		float ResolutionScale = 100.0f;
		int32 Quality[NumQualityGroups] = {};

		bool operator==(const FGraphicsState& Other) const;
	};

	void CaptureFromEngine(FGraphicsState& OutState) const;
	void PushToEngine(const FGraphicsState& State) const;
	void RefreshRows();
	void RefreshPresetRow();
	void RefreshResolutionRowEnabled();
	UOptionSettingRowWidget* GetQualityRow(int32 Group) const;
	int32 GetMaxQualityLevel() const;

	UFUNCTION()
	void HandleOptionChanged(UOptionSettingRowWidget* Row, int32 SelectedIndex);

	UFUNCTION()
	void HandleToggleChanged(UToggleSettingRowWidget* Row, bool bIsOn);

	UFUNCTION()
	void HandleSliderChanged(USliderSettingRowWidget* Row, float Value);

	FGraphicsState Snapshot;
	FGraphicsState Staged;
	TArray<FIntPoint> Resolutions;
	bool bRefreshing = false;
};
