#include "EnvironmentalEventsManager.h"
#include "Kismet/GameplayStatics.h"
#include "AWormCharacter.h"
#include "AVoxelBuilding.h"
#include "EngineUtils.h"
#include "WormGameMode.h"
#include "WormGameState.h"
#include "Engine/World.h"
#include "Camera/PlayerCameraManager.h"
#include "Net/UnrealNetwork.h"
// MODIFICATION 1: Dans EnvironmentalEventsManager.cpp
// Modifier le constructeur pour initialiser le WaterSystem correctement
// Vers la ligne 14, mettre à jour:

AEnvironmentalEventsManager::AEnvironmentalEventsManager()
{
    PrimaryActorTick.bCanEverTick = true;
    
    // Create the WaterSystem component
    WaterSystem = CreateDefaultSubobject<UWaterSystem>(TEXT("WaterSystem"));
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    
    // S'assurer que le WaterSystem est attaché au Root pour être positionné correctement
    if (WaterSystem)
    {
        // Pas besoin d'attacher explicitement - CreateDefaultSubobject le fait déjà
        // Mais on définit les paramètres initiaux
        WaterSystem->SetIsReplicated(true);
    }
    
    // Initialize variables for water
    bIsWaterRisingActive = false;
    TimeUntilNextWaterEvent = InitialWaterDelay;
    TurnCounter = 0;
    TurnsUntilNextWaterRise = 1; // By default, water rises each turn
    
    // Make sure the actor replicates
    bReplicates = true;
    
    // Initialize other event systems
    bIsEarthquakeActive = false;
    TimeUntilNextEarthquake = InitialEarthquakeDelay;
    CurrentEarthquakeIntensity = 0.0f;
    CurrentEarthquakeLocation = FVector::ZeroVector;
    
    // Default event probabilities
    WaterEventProbability = 100.0f; // 100% chance at start
    EarthquakeEventProbability = 0.0f; // Disabled by default
    
    // Default enabled event types
    ActiveEventTypes = EEventType::Water; // Only water active by default
    
    // Initialize turn system
    TurnsUntilNextEvent = 1;
}



void AEnvironmentalEventsManager::BeginPlay()
{
    Super::BeginPlay();
    
    // Initialize the water system first
    if (HasAuthority()) // Only on server
    {
        InitializeWaterLevel();
        
        // Start events if enabled
        if (EnumHasAnyFlags(ActiveEventTypes, EEventType::Water) && bEnableWaterEvents)
        {
            // Start after initial delay
            GetWorldTimerManager().SetTimer(
                WaterEventTimerHandle,
                this,
                &AEnvironmentalEventsManager::StartWaterRising,
                InitialWaterDelay,
                false
            );
            
            UE_LOG(LogTemp, Log, TEXT("Water will begin rising in %.1f seconds"), InitialWaterDelay);
        }
        
        // Initialize earthquakes (disabled by default)
        if (EnumHasAnyFlags(ActiveEventTypes, EEventType::Earthquake) && bEnableEarthquakes)
        {
            GetWorldTimerManager().SetTimer(
                EarthquakeTimerHandle,
                this,
                &AEnvironmentalEventsManager::TriggerRandomEarthquake,
                InitialEarthquakeDelay,
                false
            );
            
            UE_LOG(LogTemp, Log, TEXT("Earthquakes are initialized"));
        }
        
        // Create event indicator in UI
        CreateEnvironmentalEventIndicators();
        
        // Configure next random event
        ChooseNextEvent();
    }
}

