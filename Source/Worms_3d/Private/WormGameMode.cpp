// WormGameMode.cpp
#include "WormGameMode.h"

#include "AIController.h"
#include "EngineUtils.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "WormGameState.h"
#include "WormPlayerController.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Worms_3d/AVoxelBuilding.h"
#include "Worms_3d/EnvironmentalEventsManager.h"
#include "Worms_3d/GameInitFactorySubsystem.h"
#include "Worms_3d/VoxelTerrainSettings.h"
#include "Worms_3d/WormGameInstance.h"
#include "Worms_3d/W_GameResultsScreen.h"

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
    
    // Setup game initialization manager
    if (bUseGameInitManager)
    {
        InitializeWaterSystem();
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


void AWormGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
    Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
    
    // Get expected players from GameInstance
    UWormGameInstance* GameInstance = Cast<UWormGameInstance>(GetGameInstance());
    if (GameInstance)
    {
        NumPlayers = GameInstance->ExpectedPlayerCount;
        UE_LOG(LogTemp, Warning, TEXT("Setting NumPlayers from GameInstance: %d"), NumPlayers);
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
    if (AllPlayerControllers.Num() == 0) return;

    AWormGameState* WormGS = GetGameState<AWormGameState>();
    if (!WormGS) return;

    // Store original indices to prevent infinite loop
    int32 startTeamIndex = CurrentTeamIndex;
    int32 startCharIndex = CurrentCharacterIndex;
    bool foundValidCharacter = false;

    UE_LOG(LogTemp, Log, TEXT("Starting next turn from team %d, character %d"), 
        CurrentTeamIndex, CurrentCharacterIndex);

    do {
        // Get current team's characters
        TArray<AWormCharacter*> TeamMembers = WormGS->GetTeamMembers(CurrentTeamIndex);
        //For debugging purposes print all the team  and all the team members names
        UE_LOG(LogTemp, Log, TEXT("Team %d has %d members"), CurrentTeamIndex, TeamMembers.Num());
        for (int i = 0; i < NumTeams; i++)
        {
            TArray<AWormCharacter*> TeamMembersPrint = WormGS->GetTeamMembers(i);
            UE_LOG(LogTemp, Log, TEXT("Team %d has %d members"), i, TeamMembersPrint.Num());
            for (int j = 0; j < TeamMembersPrint.Num(); j++)
            {
                UE_LOG(LogTemp, Log, TEXT("Team %d member %d: %s"), i, j, *TeamMembersPrint[j]->GetName());
            }
        }
      
        // Check if current character in team is valid and alive
        if (TeamMembers.IsValidIndex(CurrentCharacterIndex))
        {
            AWormCharacter* Character = TeamMembers[CurrentCharacterIndex];
            if (Character && Character->GetHealth() > 0)
            {
                foundValidCharacter = true;
                break;
            }
        } else {
            // Invalid index, move to next character
            CurrentCharacterIndex++;
            UE_LOG(LogTemp, Log, TEXT("Invalid character index, moving to next"));
        }
        

        // Move to next character/team
        CurrentCharacterIndex++;
        if (CurrentCharacterIndex >= CharactersPerTeam)
        {
            CurrentCharacterIndex = 0;
            CurrentTeamIndex = (CurrentTeamIndex + 1) % NumTeams;
        }

    } while (!foundValidCharacter && 
             (CurrentTeamIndex != startTeamIndex || 
              CurrentCharacterIndex != startCharIndex));

    if (!foundValidCharacter)
    {
        CheckGameEndCondition();
        return;
    }

    // Activate the character's turn
    TArray<AWormCharacter*> TeamMembers = WormGS->GetTeamMembers(CurrentTeamIndex);
    AWormCharacter* ActiveCharacter = TeamMembers[CurrentCharacterIndex];
    if (ActiveCharacter)
    {
        // Deactivate all other characters
        for (AController* Controller : AllPlayerControllers)
        {
            if (AWormCharacter* Character = GetWormCharacterFromController(Controller))
            {
                Character->SetIsMyTurn(Character == ActiveCharacter);
            }
        }

        // Update GameState with correct individual character index
        int32 globalCharacterIndex = (CurrentTeamIndex * CharactersPerTeam) + CurrentCharacterIndex;
        WormGS->SetCurrentPlayerByIndex(globalCharacterIndex);
        
        StartTurnTimer();
    }
}
void AWormGameMode::EndCurrentTurn()
{
    // Cancel current timer
    if (WaterSystemManager)
    {
        IWaterSystemInterface* WaterInterface = Cast<IWaterSystemInterface>(WaterSystemManager);
        if (WaterInterface)
        {
            // Use interface to call methods
            WaterInterface->Execute_NotifyTurnEnded(WaterSystemManager);
        }
        else
        {
            // Alternative direct function call if interface isn't used
            UFunction* TurnEndedFunction = WaterSystemManager->FindFunction(FName("NotifyTurnEnded"));
            if (TurnEndedFunction)
            {
                WaterSystemManager->ProcessEvent(TurnEndedFunction, nullptr);
            }
        }
    }
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
            if (LoadingWidgetClass)
            {
                WormGS->LoadingManager->LoadingWidgetClass = LoadingWidgetClass;
                GameInitManager->LoadingWidgetClass = LoadingWidgetClass;
            }
        }
    }

    return GameInitManager;
}

