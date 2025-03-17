#include "WormTutorialGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Worms_3d/Init/PlayerSpawnManager.h"
#include "Worms_3d/Building/AVoxelBuilding.h"
#include "Worms_3d/Env/EnvironmentalEventsManager.h"
#include "WormGameState.h"
#include "WormPlayerController.h"
#include "EngineUtils.h"
#include "TutorialTargetBuilding.h"
#include "WormTutorialCharacter.h"

AWormTutorialGameMode::AWormTutorialGameMode()
{
    // Default setup
    NumTeams = 1;
    CharactersPerTeam = 1;
    
    // Default tutorial stages
    TutorialStages.Add("Welcome to Worms 3D! Use WASD keys to move around.");
    TutorialStages.Add("Press SPACE to jump.");
    TutorialStages.Add("Press LEFT MOUSE BUTTON to fire your weapon.");
    TutorialStages.Add("Buildings and terrain can be destroyed. Try shooting at the target wall.");
    TutorialStages.Add("Watch out for water! It's deadly if you fall in.");
    TutorialStages.Add("Congratulations! You've completed the tutorial.");
    
    // Initialize tracking variables
    bHasPlayerMoved = false;
    bHasPlayerJumped = false;
    bHasPlayerFired = false;
    bHasPlayerDestroyedTarget = false;
    CurrentStageIndex = 0;
}

void AWormTutorialGameMode::BeginPlay()
{
    Super::BeginPlay();
    
    // Initialize game state for single player
    AWormGameState* WormGS = GetGameState<AWormGameState>();
    if (WormGS)
    {
        WormGS->InitializeTeams(1); // Just one team for tutorial
    }
    
    // Generate tutorial environment
    GenerateTutorialEnvironment();
    
    // Start tutorial
    FTimerHandle StartTutorialTimer;
    GetWorld()->GetTimerManager().SetTimer(
        StartTutorialTimer,
        this,
        &AWormTutorialGameMode::StartTutorial,
        2.0f,
        false
    );
}

void AWormTutorialGameMode::GenerateTutorialEnvironment()
{
    UE_LOG(LogTemp, Warning, TEXT("Generating tutorial environment"));
    
    // First clear any existing buildings
    TArray<AActor*> ExistingBuildings;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AImprovedVoxelBuilding::StaticClass(), ExistingBuildings);
    for (AActor* Building : ExistingBuildings)
    {
        Building->Destroy();
    }
    
    // Setup buildings
    SetupBuildings();
    
    // Setup water
    SetupWaterSystem();
    
    // Setup characters after environment is ready
    SetupCharacters();
}

void AWormTutorialGameMode::SetupBuildings()
{
    UE_LOG(LogTemp, Warning, TEXT("Setting up tutorial buildings"));
    
    if (!TargetBuildingClass)
    {
        UE_LOG(LogTemp, Error, TEXT("Building classes not defined in tutorial game mode!"));
        return;
    }
    
    // Spawn corridor building (main platform)
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    
    // Main corridor/platform
    // Target building (for shooting practice)
    TargetBuilding = GetWorld()->SpawnActor<AImprovedVoxelBuilding>(
        TargetBuildingClass,
        FVector(600, 1500, 0), // Position to the side of the main corridor
        FRotator::ZeroRotator,
        SpawnParams
    );
    
    if (TargetBuilding)
    {
        UE_LOG(LogTemp, Warning, TEXT("Tutorial target building created"));
        
        // Configure building
        TargetBuilding->GridSizeX = 3;
        TargetBuilding->GridSizeY = 10;
        TargetBuilding->GridSizeZ = 8;
        TargetBuilding->VoxelSize = 100.0f;
        TargetBuilding->BuildingType = EVoxelBuildingType::Standard;
        TargetBuilding->GenerateBuilding();
    }
}

