// WormGameMode.cpp
#include "WormGameMode.h"

#include "AIController.h"
#include "EngineUtils.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "WormGameState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Worms_3d/AVoxelBuilding.h"
#include "Worms_3d/GameInitFactorySubsystem.h"
#include "Worms_3d/VoxelTerrainSettings.h"

AWormGameMode::AWormGameMode()
{
    PrimaryActorTick.bCanEverTick = true;
    
    // Default values
    TurnDuration = 30.0f;
    CurrentPlayerIndex = 0;
    NewVar = 0;
    local = false;
    
    // Define GameState class explicitly
    GameStateClass = AWormGameState::StaticClass();
    
    // Default values for voxel buildings
    NumberOfBuildings = 3;
    SpawnAreaSize = 2000.0f;
    
    UE_LOG(LogTemp, Log, TEXT("WormGameMode constructor - Setting GameStateClass to: %s"), 
        *GameStateClass->GetName());
}

void AWormGameMode::BeginPlay()
{
    Super::BeginPlay();
    
    // If using game init manager, set it up first and let it handle initialization
    if (bUseGameInitManager)
    {
        // Setup the game initialization manager
        GameInitManager = SetupGameInitialization();
        
        // Let the game init manager handle the initialization sequence
        // (it will call our functions in the proper order)
    }
    else
    {
        // Use original initialization logic
        // Collect all controllers
        GatherAllPlayerControllers();
        
        // Collect spawn points
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), AActor::StaticClass(), SpawnPoints);
        
        // Initialize voxel buildings first (changed order to prioritize voxel buildings)
        GetWorldTimerManager().SetTimer(VoxelBuildingsSpawnTimerHandle, this, &AWormGameMode::GenerateVoxelBuildings, 1.0f, false);

        // Initialize weapons for all players
        GetWorldTimerManager().SetTimer(WeaponSpawnTimerHandle, this, &AWormGameMode::InitializeWeaponsForAllPlayers, 1.5f, false);
     
        // Start first turn after a delay
        GetWorldTimerManager().SetTimer(TurnTimerHandle, this, &AWormGameMode::StartNextTurn, 2.5f, false);
    }
}

void AWormGameMode::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    
    // Update remaining time
    if (GetWorldTimerManager().IsTimerActive(TurnTimerHandle))
    {
        RemainingTurnTime = GetWorldTimerManager().GetTimerRemaining(TurnTimerHandle);
        
        // Update GameState for replication
        AWormGameState* WormGS = GetGameState<AWormGameState>();
        if (WormGS)
        {
            WormGS->RemainingTurnTime = RemainingTurnTime;
        }
    }
}

void AWormGameMode::GatherAllPlayerControllers()
{
    // Empty existing array
    AllPlayerControllers.Empty();
    
    // Find all player controllers
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PlayerController = It->Get();
        if (PlayerController)
        {
            AllPlayerControllers.Add(PlayerController);
        }
    }
    
    // Find all AI controllers if needed
    TArray<AActor*> AIControllers;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AAIController::StaticClass(), AIControllers);
    for (AActor* Actor : AIControllers)
    {
        AController* AIController = Cast<AController>(Actor);
        if (AIController)
        {
            AllPlayerControllers.Add(AIController);
        }
    }
    
    AWormGameState* WormGS = GetGameState<AWormGameState>();
    if (WormGS)
    {
        WormGS->UpdatePlayerList(AllPlayerControllers);
    }
}

AWormCharacter* AWormGameMode::GetWormCharacterFromController(AController* Controller)
{
    if (!Controller)
    {
        return nullptr;
    }
    
    APawn* ControlledPawn = Controller->GetPawn();
    return Cast<AWormCharacter>(ControlledPawn);
}

