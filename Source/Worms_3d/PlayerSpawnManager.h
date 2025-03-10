#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/PlayerStart.h"
#include "AVoxelBuilding.h"

#include "PlayerSpawnManager.generated.h"

/**
 * Component to manage player spawn points and position them on top of voxel buildings
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class WORMS_3D_API UPlayerSpawnManager : public UActorComponent
{
    GENERATED_BODY()

public:
    UPlayerSpawnManager();

    // Called when the game starts
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    
    // Delays positioning until buildings are fully generated
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawning")
    float InitialDelay = 2.0f;

    // Distance above the building surface to place spawn points
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawning")
    float HeightOffset = 100.0f;

    // Minimum distance between spawn points
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawning")
    float MinDistanceBetweenSpawns = 300.0f;

    // Whether to reposition existing player starts or create new ones
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawning")
    bool bRepositionExistingPlayerStarts = true;


    // Function to directly teleport players to building tops without using PlayerStarts
    UFUNCTION(BlueprintCallable, Category = "Spawning")
    void TeleportPlayersToBuildings();

    // Teleport players to specific positions
    UFUNCTION(BlueprintCallable, Category = "Spawning")
    void TeleportPlayersToPositions(const TArray<FVector>& SpawnLocations);

private:
    // Timer handle for delayed initialization
    FTimerHandle InitializationTimerHandle;

    // Find suitable spawn location on a building
    FVector FindSpawnLocationOnBuilding(AImprovedVoxelBuilding* Building, TArray<FVector>& ExistingLocations);
    float FindMaximumZValueInLevel();

    // Helper function to check if a position is far enough from existing positions
    bool IsPositionValid(const FVector& Position, const TArray<FVector>& ExistingLocations);
};