void AEnvironmentalEventsManager::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!HasAuthority()) 
    {
        return; // Les événements sont gérés uniquement sur le serveur
    }
    
    // Gestion de l'eau montante
    if (bIsWaterRisingActive && !bRiseAfterEachTurn && EnumHasAnyFlags(ActiveEventTypes, EEventType::Water))
    {
        // Compte à rebours jusqu'à la prochaine montée d'eau
        TimeUntilNextWaterEvent -= DeltaTime;
        
        if (TimeUntilNextWaterEvent <= 0.0f)
        {
            // Faire monter l'eau
            RaiseWater();
            
            // Réinitialiser le timer
            TimeUntilNextWaterEvent = WaterRiseInterval;
            
            // Accélérer la montée si activé
            if (bWaterRiseFaster)
            {
                WaterRiseInterval = FMath::Max(WaterRiseInterval * 0.9f, 10.0f);
            }
        }
        
        // Vérifier le niveau d'eau dangereux
        CheckForDangerousWaterLevel();
    }
    
    // Gestion des tremblements de terre
    if (bIsEarthquakeActive && EnumHasAnyFlags(ActiveEventTypes, EEventType::Earthquake))
    {
        // L'effet visuel est géré par la fonction ProcessEarthquake
        // et la fin est gérée par EndEarthquake via un timer
    }
    else if (bEnableEarthquakes && !bIsEarthquakeActive && EnumHasAnyFlags(ActiveEventTypes, EEventType::Earthquake))
    {
        // Compte à rebours jusqu'au prochain tremblement de terre
        TimeUntilNextEarthquake -= DeltaTime;
        
        if (TimeUntilNextEarthquake <= 0.0f)
        {
            // Déclencher un tremblement de terre aléatoire
            TriggerRandomEarthquake();
            
            // Réinitialiser le timer (avec variation aléatoire)
            TimeUntilNextEarthquake = EarthquakeInterval * GetRandomValueBetween(0.8f, 1.2f);
        }
    }
    
    // Mise à jour de l'UI
    UpdateUI();
}

void AEnvironmentalEventsManager::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    
    // Corriger la réplication de ActiveEventTypes
    DOREPLIFETIME(AEnvironmentalEventsManager, ActiveEventTypes);
    DOREPLIFETIME(AEnvironmentalEventsManager, bIsWaterRisingActive);
    DOREPLIFETIME(AEnvironmentalEventsManager, TimeUntilNextWaterEvent);
    DOREPLIFETIME(AEnvironmentalEventsManager, bIsEarthquakeActive);
    DOREPLIFETIME(AEnvironmentalEventsManager, CurrentEarthquakeIntensity);
    DOREPLIFETIME(AEnvironmentalEventsManager, CurrentEarthquakeLocation);
}

// ===== FONCTIONS DE GESTION DES ÉVÉNEMENTS =====

void AEnvironmentalEventsManager::TriggerRandomEvent()
{
    if (!bEnableRandomEvents || !HasAuthority())
    {
        return;
    }
    
    // Sélectionner un type d'événement aléatoire parmi ceux activés
    EEventType SelectedEventType = SelectRandomEventType();
    
    switch (SelectedEventType)
    {
        case EEventType::Water:
            RaiseWater(WaterRisePerTurn * GetRandomValueBetween(0.8f, 1.2f));
            break;
            
        case EEventType::Earthquake:
            TriggerRandomEarthquake();
            break;
            
        default:
            UE_LOG(LogTemp, Warning, TEXT("Aucun événement aléatoire disponible ou sélectionné"));
            break;
    }
    
    // Préparer le prochain événement
    ChooseNextEvent();
}

void AEnvironmentalEventsManager::SetActiveEventTypes(EEventType NewActiveTypes)
{
    ActiveEventTypes = NewActiveTypes;
    
    UE_LOG(LogTemp, Log, TEXT("Types d'événements actifs mis à jour: %d"), static_cast<int32>(ActiveEventTypes));
    
    // Réinitialiser les événements en fonction des nouveaux types actifs
    if (HasAuthority())
    {
        if (EnumHasAnyFlags(ActiveEventTypes, EEventType::Water) && !bIsWaterRisingActive && bEnableWaterEvents)
        {
            StartWaterRising();
        }
        else if (!EnumHasAnyFlags(ActiveEventTypes, EEventType::Water) && bIsWaterRisingActive)
        {
            StopWaterRising();
        }
        
        if (EnumHasAnyFlags(ActiveEventTypes, EEventType::Earthquake) && !GetWorldTimerManager().IsTimerActive(EarthquakeTimerHandle) && bEnableEarthquakes)
        {
            GetWorldTimerManager().SetTimer(
                EarthquakeTimerHandle,
                this,
                &AEnvironmentalEventsManager::TriggerRandomEarthquake,
                GetRandomValueBetween(10.0f, 30.0f),
                false
            );
        }
        else if (!EnumHasAnyFlags(ActiveEventTypes, EEventType::Earthquake))
        {
            GetWorldTimerManager().ClearTimer(EarthquakeTimerHandle);
            if (bIsEarthquakeActive)
            {
                EndEarthquake();
            }
        }
    }
}

