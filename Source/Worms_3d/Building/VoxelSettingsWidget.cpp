// VoxelSettingsWidget.cpp
#include "VoxelSettingsWidget.h"
#include "VoxelTerrainSettings.h"
#include "Components/SpinBox.h"
#include "Components/Slider.h"
#include "Components/CheckBox.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"

void UVoxelSettingsWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // Initializer flags
    bUpdatingUI = false;
    UpdateTimer = 0.0f;

    // Configure title
    if (TitleText)
    {
        TitleText->SetText(FText::FromString(TEXT("Voxel Terrain Settings")));
    }

    // Connect button callbacks
    if (SaveButton)
    {
        SaveButton->OnClicked.AddDynamic(this, &UVoxelSettingsWidget::OnSaveButtonClicked);
    }

    if (LoadButton)
    {
        LoadButton->OnClicked.AddDynamic(this, &UVoxelSettingsWidget::OnLoadButtonClicked);
    }

    if (DefaultsButton)
    {
        DefaultsButton->OnClicked.AddDynamic(this, &UVoxelSettingsWidget::OnDefaultsButtonClicked);
    }

    // Connect building count control callbacks
    if (NumberOfBuildingsSpinBox)
    {
        NumberOfBuildingsSpinBox->SetMinValue(1);
        NumberOfBuildingsSpinBox->SetMaxValue(10);
        NumberOfBuildingsSpinBox->OnValueChanged.AddDynamic(this, &UVoxelSettingsWidget::OnNumberOfBuildingsChanged);

        if (NumberOfBuildingsLabel)
        {
            NumberOfBuildingsLabel->SetText(FText::FromString(TEXT("Number of buildings:")));
        }
    }

    // Connect staircase building controls
    if (EnableStaircaseBuildingsCheckBox)
    {
        EnableStaircaseBuildingsCheckBox->OnCheckStateChanged.AddDynamic(
            this, &UVoxelSettingsWidget::OnEnableStaircaseBuildingsChanged);

        if (EnableStaircaseBuildingsLabel)
        {
            EnableStaircaseBuildingsLabel->SetText(FText::FromString(TEXT("Enable staircase buildings")));
        }
    }

    if (NumberOfStaircaseBuildingsSpinBox)
    {
        NumberOfStaircaseBuildingsSpinBox->SetMinValue(0);
        NumberOfStaircaseBuildingsSpinBox->SetMaxValue(5);
        NumberOfStaircaseBuildingsSpinBox->OnValueChanged.AddDynamic(
            this, &UVoxelSettingsWidget::OnNumberOfStaircaseBuildingsChanged);

        if (NumberOfStaircaseBuildingsLabel)
        {
            NumberOfStaircaseBuildingsLabel->SetText(FText::FromString(TEXT("Number of staircases:")));
        }
    }

    // Connect spawn area size control callbacks
    if (SpawnAreaSizeSlider)
    {
        SpawnAreaSizeSlider->SetMinValue(0.0f);
        SpawnAreaSizeSlider->SetMaxValue(1.0f);
        SpawnAreaSizeSlider->OnValueChanged.AddDynamic(this, &UVoxelSettingsWidget::OnSpawnAreaSizeChanged);

        if (SpawnAreaSizeLabel)
        {
            SpawnAreaSizeLabel->SetText(FText::FromString(TEXT("Spawn area size:")));
        }
    }

    // Connect grid size control callbacks
    if (GridSizeXSpinBox)
    {
        GridSizeXSpinBox->SetMinValue(5);
        GridSizeXSpinBox->SetMaxValue(20);
        GridSizeXSpinBox->OnValueChanged.AddDynamic(this, &UVoxelSettingsWidget::OnGridSizeXChanged);

        if (GridSizeXLabel)
        {
            GridSizeXLabel->SetText(FText::FromString(TEXT("Size X:")));
        }
    }

    if (GridSizeYSpinBox)
    {
        GridSizeYSpinBox->SetMinValue(5);
        GridSizeYSpinBox->SetMaxValue(20);
        GridSizeYSpinBox->OnValueChanged.AddDynamic(this, &UVoxelSettingsWidget::OnGridSizeYChanged);

        if (GridSizeYLabel)
        {
            GridSizeYLabel->SetText(FText::FromString(TEXT("Size Y:")));
        }
    }

    if (GridSizeZSpinBox)
    {
        GridSizeZSpinBox->SetMinValue(5);
        GridSizeZSpinBox->SetMaxValue(20);
        GridSizeZSpinBox->OnValueChanged.AddDynamic(this, &UVoxelSettingsWidget::OnGridSizeZChanged);

        if (GridSizeZLabel)
        {
            GridSizeZLabel->SetText(FText::FromString(TEXT("Size Z:")));
        }
    }

    // Configure height variation control
    if (MaxHeightVariationSpinBox)
    {
        MaxHeightVariationSpinBox->SetMinValue(0);
        MaxHeightVariationSpinBox->SetMaxValue(10);
        MaxHeightVariationSpinBox->OnValueChanged.AddDynamic(this, &UVoxelSettingsWidget::OnMaxHeightVariationChanged);

        if (MaxHeightVariationLabel)
        {
            MaxHeightVariationLabel->SetText(FText::FromString(TEXT("Height variation:")));
        }
    }

    // Connect voxel size control callbacks
    if (VoxelSizeSlider)
    {
        VoxelSizeSlider->SetMinValue(0.0f);
        VoxelSizeSlider->SetMaxValue(1.0f);
        VoxelSizeSlider->OnValueChanged.AddDynamic(this, &UVoxelSettingsWidget::OnVoxelSizeChanged);

        if (VoxelSizeLabel)
        {
            VoxelSizeLabel->SetText(FText::FromString(TEXT("Voxel size:")));
        }
    }

    // Connect smoothing control callbacks
    if (SmoothingFactorSlider)
    {
        SmoothingFactorSlider->SetMinValue(0.0f);
        SmoothingFactorSlider->SetMaxValue(1.0f);
        SmoothingFactorSlider->OnValueChanged.AddDynamic(this, &UVoxelSettingsWidget::OnSmoothingFactorChanged);

        if (SmoothingFactorLabel)
        {
            SmoothingFactorLabel->SetText(FText::FromString(TEXT("Smoothing factor:")));
        }
    }

    // Connect random colors control callbacks
    if (UseRandomColorsCheckBox)
    {
        UseRandomColorsCheckBox->OnCheckStateChanged.AddDynamic(this, &UVoxelSettingsWidget::OnUseRandomColorsChanged);

        if (UseRandomColorsLabel)
        {
            UseRandomColorsLabel->SetText(FText::FromString(TEXT("Use random colors")));
        }
    }

    // Connect cube margin control callbacks
    if (CubeMarginSlider)
    {
        CubeMarginSlider->SetMinValue(0.0f);
        CubeMarginSlider->SetMaxValue(1.0f);
        CubeMarginSlider->OnValueChanged.AddDynamic(this, &UVoxelSettingsWidget::OnCubeMarginChanged);

        if (CubeMarginLabel)
        {
            CubeMarginLabel->SetText(FText::FromString(TEXT("Cube margin:")));
        }
    }

    // Connect debris control callbacks
    if (SpawnDebrisCheckBox)
    {
        SpawnDebrisCheckBox->OnCheckStateChanged.AddDynamic(this, &UVoxelSettingsWidget::OnSpawnDebrisChanged);

        if (SpawnDebrisLabel)
        {
            SpawnDebrisLabel->SetText(FText::FromString(TEXT("Generate debris")));
        }
    }

    // Connect debris multiplier control callbacks
    if (DebrisAmountSlider)
    {
        DebrisAmountSlider->SetMinValue(0.0f);
        DebrisAmountSlider->SetMaxValue(1.0f);
        DebrisAmountSlider->OnValueChanged.AddDynamic(this, &UVoxelSettingsWidget::OnDebrisAmountChanged);

        if (DebrisAmountLabel)
        {
            DebrisAmountLabel->SetText(FText::FromString(TEXT("Debris amount:")));
        }
    }

    // Connect impact cloud control callbacks
    if (SpawnImpactCloudCheckBox)
    {
        SpawnImpactCloudCheckBox->OnCheckStateChanged.AddDynamic(this, &UVoxelSettingsWidget::OnSpawnImpactCloudChanged);

        if (SpawnImpactCloudLabel)
        {
            SpawnImpactCloudLabel->SetText(FText::FromString(TEXT("Generate impact cloud")));
        }
    }

    // Configure buttons
    if (SaveButton)
    {
        SaveButton->SetToolTipText(FText::FromString(TEXT("Save settings")));
    }

    if (LoadButton)
    {
        LoadButton->SetToolTipText(FText::FromString(TEXT("Load saved settings")));
    }

    if (DefaultsButton)
    {
        DefaultsButton->SetToolTipText(FText::FromString(TEXT("Reset to default values")));
    }

    // Load current settings
    LoadSettings();
}