void AWormTutorialGameMode::SetupWaterSystem()
{
    UE_LOG(LogTemp, Warning, TEXT("Setting up tutorial water system"));
    
    // Find or create water system manager
    AEnvironmentalEventsManager* EnvManager = nullptr;
    
    // First try to find existing manager
    for (TActorIterator<AEnvironmentalEventsManager> It(GetWorld()); It; ++It)
    {
        EnvManager = *It;
        break;
    }
    
    // If no manager exists and we have a class defined, create one
    if (!EnvManager && WaterSystemManagerClass)
    {
        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        
        EnvManager = GetWorld()->SpawnActor<AEnvironmentalEventsManager>(
            WaterSystemManagerClass,
            FVector::ZeroVector,
            FRotator::ZeroRotator,
            SpawnParams
        );
    }
    
    if (EnvManager && EnvManager->WaterSystem)
    {
        UE_LOG(LogTemp, Warning, TEXT("Tutorial water system initialized"));
        
        // Store reference
        WaterSystemManager = EnvManager;
        
        // Configure water for tutorial
        EnvManager->WaterSystem->InitialWaterLevel = -800.0f; // Start below platform
        EnvManager->WaterSystem->MinWaterLevel = -1000.0f;
        EnvManager->WaterSystem->MaxWaterLevel = 800.0f;
        
        // Initialize water at low level
        EnvManager->WaterSystem->SetWaterLevel(EnvManager->WaterSystem->InitialWaterLevel, true);
        
        // Enable water events but don't auto-start rising
        EnvManager->bEnableWaterEvents = true;
        EnvManager->bIsWaterRisingActive = false;
        EnvManager->SetActiveEventTypes(EEventType::Water);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to initialize water system for tutorial!"));
    }
}

void AWormTutorialGameMode::SetupCharacters()
{
    UE_LOG(LogTemp, Warning, TEXT("Setting up tutorial characters"));
    
    // Get player controller
    APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    if (!PC)
    {
        UE_LOG(LogTemp, Error, TEXT("No player controller found for tutorial!"));
        return;
    }
    
    // Get character class from player settings
    TSubclassOf<AWormCharacter> CharacterClass = nullptr;
    AWormPlayerController* WPC = Cast<AWormPlayerController>(PC);
    if (WPC && WPC->PlayerSettings.MyPlayerCharacter)
    {
        CharacterClass = WPC->PlayerSettings.MyPlayerCharacter;
    }
    else
    {
        // Default character class if not found in player settings
        for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
        {
            UClass* Class = *ClassIt;
            if (Class->IsChildOf(AWormCharacter::StaticClass()) && !Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated))
            {
                CharacterClass = Class;
                break;
            }
        }
    }
    
    if (!CharacterClass)
    {
        UE_LOG(LogTemp, Error, TEXT("No character class found for tutorial!"));
        return;
    }
    
    // Spawn player character
    FVector SpawnLocation = FVector(0, 0, 700); // Above the main corridor
    if (CorridorBuilding)
    {
        // Spawn at the start of the corridor
        SpawnLocation = CorridorBuilding->GetTopSpawnPoint() + FVector(-500, -2000, 200);
    }
    
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    
    PlayerCharacter = GetWorld()->SpawnActor<AWormCharacter>(
        CharacterClass,
        SpawnLocation,
        FRotator::ZeroRotator,
        SpawnParams
    );
    
    if (PlayerCharacter)
    {
        UE_LOG(LogTemp, Warning, TEXT("Tutorial player character spawned"));
        
        // Configure character
        PlayerCharacter->Health = 100.0f;
        PlayerCharacter->TeamId = 0;
        PlayerCharacter->CharacterIndexInTeam = 0;
        PlayerCharacter->SetIsMyTurn(true);
        
        // Set weapon for player
        if (AvailableWeaponTypes.Num() > 0)
        {
            PlayerCharacter->SetAvailableWeapons(AvailableWeaponTypes);
        }
        
        // Let player controller possess character
        PC->Possess(PlayerCharacter);
        
        // Add to team in GameState
        AWormGameState* WormGS = GetGameState<AWormGameState>();
        if (WormGS)
        {
            WormGS->AddCharacterToTeam(PlayerCharacter, 0);
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to spawn player character for tutorial!"));
    }
    
    // Spawn dummy target (optional - for combat training)
    FVector DummyLocation = FVector(600, 2500, 600); // Near target building
    if (TargetBuilding)
    {
        DummyLocation = TargetBuilding->GetTopSpawnPoint() + FVector(0, 200, 0);
    }
    
    DummyTarget = GetWorld()->SpawnActor<AWormCharacter>(
        CharacterClass,
        DummyLocation,
        FRotator::ZeroRotator,
        SpawnParams
    );
    
    if (DummyTarget)
    {
        UE_LOG(LogTemp, Warning, TEXT("Tutorial dummy target spawned"));
        
        // Configure dummy
        DummyTarget->Health = 100.0f;
        DummyTarget->TeamId = 1; // Different team from player
        
        // Disable AI or player control for dummy
        if (DummyTarget->GetController())
        {
            DummyTarget->GetController()->UnPossess();
        }
        
        // Disable movement
        if (DummyTarget->GetCharacterMovement())
        {
            DummyTarget->GetCharacterMovement()->MaxWalkSpeed = 0;
        }
        
        // Don't add dummy to teams in GameState - it's just a prop
    }
}

