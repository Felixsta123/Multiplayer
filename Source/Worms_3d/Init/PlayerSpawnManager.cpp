#include "PlayerSpawnManager.h"

#include "Worms_3d/AWormCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "WormGameMode.h"
#include "WormGameState.h"
#include "WormPlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Worms_3d/Misc/PlayerDataStruct.h"

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
    UE_LOG(LogTemp, Warning, TEXT("===================================="));
    UE_LOG(LogTemp, Warning, TEXT("Starting player teleportation process"));
    UE_LOG(LogTemp, Warning, TEXT("===================================="));

    TArray<AImprovedVoxelBuilding*> Buildings = AImprovedVoxelBuilding::FindAllVoxelBuildings(this);
    if (Buildings.Num() == 0) return;

    AWormGameMode* GameMode = Cast<AWormGameMode>(GetWorld()->GetAuthGameMode());
    if (!GameMode || GameMode->NumTeams == 0) return;

    // Répartir les bâtiments entre les équipes
    int32 BuildingsPerTeam = FMath::Max(1, Buildings.Num() / GameMode->NumTeams);
    UE_LOG(LogTemp, Warning, TEXT("Found %d buildings, preparing to spawn %d teams with %d characters each"),
     Buildings.Num(), GameMode->NumTeams, GameMode->CharactersPerTeam);

    
    for (int32 TeamIndex = 0; TeamIndex < GameMode->NumTeams; TeamIndex++)
    {
        UE_LOG(LogTemp, Warning, TEXT("\nInitializing Team %d:"), TeamIndex);

        // Base building index for this team
        int32 BaseBuildingIndex = TeamIndex * BuildingsPerTeam;
        AImprovedVoxelBuilding* TeamBuilding = Buildings[BaseBuildingIndex % Buildings.Num()];
        
        // Get base spawn position for team
        FVector BaseSpawnPoint = TeamBuilding->GetTopSpawnPoint() + FVector(0, 0, 200.0f);
        UE_LOG(LogTemp, Warning, TEXT("Team %d base spawn point: %s"), 
               TeamIndex, *BaseSpawnPoint.ToString());
        // Spawn each character for this team
        for (int32 CharIndex = 0; CharIndex < GameMode->CharactersPerTeam; CharIndex++)
        {
            // Calculate offset for this character
            float AngleStep = 360.0f / GameMode->CharactersPerTeam;
            float CurrentAngle = AngleStep * CharIndex;
            FVector Offset = FVector(
                FMath::Cos(FMath::DegreesToRadians(CurrentAngle)) * GameMode->TeamSpawnOffset,
                FMath::Sin(FMath::DegreesToRadians(CurrentAngle)) * GameMode->TeamSpawnOffset,
                0
            );

            FVector SpawnLocation = BaseSpawnPoint + Offset;

            // Get the controller for this team
            AController* Controller = GameMode->AllPlayerControllers[TeamIndex];
            AWormPlayerController* WPC = Cast<AWormPlayerController>(Controller);

            if (WPC && WPC->PlayerSettings.MyPlayerCharacter)
            {
                FActorSpawnParameters SpawnParams;
                SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        
                AWormCharacter* Character = GetWorld()->SpawnActor<AWormCharacter>(
                    WPC->PlayerSettings.MyPlayerCharacter,
                    SpawnLocation,
                    FRotator::ZeroRotator,
                    SpawnParams
                );
                Character->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
                Character->GetCharacterMovement()->GravityScale = 1.5f;
                Character->GetCharacterMovement()->AddForce(FVector(0, 0, -980.0f));
                Character->GetCharacterMovement()->UpdateComponentVelocity();
                Character->ForceNetUpdate();
                UE_LOG(LogTemp, Warning, TEXT("  Spawning Character %d for Team %d:"), CharIndex, TeamIndex);
                UE_LOG(LogTemp, Warning, TEXT("    - Location: %s"), *SpawnLocation.ToString());
                UE_LOG(LogTemp, Warning, TEXT("    - Character Class: %s"), 
                    *WPC->PlayerSettings.MyPlayerCharacter->GetName());
                if (Character)
                {
                    Character->TeamId = TeamIndex;
                    Character->CharacterIndexInTeam = CharIndex;
                    WPC->Possess(Character);
                    Character->InGameName = GameMode->GetCharacterInGameName(
                           Character->GetClass(),
                           TeamIndex,
                           CharIndex
                       );
                
                
                    // Add to team in GameState
                    AWormGameState* WormGS = GetWorld()->GetGameState<AWormGameState>();
                    if (WormGS)
                    {
                        WormGS->AddCharacterToTeam(Character, TeamIndex);
                    }
                    UE_LOG(LogTemp, Warning, TEXT("    - Successfully spawned and added to team"));

                }
            }
        }
    }

    //check every WormsCharacter and destroy the one that are not in the team
    TArray<AActor*> AllCharacters;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AWormCharacter::StaticClass(), AllCharacters);
    for (AActor* Actor : AllCharacters)
    {
        AWormCharacter* Character = Cast<AWormCharacter>(Actor);
        if (Character && Character->TeamId == -1)
        {
            // Destroy the character
            UE_LOG(LogTemp, Warning, TEXT("Destroying character %s"), *Character->GetName());
            Character->Destroy();
        }
    }
    FTimerHandle PhysicsTimerHandle;
    GetWorld()->GetTimerManager().SetTimer(
        PhysicsTimerHandle,
        [this]() {
            TArray<AActor*> AllCharacters;
            UGameplayStatics::GetAllActorsOfClass(GetWorld(), AWormCharacter::StaticClass(), AllCharacters);
            
            for (AActor* Actor : AllCharacters)
            {
                AWormCharacter* Character = Cast<AWormCharacter>(Actor);
                if (Character && Character->GetCharacterMovement()) 
                {
                    // Activer la simulation physique
                    Character->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
                    Character->GetCharacterMovement()->GravityScale = 1.5f;
                    Character->GetCharacterMovement()->AddForce(FVector(0, 0, -980.0f));
                    Character->GetCharacterMovement()->UpdateComponentVelocity();
                    
                    UE_LOG(LogTemp, Warning, TEXT("Activated physics for %s"), *Character->GetName());
                }
            }
        },
        0.25f, // Small delay to ensure all characters are spawned
        false
    );
    UE_LOG(LogTemp, Warning, TEXT("===================================="));
    UE_LOG(LogTemp, Warning, TEXT("Completed player teleportation process"));
    UE_LOG(LogTemp, Warning, TEXT("===================================="));
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