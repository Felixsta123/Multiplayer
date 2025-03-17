// TerrainLobbySettings.cpp
#include "TerrainLobbySettings.h"
#include "Components/SpinBox.h"
#include "Components/Slider.h"
#include "Components/CheckBox.h"
#include "Components/Button.h"

void UTerrainLobbySettings::NativeConstruct()
{
    Super::NativeConstruct();

    // Lier les callbacks des boutons
    if (SaveButton)
    {
        SaveButton->OnClicked.AddDynamic(this, &UTerrainLobbySettings::OnSaveButtonClicked);
    }

    if (LoadButton)
    {
        LoadButton->OnClicked.AddDynamic(this, &UTerrainLobbySettings::OnLoadButtonClicked);
    }

    if (DefaultsButton)
    {
        DefaultsButton->OnClicked.AddDynamic(this, &UTerrainLobbySettings::OnDefaultsButtonClicked);
    }

    // Set up staircase checkboxes and callbacks
    if (EnableStaircaseBuildingsCheckBox)
    {
        EnableStaircaseBuildingsCheckBox->OnCheckStateChanged.AddDynamic(
            this, &UTerrainLobbySettings::OnEnableStaircaseBuildingsChanged);
    }

    // Initialiser les widgets avec les paramètres actuels
    LoadSettings();
}

void UTerrainLobbySettings::SaveSettings()
{
    // Obtenir les paramètres à partir des widgets
    FVoxelTerrainSettings NewSettings = GetSettingsFromWidgets();

    // Mettre à jour le gestionnaire de paramètres
    UVoxelTerrainSettingsManager* Manager = UVoxelTerrainSettingsManager::GetInstance();
    if (Manager)
    {
        Manager->SetSettings(NewSettings);
        Manager->SaveSettings();

        // Mettre à jour notre copie locale
        CurrentSettings = Manager->GetSettings();
    }
}

void UTerrainLobbySettings::LoadSettings()
{
    // Charger les paramètres depuis le gestionnaire
    UVoxelTerrainSettingsManager* Manager = UVoxelTerrainSettingsManager::GetInstance();
    if (Manager)
    {
        CurrentSettings = Manager->GetSettings();

        // Mettre à jour les widgets
        UpdateWidgetsFromSettings(CurrentSettings);
    }
}

void UTerrainLobbySettings::ResetToDefaults()
{
    // Réinitialiser les paramètres aux valeurs par défaut
    UVoxelTerrainSettingsManager* Manager = UVoxelTerrainSettingsManager::GetInstance();
    if (Manager)
    {
        Manager->ResetToDefaults();
        CurrentSettings = Manager->GetSettings();

        // Mettre à jour les widgets
        UpdateWidgetsFromSettings(CurrentSettings);
    }
}

void UTerrainLobbySettings::OnSaveButtonClicked()
{
    SaveSettings();
}

void UTerrainLobbySettings::OnLoadButtonClicked()
{
    LoadSettings();
}

void UTerrainLobbySettings::OnDefaultsButtonClicked()
{
    ResetToDefaults();
}

void UTerrainLobbySettings::OnEnableStaircaseBuildingsChanged(bool bIsChecked)
{
    // Update dependent widget states
    if (NumberOfStaircaseBuildingsSpinBox)
    {
        NumberOfStaircaseBuildingsSpinBox->SetIsEnabled(bIsChecked);
    }
}

void UTerrainLobbySettings::UpdateWidgetsFromSettings(const FVoxelTerrainSettings& Settings)
{
    // Mettre à jour chaque widget avec la valeur correspondante
    if (NumberOfBuildingsSpinBox)
    {
        NumberOfBuildingsSpinBox->SetValue(Settings.NumberOfBuildings);
    }

    if (SpawnAreaSizeSlider)
    {
        SpawnAreaSizeSlider->SetValue((Settings.SpawnAreaSize - 500.0f) / 4500.0f); // Normaliser entre 0 et 1
    }

    // Update staircase settings
    if (EnableStaircaseBuildingsCheckBox)
    {
        EnableStaircaseBuildingsCheckBox->SetCheckedState(
            Settings.bEnableStaircaseBuildings ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
    }

    if (NumberOfStaircaseBuildingsSpinBox)
    {
        NumberOfStaircaseBuildingsSpinBox->SetValue(Settings.NumberOfStaircaseBuildings);
        NumberOfStaircaseBuildingsSpinBox->SetIsEnabled(Settings.bEnableStaircaseBuildings);
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
        VoxelSizeSlider->SetValue((Settings.VoxelSize - 50.0f) / 150.0f); // Normaliser entre 0 et 1
    }

    if (SmoothingFactorSlider)
    {
        SmoothingFactorSlider->SetValue(Settings.SmoothingFactor / 0.1f); // Normaliser entre 0 et 1
    }

    if (UseRandomColorsCheckBox)
    {
        UseRandomColorsCheckBox->SetCheckedState(Settings.bUseRandomColors ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
    }

    if (CubeMarginSlider)
    {
        CubeMarginSlider->SetValue(Settings.CubeMargin / 0.05f); // Normaliser entre 0 et 1
    }

    if (SpawnDebrisCheckBox)
    {
        SpawnDebrisCheckBox->SetCheckedState(Settings.bSpawnDebrisOnDestruction ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
    }

    if (DebrisAmountSlider)
    {
        DebrisAmountSlider->SetValue((Settings.DebrisAmountMultiplier - 0.1f) / 2.9f); // Normaliser entre 0 et 1
    }

    if (SpawnImpactCloudCheckBox)
    {
        SpawnImpactCloudCheckBox->SetCheckedState(Settings.bSpawnImpactCloud ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
    }
}

FVoxelTerrainSettings UTerrainLobbySettings::GetSettingsFromWidgets()
{
    FVoxelTerrainSettings Settings;

    // Lire les valeurs de chaque widget
    if (NumberOfBuildingsSpinBox)
    {
        Settings.NumberOfBuildings = FMath::RoundToInt(NumberOfBuildingsSpinBox->GetValue());
    }

    if (SpawnAreaSizeSlider)
    {
        Settings.SpawnAreaSize = 500.0f + (SpawnAreaSizeSlider->GetValue() * 4500.0f);
    }

    // Get staircase settings
    if (EnableStaircaseBuildingsCheckBox)
    {
        Settings.bEnableStaircaseBuildings = (EnableStaircaseBuildingsCheckBox->GetCheckedState() == ECheckBoxState::Checked);
    }

    if (NumberOfStaircaseBuildingsSpinBox)
    {
        Settings.NumberOfStaircaseBuildings = FMath::RoundToInt(NumberOfStaircaseBuildingsSpinBox->GetValue());
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
        MaxHeightVariationSpinBox->SetValue(Settings.MaxHeightVariation);
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

    // Valider les paramètres avant de les renvoyer
    Settings.Validate();

    return Settings;
}