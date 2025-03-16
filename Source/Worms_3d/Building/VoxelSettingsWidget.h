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
 * Widget pour configurer les paramètres de terrain voxel dans un lobby
 */
UCLASS()
class WORMS_3D_API UVoxelSettingsWidget : public UUserWidget
{
    GENERATED_BODY()
    
public:
    // Initialisation du widget
    virtual void NativeConstruct() override;
    
    // Mise à jour du widget à chaque frame
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    
    // Fonctions principales
    UFUNCTION(BlueprintCallable, Category = "Voxel Settings")
    void SaveSettings();
    
    UFUNCTION(BlueprintCallable, Category = "Voxel Settings")
    void LoadSettings();
    
    UFUNCTION(BlueprintCallable, Category = "Voxel Settings")
    void ResetToDefaults();
    
    // Événement déclenché lorsque les paramètres sont modifiés
    UFUNCTION(BlueprintImplementableEvent, Category = "Voxel Settings")
    void OnSettingsChanged();
    
protected:
    // === Section des contrôles de base ===
    
    // Container vertical principal
    UPROPERTY(meta = (BindWidget))
    UVerticalBox* MainContainer;
    
    // Titre du widget
    UPROPERTY(meta = (BindWidget))
    UTextBlock* TitleText;
    
    // === Section des paramètres de bâtiments ===
    
    // Nombre de bâtiments
    UPROPERTY(meta = (BindWidget))
    USpinBox* NumberOfBuildingsSpinBox;
    
    UPROPERTY(meta = (BindWidget))
    UTextBlock* NumberOfBuildingsLabel;
    
    // Taille de la zone de spawn
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

    // === Section des paramètres de voxels ===

    // Taille de la grille X
    UPROPERTY(meta = (BindWidget))
    USpinBox* GridSizeXSpinBox;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* GridSizeXLabel;

    // Taille de la grille Y
    UPROPERTY(meta = (BindWidget))
    USpinBox* GridSizeYSpinBox;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* GridSizeYLabel;

    // Taille de la grille Z
    UPROPERTY(meta = (BindWidget))
    USpinBox* GridSizeZSpinBox;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* GridSizeZLabel;

    // Taille de voxel
    UPROPERTY(meta = (BindWidget))
    USlider* VoxelSizeSlider;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* VoxelSizeLabel;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* VoxelSizeValue;

    // Facteur de lissage
    UPROPERTY(meta = (BindWidget))
    USlider* SmoothingFactorSlider;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* SmoothingFactorLabel;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* SmoothingFactorValue;

    // Utiliser des couleurs aléatoires
    UPROPERTY(meta = (BindWidget))
    UCheckBox* UseRandomColorsCheckBox;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* UseRandomColorsLabel;

    // Marge entre cubes
    UPROPERTY(meta = (BindWidget))
    USlider* CubeMarginSlider;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* CubeMarginLabel;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* CubeMarginValue;

    // === Section des paramètres de débris ===

    // Débris à la destruction
    UPROPERTY(meta = (BindWidget))
    UCheckBox* SpawnDebrisCheckBox;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* SpawnDebrisLabel;

    // Multiplicateur de débris
    UPROPERTY(meta = (BindWidget))
    USlider* DebrisAmountSlider;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* DebrisAmountLabel;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* DebrisAmountValue;

    // Nuage d'impact
    UPROPERTY(meta = (BindWidget))
    UCheckBox* SpawnImpactCloudCheckBox;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* SpawnImpactCloudLabel;

    // === Section des boutons d'action ===

    UPROPERTY(meta = (BindWidget))
    UHorizontalBox* ButtonsContainer;

    UPROPERTY(meta = (BindWidget))
    UButton* SaveButton;

    UPROPERTY(meta = (BindWidget))
    UButton* LoadButton;

    UPROPERTY(meta = (BindWidget))
    UButton* DefaultsButton;

    // === Callbacks de boutons ===

    UFUNCTION()
    void OnSaveButtonClicked();

    UFUNCTION()
    void OnLoadButtonClicked();

    UFUNCTION()
    void OnDefaultsButtonClicked();

    // === Callbacks de changement de valeur ===

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

    // === Fonctions utilitaires ===

    // Mise à jour des widgets à partir des paramètres
    void UpdateWidgetsFromSettings(const FVoxelTerrainSettings& Settings);

    // Obtention des paramètres à partir des widgets
    FVoxelTerrainSettings GetSettingsFromWidgets();

    // Mise à jour des valeurs textuelles des sliders
    void UpdateSliderLabels();

    // Activation/désactivation des contrôles selon les dépendances
    void UpdateControlDependencies();

    // Paramètres courants
    UPROPERTY()
    FVoxelTerrainSettings CurrentSettings;

    // Drapeaux de mise à jour
    bool bUpdatingUI;
    float UpdateTimer;
    const float UpdateInterval = 0.5f;
};