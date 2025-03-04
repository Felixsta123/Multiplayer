#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ChunkBasedDestructibleTerrain.h"
#include "DestructibleTerrainIntegration.generated.h"

/**
 * Component for integrating the new chunk-based terrain system with existing game systems.
 * Attach this to the TestWormGameMode to enable the new terrain system.
 */
UCLASS(Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class WORMS_3D_API UDestructibleTerrainIntegration : public UActorComponent
{
    GENERATED_BODY()

public:
    // Sets default values for this component's properties
    UDestructibleTerrainIntegration();

    // Use the new terrain system instead of the old one
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TerrainIntegration")
    bool bUseChunkBasedTerrain = true;

    // Reference to the new terrain actor (will be spawned if not set)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TerrainIntegration")
    AChunkBasedDestructibleTerrain* ChunkBasedTerrain;

    // Class of new terrain to spawn if needed
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TerrainIntegration")
    TSubclassOf<AChunkBasedDestructibleTerrain> ChunkBasedTerrainClass;

    // Terrain Settings
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TerrainConfig")
    FVector TerrainSize = FVector(5000.0f, 5000.0f, 2000.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TerrainConfig")
    float VoxelSize = 50.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TerrainConfig")
    FIntVector ChunkSize = FIntVector(16, 16, 16);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TerrainConfig")
    ETerrainType TerrainType = ETerrainType::Flat;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TerrainConfig")
    UMaterialInterface* TerrainMaterial;

    // Spawn the terrain actor if it doesn't exist
    UFUNCTION(BlueprintCallable, Category = "TerrainIntegration")
    AChunkBasedDestructibleTerrain* SpawnTerrain();

    // Apply an explosion at a position, with a radius
    // This interface matches the existing DestructibleTerrain interface
    UFUNCTION(BlueprintCallable, Category = "TerrainIntegration")
    void RequestDestroyTerrainAt(const FVector2D& Position, const FVector2D& Size);

    // Apply a spherical explosion at a position
    UFUNCTION(BlueprintCallable, Category = "TerrainIntegration")
    void ApplyExplosionAt(const FVector& Position, float Radius, float Falloff = 0.3f);

    // Gets the active terrain system
    UFUNCTION(BlueprintPure, Category = "TerrainIntegration")
    AActor* GetActiveTerrain() const;

    // Toggle between new and old terrain systems
    UFUNCTION(BlueprintCallable, Category = "TerrainIntegration")
    void ToggleTerrainSystem();

    // Get the name of the active terrain system
    UFUNCTION(BlueprintPure, Category = "TerrainIntegration")
    FString GetActiveTerrainSystemName() const;

protected:
    // Called when the game starts
    virtual void BeginPlay() override;

    // Called when the component is destroyed
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    // Reference to the old terrain system (if any)
    class ADestructibleTerrain* OldTerrain;
};