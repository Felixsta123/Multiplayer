
// VoxelTerrainSettings.cpp
#include "VoxelTerrainSettings.h"
#include "Kismet/GameplayStatics.h"

// Initialisation du pointeur d'instance à nullptr
UVoxelTerrainSettingsManager* UVoxelTerrainSettingsManager::Instance = nullptr;

UVoxelTerrainSettingsSave::UVoxelTerrainSettingsSave()
{
    SaveSlotName = "VoxelTerrainSettings";
    UserIndex = 0;
    
    // Initialiser les paramètres par défaut
    TerrainSettings = FVoxelTerrainSettings();
}

UVoxelTerrainSettingsManager::UVoxelTerrainSettingsManager()
{
    // Initialiser avec des valeurs par défaut
    CurrentSettings = FVoxelTerrainSettings();
    
    // Définir le nom du slot de sauvegarde
    SaveSlotName = "VoxelTerrainSettings";
    UserIndex = 0;
}

UVoxelTerrainSettingsManager* UVoxelTerrainSettingsManager::GetInstance()
{
    // Créer l'instance si elle n'existe pas encore
    if (!Instance)
    {
        Instance = NewObject<UVoxelTerrainSettingsManager>();
        Instance->AddToRoot(); // Empêcher le garbage collector de détruire l'instance
        
        // Charger les paramètres enregistrés
        Instance->LoadSettings();
    }
    
    return Instance;
}

FVoxelTerrainSettings UVoxelTerrainSettingsManager::GetSettings()
{
    return CurrentSettings;
}

void UVoxelTerrainSettingsManager::SetSettings(const FVoxelTerrainSettings& NewSettings)
{
    // Copier les nouveaux paramètres
    CurrentSettings = NewSettings;
    
    // Valider les paramètres
    CurrentSettings.Validate();
}

bool UVoxelTerrainSettingsManager::SaveSettings()
{
    // Créer un objet de sauvegarde
    UVoxelTerrainSettingsSave* SaveGameInstance = Cast<UVoxelTerrainSettingsSave>(UGameplayStatics::CreateSaveGameObject(UVoxelTerrainSettingsSave::StaticClass()));
    
    if (SaveGameInstance)
    {
        // Définir les données à sauvegarder
        SaveGameInstance->TerrainSettings = CurrentSettings;
        SaveGameInstance->SaveSlotName = SaveSlotName;
        SaveGameInstance->UserIndex = UserIndex;
        
        // Sauvegarder les données
        return UGameplayStatics::SaveGameToSlot(SaveGameInstance, SaveSlotName, UserIndex);
    }
    
    return false;
}

bool UVoxelTerrainSettingsManager::LoadSettings()
{
    // Vérifier si une sauvegarde existe
    if (UGameplayStatics::DoesSaveGameExist(SaveSlotName, UserIndex))
    {
        // Charger la sauvegarde
        UVoxelTerrainSettingsSave* LoadedGame = Cast<UVoxelTerrainSettingsSave>(UGameplayStatics::LoadGameFromSlot(SaveSlotName, UserIndex));
        
        if (LoadedGame)
        {
            // Copier les paramètres chargés
            CurrentSettings = LoadedGame->TerrainSettings;
            
            // Valider les paramètres chargés
            CurrentSettings.Validate();
            
            return true;
        }
    }
    
    // Si pas de sauvegarde, utiliser les valeurs par défaut
    ResetToDefaults();
    return false;
}

void UVoxelTerrainSettingsManager::ResetToDefaults()
{
    // Réinitialiser aux valeurs par défaut
    CurrentSettings = FVoxelTerrainSettings();
}