// ===== FONCTIONS DE L'EAU =====
void AEnvironmentalEventsManager::StartWaterRising()
{
    if (!WaterSystem || !EnumHasAnyFlags(ActiveEventTypes, EEventType::Water) || !bEnableWaterEvents)
    {
        return;
    }
    
    bIsWaterRisingActive = true;
    
    if (bRiseAfterEachTurn)
    {
        UE_LOG(LogTemp, Log, TEXT("Water will rise after each turn"));
    }
    else
    {
        // Set up timer for periodic rising
        TimeUntilNextWaterEvent = WaterRiseInterval;
        UE_LOG(LogTemp, Log, TEXT("Water will rise every %.1f seconds"), WaterRiseInterval);
    }
    
    // Blueprint notification
    OnWaterStartsRising();
}

void AEnvironmentalEventsManager::StopWaterRising()
{
    bIsWaterRisingActive = false;
    
    UE_LOG(LogTemp, Log, TEXT("Montée de l'eau arrêtée"));
}

void AEnvironmentalEventsManager::RaiseWater(float Amount)
{
    if (!WaterSystem || !EnumHasAnyFlags(ActiveEventTypes, EEventType::Water))
    {
        return;
    }
    
    // If no amount specified, use the amount per turn
    if (Amount <= 0.0f)
    {
        Amount = WaterRisePerTurn;
    }
    
    // Make the water rise (not lower)
    WaterSystem->SetWaterLevel(WaterSystem->GetCurrentWaterLevel() + Amount, false);
    
    // Create an event for notification
    FEnvironmentalEventData EventData;
    EventData.EventType = EEventType::Water;
    EventData.Intensity = Amount / WaterRisePerTurn; // Normalize intensity
    EventData.Location = GetActorLocation();
    EventData.Description = FString::Printf(TEXT("Water rose by %.1f units"), Amount);
    EventData.Duration = 0.0f; // Instantaneous
    
    // Blueprint notification
    OnEventTriggered(EventData);
    
    UE_LOG(LogTemp, Log, TEXT("Water rose by %.1f units. Current level: %.1f"), Amount, WaterSystem->GetCurrentWaterLevel());
    
    // Check if any characters are in danger
    CheckForDangerousWaterLevel();
}

// ===== FONCTIONS DES TREMBLEMENTS DE TERRE =====