void UVoxelSettingsWidget::OnEnableStaircaseBuildingsChanged(bool bIsChecked)
{
    if (!bUpdatingUI)
    {
        CurrentSettings.bEnableStaircaseBuildings = bIsChecked;
        UpdateControlDependencies();
        OnSettingsChanged();
    }
}

void UVoxelSettingsWidget::OnNumberOfStaircaseBuildingsChanged(float Value)
{
    if (!bUpdatingUI)
    {
        CurrentSettings.NumberOfStaircaseBuildings = FMath::RoundToInt(Value);
        OnSettingsChanged();
    }
}

void UVoxelSettingsWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    // Update slider labels at regular intervals
    if (!bUpdatingUI)
    {
        UpdateTimer += InDeltaTime;
        if (UpdateTimer >= UpdateInterval)
        {
            UpdateTimer = 0.0f;
            UpdateSliderLabels();
        }
    }
}

void UVoxelSettingsWidget::SaveSettings()
{
    // Get settings from widgets
    FVoxelTerrainSettings NewSettings = GetSettingsFromWidgets();

    // Update settings manager
    UVoxelTerrainSettingsManager* Manager = UVoxelTerrainSettingsManager::GetInstance();
    if (Manager)
    {
        Manager->SetSettings(NewSettings);
        Manager->SaveSettings();

        // Update our local copy
        CurrentSettings = Manager->GetSettings();

        // Update user interface
        UpdateWidgetsFromSettings(CurrentSettings);
    }

    // Trigger settings changed event
    OnSettingsChanged();
}