void AWormTutorialGameMode::StartTutorial()
{
    UE_LOG(LogTemp, Warning, TEXT("Starting tutorial"));
    
    CurrentStageIndex = 0;
    
    // Create tutorial UI
    if (TutorialWidgetClass)
    {
        APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
        if (PC)
        {
            TutorialWidget = CreateWidget<UUserWidget>(PC, TutorialWidgetClass);
            if (TutorialWidget)
            {
                TutorialWidget->AddToViewport(100); // High Z-order to be on top
                
                // Set initial instruction via event dispatcher or interface
                // This would be implemented in BP
            }
            AWormTutorialCharacter* TutorialChar = Cast<AWormTutorialCharacter>(PlayerCharacter);
            if (TutorialChar)
            {
                TutorialChar->OnCharacterMoved.AddDynamic(this, &AWormTutorialGameMode::OnPlayerMoved);
                TutorialChar->OnCharacterJumped.AddDynamic(this, &AWormTutorialGameMode::OnPlayerJumped);
                TutorialChar->OnCharacterFired.AddDynamic(this, &AWormTutorialGameMode::OnPlayerFired);
            }
            
        }
    }
    
    // Make sure player character is in control
    if (PlayerCharacter)
    {
        PlayerCharacter->SetIsMyTurn(true);
    }
    
    // Trigger first tutorial stage
    OnStageCompleted(CurrentStageIndex);
}

void AWormTutorialGameMode::StartNextTurn()
{
    // Override to keep player in control during tutorial
    if (PlayerCharacter)
    {
        PlayerCharacter->SetIsMyTurn(true);
    }
}

void AWormTutorialGameMode::AdvanceToNextStage()
{
    if (CurrentStageIndex < TutorialStages.Num() - 1)
    {
        CurrentStageIndex++;
        OnStageCompleted(CurrentStageIndex);
    }
    else
    {
        // Tutorial complete
        UE_LOG(LogTemp, Warning, TEXT("Tutorial completed!"));
        
        // You could add completion reward or return to main menu logic here
    }
}

void AWormTutorialGameMode::CompleteStage(int32 StageIndex)
{
    if (StageIndex == CurrentStageIndex)
    {
        AdvanceToNextStage();
    }
}

bool AWormTutorialGameMode::CheckStageObjective()
{
    // Check objectives based on current stage
    switch (CurrentStageIndex)
    {
        case 0: // Movement
            return bHasPlayerMoved;
            
        case 1: // Jump
            return bHasPlayerJumped;
            
        case 2: // Fire weapon
            return bHasPlayerFired;
            
        case 3: // Destroy target
            return bHasPlayerDestroyedTarget;
            
        case 4: // Water hazard
            // This would be triggered by special event or timer
            return false;
            
        default:
            return false;
    }
}

void AWormTutorialGameMode::OnStageCompleted_Implementation(int32 StageIndex)
{
    // Blueprint implementable function to update UI with new instructions
    UE_LOG(LogTemp, Warning, TEXT("Tutorial stage %d: %s"), 
        StageIndex, *TutorialStages[FMath::Min(StageIndex, TutorialStages.Num()-1)]);
}

void AWormTutorialGameMode::OnPlayerMoved()
{
    bHasPlayerMoved = true;
    
    if (CurrentStageIndex == 0 && CheckStageObjective())
    {
        CompleteStage(0);
    }
}

void AWormTutorialGameMode::OnPlayerJumped()
{
    bHasPlayerJumped = true;
    
    if (CurrentStageIndex == 1 && CheckStageObjective())
    {
        CompleteStage(1);
    }
}

void AWormTutorialGameMode::OnPlayerFired()
{
    bHasPlayerFired = true;
    
    if (CurrentStageIndex == 2 && CheckStageObjective())
    {
        CompleteStage(2);
    }
}