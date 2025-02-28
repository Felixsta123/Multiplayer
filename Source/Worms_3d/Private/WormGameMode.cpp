// Fichier AWormGameMode.cpp
#include "WormGameMode.h"

#include "AIController.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "WormGameState.h"

AWormGameMode::AWormGameMode()
{
    PrimaryActorTick.bCanEverTick = true;
    
    // Valeurs par défaut
    TurnDuration = 30.0f;
    CurrentPlayerIndex = 0;
    NewVar = 0;
    local = false;
    
    // Définir explicitement la classe du GameState
    GameStateClass = AWormGameState::StaticClass();
    
    UE_LOG(LogTemp, Log, TEXT("WormGameMode constructor - Setting GameStateClass to: %s"), 
        *GameStateClass->GetName());
}

void AWormGameMode::BeginPlay()
{
    Super::BeginPlay();
    
    // Collecter tous les controllers
    GatherAllPlayerControllers();
    
    // Collecter les points de spawn
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AActor::StaticClass(), SpawnPoints);
    
    GetWorldTimerManager().SetTimer(TerrainSpawnTimerHandle, this, &AWormGameMode::SpawnDestructibleTerrain, 2.0f, false);
    // Initialiser les armes pour tous les joueurs
    GetWorldTimerManager().SetTimer(WeaponSpawnTimerHandle, this, &AWormGameMode::InitializeWeaponsForAllPlayers, 1.0f, false);
 
    // Démarrer le premier tour après un délai
    GetWorldTimerManager().SetTimer(TurnTimerHandle, this, &AWormGameMode::StartNextTurn, 2.0f, false);
}

void AWormGameMode::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    
    // Mettre à jour le temps restant
    if (GetWorldTimerManager().IsTimerActive(TurnTimerHandle))
    {
        RemainingTurnTime = GetWorldTimerManager().GetTimerRemaining(TurnTimerHandle);
        
        // Mettre à jour le GameState pour la réplication
        AWormGameState* WormGS = GetGameState<AWormGameState>();
        if (WormGS)
        {
            WormGS->RemainingTurnTime = RemainingTurnTime;
        }
    }
}


void AWormGameMode::GatherAllPlayerControllers()
{
    // Vider le tableau existant
    AllPlayerControllers.Empty();
    
    // Trouver tous les player controllers
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PlayerController = It->Get();
        if (PlayerController)
        {
            AllPlayerControllers.Add(PlayerController);
        }
    }
    
    // Trouver tous les AI controllers si nécessaire
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
    // Vérifier qu'il y a des controllers actifs
    if (AllPlayerControllers.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("No active controllers found!"));
        return;
    }

    // D'abord, recollectons les controllers pour être sûr que notre liste est à jour
    GatherAllPlayerControllers();

    // Vérifier si la partie est terminée
    if (CheckGameEndCondition())
    {
        // Gérer la fin de partie
        return;
    }

    // Mise à jour du GameState - IMPORTANT: faire ceci AVANT de changer le joueur actif
    AWormGameState* WormGS = GetGameState<AWormGameState>();
    if (WormGS)
    {
        // D'abord, mettre à jour la liste des joueurs avec la liste fraîchement collectée
        WormGS->UpdatePlayerList(AllPlayerControllers);
    }
    
    // Trouver le prochain joueur valide
    int32 OriginalIndex = CurrentPlayerIndex;
    bool FoundValidPlayer = false;

    do {
        // Passer au joueur suivant
        CurrentPlayerIndex = (CurrentPlayerIndex + 1) % AllPlayerControllers.Num();

        // Vérifier si ce joueur est valide (en vie)
        AController* Controller = AllPlayerControllers[CurrentPlayerIndex];
        AWormCharacter* Character = GetWormCharacterFromController(Controller);

        if (Character && Character->GetHealth() > 0)
        {
            FoundValidPlayer = true;
            break;
        }

        // Si on a fait un tour complet et qu'on n'a pas trouvé de joueur valide
        if (CurrentPlayerIndex == OriginalIndex)
        {
            UE_LOG(LogTemp, Warning, TEXT("No valid players found for next turn!"));
            return;
        }

    } while (!FoundValidPlayer);

    // Récupérer le controller actif
    AController* ActiveController = AllPlayerControllers[CurrentPlayerIndex];
    UE_LOG(LogTemp, Log, TEXT("Starting turn for player index %d: %s"),
        CurrentPlayerIndex, *ActiveController->GetName());

    // Assurez-vous d'utiliser le nom du pawn et non du controller
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

    // IMPORTANT: Utiliser la nouvelle fonction pour définir le joueur actif avec le bon nom
    if (WormGS)
    {
        WormGS->SetCurrentPlayerByIndex(CurrentPlayerIndex);
        WormGS->TurnDuration = TurnDuration;

        // Logging supplémentaire
        UE_LOG(LogTemp, Log, TEXT("Turn duration set to %.1f seconds, player name: %s"),
            TurnDuration, *WormGS->CurrentPlayerName);
    }

    // Désactiver tous les personnages
    for (AController* Controller : AllPlayerControllers)
    {
        AWormCharacter* Character = GetWormCharacterFromController(Controller);
        if (Character)
        {
            Character->SetIsMyTurn(false);
        }
    }

    // Activer le personnage du controller actif
    ActiveCharacter = GetWormCharacterFromController(ActiveController);
    if (ActiveCharacter)
    {
        ActiveCharacter->SetIsMyTurn(true);

        // Log pour vérifier que le personnage est bien activé
        UE_LOG(LogTemp, Log, TEXT("Activated character: %s (Is it local: %s)"),
            *ActiveCharacter->GetName(),
            ActiveController->IsLocalController() ? TEXT("Yes") : TEXT("No"));
    }

    // Appeler l'événement de début de tour
    OnTurnStarted(ActiveController);

    // Démarrer le timer pour ce tour
    GetWorldTimerManager().SetTimer(TurnTimerHandle, this, &AWormGameMode::OnTurnTimeExpired, TurnDuration, false);
}