void UVoxelSettingsWidget::LoadSettings()
{
    // Load settings from manager
    UVoxelTerrainSettingsManager* Manager = UVoxelTerrainSettingsManager::GetInstance();
    if (Manager)
    {
        CurrentSettings = Manager->GetSettings();

        // Update widgets
        UpdateWidgetsFromSettings(CurrentSettings);
    }
}

void UVoxelSettingsWidget::ResetToDefaults()
{
    // Reset settings to default values
    UVoxelTerrainSettingsManager* Manager = UVoxelTerrainSettingsManager::GetInstance();
    if (Manager)
    {
        Manager->ResetToDefaults();
        CurrentSettings = Manager->GetSettings();

        // Update widgets
        UpdateWidgetsFromSettings(CurrentSettings);

        // Trigger settings changed event
        OnSettingsChanged();
    }
}

void UVoxelSettingsWidget::OnSaveButtonClicked()
{
    SaveSettings();
}

void UVoxelSettingsWidget::OnLoadButtonClicked()
{
    LoadSettings();
}

void UVoxelSettingsWidget::OnDefaultsButtonClicked()
{
    ResetToDefaults();
}

void UVoxelSettingsWidget::OnNumberOfBuildingsChanged(float Value)
{
    if (!bUpdatingUI)
    {
        CurrentSettings.NumberOfBuildings = FMath::RoundToInt(Value);
        OnSettingsChanged();
    }
}

