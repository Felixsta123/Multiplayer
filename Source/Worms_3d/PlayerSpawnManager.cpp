#include "PlayerSpawnManager.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"

UPlayerSpawnManager::UPlayerSpawnManager()
{
    PrimaryComponentTick.bCanEverTick = false;

    // Set default values
    InitialDelay = 2.0f;
    HeightOffset = 100.0f;
    MinDistanceBetweenSpawns = 300.0f;
    bRepositionExistingPlayerStarts = true;
}

void UPlayerSpawnManager::BeginPlay()
{
    Super::BeginPlay();
    
    // Schedule the positioning of player starts after a delay
    // This ensures voxel buildings have time to generate
    GetWorld()->GetTimerManager().SetTimer(
        InitializationTimerHandle,
        this,
        &UPlayerSpawnManager::TeleportPlayersToBuildings,
        InitialDelay,
        false  // Don't loop
    );
}

void UPlayerSpawnManager::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

FVector UPlayerSpawnManager::FindSpawnLocationOnBuilding(AImprovedVoxelBuilding* Building, TArray<FVector>& ExistingLocations)
{
    if (!Building)
    {
        return FVector::ZeroVector;
    }
    
    // Calculate building dimensions and center point
    FVector BuildingOrigin = Building->GetActorLocation();
    float BuildingWidth = Building->GridSizeX * Building->VoxelSize;
    float BuildingDepth = Building->GridSizeY * Building->VoxelSize;
    float BuildingHeight = Building->GridSizeZ * Building->VoxelSize;
    
    // Find the top center of the building
    FVector TopCenter = BuildingOrigin + FVector(BuildingWidth * 0.5f, BuildingDepth * 0.5f, BuildingHeight);
    
    // Add some height offset for the spawn point
    FVector SpawnLocation = TopCenter + FVector(0, 0, HeightOffset);
    
    // Try to find a valid position (not too close to existing spawns)
    if (!IsPositionValid(SpawnLocation, ExistingLocations))
    {
        // Try a few random positions on top of the building
        for (int32 Attempts = 0; Attempts < 10; Attempts++)
        {
            // Calculate a random position on top of the building
            float RandomX = FMath::RandRange(0.2f, 0.8f) * BuildingWidth;
            float RandomY = FMath::RandRange(0.2f, 0.8f) * BuildingDepth;
            
            FVector RandomPos = BuildingOrigin + FVector(RandomX, RandomY, BuildingHeight + HeightOffset);
            
            if (IsPositionValid(RandomPos, ExistingLocations))
            {
                return RandomPos;
            }
        }
    }
    
    // If all attempts failed, just return the original position
    return SpawnLocation;
}

bool UPlayerSpawnManager::IsPositionValid(const FVector& Position, const TArray<FVector>& ExistingLocations)
{
    // Check if position is far enough from all existing locations
    for (const FVector& ExistingLocation : ExistingLocations)
    {
        float DistanceSquared = FVector::DistSquared(Position, ExistingLocation);
        if (DistanceSquared < (MinDistanceBetweenSpawns * MinDistanceBetweenSpawns))
        {
            return false;
        }
    }
    
    return true;
}
// PlayerSpawnManager.cpp - Add the missing TeleportPlayersToPositions function