void AWormGameMode::EndCurrentTurn()
{
    // Annuler le timer actuel
    GetWorldTimerManager().ClearTimer(TurnTimerHandle);
    
    // Récupérer le controller actif
    AController* ActiveController = nullptr;
    if (AllPlayerControllers.IsValidIndex(CurrentPlayerIndex))
    {
        ActiveController = AllPlayerControllers[CurrentPlayerIndex];
        
        // Désactiver le personnage actif
        AWormCharacter* ActiveCharacter = GetWormCharacterFromController(ActiveController);
        if (ActiveCharacter)
        {
            ActiveCharacter->SetIsMyTurn(false);
        }
    }
    
    // Appeler l'événement de fin de tour
    OnTurnEnded(ActiveController);
    
    // Démarrer le prochain tour après un délai
    GetWorldTimerManager().SetTimer(TurnTimerHandle, this, &AWormGameMode::StartNextTurn, 2.0f, false);
}

void AWormGameMode::OnTurnTimeExpired()
{
    // Le temps est écoulé, terminer le tour
    EndCurrentTurn();
}

bool AWormGameMode::CheckGameEndCondition()
{
    // Compter les équipes actives
    TSet<int32> ActiveTeams;
    int32 AlivePlayerCount = 0;
    
    for (AController* Controller : AllPlayerControllers)
    {
        AWormCharacter* Character = GetWormCharacterFromController(Controller);
        if (Character && Character->GetHealth() > 0)
        {
            AlivePlayerCount++;
            
            // Si vous avez un système d'équipes, vous pouvez ajouter:
            // int32 TeamID = Character->GetTeamID();
            // ActiveTeams.Add(TeamID);
        }
    }
    
    // Si un seul joueur reste (ou une seule équipe), la partie est terminée
    if (AlivePlayerCount <= 1)
    {
        // Gérer la fin de partie
        return true;
    }
    
    return false;
}