void UVoxelSettingsWidget::OnSpawnAreaSizeChanged(float Value)
{
    if (!bUpdatingUI)
    {
        CurrentSettings.SpawnAreaSize = 500.0f + (Value * 4500.0f);
        UpdateSliderLabels();
        OnSettingsChanged();
    }
}

void UVoxelSettingsWidget::OnGridSizeXChanged(float Value)
{
    if (!bUpdatingUI)
    {
        CurrentSettings.GridSizeX = FMath::RoundToInt(Value);
        OnSettingsChanged();
    }
}

void UVoxelSettingsWidget::OnGridSizeYChanged(float Value)
{
    if (!bUpdatingUI)
    {
        CurrentSettings.GridSizeY = FMath::RoundToInt(Value);
        OnSettingsChanged();
    }
}

void UVoxelSettingsWidget::OnGridSizeZChanged(float Value)
{
    if (!bUpdatingUI)
    {
        CurrentSettings.GridSizeZ = FMath::RoundToInt(Value);
        OnSettingsChanged();
    }
}

void UVoxelSettingsWidget::OnMaxHeightVariationChanged(float Value)
{
    if (!bUpdatingUI)
    {
        CurrentSettings.MaxHeightVariation = FMath::RoundToInt(Value);
        OnSettingsChanged();
    }
}

void UVoxelSettingsWidget::OnVoxelSizeChanged(float Value)
{
    if (!bUpdatingUI)
    {
        CurrentSettings.VoxelSize = 50.0f + (Value * 150.0f);
        UpdateSliderLabels();
        OnSettingsChanged();
    }
}

void UVoxelSettingsWidget::OnSmoothingFactorChanged(float Value)
{
    if (!bUpdatingUI)
    {
        CurrentSettings.SmoothingFactor = Value * 0.1f;
        UpdateSliderLabels();
        OnSettingsChanged();
    }
}

void UVoxelSettingsWidget::OnUseRandomColorsChanged(bool Value)
{
    if (!bUpdatingUI)
    {
        CurrentSettings.bUseRandomColors = Value;
        OnSettingsChanged();
    }
}

void UVoxelSettingsWidget::OnCubeMarginChanged(float Value)
{
    if (!bUpdatingUI)
    {
        CurrentSettings.CubeMargin = Value * 0.05f;
        UpdateSliderLabels();
        OnSettingsChanged();
    }
}

void UVoxelSettingsWidget::OnSpawnDebrisChanged(bool Value)
{
    if (!bUpdatingUI)
    {
        CurrentSettings.bSpawnDebrisOnDestruction = Value;
        UpdateControlDependencies();
        OnSettingsChanged();
    }
}

void UVoxelSettingsWidget::OnDebrisAmountChanged(float Value)
{
    if (!bUpdatingUI)
    {
        CurrentSettings.DebrisAmountMultiplier = 0.1f + (Value * 2.9f);
        UpdateSliderLabels();
        OnSettingsChanged();
    }
}

void UVoxelSettingsWidget::OnSpawnImpactCloudChanged(bool Value)
{
    if (!bUpdatingUI)
    {
        CurrentSettings.bSpawnImpactCloud = Value;
        OnSettingsChanged();
    }
}

