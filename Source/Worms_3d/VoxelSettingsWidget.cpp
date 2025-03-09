
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
    
    // Initialiser les drapeaux
    bUpdatingUI = false;
    UpdateTimer = 0.0f;
    
    // Configurer le titre
    if (TitleText)
    {
        TitleText->SetText(FText::FromString(TEXT("Paramètres de Terrain Voxel")));
    }
    
    // Connecter les callbacks des boutons
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
    
    // Connecter les callbacks des contrôles de nombre de bâtiments
    if (NumberOfBuildingsSpinBox)
    {
        NumberOfBuildingsSpinBox->SetMinValue(1);
        NumberOfBuildingsSpinBox->SetMaxValue(10);
        NumberOfBuildingsSpinBox->OnValueChanged.AddDynamic(this, &UVoxelSettingsWidget::OnNumberOfBuildingsChanged);
        
        if (NumberOfBuildingsLabel)
        {
            NumberOfBuildingsLabel->SetText(FText::FromString(TEXT("Nombre de bâtiments:")));
        }
    }
    
    // Connecter les callbacks des contrôles de taille de zone
    if (SpawnAreaSizeSlider)
    {
        SpawnAreaSizeSlider->SetMinValue(0.0f);
        SpawnAreaSizeSlider->SetMaxValue(1.0f);
        SpawnAreaSizeSlider->OnValueChanged.AddDynamic(this, &UVoxelSettingsWidget::OnSpawnAreaSizeChanged);
        
        if (SpawnAreaSizeLabel)
        {
            SpawnAreaSizeLabel->SetText(FText::FromString(TEXT("Taille de la zone de spawn:")));
        }
    }
    
    // Connecter les callbacks des contrôles de taille de grille
    if (GridSizeXSpinBox)
    {
        GridSizeXSpinBox->SetMinValue(5);
        GridSizeXSpinBox->SetMaxValue(20);
        GridSizeXSpinBox->OnValueChanged.AddDynamic(this, &UVoxelSettingsWidget::OnGridSizeXChanged);
        
        if (GridSizeXLabel)
        {
            GridSizeXLabel->SetText(FText::FromString(TEXT("Taille X:")));
        }
    }
    
    if (GridSizeYSpinBox)
    {
        GridSizeYSpinBox->SetMinValue(5);
        GridSizeYSpinBox->SetMaxValue(20);
        GridSizeYSpinBox->OnValueChanged.AddDynamic(this, &UVoxelSettingsWidget::OnGridSizeYChanged);
        
        if (GridSizeYLabel)
        {
            GridSizeYLabel->SetText(FText::FromString(TEXT("Taille Y:")));
        }
    }
    
    if (GridSizeZSpinBox)
    {
        GridSizeZSpinBox->SetMinValue(5);
        GridSizeZSpinBox->SetMaxValue(20);
        GridSizeZSpinBox->OnValueChanged.AddDynamic(this, &UVoxelSettingsWidget::OnGridSizeZChanged);
        
        if (GridSizeZLabel)
        {
            GridSizeZLabel->SetText(FText::FromString(TEXT("Taille Z:")));
        }
    }
    
    // Connecter les callbacks des contrôles de taille de voxel
    if (VoxelSizeSlider)
    {
        VoxelSizeSlider->SetMinValue(0.0f);
        VoxelSizeSlider->SetMaxValue(1.0f);
        VoxelSizeSlider->OnValueChanged.AddDynamic(this, &UVoxelSettingsWidget::OnVoxelSizeChanged);
        
        if (VoxelSizeLabel)
        {
            VoxelSizeLabel->SetText(FText::FromString(TEXT("Taille des voxels:")));
        }
    }
    
    // Connecter les callbacks des contrôles de lissage
    if (SmoothingFactorSlider)
    {
        SmoothingFactorSlider->SetMinValue(0.0f);
        SmoothingFactorSlider->SetMaxValue(1.0f);
        SmoothingFactorSlider->OnValueChanged.AddDynamic(this, &UVoxelSettingsWidget::OnSmoothingFactorChanged);
        
        if (SmoothingFactorLabel)
        {
            SmoothingFactorLabel->SetText(FText::FromString(TEXT("Facteur de lissage:")));
        }
    }
    
    // Connecter les callbacks des contrôles de couleurs aléatoires
    if (UseRandomColorsCheckBox)
    {
        UseRandomColorsCheckBox->OnCheckStateChanged.AddDynamic(this, &UVoxelSettingsWidget::OnUseRandomColorsChanged);
        
        if (UseRandomColorsLabel)
        {
            UseRandomColorsLabel->SetText(FText::FromString(TEXT("Utiliser des couleurs aléatoires")));
        }
    }
    
    // Connecter les callbacks des contrôles de marge entre cubes
    if (CubeMarginSlider)
    {
        CubeMarginSlider->SetMinValue(0.0f);
        CubeMarginSlider->SetMaxValue(1.0f);
        CubeMarginSlider->OnValueChanged.AddDynamic(this, &UVoxelSettingsWidget::OnCubeMarginChanged);
        
        if (CubeMarginLabel)
        {
            CubeMarginLabel->SetText(FText::FromString(TEXT("Marge entre cubes:")));
        }
    }
    
    // Connecter les callbacks des contrôles de débris
    if (SpawnDebrisCheckBox)
    {
        SpawnDebrisCheckBox->OnCheckStateChanged.AddDynamic(this, &UVoxelSettingsWidget::OnSpawnDebrisChanged);
        
        if (SpawnDebrisLabel)
        {
            SpawnDebrisLabel->SetText(FText::FromString(TEXT("Générer des débris")));
        }
    }
    
    // Connecter les callbacks des contrôles de multiplicateur de débris
    if (DebrisAmountSlider)
    {
        DebrisAmountSlider->SetMinValue(0.0f);
        DebrisAmountSlider->SetMaxValue(1.0f);
        DebrisAmountSlider->OnValueChanged.AddDynamic(this, &UVoxelSettingsWidget::OnDebrisAmountChanged);
        
        if (DebrisAmountLabel)
        {
            DebrisAmountLabel->SetText(FText::FromString(TEXT("Quantité de débris:")));
        }
    }
    
    // Connecter les callbacks des contrôles de nuage d'impact
    if (SpawnImpactCloudCheckBox)
    {
        SpawnImpactCloudCheckBox->OnCheckStateChanged.AddDynamic(this, &UVoxelSettingsWidget::OnSpawnImpactCloudChanged);
        
        if (SpawnImpactCloudLabel)
        {
            SpawnImpactCloudLabel->SetText(FText::FromString(TEXT("Générer un nuage d'impact")));
        }
    }
    
    // Configurer les boutons
    if (SaveButton)
    {
        SaveButton->SetToolTipText(FText::FromString(TEXT("Sauvegarder les paramètres")));
    }
    
    if (LoadButton)
    {
        LoadButton->SetToolTipText(FText::FromString(TEXT("Charger les paramètres sauvegardés")));
    }
    
    if (DefaultsButton)
    {
        DefaultsButton->SetToolTipText(FText::FromString(TEXT("Réinitialiser aux valeurs par défaut")));
    }
    
    // Charger les paramètres actuels
    LoadSettings();
}

void UVoxelSettingsWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    
    // Mettre à jour les labels des sliders à intervalle régulier
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
        
        // Mettre à jour l'interface utilisateur
        UpdateWidgetsFromSettings(CurrentSettings);
    }
    
    // Déclencher l'événement de changement de paramètres
    OnSettingsChanged();
}

void UVoxelSettingsWidget::LoadSettings()
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

void UVoxelSettingsWidget::ResetToDefaults()
{
    // Réinitialiser les paramètres aux valeurs par défaut
    UVoxelTerrainSettingsManager* Manager = UVoxelTerrainSettingsManager::GetInstance();
    if (Manager)
    {
        Manager->ResetToDefaults();
        CurrentSettings = Manager->GetSettings();
        
        // Mettre à jour les widgets
        UpdateWidgetsFromSettings(CurrentSettings);
        
        // Déclencher l'événement de changement de paramètres
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
    // Marquer comme en cours de mise à jour pour éviter les callbacks en boucle
    bUpdatingUI = true;
    
    // Mettre à jour chaque widget avec la valeur correspondante
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
    
    // Mettre à jour les labels et les dépendances
    UpdateSliderLabels();
    UpdateControlDependencies();
    
    // Fin de la mise à jour
    bUpdatingUI = false;
}

FVoxelTerrainSettings UVoxelSettingsWidget::GetSettingsFromWidgets()
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

void UVoxelSettingsWidget::UpdateSliderLabels()
{
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
    // Désactiver les contrôles de débris si la génération de débris est désactivée
    bool bDebrisEnabled = CurrentSettings.bSpawnDebrisOnDestruction;
    
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