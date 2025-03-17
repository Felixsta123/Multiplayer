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

        // Spawn each character for this team
        for (int32 CharIndex = 0; CharIndex < GameMode->CharactersPerTeam; CharIndex++)
        {
            AImprovedVoxelBuilding* RandomBuilding = Buildings[FMath::RandRange(0, Buildings.Num() - 1)];
            FVector SpawnLocation = RandomBuilding->GetTopSpawnPoint() + FVector(0, 0, 200.0f);

            // Calculate offset 

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

                    Character->ForceNetUpdate();
                    UE_LOG(LogTemp, Warning, TEXT("    - InGameName: %s"), *Character->InGameName);
                    //init the name widget
                    Character->InitializeNameWidget();
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