void UPlayerSpawnManager::TeleportPlayersToPositions(const TArray<FVector>& SpawnLocations)
{
    UE_LOG(LogTemp, Warning, TEXT("Teleporting players to %d calculated positions"), SpawnLocations.Num());
    
    // Get all player controllers
    TArray<APlayerController*> PlayerControllers;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        if (PC)
        {
            PlayerControllers.Add(PC);
        }
    }
    
    // No players to teleport or no positions
    if (PlayerControllers.Num() == 0 || SpawnLocations.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("No players to teleport or no positions available"));
        return;
    }
    
    // Teleport each player controller's pawn to a position
    for (int32 i = 0; i < PlayerControllers.Num(); i++)
    {
        APlayerController* PC = PlayerControllers[i];
        if (!PC)
        {
            continue;
        }
        
        // Skip if player has no pawn
        APawn* Pawn = PC->GetPawn();
        if (!Pawn)
        {
            UE_LOG(LogTemp, Warning, TEXT("Player controller %d has no pawn to teleport"), i);
            continue;
        }
        
        // Choose a spawn location (cycle through available ones)
        int32 LocationIndex = i % SpawnLocations.Num();
        FVector Location = SpawnLocations[LocationIndex];
        FRotator Rotation = FRotator::ZeroRotator;
        
        // Adjust height based on pawn's collision to ensure they're not inside the ground
        UCapsuleComponent* CapsuleComp = Cast<UCapsuleComponent>(Pawn->GetComponentByClass(UCapsuleComponent::StaticClass()));
        if (CapsuleComp)
        {
            // Adjust position up by half the capsule height
            Location.Z += CapsuleComp->GetScaledCapsuleHalfHeight();
        }
        
        // Teleport the pawn
        bool bSuccess = Pawn->TeleportTo(Location, Rotation);
        
        UE_LOG(LogTemp, Warning, TEXT("Teleported player %d to position %d: %s"), 
            i, LocationIndex, bSuccess ? TEXT("Success") : TEXT("Failed"));
    }
}

void UPlayerSpawnManager::TeleportPlayersToBuildings()
{
    UE_LOG(LogTemp, Warning, TEXT("Teleporting players directly to building tops..."));
    
    // Get all voxel buildings in the level
    TArray<AImprovedVoxelBuilding*> VoxelBuildings = AImprovedVoxelBuilding::FindAllVoxelBuildings(this);
    
    if (VoxelBuildings.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("No voxel buildings found for player teleportation"));
        return;
    }
    
    UE_LOG(LogTemp, Warning, TEXT("Found %d voxel buildings for player teleportation"), VoxelBuildings.Num());
    
    // Get all player controllers
    TArray<APlayerController*> PlayerControllers;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        if (PC)
        {
            PlayerControllers.Add(PC);
        }
    }
    
    // No players to teleport
    if (PlayerControllers.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("No players to teleport"));
        return;
    }
    
    // Calculate spawn locations on buildings
    TArray<FVector> SpawnLocations;
    
    // For each building, find suitable spawn points
    for (AImprovedVoxelBuilding* Building : VoxelBuildings)
    {
        // Find spawn location on this building
        FVector SpawnLocation = FindSpawnLocationOnBuilding(Building, SpawnLocations);
        
        // Add to our list of locations
        SpawnLocations.Add(SpawnLocation);
        
        // If we have enough locations for all players, we can stop
        if (SpawnLocations.Num() >= PlayerControllers.Num())
        {
            break;
        }
        
        // Try to find multiple spawn points on larger buildings
        if (Building->GridSizeX > 15 || Building->GridSizeY > 15)
        {
            for (int32 i = 0; i < FMath::Min(3, PlayerControllers.Num() - SpawnLocations.Num()); i++)
            {
                FVector AdditionalLocation = FindSpawnLocationOnBuilding(Building, SpawnLocations);
                SpawnLocations.Add(AdditionalLocation);
            }
        }
    }
    
    // If we don't have enough buildings, reuse existing spawn locations
    while (SpawnLocations.Num() < PlayerControllers.Num())
    {
        int32 BuildingIndex = FMath::RandRange(0, VoxelBuildings.Num() - 1);
        AImprovedVoxelBuilding* Building = VoxelBuildings[BuildingIndex];
        
        FVector AdditionalLocation = FindSpawnLocationOnBuilding(Building, SpawnLocations);
        SpawnLocations.Add(AdditionalLocation);
    }
    
    // Now teleport players to these positions
    TeleportPlayersToPositions(SpawnLocations);
}
