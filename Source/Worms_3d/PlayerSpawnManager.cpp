#include "PlayerSpawnManager.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "WormPlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
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
    UE_LOG(LogTemp, Warning, TEXT("Téléportation des joueurs sur les bâtiments..."));
    
    // 1. Get the buildings
    TArray<AImprovedVoxelBuilding*> VoxelBuildings = AImprovedVoxelBuilding::FindAllVoxelBuildings(this);
    
    if (VoxelBuildings.Num() == 0) {
        UE_LOG(LogTemp, Error, TEXT("Aucun bâtiment voxel trouvé"));
        return;
    }
    
    // 2. Get spawn points directly from buildings
    TArray<FVector> SpawnLocations;
    
    for (int32 i = 0; i < VoxelBuildings.Num(); i++) {
        AImprovedVoxelBuilding* Building = VoxelBuildings[i];
        if (!Building) continue;
        
        // Use pre-calculated spawn point with extra height for safety
        FVector SpawnPoint = Building->GetTopSpawnPoint() + FVector(0, 0, 200.0f);
        SpawnLocations.Add(SpawnPoint);
        
        UE_LOG(LogTemp, Warning, TEXT("Point de spawn %d: %s"), i, *SpawnPoint.ToString());
        
        // Visualize spawn point
        DrawDebugSphere(GetWorld(), SpawnPoint, 25.0f, 8, FColor::Yellow, false, 10.0f);
    }
    
    // 3. Get controllers
    TArray<APlayerController*> PlayerControllers;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It) {
        APlayerController* PC = It->Get();
        if (PC) {
            PlayerControllers.Add(PC);
        }
    }
    
    // 4. Spawn players with delay between each
    for (int32 i = 0; i < PlayerControllers.Num(); i++) {
        APlayerController* PC = PlayerControllers[i];
        if (!PC) continue;
        
        AWormPlayerController* WPC = Cast<AWormPlayerController>(PC);
        if (!WPC) continue;
        
        // Choose a position (cycling if more players than positions)
        int32 PosIndex = i % SpawnLocations.Num();
        FVector SpawnPos = SpawnLocations[PosIndex];
        
        // Destroy old pawn to avoid conflicts
        if (PC->GetPawn()) {
            PC->GetPawn()->Destroy();
            PC->UnPossess();
        }
        
        // Progressive delay between each spawn - increased delays for network safety
        float Delay = 1.5f + (i * 0.8f);
        
        FTimerHandle SpawnTimer;
        FTimerDelegate SpawnDelegate;
        
        // Use weak pointers for safety
        TWeakObjectPtr<APlayerController> WeakPC(PC);
        TWeakObjectPtr<AWormPlayerController> WeakWPC(WPC);
        
        SpawnDelegate.BindLambda([this, WeakPC, WeakWPC, SpawnPos, i]() {
            // Safety check
            if (!WeakPC.IsValid() || !WeakWPC.IsValid()) {
                UE_LOG(LogTemp, Error, TEXT("Invalid controller in spawn delegate"));
                return;
            }
            
            // Check character class
            UClass* CharClass = WeakWPC->PlayerSettings.MyPlayerCharacter;
            if (!CharClass) {
                UE_LOG(LogTemp, Error, TEXT("Pas de classe de personnage pour le joueur %d"), i);
                return;
            }
            
            // Spawn character with non-colliding approach
            FActorSpawnParameters SpawnParams;
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            
            UE_LOG(LogTemp, Warning, TEXT("Spawn du personnage [%s] pour %s à %s"), 
                *CharClass->GetName(), *WeakPC->GetName(), *SpawnPos.ToString());
            
            APawn* NewPawn = GetWorld()->SpawnActor<APawn>(CharClass, SpawnPos, FRotator::ZeroRotator, SpawnParams);
            
            if (NewPawn) {
                // Possess the new pawn
                WeakPC->Possess(NewPawn);
                
                // Stabilize the character with delayed movement enabling
                UCharacterMovementComponent* MovementComp = 
                    Cast<UCharacterMovementComponent>(NewPawn->GetMovementComponent());
                
                if (MovementComp) {
                    // Disable movement temporarily and reset velocity
                    MovementComp->StopMovementImmediately();
                    MovementComp->Velocity = FVector::ZeroVector;
                    MovementComp->DisableMovement();
                    
                    // Schedule multiple movement re-enabling attempts
                    for (float EnableDelay : {0.5f, 1.0f, 2.0f}) {
                        FTimerHandle EnableMovementTimer;
                        TWeakObjectPtr<UCharacterMovementComponent> WeakMovement(MovementComp);
                        
                        GetWorld()->GetTimerManager().SetTimer(
                            EnableMovementTimer, 
                            [WeakMovement]() {
                                if (WeakMovement.IsValid()) {
                                    WeakMovement->SetMovementMode(MOVE_Walking);
                                }
                            }, 
                            EnableDelay, 
                            false
                        );
                    }
                }
            } else {
                UE_LOG(LogTemp, Error, TEXT("Failed to spawn character for player %d"), i);
            }
        });
        
        GetWorld()->GetTimerManager().SetTimer(SpawnTimer, SpawnDelegate, Delay, false);
        UE_LOG(LogTemp, Warning, TEXT("Programmation spawn joueur %d dans %.1f secondes"), i, Delay);
    }
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
    
    // Use MUCH larger height offset for safety - this is critical
    float SafetyHeightBuffer = 400.0f; // Significantly increased from 250
    FVector SpawnLocation = TopCenter + FVector(0, 0, HeightOffset + SafetyHeightBuffer);
    
    // Log the building dimensions and calculated spawn point for debugging
    UE_LOG(LogTemp, Warning, TEXT("Building at %s: Width=%.1f, Depth=%.1f, Height=%.1f"),
        *BuildingOrigin.ToString(), BuildingWidth, BuildingDepth, BuildingHeight);
    UE_LOG(LogTemp, Warning, TEXT("Calculated spawn location: %s (HeightOffset=%.1f, SafetyBuffer=%.1f)"),
        *SpawnLocation.ToString(), HeightOffset, SafetyHeightBuffer);
        
    // Try to find a valid position (not too close to existing spawns)
    if (!IsPositionValid(SpawnLocation, ExistingLocations))
    {
        // Try a few more positions if initial position isn't valid
        bool foundValid = false;
        for (int32 Attempts = 0; Attempts < 15; Attempts++)
        {
            // More conservative range to stay closer to building center (45-55% range)
            // This prevents spawning too close to edges where falling is more likely
            float RandomX = FMath::RandRange(0.45f, 0.55f) * BuildingWidth;
            float RandomY = FMath::RandRange(0.45f, 0.55f) * BuildingDepth;
            
            FVector RandomPos = BuildingOrigin + FVector(RandomX, RandomY, BuildingHeight + HeightOffset + SafetyHeightBuffer);
            
            // Check if we're far enough from existing spawn points
            if (IsPositionValid(RandomPos, ExistingLocations))
            {
                UE_LOG(LogTemp, Warning, TEXT("Found valid spawn position after %d attempts: %s"), 
                    Attempts, *RandomPos.ToString());
                foundValid = true;
                return RandomPos;
            }
        }
        
        if (!foundValid) {
            // If no valid position found, log warning but use center with extra height
            UE_LOG(LogTemp, Warning, TEXT("No valid spawn position found after multiple attempts, using building center with extra height"));
        }
    }
    
    // If no valid position found, log warning but use center with extra height
    return TopCenter + FVector(0, 0, HeightOffset + SafetyHeightBuffer + 100.0f);
}

float UPlayerSpawnManager::FindMaximumZValueInLevel()
{
    float MaxZ = 0.0f;
    
    // Iterate through all buildings to find highest point
    TArray<AImprovedVoxelBuilding*> Buildings = AImprovedVoxelBuilding::FindAllVoxelBuildings(GetWorld());
    for (AImprovedVoxelBuilding* Building : Buildings) {
        if (Building) {
            float BuildingHeight = Building->GetActorLocation().Z + (Building->GridSizeZ * Building->VoxelSize);
            MaxZ = FMath::Max(MaxZ, BuildingHeight);
        }
    }
    
    // Add safety margin
    return MaxZ + 500.0f;
}