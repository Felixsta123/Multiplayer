// TerrainLobbySettings.h
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "VoxelTerrainSettings.h"
#include "Components/Slider.h"
#include "Components/SpinBox.h"
#include "Components/CheckBox.h"
#include "Components/Button.h"
#include "TerrainLobbySettings.generated.h"

/**
 * Widget de lobby pour configurer les paramètres de terrain
 */
UCLASS()
class WORMS_3D_API UTerrainLobbySettings : public UUserWidget
{
    GENERATED_BODY()
    
public:
    // Initialisation
    virtual void NativeConstruct() override;
    
    // Sauvegarder les paramètres
    UFUNCTION(BlueprintCallable, Category = "Terrain Settings")
    void SaveSettings();
    
    // Charger les paramètres
    UFUNCTION(BlueprintCallable, Category = "Terrain Settings")
    void LoadSettings();
    
    // Réinitialiser aux paramètres par défaut
    UFUNCTION(BlueprintCallable, Category = "Terrain Settings")
    void ResetToDefaults();
    
protected:
    // Widgets liés via le système de binding UMG
    
    // Nombre de bâtiments
    UPROPERTY(meta = (BindWidget))
    USpinBox* NumberOfBuildingsSpinBox;
    
    // Taille de la zone de spawn
    UPROPERTY(meta = (BindWidget))
    USlider* SpawnAreaSizeSlider;
    
    // Staircase buildings settings (simplified)
    UPROPERTY(meta = (BindWidget))
    UCheckBox* EnableStaircaseBuildingsCheckBox;

    UPROPERTY(meta = (BindWidget))
    USpinBox* NumberOfStaircaseBuildingsSpinBox;

    // Taille de la grille X
    UPROPERTY(meta = (BindWidget))
    USpinBox* GridSizeXSpinBox;

    // Taille de la grille Y
    UPROPERTY(meta = (BindWidget))
    USpinBox* GridSizeYSpinBox;

    // Taille de la grille Z
    UPROPERTY(meta = (BindWidget))
    USpinBox* GridSizeZSpinBox;

    // Taille de voxel
    UPROPERTY(meta = (BindWidget))
    USlider* VoxelSizeSlider;

    // Facteur de lissage
    UPROPERTY(meta = (BindWidget))
    USlider* SmoothingFactorSlider;

    // Utiliser des couleurs aléatoires
    UPROPERTY(meta = (BindWidget))
    UCheckBox* UseRandomColorsCheckBox;

    // Marge entre cubes
    UPROPERTY(meta = (BindWidget))
    USlider* CubeMarginSlider;

    // Débris à la destruction
    UPROPERTY(meta = (BindWidget))
    UCheckBox* SpawnDebrisCheckBox;

    // Multiplicateur de débris
    UPROPERTY(meta = (BindWidget))
    USlider* DebrisAmountSlider;

    // Nuage d'impact
    UPROPERTY(meta = (BindWidget))
    UCheckBox* SpawnImpactCloudCheckBox;

    // Boutons d'action
    UPROPERTY(meta = (BindWidget))
    UButton* SaveButton;

    UPROPERTY(meta = (BindWidget))
    UButton* LoadButton;

    UPROPERTY(meta = (BindWidget))
    UButton* DefaultsButton;

    // Callbacks de widget

    UFUNCTION()
    void OnSaveButtonClicked();

    UFUNCTION()
    void OnLoadButtonClicked();

    UFUNCTION()
    void OnDefaultsButtonClicked();

    // Callback for staircase checkbox
    UFUNCTION()
    void OnEnableStaircaseBuildingsChanged(bool bIsChecked);

    // Mise à jour des UI widgets à partir des paramètres
    void UpdateWidgetsFromSettings(const FVoxelTerrainSettings& Settings);

    // Lecture des valeurs des widgets pour créer des paramètres
    FVoxelTerrainSettings GetSettingsFromWidgets();

    // Référence aux paramètres actuels
    UPROPERTY()
    FVoxelTerrainSettings CurrentSettings;
};