void AWormGameMode::StartNextTurn()
{
    // Check if there are active controllers
    if (AllPlayerControllers.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("No active controllers found!"));
        return;
    }

    // First, recollect controllers to ensure our list is up to date
    GatherAllPlayerControllers();

    // Check if the game is over
    if (CheckGameEndCondition())
    {
        // Handle game end
        return;
    }

    // Update GameState - IMPORTANT: do this BEFORE changing active player
    AWormGameState* WormGS = GetGameState<AWormGameState>();
    if (WormGS)
    {
        // First, update player list with freshly collected list
        WormGS->UpdatePlayerList(AllPlayerControllers);
    }
    
    // Find next valid player
    int32 OriginalIndex = CurrentPlayerIndex;
    bool FoundValidPlayer = false;

    do {
        // Move to next player
        CurrentPlayerIndex = (CurrentPlayerIndex + 1) % AllPlayerControllers.Num();

        // Check if this player is valid (alive)
        AController* Controller = AllPlayerControllers[CurrentPlayerIndex];
        AWormCharacter* Character = GetWormCharacterFromController(Controller);

        if (Character && Character->GetHealth() > 0)
        {
            FoundValidPlayer = true;
            break;
        }

        // If we've done a full loop and haven't found a valid player
        if (CurrentPlayerIndex == OriginalIndex)
        {
            UE_LOG(LogTemp, Warning, TEXT("No valid players found for next turn!"));
            return;
        }

    } while (!FoundValidPlayer);

    // Get active controller
    AController* ActiveController = AllPlayerControllers[CurrentPlayerIndex];
    UE_LOG(LogTemp, Log, TEXT("Starting turn for player index %d: %s"),
        CurrentPlayerIndex, *ActiveController->GetName());

    // Make sure to use pawn name and not controller
    FString PlayerName;
    AWormCharacter* ActiveCharacter = GetWormCharacterFromController(ActiveController);
    if (ActiveCharacter)
    {
        PlayerName = ActiveCharacter->GetName();
    }
    else
    {
        PlayerName = ActiveController->GetName();
    }

    // IMPORTANT: Use new function to define active player with correct name
    if (WormGS)
    {
        WormGS->SetCurrentPlayerByIndex(CurrentPlayerIndex);
        WormGS->TurnDuration = TurnDuration;

        // Additional logging
        UE_LOG(LogTemp, Log, TEXT("Turn duration set to %.1f seconds, player name: %s"),
            TurnDuration, *WormGS->CurrentPlayerName);
    }

    // Deactivate all characters
    for (AController* Controller : AllPlayerControllers)
    {
        AWormCharacter* Character = GetWormCharacterFromController(Controller);
        if (Character)
        {
            Character->SetIsMyTurn(false);
        }
        if (Character && Character->CurrentWeapon)
        {
            // Force weapon synchronization for each player
             Character->AttachWeaponToSocket(Character->CurrentWeapon);
        }
    }

    // Activate character of active controller
    ActiveCharacter = GetWormCharacterFromController(ActiveController);
    if (ActiveCharacter)
    {
        ActiveCharacter->SetIsMyTurn(true);

        // Log to verify character is activated
        UE_LOG(LogTemp, Log, TEXT("Activated character: %s (Is it local: %s)"),
            *ActiveCharacter->GetName(),
            ActiveController->IsLocalController() ? TEXT("Yes") : TEXT("No"));
    }

    // Call turn start event
    OnTurnStarted(ActiveController);

    // Start timer for this turn
    GetWorldTimerManager().SetTimer(TurnTimerHandle, this, &AWormGameMode::OnTurnTimeExpired, TurnDuration, false);
}

void AWormGameMode::EndCurrentTurn()
{
    // Cancel current timer
    GetWorldTimerManager().ClearTimer(TurnTimerHandle);
    
    // Get active controller
    AController* ActiveController = nullptr;
    if (AllPlayerControllers.IsValidIndex(CurrentPlayerIndex))
    {
        ActiveController = AllPlayerControllers[CurrentPlayerIndex];
        
        // Deactivate active character
        AWormCharacter* ActiveCharacter = GetWormCharacterFromController(ActiveController);
        if (ActiveCharacter)
        {
            ActiveCharacter->SetIsMyTurn(false);
        }
    }
    
    // Call turn end event
    OnTurnEnded(ActiveController);
    
    // Start next turn after a delay
    GetWorldTimerManager().SetTimer(TurnTimerHandle, this, &AWormGameMode::StartNextTurn, 2.0f, false);
}

