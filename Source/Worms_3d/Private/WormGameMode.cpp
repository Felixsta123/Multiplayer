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
#include "Worms_3d/Building/AVoxelBuilding.h"
#include "Worms_3d/Env/EnvironmentalEventsManager.h"
#include "Worms_3d/Init/GameInitFactorySubsystem.h"
#include "Worms_3d/Building/VoxelTerrainSettings.h"
#include "Worms_3d/Init/NetworkLoadingManager.h"
#include "Worms_3d/Misc/WormGameInstance.h"
#include "Worms_3d/UI/W_GameResultsScreen.h"

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
    FCharacterNameList LauraNames;
    LauraNames.Names = { "Athena", "Nova", "Spark" };
    
    FCharacterNameList GuyNames;
    GuyNames.Names = { "Titan", "Specter", "Orion" };
    
    FCharacterNameList DavidNames;
    DavidNames.Names = { "Cipher", "Phoenix", "Echo" };
    
    FCharacterNameList EmilyNames;
    EmilyNames.Names = { "Raven", "Aurora", "Luna" };
    
    CharacterNamesByType.Add("Laura", LauraNames);
    CharacterNamesByType.Add("Guy", GuyNames);
    CharacterNamesByType.Add("David", DavidNames);
    CharacterNamesByType.Add("Emily", EmilyNames);
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

    // IMPORTANT: First increment team index, then character index when we've gone through all teams
    CurrentTeamIndex = (CurrentTeamIndex + 1) % NumTeams;
    
    // If we've gone through all teams for this character position, move to the next character
    if (CurrentTeamIndex == 0)
    {
        CurrentCharacterIndex = (CurrentCharacterIndex + 1) % CharactersPerTeam;
    }

    // Store starting point to avoid infinite loop
    int32 startTeamIndex = CurrentTeamIndex;
    int32 startCharIndex = CurrentCharacterIndex;
    bool foundValidCharacter = false;
    
    do {
        // Get the characters of the current team
        TArray<AWormCharacter*> TeamMembers = WormGS->GetTeamMembers(CurrentTeamIndex);
        UE_LOG(LogTemp, Warning, TEXT("Looking for active character - Team %d, Char %d (%d members)"), 
            CurrentTeamIndex, CurrentCharacterIndex, TeamMembers.Num());
                
        // Check if the current index is valid
        if (TeamMembers.IsValidIndex(CurrentCharacterIndex))
        {
            AWormCharacter* Character = TeamMembers[CurrentCharacterIndex];
            if (Character && Character->GetHealth() > 0)
            {
                // Deactivate all characters first
                for (int32 i = 0; i < WormGS->Teams.Num(); i++) {
                    for (AWormCharacter* Member : WormGS->Teams[i].TeamMembers) {
                        if (Member) Member->SetIsMyTurn(false);
                    }
                }
                
                // Activate only this character
                Character->SetIsMyTurn(true);
                
                // Make the team controller possess this character
                AController* TeamController = AllPlayerControllers[CurrentTeamIndex];
                if (TeamController) {
                    if (TeamController->GetPawn()) {
                        TeamController->UnPossess();
                    }
                    TeamController->Possess(Character);
                }
                
                foundValidCharacter = true;
                break;
            }
        }

        // If this character isn't valid, try the next team
        CurrentTeamIndex = (CurrentTeamIndex + 1) % NumTeams;
        
        // If we've gone through all teams for this character position, move to the next character
        if (CurrentTeamIndex == 0)
        {
            CurrentCharacterIndex = (CurrentCharacterIndex + 1) % CharactersPerTeam;
        }
        
    } while (!foundValidCharacter && 
            (CurrentTeamIndex != startTeamIndex || 
             CurrentCharacterIndex != startCharIndex));

    // If no valid character is found, end the game
    if (!foundValidCharacter) {
        CheckGameEndCondition();
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

    UE_LOG(LogTemp, Warning, TEXT("✅ Initializing weapons for all characters (%d weapon types available)"), 
        AvailableWeaponTypes.Num());

    // Option 1: Initialize weapons for ALL characters, not just one per team
    
    // Find all worm characters in the world
    TArray<AActor*> AllCharacters;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AWormCharacter::StaticClass(), AllCharacters);
    
    UE_LOG(LogTemp, Warning, TEXT("Found %d worm characters to equip with weapons"), AllCharacters.Num());
    
    // Process each character with individual timers to ensure clean processing
    for (int32 i = 0; i < AllCharacters.Num(); i++)
    {
        AWormCharacter* Character = Cast<AWormCharacter>(AllCharacters[i]);
        if (!Character) continue;
        
        int32 CharIndex = i;
        FTimerHandle* CharWeaponTimer = new FTimerHandle();
        
        GetWorld()->GetTimerManager().SetTimer(
            *CharWeaponTimer,
            [this, Character, CharIndex, CharWeaponTimer]() {
                if (Character && IsValid(Character))
                {
                    UE_LOG(LogTemp, Warning, TEXT("Assigning weapons to character %s (Team %d, Index %d)"), 
                        *Character->GetName(), Character->TeamId, Character->CharacterIndexInTeam);
                    
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
                    UE_LOG(LogTemp, Error, TEXT("Character no longer valid at index %d"), CharIndex);
                }
                
                // Clean up the timer handle
                delete CharWeaponTimer;
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
    
    // Check all characters, not just those controlled by players
    TArray<AActor*> AllCharacters;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AWormCharacter::StaticClass(), AllCharacters);
    
    // Check each character
    for (AActor* Actor : AllCharacters)
    {
        AWormCharacter* Character = Cast<AWormCharacter>(Actor);
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
                //TO DO: Set the character movement mode to walking
            }
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
        
        // Reset teams
        for (FTeamInfo& Team : WormGS->Teams)
        {
            Team.TeamMembers.Empty();
        }
        
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
    
    // IMPORTANT: Clean up ALL worm characters, not just possessed ones
    TArray<AActor*> AllCharacters;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AWormCharacter::StaticClass(), AllCharacters);
    UE_LOG(LogTemp, Warning, TEXT("Cleaning up %d worm characters from previous session"), AllCharacters.Num());
    
    // First unpossess all controllers to avoid crashes
    for (FConstControllerIterator It = GetWorld()->GetControllerIterator(); It; ++It)
    {
        AController* Controller = It->Get();
        if (Controller && Controller->GetPawn())
        {
            Controller->UnPossess();
        }
    }
    
    // Now safely destroy all characters
    for (AActor* Actor : AllCharacters)
    {
        if (Actor)
        {
            UE_LOG(LogTemp, Warning, TEXT("Destroying character: %s"), *Actor->GetName());
            Actor->Destroy();
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
    
    // Wait a short moment to ensure all actors are properly destroyed
    FTimerHandle CleanupTimerHandle;
    GetWorld()->GetTimerManager().SetTimer(
        CleanupTimerHandle,
        [this]() {
            // Double-check that everything is cleaned up
            TArray<AActor*> RemainingCharacters;
            UGameplayStatics::GetAllActorsOfClass(GetWorld(), AWormCharacter::StaticClass(), RemainingCharacters);
            if (RemainingCharacters.Num() > 0)
            {
                UE_LOG(LogTemp, Warning, TEXT("Found %d remaining characters after cleanup, forcing destruction"), 
                    RemainingCharacters.Num());
                for (AActor* Actor : RemainingCharacters)
                {
                    if (Actor)
                    {
                        Actor->Destroy();
                    }
                }
            }
            
            // Use GameInitManager to handle the restart
            AWormGameState* WormGS = GetGameState<AWormGameState>();
            if (GameInitManager)
            {
                // Show loading screen first
                if (WormGS && WormGS->LoadingManager)
                {
                    WormGS->ShowLoadingScreen(10.0f);
                }

                // Start initialization sequence
                if (GameInitManager)
                {
                    GameInitManager->StartInitializationSequence();
                }
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("Cannot restart: GameInitManager is null"));
            }
        },
        0.5f, // Small delay to ensure destruction completes
        false
    );
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
FString AWormGameMode::GetCharacterInGameName(UClass* CharacterClass, int32 TeamId, int32 CharIndexInTeam)
{
    if (!CharacterClass)
        return FString("Unknown");
    
    // Get the character class name
    FString ClassName = CharacterClass->GetName();
    
    // Log the input parameters
    UE_LOG(LogTemp, Warning, TEXT("Getting in-game name for class: %s"), *ClassName);
    
    // Find which character type this is
    FString CharacterType;
    for (const FString& Type : {"Laura", "Guy", "David", "Emily"})
    {
        if (ClassName.Contains(Type))
        {
            CharacterType = Type;
            UE_LOG(LogTemp, Warning, TEXT("Found character type: %s"), *CharacterType);
            break;
        }
    }
    
    if (CharacterType.IsEmpty())
        return FString::Printf(TEXT("Agent %d-%d"), TeamId, CharIndexInTeam);
    
    // Get name array for this type
    FCharacterNameList* NameList = CharacterNamesByType.Find(CharacterType);
    if (!NameList || NameList->Names.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("No names found for character type: %s"), *CharacterType);
        return FString::Printf(TEXT("%s %d-%d"), *CharacterType, TeamId, CharIndexInTeam);
    }
    // Use the character index to pick a name, wrapping around if needed
    int32 NameIndex = CharIndexInTeam % NameList->Names.Num();
    
    // Make sure we have a valid name
    if (!NameList->Names.IsValidIndex(NameIndex))
        return FString::Printf(TEXT("%s %d-%d"), *CharacterType, TeamId, CharIndexInTeam);
    
    FString CharacterName = NameList->Names[NameIndex];
    //print the list of names and the selected name
    FString NamesList;
    for (const FString& Name : NameList->Names)
    {
        NamesList += Name + ", ";
    }
    UE_LOG(LogTemp, Warning, TEXT("Available names for %s: %s"), *CharacterType, *NamesList);
    UE_LOG(LogTemp, Warning, TEXT("Selected character name: %s"), *CharacterName);
    // Double check the name is valid
    if (CharacterName.IsEmpty())
        return FString::Printf(TEXT("%s %d-%d"), *CharacterType, TeamId, CharIndexInTeam);
    
    // Log the selected name for debugging
    UE_LOG(LogTemp, Warning, TEXT("Selected character name: %s"), *CharacterName);
    
    return CharacterName;
}