void AWormGameMode::ShowGameOverWidget()
{
    UE_LOG(LogTemp, Warning, TEXT("Showing game over widget"));
    
    // Get Game State for results data
    AWormGameState* WormGS = GetGameState<AWormGameState>();
    if (!WormGS)
    {
        UE_LOG(LogTemp, Error, TEXT("Cannot show game over widget: GameState not valid"));
        return;
    }

    if (!GameOverWidgetClass)
    {
        UE_LOG(LogTemp, Error, TEXT("GameOverWidgetClass not set in GameMode!"));
        return;
    }
    
    // Get local player controller
    APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    if (!PC)
    {
        UE_LOG(LogTemp, Error, TEXT("Cannot show game over widget: PlayerController not found"));
        return;
    }
    
    // Create the widget
    UUserWidget* GameOverWidget = CreateWidget<UUserWidget>(PC, GameOverWidgetClass);
    if (!GameOverWidget)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to create GameOverWidget"));
        return;
    }
    
    // Try to cast to your specific widget class to set up data
    UW_GameResultsScreen* ResultsWidget = Cast<UW_GameResultsScreen>(GameOverWidget);
    if (ResultsWidget)
    {
        // Pass game results directly from GameState to the widget
        ResultsWidget->DisplayResults(WormGS->WinnerName, WormGS->PlayerDamageDealt);
    }
    
    // Show the widget
    GameOverWidget->AddToViewport(1000); // High Z-order to be on top
    
    // Set input mode to UI
    PC->SetInputMode(FInputModeUIOnly());
    PC->bShowMouseCursor = true;
    
    // Freeze the game
    UGameplayStatics::SetGamePaused(GetWorld(), true);
}


void AWormGameMode::StartRestartSequence()
{
    if (!HasAuthority())
    {
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("Starting game restart sequence"));
    
    // Reset game state
    AWormGameState* WormGS = GetGameState<AWormGameState>();
    if (WormGS)
    {
        WormGS->bGameOver = false;
        WormGS->WinnerName = TEXT("");
        WormGS->PlayerDamageDealt.Empty();
        
        // Force replication
        WormGS->ForceNetUpdate();
    }
    // Clear any existing game over widget from all screens
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        if (PC)
        {
            // Check if our results widget is showing and remove it
            TArray<UUserWidget*> AllWidgets;
            UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), AllWidgets, UW_GameResultsScreen::StaticClass(), false);
            for (UUserWidget* Widget : AllWidgets)
            {
                if (Widget && Widget->IsInViewport())
                {
                    Widget->RemoveFromParent();
                }
            }
        }
    }
    // Reset all characters
    for (FConstControllerIterator It = GetWorld()->GetControllerIterator(); It; ++It)
    {
        AController* Controller = It->Get();
        if (Controller)
        {
            // Respawn controlled characters
            if (Controller->GetPawn())
            {
                Controller->GetPawn()->Destroy();
            }
            
            // Force possession of a new character when restart completes
            // This will be handled by GameInitManager for player positioning
        }
    }
    
    // Destroy existing voxel buildings
    TArray<AImprovedVoxelBuilding*> ExistingBuildings = AImprovedVoxelBuilding::FindAllVoxelBuildings(this);
    for (AImprovedVoxelBuilding* Building : ExistingBuildings)
    {
        if (Building)
        {
            Building->Destroy();
        }
    }
    // Clean up any leftover weapons
    TArray<AActor*> LeftoverWeapons;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AWormWeapon::StaticClass(), LeftoverWeapons);
    for (AActor* Weapon : LeftoverWeapons)
    {
        if (Weapon)
        {
            UE_LOG(LogTemp, Warning, TEXT("Cleaning up leftover weapon: %s"), *Weapon->GetName());
            Weapon->Destroy();
        }
    }
    // Use GameInitManager to handle the restart
    if (GameInitManager)
    {
        // Show loading screen first
        if (WormGS && WormGS->LoadingManager)
        {
            WormGS->ShowLoadingScreen(10.0f);
        }

        // Start initialization sequence after a short delay
        FTimerHandle RestartTimerHandle;
        GetWorld()->GetTimerManager().SetTimer(
            RestartTimerHandle,
            [this]() {
                if (GameInitManager)
                {
                    GameInitManager->StartInitializationSequence();
                }
            },
            1.0f, // Small delay to ensure everything is ready
            false
        );
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Cannot restart: GameInitManager is null"));
    }
}