void UVoxelSettingsWidget::UpdateWidgetsFromSettings(const FVoxelTerrainSettings& Settings)
{
    // Mark as updating to avoid callback loops
    bUpdatingUI = true;

    if (NumberOfStaircaseBuildingsSpinBox)
    {
        NumberOfStaircaseBuildingsSpinBox->SetValue(Settings.NumberOfStaircaseBuildings);
    }

    // Update each widget with corresponding value
    if (NumberOfBuildingsSpinBox)
    {
        NumberOfBuildingsSpinBox->SetValue(Settings.NumberOfBuildings);
    }

    if (SpawnAreaSizeSlider)
    {
        SpawnAreaSizeSlider->SetValue((Settings.SpawnAreaSize - 500.0f) / 4500.0f);
    }

    if (GridSizeXSpinBox)
    {
        GridSizeXSpinBox->SetValue(Settings.GridSizeX);
    }

    if (GridSizeYSpinBox)
    {
        GridSizeYSpinBox->SetValue(Settings.GridSizeY);
    }

    if (GridSizeZSpinBox)
    {
        GridSizeZSpinBox->SetValue(Settings.GridSizeZ);
    }

    if (MaxHeightVariationSpinBox)
    {
        MaxHeightVariationSpinBox->SetValue(Settings.MaxHeightVariation);
    }

    if (VoxelSizeSlider)
    {
        VoxelSizeSlider->SetValue((Settings.VoxelSize - 50.0f) / 150.0f);
    }

    if (SmoothingFactorSlider)
    {
        SmoothingFactorSlider->SetValue(Settings.SmoothingFactor / 0.1f);
    }

    if (UseRandomColorsCheckBox)
    {
        UseRandomColorsCheckBox->SetCheckedState(Settings.bUseRandomColors ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
    }

    if (CubeMarginSlider)
    {
        CubeMarginSlider->SetValue(Settings.CubeMargin / 0.05f);
    }

    if (SpawnDebrisCheckBox)
    {
        SpawnDebrisCheckBox->SetCheckedState(Settings.bSpawnDebrisOnDestruction ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
    }

    if (DebrisAmountSlider)
    {
        DebrisAmountSlider->SetValue((Settings.DebrisAmountMultiplier - 0.1f) / 2.9f);
    }

    if (SpawnImpactCloudCheckBox)
    {
        SpawnImpactCloudCheckBox->SetCheckedState(Settings.bSpawnImpactCloud ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
    }

    // Update labels and dependencies
    UpdateSliderLabels();
    UpdateControlDependencies();

    // End of update
    bUpdatingUI = false;
}

FVoxelTerrainSettings UVoxelSettingsWidget::GetSettingsFromWidgets()
{
    FVoxelTerrainSettings Settings;

    if (EnableStaircaseBuildingsCheckBox)
    {
        Settings.bEnableStaircaseBuildings =
            (EnableStaircaseBuildingsCheckBox->GetCheckedState() == ECheckBoxState::Checked);
    }

    if (NumberOfStaircaseBuildingsSpinBox)
    {
        Settings.NumberOfStaircaseBuildings = FMath::RoundToInt(NumberOfStaircaseBuildingsSpinBox->GetValue());
    }

    // Read values from each widget
    if (NumberOfBuildingsSpinBox)
    {
        Settings.NumberOfBuildings = FMath::RoundToInt(NumberOfBuildingsSpinBox->GetValue());
    }

    if (SpawnAreaSizeSlider)
    {
        Settings.SpawnAreaSize = 500.0f + (SpawnAreaSizeSlider->GetValue() * 4500.0f);
    }

    if (GridSizeXSpinBox)
    {
        Settings.GridSizeX = FMath::RoundToInt(GridSizeXSpinBox->GetValue());
    }

    if (GridSizeYSpinBox)
    {
        Settings.GridSizeY = FMath::RoundToInt(GridSizeYSpinBox->GetValue());
    }

    if (GridSizeZSpinBox)
    {
        Settings.GridSizeZ = FMath::RoundToInt(GridSizeZSpinBox->GetValue());
    }

    if (MaxHeightVariationSpinBox)
    {
        Settings.MaxHeightVariation = FMath::RoundToInt(MaxHeightVariationSpinBox->GetValue());
    }

    if (VoxelSizeSlider)
    {
        Settings.VoxelSize = 50.0f + (VoxelSizeSlider->GetValue() * 150.0f);
    }

    if (SmoothingFactorSlider)
    {
        Settings.SmoothingFactor = SmoothingFactorSlider->GetValue() * 0.1f;
    }

    if (UseRandomColorsCheckBox)
    {
        Settings.bUseRandomColors = (UseRandomColorsCheckBox->GetCheckedState() == ECheckBoxState::Checked);
    }

    if (CubeMarginSlider)
    {
        Settings.CubeMargin = CubeMarginSlider->GetValue() * 0.05f;
    }

    if (SpawnDebrisCheckBox)
    {
        Settings.bSpawnDebrisOnDestruction = (SpawnDebrisCheckBox->GetCheckedState() == ECheckBoxState::Checked);
    }

    if (DebrisAmountSlider)
    {
        Settings.DebrisAmountMultiplier = 0.1f + (DebrisAmountSlider->GetValue() * 2.9f);
    }

    if (SpawnImpactCloudCheckBox)
    {
        Settings.bSpawnImpactCloud = (SpawnImpactCloudCheckBox->GetCheckedState() == ECheckBoxState::Checked);
    }

    // Validate settings before returning
    Settings.Validate();

    return Settings;
}

void UVoxelSettingsWidget::UpdateSliderLabels()
{
    if (SpawnAreaSizeValue)
    {
        SpawnAreaSizeValue->SetText(FText::FromString(FString::Printf(TEXT("%.0f"), CurrentSettings.SpawnAreaSize)));
    }

    if (SpawnAreaSizeValue)
    {
        SpawnAreaSizeValue->SetText(FText::FromString(FString::Printf(TEXT("%.0f"), CurrentSettings.SpawnAreaSize)));
    }

    if (VoxelSizeValue)
    {
        VoxelSizeValue->SetText(FText::FromString(FString::Printf(TEXT("%.0f"), CurrentSettings.VoxelSize)));
    }

    if (SmoothingFactorValue)
    {
        SmoothingFactorValue->SetText(FText::FromString(FString::Printf(TEXT("%.3f"), CurrentSettings.SmoothingFactor)));
    }

    if (CubeMarginValue)
    {
        CubeMarginValue->SetText(FText::FromString(FString::Printf(TEXT("%.3f"), CurrentSettings.CubeMargin)));
    }

    if (DebrisAmountValue)
    {
        DebrisAmountValue->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), CurrentSettings.DebrisAmountMultiplier)));
    }
}

