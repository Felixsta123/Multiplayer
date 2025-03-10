#include "TestWormGameMode.h"
#include "AVoxelBuilding.h"
#include "AWormCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "TestWormPlayerController.h"

ATestWormGameMode::ATestWormGameMode()
{
    // Définir le PlayerController par défaut pour le mode test
    PlayerControllerClass = ATestWormPlayerController::StaticClass();
    
    // Valeurs par défaut
    TurnDuration = 60.0f;
    RemainingTurnTime = TurnDuration;
    NumberOfBuildings = 1;
    SpawnAreaSize = 1000.0f;
    
    // Activer le Tick pour surveiller le temps
    PrimaryActorTick.bCanEverTick = true;
}

void ATestWormGameMode::BeginPlay()
{
    Super::BeginPlay();

    // Générer les bâtiments voxel après un court délai
    GetWorldTimerManager().SetTimer(BuildingsSpawnTimerHandle, this, &ATestWormGameMode::GenerateVoxelBuildings, 2.0f, false);
    
    // Initialiser les armes pour le joueur
    GetWorldTimerManager().SetTimer(WeaponSpawnTimerHandle, this, &ATestWormGameMode::InitializeWeaponsForPlayer, 1.0f, false);
}

void ATestWormGameMode::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    
    // Mettre à jour le temps restant pour le tour
    if (RemainingTurnTime > 0)
    {
        RemainingTurnTime -= DeltaTime;
        
        // Si le temps est écoulé, terminer le tour
        if (RemainingTurnTime <= 0)
        {
            RemainingTurnTime = 0;
            // Dans un mode solo, on pourrait ici réinitialiser le tour
            ResetTurn();
        }
    }
}

void ATestWormGameMode::GenerateVoxelBuildings()
{
    if (!BuildingClass)
    {
        UE_LOG(LogTemp, Error, TEXT("BuildingClass not specified in TestWormGameMode!"));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("Generating %d voxel buildings..."), NumberOfBuildings);

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    // Utiliser des valeurs fixes pour la cohérence
    for (int32 i = 0; i < NumberOfBuildings; i++)
    {
        // Calculer une position aléatoire dans la zone
        float X = FMath::RandRange(-SpawnAreaSize, SpawnAreaSize);
        float Y = FMath::RandRange(-SpawnAreaSize, SpawnAreaSize);
        float Z = 0.0f; // Positionner les bâtiments au niveau du sol

        FVector Location = FVector(X, Y, Z);
        FRotator Rotation = FRotator(0.0f, FMath::RandRange(0.0f, 360.0f), 0.0f);

        // Spawner le bâtiment voxel
        AImprovedVoxelBuilding* Building = GetWorld()->SpawnActor<AImprovedVoxelBuilding>(
            BuildingClass,
            Location,
            Rotation,
            SpawnParams
        );
        if (Building)
        {
            UE_LOG(LogTemp, Warning, TEXT("Voxel building %d generated at %s"), i, *Location.ToString());

            // Configurer avec des valeurs fixes
            Building->GridSizeX = 10;
            Building->GridSizeY = 10;
            Building->GridSizeZ = 10;
            Building->VoxelSize = 100.0f;
            Building->SmoothingFactor = 0.01f;
            Building->bUseRandomColors = true;
            Building->CubeMargin = 0.02f;
            Building->bUseDoubleSidedGeometry = true;

            // Générer le bâtiment
            Building->GenerateBuilding();
        }
    }
}

void ATestWormGameMode::ResetTurn()
{
    // Réinitialiser le temps du tour
    RemainingTurnTime = TurnDuration;
    
    // Ici, on pourrait ajouter d'autres mécaniques de réinitialisation si nécessaire
    // Par exemple, recharger les armes, réinitialiser les points de mouvement, etc.
    
    UE_LOG(LogTemp, Warning, TEXT("Tour réinitialisé - Temps restant: %f"), RemainingTurnTime);
}

void ATestWormGameMode::InitializeWeaponsForPlayer()
{
    // Vérifier qu'on a des armes définies
    if (AvailableWeaponTypes.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ No weapon types defined in TestGameMode!"));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("✅ Initializing weapons for player (%d weapon types available)"), 
        AvailableWeaponTypes.Num());

    // Obtenir le PlayerController
    APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    if (!PC)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Cannot find PlayerController!"));
        return;
    }

    // Obtenir le Pawn contrôlé
    APawn* Pawn = PC->GetPawn();
    AWormCharacter* WormCharacter = Cast<AWormCharacter>(Pawn);
    
    if (WormCharacter)
    {
        UE_LOG(LogTemp, Warning, TEXT("Assigning weapons to player: %s"), *WormCharacter->GetName());
        
        // Assigner les armes
        WormCharacter->SetAvailableWeapons(AvailableWeaponTypes);
        
        // Activer le tour pour ce personnage
        WormCharacter->SetIsMyTurn(true);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Cannot find valid WormCharacter for player!"));
    }
}