void AEnvironmentalEventsManager::TriggerEarthquake(float Intensity, FVector Location)
{
    if (!EnumHasAnyFlags(ActiveEventTypes, EEventType::Earthquake) || !bEnableEarthquakes || bIsEarthquakeActive)
    {
        return;
    }
    
    // Limiter l'intensité à la plage configurée
    Intensity = FMath::Clamp(Intensity, EarthquakeIntensityMin, EarthquakeIntensityMax);
    
    // Si aucune position spécifiée, utiliser le centre de la carte
    if (Location.IsZero())
    {
        TArray<AActor*> Buildings;
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), AImprovedVoxelBuilding::StaticClass(), Buildings);
        
        // Si des bâtiments existent, choisir un bâtiment aléatoire comme épicentre
        if (Buildings.Num() > 0)
        {
            int32 RandomIndex = FMath::RandRange(0, Buildings.Num() - 1);
            Location = Buildings[RandomIndex]->GetActorLocation();
        }
        else
        {
            // Sinon, utiliser la position du gestionnaire
            Location = GetActorLocation();
        }
    }
    
    // Stocker les données du tremblement de terre
    bIsEarthquakeActive = true;
    CurrentEarthquakeIntensity = Intensity;
    CurrentEarthquakeLocation = Location;
    
    // Traiter le tremblement de terre
    ProcessEarthquake(Intensity, Location);
    
    // Définir un timer pour la fin du tremblement de terre
    GetWorldTimerManager().SetTimer(
        EarthquakeDurationTimerHandle,
        this,
        &AEnvironmentalEventsManager::EndEarthquake,
        EarthquakeDuration,
        false
    );
    
    // Créer un événement pour la notification
    FEnvironmentalEventData EventData;
    EventData.EventType = EEventType::Earthquake;
    EventData.Intensity = Intensity;
    EventData.Location = Location;
    EventData.Description = FString::Printf(TEXT("Tremblement de terre d'intensité %.1f"), Intensity);
    EventData.Duration = EarthquakeDuration;
    
    // Notification Blueprint
    OnEventTriggered(EventData);
    OnEarthquakeStart(Intensity, Location);
    
    UE_LOG(LogTemp, Warning, TEXT("Tremblement de terre déclenché: Intensité=%.1f, Position=%s"), 
        Intensity, *Location.ToString());
}

void AEnvironmentalEventsManager::TriggerRandomEarthquake()
{
    // Vérifier si les tremblements de terre sont actifs
    if (!EnumHasAnyFlags(ActiveEventTypes, EEventType::Earthquake) || !bEnableEarthquakes || bIsEarthquakeActive)
    {
        return;
    }
    
    // Vérifier la probabilité
    if (!ShouldTriggerEventWithProbability(EarthquakeEventProbability))
    {
        // Programmer un autre essai plus tard
        GetWorldTimerManager().SetTimer(
            EarthquakeTimerHandle,
            this,
            &AEnvironmentalEventsManager::TriggerRandomEarthquake,
            GetRandomValueBetween(30.0f, 60.0f),
            false
        );
        return;
    }
    
    // Générer une intensité aléatoire
    float RandomIntensity = GetRandomValueBetween(EarthquakeIntensityMin, EarthquakeIntensityMax);
    
    // Trouver une position aléatoire
    FVector RandomLocation = FVector::ZeroVector;
    TArray<AActor*> Buildings;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AImprovedVoxelBuilding::StaticClass(), Buildings);
    
    if (Buildings.Num() > 0)
    {
        int32 RandomIndex = FMath::RandRange(0, Buildings.Num() - 1);
        RandomLocation = Buildings[RandomIndex]->GetActorLocation();
    }
    
    // Déclencher le tremblement de terre
    TriggerEarthquake(RandomIntensity, RandomLocation);
}

void AEnvironmentalEventsManager::ProcessEarthquake(float Intensity, FVector Location)
{
    if (!bIsEarthquakeActive)
    {
        return;
    }
    
    // Appliquer les effets aux bâtiments
    ApplyEarthquakeEffectToBuildings(Intensity, Location);
    
    // Secouer la caméra
    ShakeCamera(Intensity);
    
    // Jouer un son
    if (EarthquakeSound)
    {
        UGameplayStatics::PlaySoundAtLocation(
            GetWorld(),
            EarthquakeSound,
            Location,
            Intensity * 0.5f // Volume proportionnel à l'intensité
        );
    }
    
    // Afficher un effet de particules
    if (EarthquakeParticleEffect)
    {
        UGameplayStatics::SpawnEmitterAtLocation(
            GetWorld(),
            EarthquakeParticleEffect,
            Location,
            FRotator::ZeroRotator,
            FVector(Intensity), // Échelle proportionnelle à l'intensité
            true
        );
    }
}

void AEnvironmentalEventsManager::EndEarthquake()
{
    if (!bIsEarthquakeActive)
    {
        return;
    }
    
    bIsEarthquakeActive = false;
    
    // Notification Blueprint
    OnEarthquakeEnd();
    
    UE_LOG(LogTemp, Log, TEXT("Fin du tremblement de terre"));
    
    // Planifier le prochain tremblement de terre
    if (bEnableEarthquakes && EnumHasAnyFlags(ActiveEventTypes, EEventType::Earthquake))
    {
        TimeUntilNextEarthquake = EarthquakeInterval * GetRandomValueBetween(0.8f, 1.2f);
    }
}