void UVoxelSettingsWidget::UpdateControlDependencies()
{
    // Disable debris controls if debris generation is disabled
    bool bDebrisEnabled = CurrentSettings.bSpawnDebrisOnDestruction;
    bool bStaircasesEnabled = CurrentSettings.bEnableStaircaseBuildings;

    if (NumberOfStaircaseBuildingsSpinBox)
    {
        NumberOfStaircaseBuildingsSpinBox->SetIsEnabled(bStaircasesEnabled);
    }

    if (NumberOfStaircaseBuildingsLabel)
    {
        NumberOfStaircaseBuildingsLabel->SetColorAndOpacity(
            bStaircasesEnabled ? FSlateColor(FLinearColor::White) :
                               FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f, 1.0f)));
    }

    if (DebrisAmountSlider)
    {
        DebrisAmountSlider->SetIsEnabled(bDebrisEnabled);
    }
    
    if (DebrisAmountLabel)
    {
        DebrisAmountLabel->SetColorAndOpacity(bDebrisEnabled ? FSlateColor(FLinearColor::White) : FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f, 1.0f)));
    }
    
    if (DebrisAmountValue)
    {
        DebrisAmountValue->SetColorAndOpacity(bDebrisEnabled ? FSlateColor(FLinearColor::White) : FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f, 1.0f)));
    }
    
    if (SpawnImpactCloudCheckBox)
    {
        SpawnImpactCloudCheckBox->SetIsEnabled(bDebrisEnabled);
    }
    
    if (SpawnImpactCloudLabel)
    {
        SpawnImpactCloudLabel->SetColorAndOpacity(bDebrisEnabled ? FSlateColor(FLinearColor::White) : FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f, 1.0f)));
    }
}