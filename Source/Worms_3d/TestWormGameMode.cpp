#include "TestWormGameMode.h"
// Temporairement commenté pour éviter les erreurs de compilation
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
    
    // Activer le Tick pour surveiller le temps
    PrimaryActorTick.bCanEverTick = true;
}

void ATestWormGameMode::BeginPlay()
{
    Super::BeginPlay();

    // Créer un terrain de test
    SpawnDestructibleTerrain();
    
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

void ATestWormGameMode::ResetTurn()
{
    // Réinitialiser le temps du tour
    RemainingTurnTime = TurnDuration;
    
    // Ici, on pourrait ajouter d'autres mécaniques de réinitialisation si nécessaire
    // Par exemple, recharger les armes, réinitialiser les points de mouvement, etc.
    
    UE_LOG(LogTemp, Warning, TEXT("Tour réinitialisé - Temps restant: %f"), RemainingTurnTime);
}

void ATestWormGameMode::SpawnDestructibleTerrain()
{
    // Vérifier si nous devons utiliser le terrain en chunks
    if (bUseChunkBasedTerrain)
    {
        SpawnChunkBasedTerrain();
        return;
    }
    
    // Continuer avec le code original pour l'ancien terrain
    // Position et rotation au centre de la carte
    FVector Location = FVector(-1000.0f, -100.0f, -2250.0f);
    FRotator Rotation = FRotator::ZeroRotator;
    
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    // Nettoyer les instances précédentes
    if (OldDestructibleTerrain)
    {
        OldDestructibleTerrain->Destroy();
        OldDestructibleTerrain = nullptr;
    }
    
    if (ChunkBasedTerrain)
    {
        ChunkBasedTerrain->Destroy();
        ChunkBasedTerrain = nullptr;
    }

    // Spawner l'acteur de l'ancien système
    if (!OldDestructibleTerrainClass)
    {
        UE_LOG(LogTemp, Error, TEXT("OldDestructibleTerrainClass non défini dans TestGameMode!"));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("Génération de l'ancien terrain destructible de test..."));

    OldDestructibleTerrain = GetWorld()->SpawnActor<ADestructibleTerrain>(
        OldDestructibleTerrainClass,
        Location,
        Rotation,
        SpawnParams
    );

    if (OldDestructibleTerrain)
    {
        UE_LOG(LogTemp, Warning, TEXT("Ancien terrain destructible généré avec succès: %s"), 
            *OldDestructibleTerrain->GetName());
        
        // Forcer une mise à jour visuelle après un délai
        FTimerHandle UpdateTimerHandle;
        GetWorld()->GetTimerManager().SetTimer(
            UpdateTimerHandle,
            [this]() { 
                if (OldDestructibleTerrain) {
                    OldDestructibleTerrain->Multicast_ForceVisualUpdate();
                }
            },
            2.0f,
            false
        );
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Échec de la génération de l'ancien terrain destructible!"));
    }
}

void ATestWormGameMode::SpawnChunkBasedTerrain()
{
    // Vérifier que la classe est définie
    if (!ChunkBasedTerrainClass)
    {
        UE_LOG(LogTemp, Error, TEXT("ChunkBasedTerrainClass non défini dans TestGameMode!"));
        return;
    }
    
    // Position et rotation au centre de la carte
    FVector Location = FVector(0.0f, 0.0f, 0.0f); // Positionnez à l'origine pour le test
    FRotator Rotation = FRotator::ZeroRotator;
    
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    
    // Nettoyer les instances précédentes
    if (ChunkBasedTerrain)
    {
        ChunkBasedTerrain->Destroy();
        ChunkBasedTerrain = nullptr;
    }
    
    if (OldDestructibleTerrain)
    {
        OldDestructibleTerrain->Destroy();
        OldDestructibleTerrain = nullptr;
    }
    
    // Spawner l'acteur du nouveau système
    ChunkBasedTerrain = GetWorld()->SpawnActor<AActor>(
        ChunkBasedTerrainClass,
        Location,
        Rotation,
        SpawnParams
    );
    
    if (ChunkBasedTerrain)
    {
        UE_LOG(LogTemp, Error, TEXT("===== CHUNK TERRAIN SPAWNED AT: %s ====="), 
            *ChunkBasedTerrain->GetActorLocation().ToString());
        
        // DEBUG: Ajouter une référence visuelle
        UStaticMeshComponent* DebugSphere = NewObject<UStaticMeshComponent>(ChunkBasedTerrain);
        if (DebugSphere)
        {
            DebugSphere->RegisterComponent();
            DebugSphere->AttachToComponent(ChunkBasedTerrain->GetRootComponent(), 
                FAttachmentTransformRules::KeepRelativeTransform);
                
            // Utiliser un mesh de sphère standard
            UStaticMesh* SphereMesh = LoadObject<UStaticMesh>(nullptr, 
                TEXT("/Engine/BasicShapes/Sphere.Sphere"));
            if (SphereMesh)
            {
                DebugSphere->SetStaticMesh(SphereMesh);
                DebugSphere->SetRelativeLocation(FVector(0, 0, 0));
                DebugSphere->SetWorldScale3D(FVector(5.0f)); // Grande sphère rouge
                
                UMaterialInstanceDynamic* DynMat = DebugSphere->CreateAndSetMaterialInstanceDynamic(0);
                if (DynMat)
                {
                    DynMat->SetVectorParameterValue("Color", FLinearColor::Red);
                }
            }
        }
    }
}

void ATestWormGameMode::ToggleTerrainSystem()
{
    // Basculer entre les systèmes de terrain
    bUseChunkBasedTerrain = !bUseChunkBasedTerrain;
    
    UE_LOG(LogTemp, Warning, TEXT("Basculement vers le système de terrain: %s"), 
        bUseChunkBasedTerrain ? TEXT("Chunk-based") : TEXT("Standard"));
    
    // Régénérer le terrain avec le nouveau système
    SpawnDestructibleTerrain();
}

FString ATestWormGameMode::GetActiveDestructionSystemName() const
{
    return bUseChunkBasedTerrain ? FString(TEXT("Chunks")) : FString(TEXT("Standard"));
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