void AWormGameMode::OnTurnTimeExpired()
{
    // Time is up, end turn
    EndCurrentTurn();
}

bool AWormGameMode::CheckGameEndCondition()
{
    // Count active teams
    TSet<int32> ActiveTeams;
    int32 AlivePlayerCount = 0;
    
    for (AController* Controller : AllPlayerControllers)
    {
        AWormCharacter* Character = GetWormCharacterFromController(Controller);
        if (Character && Character->GetHealth() > 0)
        {
            AlivePlayerCount++;
            
            // If you have a team system, you can add:
            // int32 TeamID = Character->GetTeamID();
            // ActiveTeams.Add(TeamID);
        }
    }
    
    // If only one player remains (or one team), the game is over
    if (AlivePlayerCount <= 1)
    {
        // Handle game end
        return true;
    }
    
    return false;
}

void AWormGameMode::OnTurnStarted_Implementation(AController* ActiveController)
{
    // This function can be overridden in Blueprint
    if (ActiveController)
    {
        AWormCharacter* Character = GetWormCharacterFromController(ActiveController);
        if (Character)
        {
            UE_LOG(LogTemp, Log, TEXT("Turn started for %s controlled by %s"), 
                *Character->GetName(), *ActiveController->GetName());
        }
    }
}

void AWormGameMode::OnTurnEnded_Implementation(AController* PreviousController)
{
    // This function can be overridden in Blueprint
    if (PreviousController)
    {
        AWormCharacter* Character = GetWormCharacterFromController(PreviousController);
        if (Character)
        {
            UE_LOG(LogTemp, Log, TEXT("Turn ended for %s controlled by %s"), 
                *Character->GetName(), *PreviousController->GetName());
        }
    }
}

TArray<AImprovedVoxelBuilding*> AWormGameMode::GetAllVoxelBuildings()
{
    return AImprovedVoxelBuilding::FindAllVoxelBuildings(this);
}

void AWormGameMode::GenerateVoxelBuildings()
{
    if (!VoxelBuildingClass)
    {
        UE_LOG(LogTemp, Error, TEXT("VoxelBuildingClass not specified in WormGameMode!"));
        return;
    }

    // Appliquer les paramètres de terrain avant de générer
    ApplyTerrainSettings();

    UE_LOG(LogTemp, Warning, TEXT("Generating %d voxel buildings..."), NumberOfBuildings);

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    // Obtenir les paramètres de terrain
    UVoxelTerrainSettingsManager* Manager = UVoxelTerrainSettingsManager::GetInstance();
    FVoxelTerrainSettings Settings;
    
    if (Manager)
    {
        Settings = Manager->GetSettings();
    }
    else
    {
        // Utiliser les valeurs par défaut si le manager n'est pas disponible
        Settings = FVoxelTerrainSettings();
    }

    // Utiliser des valeurs fixes pour la cohérence
    for (int32 i = 0; i < NumberOfBuildings; i++)
    {
        // Calculer une position aléatoire dans la zone
        float X = FMath::RandRange(-SpawnAreaSize, SpawnAreaSize);
        float Y = FMath::RandRange(-SpawnAreaSize, SpawnAreaSize);
        float Z = 0.0f; // Position buildings at ground level

        FVector Location = FVector(X, Y, Z);
        FRotator Rotation = FRotator(0.0f, FMath::RandRange(0.0f, 360.0f), 0.0f);

        // Spawn voxel building
        AImprovedVoxelBuilding* Building = GetWorld()->SpawnActor<AImprovedVoxelBuilding>(
            VoxelBuildingClass,
            Location,
            Rotation,
            SpawnParams
        );
        
        if (Building)
        {
            UE_LOG(LogTemp, Warning, TEXT("Voxel building %d generated at %s"), i, *Location.ToString());

            // Appliquer les paramètres de terrain
            Building->GridSizeX = Settings.GridSizeX;
            Building->GridSizeY = Settings.GridSizeY;
            Building->GridSizeZ = Settings.GridSizeZ;
            Building->VoxelSize = Settings.VoxelSize;
            Building->SmoothingFactor = Settings.SmoothingFactor;
            Building->bUseRandomColors = Settings.bUseRandomColors;
            Building->CubeMargin = Settings.CubeMargin;
            Building->bSpawnDebrisOnDestruction = Settings.bSpawnDebrisOnDestruction;
            Building->DebrisAmountMultiplier = Settings.DebrisAmountMultiplier;
            Building->bSpawnImpactCloud = Settings.bSpawnImpactCloud;
            Building->ForceNetUpdate();

            // Generate building
            Building->GenerateBuilding();
        }
    }
    
    // Update GameState with reference to buildings
    AWormGameState* WormGS = GetGameState<AWormGameState>();
    if (WormGS)
    {
        // If needed, you can expose a property in GameState to store buildings
        // For now, just log success
        UE_LOG(LogTemp, Warning, TEXT("Voxel buildings generated and available through Game Mode"));
    }
}