void AEnvironmentalEventsManager::ApplyEarthquakeEffectToBuildings(float Intensity, FVector Location)
{
    // Trouver tous les bâtiments
    TArray<AActor*> Buildings;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AImprovedVoxelBuilding::StaticClass(), Buildings);
    
    for (AActor* Actor : Buildings)
    {
        AImprovedVoxelBuilding* Building = Cast<AImprovedVoxelBuilding>(Actor);
        if (!Building)
        {
            continue;
        }
        
        // Calculer la distance à l'épicentre
        float Distance = FVector::Dist(Building->GetActorLocation(), Location);
        
        // L'intensité diminue avec la distance
        float EffectRadius = 5000.0f; // Rayon d'effet en unités
        if (Distance > EffectRadius)
        {
            continue; // En dehors du rayon d'effet
        }
        
        // Calculer l'intensité diminuée par la distance
        float DistanceFactor = 1.0f - (Distance / EffectRadius);
        float EffectiveIntensity = Intensity * DistanceFactor;
        
        if (EffectiveIntensity < 0.1f)
        {
            continue; // Intensité trop faible pour avoir un effet
        }
        
        // Pour ce prototype, nous allons simplement déplacer le bâtiment vers le bas
        // Dans une implémentation complète, vous pourriez ajouter des effets de destruction
        // à votre classe AImprovedVoxelBuilding
        
        // Déplacer le bâtiment vers le bas
        FVector NewLocation = Building->GetActorLocation();
        
        // L'intensité détermine combien le bâtiment s'enfonce
        float SinkAmount = EffectiveIntensity * 20.0f; // 20 unités par point d'intensité
        
        // Ajouter un peu de mouvement aléatoire horizontal
        NewLocation.Z -= SinkAmount;
        NewLocation.X += GetRandomValueBetween(-SinkAmount, SinkAmount);
        NewLocation.Y += GetRandomValueBetween(-SinkAmount, SinkAmount);
        
        // Appliquer la nouvelle position
        Building->SetActorLocation(NewLocation);
        
        // Log pour débogage
        UE_LOG(LogTemp, Verbose, TEXT("Bâtiment %s affecté par le séisme: Intensité=%.2f, Déplacement=%.1f"),
            *Building->GetName(), EffectiveIntensity, SinkAmount);
    }
}

void AEnvironmentalEventsManager::ShakeCamera(float Intensity)
{
    // Secouer la caméra de tous les joueurs
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        if (PC && PC->PlayerCameraManager)
        {
            // Durée et intensité proportionnelles à l'intensité du séisme
            float ShakeDuration = FMath::Min(EarthquakeDuration, 5.0f);
            
            // Lancer un camera shake
            
            UE_LOG(LogTemp, Verbose, TEXT("Camera shake appliqué au joueur: Intensité=%.2f, Durée=%.1f"),
                Intensity, ShakeDuration);
        }
    }
}

// ===== FONCTIONS DE NOTIFICATION =====
void AEnvironmentalEventsManager::NotifyTurnEnded_Implementation()
{
    UE_LOG(LogTemp, Log, TEXT("EnvironmentalEventsManager: Turn ended notification received"));
    
    // If water is active and should rise after each turn
    if (bIsWaterRisingActive && bRiseAfterEachTurn && EnumHasAnyFlags(ActiveEventTypes, EEventType::Water))
    {
        RaiseWater();
    }
    
    // Update turn counter
    TurnCounter++;
    
    // Check if it's time for a random event
    if (bEnableRandomEvents && TurnCounter >= TurnsUntilNextEvent)
    {
        TriggerRandomEvent();
        TurnCounter = 0;
    }
}

