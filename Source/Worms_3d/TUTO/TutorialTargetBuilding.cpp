#include "TutorialTargetBuilding.h"

ATutorialTargetBuilding::ATutorialTargetBuilding()
{
    // Default values
    DestroyedThreshold = 0.25f; // 25% destruction counts as "destroyed"
    DamageCount = 0;
    bIsTargetDestroyed = false;
    TotalVoxelCount = 0;
    DestroyedVoxelCount = 0;
}

void ATutorialTargetBuilding::BeginPlay()
{
    Super::BeginPlay();
    
    // Count total voxels after building is generated
    TotalVoxelCount = CountActiveVoxels();
    
    UE_LOG(LogTemp, Warning, TEXT("Tutorial target initialized with %d active voxels"), TotalVoxelCount);
}

void ATutorialTargetBuilding::DestroyVoxelsAt(FVector Location, FVector ImpactNormal, float Radius)
{
    // Get current active voxel count before destruction
    int32 PreDestructionCount = CountActiveVoxels();
    
    // Call parent implementation for actual destruction
    Super::DestroyVoxelsAt(Location, ImpactNormal, Radius);
    
    // Get count after destruction
    int32 PostDestructionCount = CountActiveVoxels();
    
    // Calculate voxels destroyed in this hit
    int32 NewlyDestroyedVoxels = PreDestructionCount - PostDestructionCount;
    
    if (NewlyDestroyedVoxels > 0)
    {
        // Increment damage count
        DamageCount++;
        
        // Update total destroyed voxels
        DestroyedVoxelCount += NewlyDestroyedVoxels;
        
        // Broadcast damage event
        OnTargetDamaged.Broadcast();
        
        // Calculate percentage destroyed
        float DestroyedPercentage = (float)DestroyedVoxelCount / (float)TotalVoxelCount;
        
        UE_LOG(LogTemp, Warning, TEXT("Tutorial target damaged: %d voxels destroyed (%.1f%% total)"), 
            NewlyDestroyedVoxels, DestroyedPercentage * 100.0f);
        
        // Check if we've exceeded the destroyed threshold
        if (!bIsTargetDestroyed && DestroyedPercentage >= DestroyedThreshold)
        {
            bIsTargetDestroyed = true;
            
            UE_LOG(LogTemp, Warning, TEXT("Tutorial target DESTROYED (%.1f%% threshold reached)"), 
                DestroyedThreshold * 100.0f);
            
            // Broadcast destruction event
            OnTargetDestroyed.Broadcast();
        }
    }
}

int32 ATutorialTargetBuilding::CountActiveVoxels() const
{
    int32 ActiveCount = 0;
    
    // Loop through 3D grid and count active voxels
    for (int32 X = 0; X < GridSizeX; X++)
    {
        for (int32 Y = 0; Y < GridSizeY; Y++)
        {
            for (int32 Z = 0; Z < GridSizeZ; Z++)
            {
                if (VoxelGrid[X][Y][Z].bIsActive)
                {
                    ActiveCount++;
                }
            }
        }
    }
    
    return ActiveCount;
}