void AWormGameMode::InitializeWeaponsForAllPlayers()
{
    // Check if we have defined weapons
    if (AvailableWeaponTypes.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ No weapon types defined in GameMode!"));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("✅ Initializing weapons for all players (%d weapon types available)"), 
        AvailableWeaponTypes.Num());

    // Recollect controllers for safety
    GatherAllPlayerControllers();
    
    // Log the controller list to verify
    UE_LOG(LogTemp, Warning, TEXT("Found %d controllers to assign weapons to:"), AllPlayerControllers.Num());
    for (int32 i = 0; i < AllPlayerControllers.Num(); i++)
    {
        AController* Controller = AllPlayerControllers[i];
        APawn* ControlledPawn = Controller ? Controller->GetPawn() : nullptr;
        
        UE_LOG(LogTemp, Warning, TEXT("  Controller %d: %s, Has Pawn: %s"), 
            i, 
            Controller ? *Controller->GetName() : TEXT("NULL"),
            ControlledPawn ? *ControlledPawn->GetName() : TEXT("NO PAWN"));
    }

    // Process each controller with individual timers to ensure clean processing
    for (int32 i = 0; i < AllPlayerControllers.Num(); i++)
    {
        AController* Controller = AllPlayerControllers[i];
        int32 PlayerIndex = i;
        
        FTimerHandle* PlayerWeaponTimer = new FTimerHandle();
        
        GetWorld()->GetTimerManager().SetTimer(
            *PlayerWeaponTimer,
            [this, Controller, PlayerIndex, PlayerWeaponTimer]() {
                // Get the character for this controller, trying harder if needed
                AWormCharacter* Character = GetWormCharacterFromController(Controller);
                if (!Character && Controller)
                {
                    // Try to find the character even if not properly possessed yet
                    for (TActorIterator<AWormCharacter> It(GetWorld()); It; ++It)
                    {
                        AWormCharacter* FoundChar = *It;
                        // Check if this character belongs to the controller
                        if (FoundChar && FoundChar->GetController() == Controller)
                        {
                            Character = FoundChar;
                            break;
                        }
                    }
                }
                
                if (Character)
                {
                    UE_LOG(LogTemp, Warning, TEXT("Assigning weapons to %s (Player %d)"), 
                        *Character->GetName(), PlayerIndex);
                    
                    // Assign weapons and ensure visibility
                    Character->SetAvailableWeapons(AvailableWeaponTypes);
                    
                    // Additional safety timer to ensure weapon is visible after assignment
                    FTimerHandle WeaponVisibilityTimer;
                    GetWorld()->GetTimerManager().SetTimer(
                        WeaponVisibilityTimer,
                        [Character]() {
                            if (Character && Character->CurrentWeapon)
                            {
                                Character->CurrentWeapon->EnsureWeaponVisibility();
                                UE_LOG(LogTemp, Warning, TEXT("Ensured weapon visibility for %s"), *Character->GetName());
                            }
                        },
                        0.5f,
                        false
                    );
                }
                else
                {
                    UE_LOG(LogTemp, Error, TEXT("Could not get character for controller %s"), 
                        Controller ? *Controller->GetName() : TEXT("NULL"));
                }
                
                // Clean up the timer handle
                delete PlayerWeaponTimer;
            },
            0.5f + (0.2f * i),  // Staggered timers for safety
            false
        );
    }

    // Schedule a delayed comprehensive check to ensure all characters have their weapons
    FTimerHandle WeaponVerificationTimer;
    GetWorld()->GetTimerManager().SetTimer(
        WeaponVerificationTimer,
        [this]() {
            VerifyWeaponsForAllPlayers();
        },
        3.0f,  // Check after all individual timers should have completed
        false
    );
}
void AWormGameMode::VerifyWeaponsForAllPlayers()
{
    UE_LOG(LogTemp, Warning, TEXT("Verifying weapons for all players..."));
    
    // First, gather a fresh list of controllers and characters
    GatherAllPlayerControllers();
    
    // Check each character
    for (AController* Controller : AllPlayerControllers)
    {
        AWormCharacter* Character = GetWormCharacterFromController(Controller);
        if (Character)
        {
            // Check if character has a weapon
            if (!Character->CurrentWeapon)
            {
                UE_LOG(LogTemp, Warning, TEXT("Character %s has no weapon - reassigning"), *Character->GetName());
                Character->SetAvailableWeapons(AvailableWeaponTypes);
            }
            else
            {
                // Ensure weapon visibility
                Character->CurrentWeapon->EnsureWeaponVisibility();
                UE_LOG(LogTemp, Warning, TEXT("Character %s already has weapon %s"), 
                    *Character->GetName(), *Character->CurrentWeapon->GetName());
            }
            
            // Ensure all movement modes are correct
            if (Character->GetCharacterMovement())
            {
                // Reset velocity and set to walking
                Character->GetCharacterMovement()->Velocity = FVector::ZeroVector;
                Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
            }
        }
        else if (Controller)
        {
            UE_LOG(LogTemp, Error, TEXT("Controller %s has no character!"), *Controller->GetName());
        }
    }
}
void AWormGameMode::ApplyTerrainSettings()
{
    // Obtenir les paramètres actuels
    UVoxelTerrainSettingsManager* Manager = UVoxelTerrainSettingsManager::GetInstance();
    if (!Manager)
    {
        return;
    }
    
    FVoxelTerrainSettings Settings = Manager->GetSettings();
    
    // Mettre à jour les propriétés du GameMode
    NumberOfBuildings = Settings.NumberOfBuildings;
    SpawnAreaSize = Settings.SpawnAreaSize;
    
    UE_LOG(LogTemp, Warning, TEXT("Applied terrain settings: Buildings=%d, Area=%.1f"), 
        NumberOfBuildings, SpawnAreaSize);
}

