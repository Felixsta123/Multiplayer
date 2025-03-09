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
    
    // Calculate building dimensions and bounds more accurately
    FVector BuildingOrigin = Building->GetActorLocation();
    float BuildingWidth = Building->GridSizeX * Building->VoxelSize;
    float BuildingDepth = Building->GridSizeY * Building->VoxelSize;
    float BuildingHeight = Building->GridSizeZ * Building->VoxelSize;
    
    // Calculate the top center of the building
    // Building origin is usually at the corner, so add half width and depth to center
    FVector TopCenter = BuildingOrigin + FVector(BuildingWidth * 0.5f, BuildingDepth * 0.5f, BuildingHeight);
    
    // Add height offset for the spawn point - increased for safety
    FVector SpawnLocation = TopCenter + FVector(0, 0, HeightOffset);
    
    // Try to find a valid position (not too close to existing spawns)
    if (!IsPositionValid(SpawnLocation, ExistingLocations))
    {
        // Try a few more positions if initial position isn't valid
        for (int32 Attempts = 0; Attempts < 15; Attempts++)
        {
            // Narrow the range to stay more centered on the building (30-70% range instead of 20-80%)
            float RandomX = FMath::RandRange(0.3f, 0.7f) * BuildingWidth;
            float RandomY = FMath::RandRange(0.3f, 0.7f) * BuildingDepth;
            
            FVector RandomPos = BuildingOrigin + FVector(RandomX, RandomY, BuildingHeight + HeightOffset);
            
            // Check if we're far enough from existing spawn points
            if (IsPositionValid(RandomPos, ExistingLocations))
            {
                return RandomPos;
            }
        }
    }
    
    // If no perfect position is found after attempts, just use the original top center with extra height
    return TopCenter + FVector(0, 0, HeightOffset + 50.0f);
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
            // Adjust position up by half the capsule height plus extra safety margin (50 units)
            Location.Z += CapsuleComp->GetScaledCapsuleHalfHeight() + 50.0f;
            
            // Also disable collision briefly during teleport to prevent falling through
            ECollisionEnabled::Type OriginalCollision = CapsuleComp->GetCollisionEnabled();
            CapsuleComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            
            // Schedule re-enabling collision after teleport
            FTimerHandle ReenableCollisionTimerHandle;
            // Lambda to capture the original collision state and component
            GetWorld()->GetTimerManager().SetTimer(
                ReenableCollisionTimerHandle,
                [CapsuleComp, OriginalCollision]() {
                    if (CapsuleComp && IsValid(CapsuleComp))
                    {
                        CapsuleComp->SetCollisionEnabled(OriginalCollision);
                    }
                },
                0.2f, // Short delay to ensure teleport completes
                false
            );
        }
        
        // Teleport with a slight delay between each player
        FTimerHandle TeleportTimerHandle;
        // Using lambda to capture variables
        GetWorld()->GetTimerManager().SetTimer(
            TeleportTimerHandle,
            [PC, Pawn, Location, Rotation]() {
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
                    }
                    
                    UE_LOG(LogTemp, Warning, TEXT("Teleported player to position: %s"), 
                        bSuccess ? TEXT("Success") : TEXT("Failed"));
                }
            },
            0.1f * i, // Stagger teleports by 0.1 seconds each
            false
        );
    }
    
    // Reset the guard flag after all teleports are scheduled
    FTimerHandle ResetGuardFlagTimerHandle;
    GetWorld()->GetTimerManager().SetTimer(
        ResetGuardFlagTimerHandle,
        []() { bTeleportInProgress = false; },
        PlayerControllers.Num() * 0.1f + 0.5f, // Wait until after all teleports complete
        false
    );
}