void AWormGameMode::OnTurnStarted_Implementation(AController* ActiveController)
{
    // Cette fonction peut être override en Blueprint
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
    // Cette fonction peut être override en Blueprint
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

void AWormGameMode::SpawnTestTerrain()
{
    // Vérifier que la classe de test est définie
    if (!TestTerrainClass)
    {
        UE_LOG(LogTemp, Error, TEXT("TestTerrainClass non défini dans GameMode!"));
        return;
    }
    
    UE_LOG(LogTemp, Warning, TEXT("Génération du terrain de test..."));

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    
    // Position et rotation au centre de la carte, mais légèrement décalé par rapport au terrain normal
    FVector Location = FVector(0.0f, 200.0f, 0.0f);  // 200 unités devant le terrain normal
    FRotator Rotation = FRotator::ZeroRotator;
    
    // Spawner l'acteur
    TestTerrain = GetWorld()->SpawnActor<ATestVisibleTerrain>(
        TestTerrainClass, 
        Location, 
        Rotation, 
        SpawnParams
    );
    
    if (TestTerrain)
    {
        UE_LOG(LogTemp, Warning, TEXT("Terrain de test généré avec succès: %s"), *TestTerrain->GetName());
        
        // Définir les dimensions
        TestTerrain->SetDimensions(2000.0f, 1000.0f, 100.0f);
        
        // Mettre à jour le GameState avec la référence
        AWormGameState* WormGS = GetGameState<AWormGameState>();
        if (WormGS)
        {
            WormGS->TestTerrain = TestTerrain;
            UE_LOG(LogTemp, Warning, TEXT("Référence au terrain de test stockée dans GameState"));
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Échec de la génération du terrain de test!"));
    }
}

void AWormGameMode::SpawnDestructibleTerrain()
{
    // Vérifier que la classe de terrain destructible est définie
    if (!DestructibleTerrainClass)
    {
        UE_LOG(LogTemp, Error, TEXT("DestructibleTerrainClass non défini dans GameMode!"));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("Génération du terrain destructible..."));

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    // Position et rotation au centre de la carte
    FVector Location = FVector(-1000.0f, -100.0f, -2250.0f);
    FRotator Rotation = FRotator::ZeroRotator;

    // Spawner l'acteur
    DestructibleTerrain = GetWorld()->SpawnActor<ADestructibleTerrain>(
        DestructibleTerrainClass,
        Location,
        Rotation,
        SpawnParams
    );

    if (DestructibleTerrain)
    {
        UE_LOG(LogTemp, Warning, TEXT("Terrain destructible généré avec succès: %s"), *DestructibleTerrain->GetName());

        // Mettre à jour le GameState avec la référence
        AWormGameState* WormGS = GetGameState<AWormGameState>();
        if (WormGS)
        {
            WormGS->DestructibleTerrain = DestructibleTerrain;
            UE_LOG(LogTemp, Warning, TEXT("Référence au terrain destructible stockée dans GameState"));
        }

        // Ajouter un appel forcé à la mise à jour visuelle après un délai
        FTimerHandle UpdateTimerHandle;
        GetWorld()->GetTimerManager().SetTimer(
            UpdateTimerHandle,
            [this]() { 
                if (DestructibleTerrain) {
                    DestructibleTerrain->Multicast_ForceVisualUpdate();
                }
            },
            2.0f,
            false
        );
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Échec de la génération du terrain destructible!"));
    }
}

void AWormGameMode::InitializeWeaponsForAllPlayers()
{
    // Vérifier qu'on a des armes définies
    if (AvailableWeaponTypes.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ No weapon types defined in GameMode!"));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("✅ Initializing weapons for all players (%d weapon types available)"), 
        AvailableWeaponTypes.Num());

    // Recollectons les controllers pour plus de sécurité
    GatherAllPlayerControllers();

    // Distribuer les armes à tous les personnages
    for (AController* Controller : AllPlayerControllers)
    {
        AWormCharacter* Character = GetWormCharacterFromController(Controller);
        if (Character)
        {
            UE_LOG(LogTemp, Warning, TEXT("Assigning weapons to %s"), *Character->GetName());
            Character->SetAvailableWeapons(AvailableWeaponTypes);
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("Could not get character for controller %s"), 
                Controller ? *Controller->GetName() : TEXT("NULL"));
        }
    }

    // Programmer une vérification retardée pour s'assurer que tous les personnages ont bien leurs armes
    FTimerHandle WeaponCheckTimer;
    GetWorld()->GetTimerManager().SetTimer(
        WeaponCheckTimer,
        [this]() {
            for (AController* Controller : AllPlayerControllers)
            {
                AWormCharacter* Character = GetWormCharacterFromController(Controller);
                if (Character && Character->CurrentWeapon == nullptr)
                {
                    UE_LOG(LogTemp, Warning, TEXT("Reassigning weapons to %s"), *Character->GetName());
                    Character->SetAvailableWeapons(AvailableWeaponTypes);
                }
            }
        },
        3.0f,
        false
    );
}