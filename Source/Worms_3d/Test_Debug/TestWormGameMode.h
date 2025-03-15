#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "WormWeapon.h"
#include "AVoxelBuilding.h"
#include "TestWormGameMode.generated.h"

UCLASS()
class WORMS_3D_API ATestWormGameMode : public AGameMode
{
    GENERATED_BODY()

public:
    ATestWormGameMode();

    // Fonctions de base du GameMode
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    // Classe de bâtiment à spawner
    UPROPERTY(EditDefaultsOnly, Category = "Voxel Building")
    TSubclassOf<AImprovedVoxelBuilding> BuildingClass;
    // Nombre de bâtiments à générer
    UPROPERTY(EditDefaultsOnly, Category = "Voxel Building")
    int32 NumberOfBuildings;

    // Zone dans laquelle placer les bâtiments
    UPROPERTY(EditDefaultsOnly, Category = "Voxel Building")
    float SpawnAreaSize;

    // Fonction pour générer les bâtiments voxel
    UFUNCTION(BlueprintCallable, Category = "Voxel Building")
    void GenerateVoxelBuildings();

    // Réinitialise le tour (pour le mode solo)
    UFUNCTION(BlueprintCallable, Category = "Game")
    void ResetTurn();
    
    // Durée du tour en secondes
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Game")
    float TurnDuration;
    
    // Temps restant pour le tour actuel
    UPROPERTY(BlueprintReadOnly, Category = "Game")
    float RemainingTurnTime;
    
    // Classes d'armes disponibles
    UPROPERTY(EditDefaultsOnly, Category = "Weapons")
    TArray<TSubclassOf<AWormWeapon>> AvailableWeaponTypes;
    
    // Initialiser les armes pour le joueur
    UFUNCTION(BlueprintCallable, Category = "Weapons")
    void InitializeWeaponsForPlayer();

protected:
    // Timer handles
    FTimerHandle WeaponSpawnTimerHandle;
    FTimerHandle BuildingsSpawnTimerHandle;
};