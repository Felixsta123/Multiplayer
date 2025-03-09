#include "PlayerSpawnManager.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "GameFramework/PawnMovementComponent.h"

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
}

void UPlayerSpawnManager::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
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
    // Add a guard flag to prevent multiple calls
    static bool bTeleportInProgress = false;
    if (bTeleportInProgress)
    {
        UE_LOG(LogTemp, Warning, TEXT("TeleportPlayersToBuildings already in progress, skipping"));
        return;
    }
    bTeleportInProgress = true;
    
    UE_LOG(LogTemp, Warning, TEXT("Teleporting players directly to building tops..."));
    
    // Get all voxel buildings in the level
    TArray<AImprovedVoxelBuilding*> VoxelBuildings = AImprovedVoxelBuilding::FindAllVoxelBuildings(this);
    
    if (VoxelBuildings.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("No voxel buildings found for player teleportation"));
        bTeleportInProgress = false;
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
        bTeleportInProgress = false;
        return;
    }
    
    // Calculate spawn locations on buildings
    TArray<FVector> SpawnLocations;
    
    // For each building, find suitable spawn points
    for (AImprovedVoxelBuilding* Building : VoxelBuildings)
    {
        // Find spawn location on this building
        FVector SpawnLocation = FindSpawnLocationOnBuilding(Building, SpawnLocations);
        
        // Verify if the position is actually above the building using line trace
        FHitResult HitResult;
        FVector Start = SpawnLocation;
        FVector End = SpawnLocation - FVector(0, 0, Building->GridSizeZ * Building->VoxelSize * 2);
        
        FCollisionQueryParams QueryParams;
        QueryParams.AddIgnoredActor(GetOwner());
        
        bool bValidSpawnPoint = false;
        if (GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, QueryParams))
        {
            if (HitResult.GetActor() == Building)
            {
                // Position is confirmed to be above the building
                bValidSpawnPoint = true;
                UE_LOG(LogTemp, Warning, TEXT("Validated spawn point at %s, height above building: %.1f"),
                    *SpawnLocation.ToString(), (SpawnLocation - HitResult.Location).Size());
                
                // Adjust the spawn location to be exactly above the hit point with proper offset
                SpawnLocation = HitResult.Location + FVector(0, 0, HeightOffset + 100.0f); // Extra 100 units for safety
            }
        }
        
        if (bValidSpawnPoint)
        {
            // Add to our list of locations
            SpawnLocations.Add(SpawnLocation);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("Spawn point failed validation check - not above building"));
            
            // Fallback: use a more conservative calculation
            FVector BuildingOrigin = Building->GetActorLocation();
            float BuildingWidth = Building->GridSizeX * Building->VoxelSize;
            float BuildingDepth = Building->GridSizeY * Building->VoxelSize;
            float BuildingHeight = Building->GridSizeZ * Building->VoxelSize;
            
            FVector TopCenter = BuildingOrigin + FVector(BuildingWidth * 0.5f, BuildingDepth * 0.5f, BuildingHeight);
            FVector SafeSpawnLocation = TopCenter + FVector(0, 0, HeightOffset + 150.0f); // Much higher offset
            
            SpawnLocations.Add(SafeSpawnLocation);
            UE_LOG(LogTemp, Warning, TEXT("Using fallback spawn location at %s"), *SafeSpawnLocation.ToString());
        }
        
        // If we have enough locations for all players, we can stop
        if (SpawnLocations.Num() >= PlayerControllers.Num())
        {
            break;
        }
    }
    
    // Now teleport players to these positions with a slight delay between each
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
            // Adjust position up by half the capsule height plus extra safety margin (100 units)
            Location.Z += CapsuleComp->GetScaledCapsuleHalfHeight() + 100.0f;
            
            // IMPROVED: Don't disable collision - instead make sure we're well above the surface
            UE_LOG(LogTemp, Warning, TEXT("Final spawn location for player %d: %s (height adjusted for capsule)"),
                i, *Location.ToString());
        }
        
        // Teleport with a slight delay between each player
        FTimerHandle TeleportTimerHandle;
        // Using lambda to capture variables
        GetWorld()->GetTimerManager().SetTimer(
            TeleportTimerHandle,
            [PC, Pawn, Location, Rotation, i]() {
                if (PC && Pawn && IsValid(Pawn))
                {
                    // Record original velocity to reset after teleport
                    FVector OriginalVelocity = Pawn->GetVelocity();
                    
                    // Teleport the pawn
                    bool bSuccess = Pawn->TeleportTo(Location, Rotation);
                    
                    // Reset velocity to zero to prevent momentum issues
                    if (bSuccess && Pawn->GetMovementComponent())
                    {
                        Pawn->GetMovementComponent()->Velocity = FVector::ZeroVector;
                        UE_LOG(LogTemp, Warning, TEXT("Successfully teleported player %d to position"), i);
                    }
                    else
                    {
                        UE_LOG(LogTemp, Error, TEXT("Failed to teleport player %d"), i);
                    }
                }
            },
            0.2f * i, // Stagger teleports by 0.2 seconds each - increased from 0.1
            false
        );
    }
    
    // Reset the guard flag after all teleports are scheduled
    FTimerHandle ResetGuardFlagTimerHandle;
    GetWorld()->GetTimerManager().SetTimer(
        ResetGuardFlagTimerHandle,
        []() { bTeleportInProgress = false; },
        PlayerControllers.Num() * 0.2f + 0.5f, // Wait until after all teleports complete
        false
    );
}

FVector UPlayerSpawnManager::FindSpawnLocationOnBuilding(AImprovedVoxelBuilding* Building, TArray<FVector>& ExistingLocations)
{
    if (!Building)
    {
        return FVector::ZeroVector;
    }
    
    // Calculate building dimensions and bounds more accurately
    FVector BuildingOrigin = Building->GetActorLocation();
    float BuildingWidth = Building->GridSizeX * Building->VoxelSize;
    float BuildingDepth = Building->GridSizeY * Building->VoxelSize;
    float BuildingHeight = Building->GridSizeZ * Building->VoxelSize;
    
    // Calculate the top center of the building
    // Building origin is usually at the corner, so add half width and depth to center
    FVector TopCenter = BuildingOrigin + FVector(BuildingWidth * 0.5f, BuildingDepth * 0.5f, BuildingHeight);
    
    // Add height offset for the spawn point - INCREASED SIGNIFICANTLY for safety
    FVector SpawnLocation = TopCenter + FVector(0, 0, HeightOffset + 100.0f);
    
    // Try to find a valid position (not too close to existing spawns)
    if (!IsPositionValid(SpawnLocation, ExistingLocations))
    {
        // Try a few more positions if initial position isn't valid
        for (int32 Attempts = 0; Attempts < 15; Attempts++)
        {
            // Narrow the range to stay more centered on the building (40-60% range)
            float RandomX = FMath::RandRange(0.4f, 0.6f) * BuildingWidth;
            float RandomY = FMath::RandRange(0.4f, 0.6f) * BuildingDepth;
            
            FVector RandomPos = BuildingOrigin + FVector(RandomX, RandomY, BuildingHeight + HeightOffset + 100.0f);
            
            // Check if we're far enough from existing spawn points
            if (IsPositionValid(RandomPos, ExistingLocations))
            {
                UE_LOG(LogTemp, Warning, TEXT("Found valid spawn position after %d attempts: %s"), 
                    Attempts, *RandomPos.ToString());
                return RandomPos;
            }
        }
    }
    
    // If no perfect position is found after attempts, just use the original top center with extra height
    return TopCenter + FVector(0, 0, HeightOffset + 150.0f);
}