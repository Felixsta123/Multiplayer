#include "PlayerSpawnManager.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "WormPlayerController.h"
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
    static bool bTeleportInProgress = false;
    if (bTeleportInProgress) return;
    bTeleportInProgress = true;
    
    UE_LOG(LogTemp, Warning, TEXT("Téléportation des joueurs sur les bâtiments..."));
    
    // Récupération des bâtiments
    TArray<AImprovedVoxelBuilding*> VoxelBuildings = AImprovedVoxelBuilding::FindAllVoxelBuildings(this);
    
    if (VoxelBuildings.Num() == 0) {
        UE_LOG(LogTemp, Error, TEXT("Aucun bâtiment voxel trouvé pour téléporter les joueurs"));
        bTeleportInProgress = false;
        return;
    }
    
    // Calcul des positions de spawn sur les bâtiments
    TArray<FVector> SpawnLocations;
    
    // Pour chaque bâtiment, trouver des points de spawn
    for (AImprovedVoxelBuilding* Building : VoxelBuildings) {
        FVector SpawnLocation = FindSpawnLocationOnBuilding(Building, SpawnLocations);
        
        // Vérifier si la position est valide
        if (!SpawnLocation.IsZero()) {
            SpawnLocations.Add(SpawnLocation);
        }
        
        // Si on a assez de positions, on arrête
        if (SpawnLocations.Num() >= 4) break; // Limite arbitraire de 4 joueurs
    }
    
    if (SpawnLocations.Num() == 0) {
        UE_LOG(LogTemp, Error, TEXT("Impossible de trouver des positions valides sur les bâtiments"));
        bTeleportInProgress = false;
        return;
    }
    
    // Maintenant, spawn ou téléporte les joueurs
    TArray<APlayerController*> PlayerControllers;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It) {
        PlayerControllers.Add(It->Get());
    }
    
    // Spawn et téléporte les joueurs
    for (int32 i = 0; i < PlayerControllers.Num(); i++) {
        APlayerController* PC = PlayerControllers[i];
        if (!PC) continue;
        
        AWormPlayerController* WPC = Cast<AWormPlayerController>(PC);
        if (!WPC) continue;
        
        // Calcul de la position de spawn
        int32 LocationIndex = i % SpawnLocations.Num();
        FVector SpawnLocation = SpawnLocations[LocationIndex];
        FRotator SpawnRotation = FRotator::ZeroRotator;
        
        // Si le controller n'a pas de pawn ou a un pawn incorrect
        bool bNeedsNewPawn = !PC->GetPawn();
        
        // Vérifier si le pawn existant est de la bonne classe
        if (PC->GetPawn() && WPC->PlayerSettings.MyPlayerCharacter) {
            if (!PC->GetPawn()->IsA(WPC->PlayerSettings.MyPlayerCharacter)) {
                bNeedsNewPawn = true;
                
                // Détruire le pawn existant s'il est de la mauvaise classe
                PC->GetPawn()->Destroy();
            }
        }
        
        // Créer un nouveau pawn si nécessaire
        if (bNeedsNewPawn && WPC->PlayerSettings.MyPlayerCharacter) {
            FActorSpawnParameters SpawnParams;
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
            
            UE_LOG(LogTemp, Warning, TEXT("Spawn d'un nouveau personnage [%s] pour %s à %s"), 
                *WPC->PlayerSettings.MyPlayerCharacter->GetName(), 
                *PC->GetName(), 
                *SpawnLocation.ToString());
            
            APawn* NewPawn = GetWorld()->SpawnActor<APawn>(
                WPC->PlayerSettings.MyPlayerCharacter,
                SpawnLocation,
                SpawnRotation,
                SpawnParams
            );
            
            if (NewPawn) {
                PC->Possess(NewPawn);
                
                // Léger délai pour s'assurer que le pawn est correctement initialisé
                FTimerHandle PossessTimerHandle;
                GetWorld()->GetTimerManager().SetTimer(
                    PossessTimerHandle,
                    [PC, NewPawn]() {
                        if (PC && NewPawn && IsValid(NewPawn)) {
                            UE_LOG(LogTemp, Warning, TEXT("Réinitialisé la vélocité pour %s"), *NewPawn->GetName());
                            if (NewPawn->GetMovementComponent()) {
                                NewPawn->GetMovementComponent()->Velocity = FVector::ZeroVector;
                            }
                        }
                    },
                    0.2f,
                    false
                );
            }
        }
        // Si le pawn existe déjà et est correct, on le téléporte
        else if (PC->GetPawn()) {
            APawn* Pawn = PC->GetPawn();
            
            // Téléportation avec décalage temporel entre chaque joueur
            FTimerHandle TeleportTimerHandle;
            GetWorld()->GetTimerManager().SetTimer(
                TeleportTimerHandle,
                [Pawn, SpawnLocation, SpawnRotation, i]() {
                    if (Pawn && IsValid(Pawn)) {
                        bool bSuccess = Pawn->TeleportTo(SpawnLocation, SpawnRotation);
                        
                        // Réinitialiser la vélocité
                        if (bSuccess && Pawn->GetMovementComponent()) {
                            Pawn->GetMovementComponent()->Velocity = FVector::ZeroVector;
                        }
                        
                        UE_LOG(LogTemp, Warning, TEXT("%s téléporté avec %s"), 
                            *Pawn->GetName(), bSuccess ? TEXT("succès") : TEXT("échec"));
                    }
                },
                0.2f * i, // Décalage progressif
                false
            );
        }
    }
    
    // Réinitialisation du flag après un délai suffisant
    FTimerHandle ResetFlagTimerHandle;
    GetWorld()->GetTimerManager().SetTimer(
        ResetFlagTimerHandle,
        []() { bTeleportInProgress = false; },
        PlayerControllers.Num() * 0.2f + 0.5f,
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