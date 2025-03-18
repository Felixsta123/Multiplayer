// VoxelSettingsWidget.h
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "VoxelTerrainSettings.h"
#include "Components/Slider.h"
#include "Components/SpinBox.h"
#include "Components/CheckBox.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "VoxelSettingsWidget.generated.h"

/**
 * Widget for configuring voxel terrain settings in a lobby
 */
UCLASS()
class WORMS_3D_API UVoxelSettingsWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // Widget initialization
    virtual void NativeConstruct() override;

    // Widget update each frame
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    // Main functions
    UFUNCTION(BlueprintCallable, Category = "Voxel Settings")
    void SaveSettings();

    UFUNCTION(BlueprintCallable, Category = "Voxel Settings")
    void LoadSettings();

    UFUNCTION(BlueprintCallable, Category = "Voxel Settings")
    void ResetToDefaults();

    // Event triggered when settings are changed
    UFUNCTION(BlueprintImplementableEvent, Category = "Voxel Settings")
    void OnSettingsChanged();

protected:
    // === Basic controls section ===

    // Main vertical container
    UPROPERTY(meta = (BindWidget))
    UVerticalBox* MainContainer;

    // Widget title
    UPROPERTY(meta = (BindWidget))
    UTextBlock* TitleText;

    // === Building settings section ===

    // Number of buildings
    UPROPERTY(meta = (BindWidget))
    USpinBox* NumberOfBuildingsSpinBox;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* NumberOfBuildingsLabel;

    // Spawn area size
    UPROPERTY(meta = (BindWidget))
    USlider* SpawnAreaSizeSlider;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* SpawnAreaSizeLabel;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* SpawnAreaSizeValue;

    // === Staircase building settings (simplified) ===
    UPROPERTY(meta = (BindWidget))
    UCheckBox* EnableStaircaseBuildingsCheckBox;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* EnableStaircaseBuildingsLabel;

    UPROPERTY(meta = (BindWidget))
    USpinBox* NumberOfStaircaseBuildingsSpinBox;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* NumberOfStaircaseBuildingsLabel;

    // === Voxel settings section ===

    // Grid size X
    UPROPERTY(meta = (BindWidget))
    USpinBox* GridSizeXSpinBox;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* GridSizeXLabel;

    // Grid size Y
    UPROPERTY(meta = (BindWidget))
    USpinBox* GridSizeYSpinBox;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* GridSizeYLabel;

    // Grid size Z
    UPROPERTY(meta = (BindWidget))
    USpinBox* GridSizeZSpinBox;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* GridSizeZLabel;

    // Maximum height variation
    UPROPERTY(meta = (BindWidget))
    USpinBox* MaxHeightVariationSpinBox;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* MaxHeightVariationLabel;

    UFUNCTION()
    void OnMaxHeightVariationChanged(float Value);

    // Voxel size
    UPROPERTY(meta = (BindWidget))
    USlider* VoxelSizeSlider;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* VoxelSizeLabel;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* VoxelSizeValue;

    // Smoothing factor
    UPROPERTY(meta = (BindWidget))
    USlider* SmoothingFactorSlider;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* SmoothingFactorLabel;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* SmoothingFactorValue;

    // Use random colors
    UPROPERTY(meta = (BindWidget))
    UCheckBox* UseRandomColorsCheckBox;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* UseRandomColorsLabel;

    // Cube margin
    UPROPERTY(meta = (BindWidget))
    USlider* CubeMarginSlider;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* CubeMarginLabel;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* CubeMarginValue;

    // === Debris settings section ===

    // Spawn debris on destruction
    UPROPERTY(meta = (BindWidget))
    UCheckBox* SpawnDebrisCheckBox;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* SpawnDebrisLabel;

    // Debris multiplier
    UPROPERTY(meta = (BindWidget))
    USlider* DebrisAmountSlider;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* DebrisAmountLabel;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* DebrisAmountValue;

    // Impact cloud
    UPROPERTY(meta = (BindWidget))
    UCheckBox* SpawnImpactCloudCheckBox;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* SpawnImpactCloudLabel;

    // === Action buttons section ===

    UPROPERTY(meta = (BindWidget))
    UHorizontalBox* ButtonsContainer;

    UPROPERTY(meta = (BindWidget))
    UButton* SaveButton;

    UPROPERTY(meta = (BindWidget))
    UButton* LoadButton;

    UPROPERTY(meta = (BindWidget))
    UButton* DefaultsButton;

    // === Button callbacks ===

    UFUNCTION()
    void OnSaveButtonClicked();

    UFUNCTION()
    void OnLoadButtonClicked();

    UFUNCTION()
    void OnDefaultsButtonClicked();

    // === Value change callbacks ===

    UFUNCTION()
    void OnNumberOfBuildingsChanged(float Value);

    UFUNCTION()
    void OnSpawnAreaSizeChanged(float Value);

    // Staircase settings callbacks (simplified)
    UFUNCTION()
    void OnEnableStaircaseBuildingsChanged(bool bIsChecked);

    UFUNCTION()
    void OnNumberOfStaircaseBuildingsChanged(float Value);

    UFUNCTION()
    void OnGridSizeXChanged(float Value);

    UFUNCTION()
    void OnGridSizeYChanged(float Value);

    UFUNCTION()
    void OnGridSizeZChanged(float Value);

    UFUNCTION()
    void OnVoxelSizeChanged(float Value);

    UFUNCTION()
    void OnSmoothingFactorChanged(float Value);

    UFUNCTION()
    void OnUseRandomColorsChanged(bool Value);

    UFUNCTION()
    void OnCubeMarginChanged(float Value);

    UFUNCTION()
    void OnSpawnDebrisChanged(bool Value);

    UFUNCTION()
    void OnDebrisAmountChanged(float Value);

    UFUNCTION()
    void OnSpawnImpactCloudChanged(bool Value);

    // === Utility functions ===

    // Update widgets from settings
    void UpdateWidgetsFromSettings(const FVoxelTerrainSettings& Settings);

    // Get settings from widgets
    FVoxelTerrainSettings GetSettingsFromWidgets();

    // Update slider text values
    void UpdateSliderLabels();

    // Enable/disable controls based on dependencies
    void UpdateControlDependencies();

    // Current settings
    UPROPERTY()
    FVoxelTerrainSettings CurrentSettings;

    // Update flags
    bool bUpdatingUI;
    float UpdateTimer;
    const float UpdateInterval = 0.5f;
};