AGameInitManager* AWormGameMode::SetupGameInitialization()
{
    if (GameInitManager)
    {
        return GameInitManager;
    }

    UE_LOG(LogTemp, Log, TEXT("Setting up game initialization manager..."));

    UGameInitFactorySubsystem* InitFactory = GetWorld()->GetGameInstance()->GetSubsystem<UGameInitFactorySubsystem>();
    if (InitFactory)
    {
        GameInitManager = InitFactory->GetOrCreateGameInitManager(this);
        if (GameInitManager)
        {
            return GameInitManager;
        }
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    UClass* ClassToUse = GameInitManagerClass ? 
        GameInitManagerClass.Get() : AGameInitManager::StaticClass();

    GameInitManager = GetWorld()->SpawnActor<AGameInitManager>(
        ClassToUse,
        FVector::ZeroVector,
        FRotator::ZeroRotator,
        SpawnParams
    );

    if (GameInitManager)
    {
        AWormGameState* WormGS = GetGameState<AWormGameState>();
        if (WormGS && WormGS->LoadingManager)
        {
            // Make sure GameInitManager and LoadingManager use the same widget class
            if (LoadingWidgetClass)
            {
                WormGS->LoadingManager->LoadingWidgetClass = LoadingWidgetClass;
                // Also set in GameInitManager for consistency
                GameInitManager->LoadingWidgetClass = LoadingWidgetClass;
            }
            UE_LOG(LogTemp, Log, TEXT("NetworkLoadingManager in GameState configured with correct widget class"));
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("WARNING: No NetworkLoadingManager found in GameState - loading screens won't be networked!"));
        }
    }

    return GameInitManager;
}