void AWormGameMode::InitializeWaterSystem()
{
    // Check if a water manager already exists
    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), WaterSystemManagerClass, FoundActors);
    
    if (FoundActors.Num() > 0)
    {
        WaterSystemManager = FoundActors[0];
        UE_LOG(LogTemp, Log, TEXT("Found existing water manager: %s"), *WaterSystemManager->GetName());
    }
    // If no manager exists, create one
    else if (WaterSystemManagerClass)
    {
        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        
        WaterSystemManager = GetWorld()->SpawnActor<AActor>(
            WaterSystemManagerClass,
            FVector::ZeroVector,
            FRotator::ZeroRotator,
            SpawnParams
        );
        
        if (WaterSystemManager)
        {
            UE_LOG(LogTemp, Log, TEXT("Water manager created successfully"));
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("Failed to create water manager"));
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("WaterSystemManagerClass not set in GameMode"));
    }
}


void AWormGameMode::NotifyPlayerReady(APlayerController* PC)
{
    if (!ReadyPlayers.Contains(PC))
    {
        ReadyPlayers.Add(PC);
        UE_LOG(LogTemp, Log, TEXT("Player ready: %s (%d/%d)"), 
            *PC->GetName(), ReadyPlayers.Num(), NumPlayers);
            
        CheckAllPlayersReady();
    }
}

void AWormGameMode::CheckAllPlayersReady()
{
    //get the number of players in the instance
    UWormGameInstance* GameInstance = Cast<UWormGameInstance>(GetGameInstance());
    int32 ExpectedPlayers = GameInstance ? GameInstance->ExpectedPlayerCount : NumPlayers;
    
    UE_LOG(LogTemp, Warning, TEXT("Checking player readiness: Ready=%d, Expected=%d"), 
        ReadyPlayers.Num(), ExpectedPlayers);

    if (!bInitializationStarted && ReadyPlayers.Num() == ExpectedPlayers)
    {
        bInitializationStarted = true;
        GameInitManager = SetupGameInitialization();
        UE_LOG(LogTemp, Log, TEXT("All players ready, starting initialization sequence : %d"), ReadyPlayers.Num());
        // Show loading screen first
        // Par défaut, on commence avec la première équipe et le premier personnage
        CurrentTeamIndex = 0;
        CurrentCharacterIndex = 0;
        AWormGameState* WormGS = GetGameState<AWormGameState>();
        if (WormGS && WormGS->LoadingManager)
        {
            NumTeams = ExpectedPlayers;
            WormGS->ShowLoadingScreen(10.0f);
            WormGS->InitializeTeams(NumTeams);

        }
        
        // Start initialization after brief delay
        FTimerHandle InitTimerHandle;
        GetWorld()->GetTimerManager().SetTimer(
            InitTimerHandle,
            [this]()
            {
                if (GameInitManager)
                {
                    GameInitManager->StartInitializationSequence();
                }
            },
            1.0f,
            false
        );
    }
}

void AWormGameMode::StartTurnTimer()
{
    GetWorldTimerManager().SetTimer(TurnTimerHandle, this, &AWormGameMode::OnTurnTimeExpired, TurnDuration, false);
    
    AWormGameState* WormGS = GetGameState<AWormGameState>();
    if (WormGS)
    {
        WormGS->RemainingTurnTime = TurnDuration;
    }
}