void AEnvironmentalEventsManager::InitializeWaterLevel()
{
    if (!WaterSystem)
    {
        UE_LOG(LogTemp, Error, TEXT("WaterSystem component is null in EnvironmentalEventsManager"));
        return;
    }
    
    // Find the average terrain height
    float AverageHeight = CalculateAverageTerrainHeight();
    
    // Set initial water level well below terrain
    float InitialLevel = AverageHeight - 500.0f; // 500 units below terrain
    
    // Limit to minimum configured level
    InitialLevel = FMath::Max(InitialLevel, WaterSystem->MinWaterLevel);
    
    // Initialize WaterSystem
    WaterSystem->SetWaterLevel(InitialLevel, true);
    
    // Configurer les limites d'eau basées sur le terrain
    WaterSystem->MinWaterLevel = InitialLevel;
   // WaterSystem->MaxWaterLevel = AverageHeight + 100.0f; // Légèrement au-dessus du terrain moyen
    WaterSystem->MaxWaterLevel = 1000; // Légèrement au-dessus du terrain moyen
    
    UE_LOG(LogTemp, Log, TEXT("Water level initialized to %.1f (terrain at %.1f)"), 
        InitialLevel, AverageHeight);
}

void AEnvironmentalEventsManager::CheckForDangerousWaterLevel()
{
    if (!WaterSystem || !EnumHasAnyFlags(ActiveEventTypes, EEventType::Water))
    {
        return;
    }
    
    float CurrentWaterLevel = WaterSystem->GetCurrentWaterLevel();
    
    // Trouver tous les personnages
    TArray<AActor*> Characters;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AWormCharacter::StaticClass(), Characters);
    
    // Vérifier la distance entre chaque personnage et l'eau
    for (AActor* Actor : Characters)
    {
        float HeightAboveWater = Actor->GetActorLocation().Z - CurrentWaterLevel;
        
        // Si un personnage est proche du niveau d'eau et que l'eau monte
        if (bIsWaterRisingActive && HeightAboveWater < WaterSystem->DangerousWaterLevelThreshold && HeightAboveWater > 0)
        {
            // Déclencher l'événement Blueprint
            OnWaterRisesTooHigh();
            
            UE_LOG(LogTemp, Warning, TEXT("Niveau d'eau dangereux: Le personnage %s est à %.1f unités au-dessus de l'eau"), 
                *Actor->GetName(), HeightAboveWater);
            
            // Sortir après le premier avertissement pour ne pas spammer
            break;
        }
    }
}

void AEnvironmentalEventsManager::UpdateUI()
{
    // Cette fonction peut être utilisée pour mettre à jour l'UI
    // Elle est appelée à chaque tick
}

void AEnvironmentalEventsManager::ChooseNextEvent()
{
    if (!bEnableRandomEvents)
    {
        return;
    }
    
    // Déterminer le nombre de tours avant le prochain événement
    TurnsUntilNextEvent = FMath::RandRange(MinTurnsBetweenEvents, MaxTurnsBetweenEvents);
    
    UE_LOG(LogTemp, Log, TEXT("Prochain événement aléatoire dans %d tours"), TurnsUntilNextEvent);
}

EEventType AEnvironmentalEventsManager::SelectRandomEventType()
{
    // Tableau des types d'événements et leurs probabilités
    TArray<TPair<EEventType, float>> EventProbabilities;
    
    if (EnumHasAnyFlags(ActiveEventTypes, EEventType::Water) && bEnableWaterEvents)
    {
        EventProbabilities.Add(TPair<EEventType, float>(EEventType::Water, WaterEventProbability));
    }
    
    if (EnumHasAnyFlags(ActiveEventTypes, EEventType::Earthquake) && bEnableEarthquakes)
    {
        EventProbabilities.Add(TPair<EEventType, float>(EEventType::Earthquake, EarthquakeEventProbability));
    }
    
    if (EventProbabilities.Num() == 0)
    {
        return EEventType::None;
    }
    
    // Calculer la somme totale des probabilités
    float TotalProbability = 0.0f;
    for (const TPair<EEventType, float>& Pair : EventProbabilities)
    {
        TotalProbability += Pair.Value;
    }
    
    // Si la somme est 0, retourner un élément aléatoire
    if (TotalProbability <= 0.0f)
    {
        int32 RandomIndex = FMath::RandRange(0, EventProbabilities.Num() - 1);
        return EventProbabilities[RandomIndex].Key;
    }
    
    // Sélection pondérée par probabilité
    float RandomValue = GetRandomValueBetween(0.0f, TotalProbability);
    float CumulativeProbability = 0.0f;
    
    for (const TPair<EEventType, float>& Pair : EventProbabilities)
    {
        CumulativeProbability += Pair.Value;
        if (RandomValue <= CumulativeProbability)
        {
            return Pair.Key;
        }
    }
    
    // Par défaut, retourner le premier type
    return EventProbabilities[0].Key;
}

