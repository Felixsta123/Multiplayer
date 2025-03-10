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
    
    // Récupération des bâtiments
    TArray<AImprovedVoxelBuilding*> VoxelBuildings = AImprovedVoxelBuilding::FindAllVoxelBuildings(this);
    
    if (VoxelBuildings.Num() == 0) {
        UE_LOG(LogTemp, Error, TEXT("Aucun bâtiment voxel trouvé pour téléporter les joueurs"));
        return;
    }
    
    // Ensure buildings are at sufficient distance from each other
    for (int i = 0; i < VoxelBuildings.Num(); i++) {
        for (int j = i+1; j < VoxelBuildings.Num(); j++) {
            float Distance = FVector::Dist(VoxelBuildings[i]->GetActorLocation(), VoxelBuildings[j]->GetActorLocation());
            if (Distance < 500.0f) {
                UE_LOG(LogTemp, Warning, TEXT("Buildings too close, adjusting position"));
                // Move second building away
                FVector Direction = (VoxelBuildings[j]->GetActorLocation() - VoxelBuildings[i]->GetActorLocation()).GetSafeNormal();
                VoxelBuildings[j]->SetActorLocation(VoxelBuildings[i]->GetActorLocation() + Direction * 1000.0f);
            }
        }
    }
    
    // Calculate maximum Z height in level for absolute safety
    float MaxZInLevel = 0.0f;
    for (AImprovedVoxelBuilding* Building : VoxelBuildings) {
        float BuildingMaxZ = Building->GetActorLocation().Z + (Building->GridSizeZ * Building->VoxelSize);
        MaxZInLevel = FMath::Max(MaxZInLevel, BuildingMaxZ);
    }
    
    // Extra safety height above maximum Z level
    float ExtraSafetyHeight = 500.0f;
    
    // Calculate highest possible spawn points
    TArray<FVector> SpawnLocations;
    int32 BuildingsNeeded = FMath::Min(4, VoxelBuildings.Num());
    
    // For each building, calculate a very high spawn point
    for (int32 i = 0; i < BuildingsNeeded; i++) {
        AImprovedVoxelBuilding* Building = VoxelBuildings[i];
        
        // Calculate building center
        FVector BuildingOrigin = Building->GetActorLocation();
        float BuildingWidth = Building->GridSizeX * Building->VoxelSize;
        float BuildingDepth = Building->GridSizeY * Building->VoxelSize;
        
        // Calculate EXACT center of building's top surface
        FVector TopCenter = BuildingOrigin + FVector(BuildingWidth * 0.5f, BuildingDepth * 0.5f, MaxZInLevel + ExtraSafetyHeight);
        
        SpawnLocations.Add(TopCenter);
        UE_LOG(LogTemp, Warning, TEXT("Added spawn location %d: %s (ABSOLUTE HEIGHT SAFETY)"), 
            SpawnLocations.Num(), *TopCenter.ToString());
    }
    
    if (SpawnLocations.Num() == 0) {
        UE_LOG(LogTemp, Error, TEXT("Impossible de trouver des positions valides sur les bâtiments"));
        return;
    }
    
    // Get all controllers with long staggered delays
    TArray<APlayerController*> PlayerControllers;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It) {
        PlayerControllers.Add(It->Get());
    }
    
    UE_LOG(LogTemp, Warning, TEXT("Spawning %d players onto %d spawn locations with EXTREME HEIGHT SAFETY"), 
        PlayerControllers.Num(), SpawnLocations.Num());
    
    // Spawn each player with a LONG delay between each
    for (int32 i = 0; i < PlayerControllers.Num(); i++) {
        APlayerController* PC = PlayerControllers[i];
        if (!PC) continue;
        
        AWormPlayerController* WPC = Cast<AWormPlayerController>(PC);
        if (!WPC) continue;
        
        // Calculate spawn position - extra vertical space between players
        int32 LocationIndex = i % SpawnLocations.Num();
        FVector SpawnLocation = SpawnLocations[LocationIndex];
        
        // Add extra height per player to avoid collisions
        SpawnLocation.Z += (i * 50.0f);
        
        FRotator SpawnRotation = FRotator::ZeroRotator;
        
        // Need new pawn?
        bool bNeedsNewPawn = !PC->GetPawn() || 
            (PC->GetPawn() && WPC->PlayerSettings.MyPlayerCharacter && 
             !PC->GetPawn()->IsA(WPC->PlayerSettings.MyPlayerCharacter));
        
        // Destroy existing pawn if needed
        if (PC->GetPawn() && bNeedsNewPawn) {
            PC->GetPawn()->Destroy();
        }
        
        // VERY long delay for each player (1 second+ between each)
        float DelayAmount = 1.0f + (i * 1.5f);
        
        // Set up spawn delegate
        FTimerHandle SpawnTimerHandle;
        FTimerDelegate SpawnDelegate;
        
        SpawnDelegate.BindLambda([this, PC, WPC, SpawnLocation, SpawnRotation, bNeedsNewPawn, i]() {
            // Check for valid character class
            UClass* CharacterClass = WPC->PlayerSettings.MyPlayerCharacter;
            if (!CharacterClass && bNeedsNewPawn) {
                UE_LOG(LogTemp, Error, TEXT("No valid character class for player %d"), i);
                return;
            }
            
            if (bNeedsNewPawn && CharacterClass) {
                // Spawn new character with collision handling set to always spawn
                FActorSpawnParameters SpawnParams;
                SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
                
                UE_LOG(LogTemp, Warning, TEXT("Spawn d'un nouveau personnage [%s] pour %s à %s (SAFETY HEIGHT)"), 
                    *CharacterClass->GetName(), 
                    *PC->GetName(), 
                    *SpawnLocation.ToString());
                
                APawn* NewPawn = GetWorld()->SpawnActor<APawn>(
                    CharacterClass,
                    SpawnLocation,
                    SpawnRotation,
                    SpawnParams
                );
                
                if (NewPawn) {
                    // Possess new pawn
                    PC->Possess(NewPawn);
                    
                    // Schedule multiple velocity resets with increasing delay
                    for (int resetAttempt = 0; resetAttempt < 5; resetAttempt++) {
                        // Create multiple timers to reset velocity
                        float ResetDelay = 0.2f + (resetAttempt * 0.3f);
                        
                        FTimerHandle VelocityResetTimerHandle;
                        GetWorld()->GetTimerManager().SetTimer(
                            VelocityResetTimerHandle,
                            [PC, NewPawn, resetAttempt]() {
                                if (PC && NewPawn && IsValid(NewPawn)) {
                                    UE_LOG(LogTemp, Warning, TEXT("Reset attempt %d: Réinitialisé la vélocité pour %s"), 
                                        resetAttempt, *NewPawn->GetName());
                                    
                                    if (NewPawn->GetMovementComponent()) {
                                        NewPawn->GetMovementComponent()->Velocity = FVector::ZeroVector;
                                        
                                        UCharacterMovementComponent* CharMoveComp = Cast<UCharacterMovementComponent>(NewPawn->GetMovementComponent());
                                        if (CharMoveComp) {
                                            // Force walking mode and higher gravity
                                            CharMoveComp->SetMovementMode(MOVE_Walking);
                                            CharMoveComp->GravityScale = 2.0f;
                                            CharMoveComp->AirControl = 1.0f;
                                            CharMoveComp->GroundFriction = 8.0f;
                                            CharMoveComp->AddForce(FVector(0, 0, -2000.0f));
                                        }
                                    }
                                }
                            },
                            ResetDelay,
                            false
                        );
                    }
                }
            }
            // If pawn exists, teleport it with similar safety measures
            else if (PC->GetPawn()) {
                APawn* Pawn = PC->GetPawn();
                UE_LOG(LogTemp, Warning, TEXT("Teleporting existing pawn %s to %s (SAFETY HEIGHT)"), 
                    *Pawn->GetName(), *SpawnLocation.ToString());
                
                bool bSuccess = Pawn->TeleportTo(SpawnLocation, SpawnRotation);
                
                // Multiple velocity reset attempts
                for (int resetAttempt = 0; resetAttempt < 5; resetAttempt++) {
                    float ResetDelay = 0.2f + (resetAttempt * 0.3f);
                    
                    FTimerHandle VelocityResetTimerHandle;
                    GetWorld()->GetTimerManager().SetTimer(
                        VelocityResetTimerHandle,
                        [Pawn, resetAttempt]() {
                            if (Pawn && IsValid(Pawn)) {
                                if (Pawn->GetMovementComponent()) {
                                    UE_LOG(LogTemp, Warning, TEXT("Reset attempt %d: Zero velocity for %s"), 
                                        resetAttempt, *Pawn->GetName());
                                        
                                    Pawn->GetMovementComponent()->Velocity = FVector::ZeroVector;
                                    
                                    UCharacterMovementComponent* CharMoveComp = Cast<UCharacterMovementComponent>(Pawn->GetMovementComponent());
                                    if (CharMoveComp) {
                                        CharMoveComp->SetMovementMode(MOVE_Walking);
                                        CharMoveComp->GravityScale = 2.0f; 
                                        CharMoveComp->AirControl = 1.0f;
                                        CharMoveComp->GroundFriction = 8.0f;
                                        CharMoveComp->AddForce(FVector(0, 0, -2000.0f));
                                    }
                                }
                            }
                        },
                        ResetDelay,
                        false
                    );
                }
            }
        });
        
        GetWorld()->GetTimerManager().SetTimer(
            SpawnTimerHandle,
            SpawnDelegate,
            DelayAmount,
            false
        );
        
        UE_LOG(LogTemp, Warning, TEXT("Scheduled player %d teleport/spawn with %.1f second delay (EXTENDED)"), i, DelayAmount);
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