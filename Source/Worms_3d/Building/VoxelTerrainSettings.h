// VoxelTerrainSettings.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "VoxelTerrainSettings.generated.h"

/**
 * Structure contenant tous les paramètres de génération de terrain voxel
 */
USTRUCT(BlueprintType)
struct FVoxelTerrainSettings
{
    GENERATED_BODY()
    
    // Nombre de bâtiments à générer
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Buildings", meta = (ClampMin = "1", ClampMax = "10"))
    int32 NumberOfBuildings = 3;
    
    // Taille de la zone de spawn
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Buildings", meta = (ClampMin = "500", ClampMax = "5000"))
    float SpawnAreaSize = 2000.0f;
    
    // Staircase buildings parameters (simplified)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|StaircaseBuildings")
    bool bEnableStaircaseBuildings = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|StaircaseBuildings", meta = (ClampMin = "0", ClampMax = "5", EditCondition = "bEnableStaircaseBuildings"))
    int32 NumberOfStaircaseBuildings = 2;

    // Dimensions des bâtiments voxel
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Voxel", meta = (ClampMin = "5", ClampMax = "20"))
    int32 GridSizeX = 10;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Voxel", meta = (ClampMin = "5", ClampMax = "20"))
    int32 GridSizeY = 10;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Voxel", meta = (ClampMin = "5", ClampMax = "20"))
    int32 GridSizeZ = 10;

    // Taille d'un voxel individuel
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Voxel", meta = (ClampMin = "50", ClampMax = "200"))
    float VoxelSize = 100.0f;

    // Facteur de lissage
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Voxel", meta = (ClampMin = "0", ClampMax = "0.1"))
    float SmoothingFactor = 0.01f;

    // Option pour utiliser des couleurs aléatoires
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Voxel")
    bool bUseRandomColors = true;

    // Marge entre cubes
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Voxel", meta = (ClampMin = "0", ClampMax = "0.05"))
    float CubeMargin = 0.02f;

    // Options de débris
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Debris")
    bool bSpawnDebrisOnDestruction = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Debris", meta = (ClampMin = "0.1", ClampMax = "3.0", EditCondition = "bSpawnDebrisOnDestruction"))
    float DebrisAmountMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Debris", meta = (EditCondition = "bSpawnDebrisOnDestruction"))
    bool bSpawnImpactCloud = true;

    // Constructeur pour initialiser avec des valeurs par défaut
    FVoxelTerrainSettings()
    {
        // Les valeurs par défaut sont déjà définies avec les déclarations de propriétés
    }

    // Fonction pour valider et corriger les paramètres si nécessaire
    void Validate()
    {
        // S'assurer que le nombre de bâtiments est dans une plage raisonnable
        NumberOfBuildings = FMath::Clamp(NumberOfBuildings, 1, 10);

        // Limiter la taille de la zone de spawn
        SpawnAreaSize = FMath::Clamp(SpawnAreaSize, 500.0f, 5000.0f);

        // Validate staircase parameters
        NumberOfStaircaseBuildings = FMath::Clamp(NumberOfStaircaseBuildings, 0, 5);

        // Limiter les dimensions de la grille
        GridSizeX = FMath::Clamp(GridSizeX, 5, 20);
        GridSizeY = FMath::Clamp(GridSizeY, 5, 20);
        GridSizeZ = FMath::Clamp(GridSizeZ, 5, 20);

        // Limiter la taille des voxels
        VoxelSize = FMath::Clamp(VoxelSize, 50.0f, 200.0f);

        // Limiter le facteur de lissage
        SmoothingFactor = FMath::Clamp(SmoothingFactor, 0.0f, 0.1f);

        // Limiter la marge entre cubes
        CubeMargin = FMath::Clamp(CubeMargin, 0.0f, 0.05f);

        // Limiter le multiplicateur de débris
        DebrisAmountMultiplier = FMath::Clamp(DebrisAmountMultiplier, 0.1f, 3.0f);
    }
};

/**
 * Objet de sauvegarde pour stocker les paramètres de terrain
 */
UCLASS()
class WORMS_3D_API UVoxelTerrainSettingsSave : public USaveGame
{
    GENERATED_BODY()

public:
    // Configuration du terrain
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
    FVoxelTerrainSettings TerrainSettings;

    // Nom du slot de sauvegarde
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SaveGame")
    FString SaveSlotName;

    // Index de l'utilisateur
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SaveGame")
    int32 UserIndex;

    UVoxelTerrainSettingsSave();
};

/**
 * Singleton pour accéder et gérer les paramètres de terrain depuis n'importe où
 */
UCLASS()
class WORMS_3D_API UVoxelTerrainSettingsManager : public UObject
{
    GENERATED_BODY()

private:
    // Instance unique du manager
    static UVoxelTerrainSettingsManager* Instance;

    // Paramètres actuels du terrain
    UPROPERTY()
    FVoxelTerrainSettings CurrentSettings;

    // Nom du slot de sauvegarde
    FString SaveSlotName;

    // Index de l'utilisateur
    int32 UserIndex;

public:
    // Obtenir l'instance du manager
    UFUNCTION(BlueprintCallable, Category = "VoxelTerrainSettings")
    static UVoxelTerrainSettingsManager* GetInstance();

    // Obtenir les paramètres actuels
    UFUNCTION(BlueprintCallable, Category = "VoxelTerrainSettings")
    FVoxelTerrainSettings GetSettings();

    // Définir de nouveaux paramètres
    UFUNCTION(BlueprintCallable, Category = "VoxelTerrainSettings")
    void SetSettings(const FVoxelTerrainSettings& NewSettings);

    // Sauvegarder les paramètres
    UFUNCTION(BlueprintCallable, Category = "VoxelTerrainSettings")
    bool SaveSettings();

    // Charger les paramètres
    UFUNCTION(BlueprintCallable, Category = "VoxelTerrainSettings")
    bool LoadSettings();

    // Réinitialiser les paramètres aux valeurs par défaut
    UFUNCTION(BlueprintCallable, Category = "VoxelTerrainSettings")
    void ResetToDefaults();

    // Constructeur
    UVoxelTerrainSettingsManager();
};