float AEnvironmentalEventsManager::CalculateAverageTerrainHeight() const
{
    // Trouver tous les bâtiments
    TArray<AActor*> Buildings;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AImprovedVoxelBuilding::StaticClass(), Buildings);
    
    if (Buildings.Num() == 0)
    {
        // Si aucun bâtiment, utiliser une valeur par défaut
        return 0.0f;
    }
    
    // Calculer la hauteur moyenne
    float TotalHeight = 0.0f;
    int32 BuildingCount = 0;
    
    for (AActor* Actor : Buildings)
    {
        AImprovedVoxelBuilding* Building = Cast<AImprovedVoxelBuilding>(Actor);
        if (Building)
        {
            // Utiliser la hauteur du bâtiment (Z + hauteur)
            float BuildingHeight = Building->GetActorLocation().Z + (Building->GridSizeZ * Building->VoxelSize);
            TotalHeight += BuildingHeight;
            BuildingCount++;
        }
    }
    
    if (BuildingCount == 0)
    {
        return 0.0f;
    }
    
    return TotalHeight / BuildingCount;
}

float AEnvironmentalEventsManager::GetRandomValueBetween(float Min, float Max) const
{
    return Min + (Max - Min) * FMath::FRand();
}

bool AEnvironmentalEventsManager::ShouldTriggerEventWithProbability(float Probability) const
{
    // Normaliser la probabilité entre 0 et 100
    Probability = FMath::Clamp(Probability, 0.0f, 100.0f);
    
    // Convertir en pourcentage (0-1)
    float NormalizedProbability = Probability / 100.0f;
    
    // Générer un nombre aléatoire et comparer
    float RandomValue = FMath::FRand();
    
    return RandomValue <= NormalizedProbability;
};

void AEnvironmentalEventsManager::LowerWater(float Amount)
{
    if (!WaterSystem || !EnumHasAnyFlags(ActiveEventTypes, EEventType::Water))
    {
        return;
    }
    
    // Si aucun montant spécifié, utiliser le montant par tour
    if (Amount <= 0.0f)
    {
        Amount = WaterRisePerTurn;
    }
    
    // Faire descendre l'eau
    WaterSystem->LowerWaterLevel(Amount);
    
    // Créer un événement pour la notification
    FEnvironmentalEventData EventData;
    EventData.EventType = EEventType::Water;
    EventData.Intensity = Amount / WaterRisePerTurn; // Normaliser l'intensité
    EventData.Location = GetActorLocation();
    EventData.Description = FString::Printf(TEXT("L'eau monte de %.1f unités"), Amount);
    EventData.Duration = 0.0f; // Instantané
    
    // Notification Blueprint
    OnEventTriggered(EventData);
    
    UE_LOG(LogTemp, Log, TEXT("L'eau a monté de %.1f unités"), Amount);
    
    // Vérifier si des personnages sont en danger
    CheckForDangerousWaterLevel();
}


AEnvironmentalEventsManager* AEnvironmentalEventsManager::GetEventsManager(const UObject* WorldContextObject)
{
    if (!WorldContextObject)
    {
        return nullptr;
    }

    UWorld* World = WorldContextObject->GetWorld();
    if (!World)
    {
        return nullptr;
    }

    for (TActorIterator<AEnvironmentalEventsManager> It(World); It; ++It)
    {
        return *It;
    }

    return nullptr;
}

