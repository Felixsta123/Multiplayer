#include "WaterSystem.h"
#include "Kismet/GameplayStatics.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundCue.h"
#include "Particles/ParticleSystemComponent.h"
#include "Worms_3d/AWormCharacter.h"
#include "EnvironmentalEventsManager.h"
#include "WormGameState.h"
#include "WormGameMode.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Worms_3d/TUTO/WormTutorialGameMode.h"

UWaterSystem::UWaterSystem()
{
    PrimaryComponentTick.bCanEverTick = true;

    // Initialiser les variables d'état
    CurrentWaterLevel = InitialWaterLevel;
    TargetWaterLevel = InitialWaterLevel;
    bIsWaterRising = false;
    bAutomaticCycleActive = false;
    CycleTimer = 0.0f;
    CycleDuration = 300.0f; // 5 minutes par défaut
}

void UWaterSystem::BeginPlay()
{
    Super::BeginPlay();
    
    // S'assurer que le propriétaire est valide
    if (!GetOwner())
    {
        UE_LOG(LogTemp, Error, TEXT("WaterSystem owner is null"));
        return;
    }
    
    // Créer le mesh d'eau s'il n'existe pas encore
    if (!WaterMeshComponent)
    {
        WaterMeshComponent = NewObject<UStaticMeshComponent>(GetOwner(), TEXT("WaterMesh"));
        if (WaterMeshComponent)
        {
            WaterMeshComponent->SetupAttachment(GetOwner()->GetRootComponent());
            WaterMeshComponent->RegisterComponent();
            
            if (WaterMesh)
            {
                WaterMeshComponent->SetStaticMesh(WaterMesh);
            }
            else 
            {
                UE_LOG(LogTemp, Warning, TEXT("WaterSystem: No water mesh assigned, using default plane"));
                // Vous pourriez charger un mesh de plan par défaut ici
            }
            
            // Configurer la physique et les collisions
            WaterMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision); // L'eau n'a pas de collisions physiques
            WaterMeshComponent->SetGenerateOverlapEvents(false);
            WaterMeshComponent->SetCastShadow(false);
            
            // Rendre l'eau semi-transparente
            WaterMeshComponent->SetTranslucentSortPriority(0);
            
            // Créer et appliquer le matériau dynamique
            if (WaterMaterial)
            {
                WaterDynamicMaterial = UMaterialInstanceDynamic::Create(WaterMaterial, this);
                if (WaterDynamicMaterial)
                {
                    WaterDynamicMaterial->SetVectorParameterValue(TEXT("WaterColor"), WaterColor);
                    WaterMeshComponent->SetMaterial(0, WaterDynamicMaterial);
                }
            }
        }
    }
    
    // Créer le volume de détection s'il n'existe pas encore
    if (!WaterVolumeComponent)
    {
        WaterVolumeComponent = NewObject<UBoxComponent>(GetOwner(), TEXT("WaterVolume"));
        if (WaterVolumeComponent)
        {
            WaterVolumeComponent->SetupAttachment(GetOwner()->GetRootComponent());
            WaterVolumeComponent->RegisterComponent();
            
            // Configurer la détection de collision
            WaterVolumeComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
            WaterVolumeComponent->SetCollisionResponseToAllChannels(ECR_Overlap);
            WaterVolumeComponent->SetGenerateOverlapEvents(true);
            
            // Lier le callback d'overlap
            WaterVolumeComponent->OnComponentBeginOverlap.AddDynamic(this, &UWaterSystem::OnWaterOverlap);
        }
    }
    
    // Configurer le son d'ambiance
    if (WaterAmbientSound && !WaterAmbientSoundComponent)
    {
        WaterAmbientSoundComponent = UGameplayStatics::SpawnSoundAttached(
            WaterAmbientSound,
            WaterMeshComponent ? WaterMeshComponent : GetOwner()->GetRootComponent(),
            NAME_None,
            FVector::ZeroVector,
            EAttachLocation::KeepRelativeOffset,
            true
        );
    }
    
    // Initialiser le niveau d'eau
    CurrentWaterLevel = InitialWaterLevel;
    TargetWaterLevel = InitialWaterLevel;
    UpdateWaterMeshAndVolume();
    
    // Add persistent effects
    AddPersistentWaterEffects();
    
    // Start automatic cycle if needed
    if (bAutomaticCycleActive)
    {
        StartAutomaticCycle(CycleDuration);
    }
    
    UE_LOG(LogTemp, Log, TEXT("WaterSystem initialized at level %.1f"), CurrentWaterLevel);
    
}

void UWaterSystem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    
    DOREPLIFETIME(UWaterSystem, CurrentWaterLevel);
    DOREPLIFETIME(UWaterSystem, TargetWaterLevel);
    DOREPLIFETIME(UWaterSystem, bIsWaterRising);

}

void UWaterSystem::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Mettre à jour le niveau d'eau en fonction de la cible
    if (CurrentWaterLevel != TargetWaterLevel)
    {
        float SpeedToUse = bIsWaterRising ? WaterRiseSpeed : WaterLowerSpeed;
        
        // Calculer le mouvement pour ce frame
        float Movement = SpeedToUse * DeltaTime;
        
        if (bIsWaterRising)
        {
            CurrentWaterLevel = FMath::Min(CurrentWaterLevel + Movement, TargetWaterLevel);
        }
        else
        {
            CurrentWaterLevel = FMath::Max(CurrentWaterLevel - Movement, TargetWaterLevel);
        }
        
        // Mettre à jour la position du mesh et du volume
        UpdateWaterMeshAndVolume();
        
        // Vérifier si on est proche d'un niveau dangereux
        CheckForDangerousWaterLevel();
    }
    
    // Gérer le cycle automatique
    if (bAutomaticCycleActive)
    {
        HandleCycleUpdate(DeltaTime);
    }
}
void UWaterSystem::SetWaterLevel(float NewLevel, bool bImmediate)
{
    if (!GetOwner() || !GetOwner()->HasAuthority())
    {
        Server_SetWaterLevel(NewLevel, bImmediate);
        return;
    }

    NewLevel = FMath::Clamp(NewLevel, MinWaterLevel, MaxWaterLevel);
    bIsWaterRising = (NewLevel > CurrentWaterLevel);
    TargetWaterLevel = NewLevel;

    if (bImmediate)
    {
        CurrentWaterLevel = TargetWaterLevel;
        UpdateWaterMeshAndVolume();
    }

    // Force replication
    MARK_PROPERTY_DIRTY_FROM_NAME(UWaterSystem, CurrentWaterLevel, this);
    MARK_PROPERTY_DIRTY_FROM_NAME(UWaterSystem, TargetWaterLevel, this);

    // Notify all clients
    Multicast_SetWaterLevel(NewLevel, bImmediate);
}

void UWaterSystem::Server_SetWaterLevel_Implementation(float NewLevel, bool bImmediate)
{
    SetWaterLevel(NewLevel, bImmediate);
}

void UWaterSystem::Multicast_SetWaterLevel_Implementation(float NewLevel, bool bImmediate)
{
    if (!GetOwner()->HasAuthority())
    {
        // Update target level
        bIsWaterRising = (NewLevel > CurrentWaterLevel);
        TargetWaterLevel = NewLevel;
        
        // If immediate, update current level too
        if (bImmediate)
        {
            CurrentWaterLevel = NewLevel;
            UpdateWaterMeshAndVolume();
        }
        // Otherwise, let Tick component handle the smooth animation
        // No need to force update here, as tick will handle it
    }
}




void UWaterSystem::RaiseWaterLevel(float AmountToRaise)
{
    SetWaterLevel(CurrentWaterLevel + AmountToRaise);
}

void UWaterSystem::LowerWaterLevel(float AmountToLower)
{
    SetWaterLevel(CurrentWaterLevel - AmountToLower);
}

void UWaterSystem::StartAutomaticCycle(float NewCycleDuration)
{
    bAutomaticCycleActive = true;
    CycleDuration = NewCycleDuration;
    CycleTimer = 0.0f;
    
    // Démarrer en montant depuis le niveau le plus bas
    SetWaterLevel(MinWaterLevel, true);
    RaiseWaterLevel(MaxWaterLevel - MinWaterLevel);
    
    UE_LOG(LogTemp, Log, TEXT("Automatic water cycle started: %.1f seconds per cycle"), CycleDuration);
}

void UWaterSystem::StopAutomaticCycle()
{
    bAutomaticCycleActive = false;
    
    // Arrêter au niveau actuel
    SetWaterLevel(CurrentWaterLevel);
    
    UE_LOG(LogTemp, Log, TEXT("Automatic water cycle stopped at level %.1f"), CurrentWaterLevel);
}

void UWaterSystem::OnWaterOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, 
                                  UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, 
                                  bool bFromSweep, const FHitResult& SweepResult)
{
    // Vérifier si c'est un personnage
    //check du game mode si on est en tuto ou non


    
    AWormCharacter* Character = Cast<AWormCharacter>(OtherActor);
    if (Character)
    {
        AWormTutorialGameMode* GameMode = Cast<AWormTutorialGameMode>(GetWorld()->GetAuthGameMode());
        if (GameMode)
        {
            // Si on est en tuto, on utilise la fonction de respawn spécifique
            GameMode->RespawnPlayerFromWater();
            return;
        } 
        // Jouer un effet de splash
        SpawnWaterSplashEffect(Character->GetActorLocation());
        
        // Tuer le personnage   
        KillCharacterInWater(Character);
        
        UE_LOG(LogTemp, Warning, TEXT("Character %s entered water and died"), *Character->GetName());
    }
    else
    {
        // Pour les autres acteurs, juste créer un splash
        SpawnWaterSplashEffect(OtherActor->GetActorLocation());
        
        UE_LOG(LogTemp, Verbose, TEXT("Actor %s entered water"), *OtherActor->GetName());
    }
}


void UWaterSystem::UpdateWaterMeshAndVolume()
{
    if (!WaterMeshComponent || !WaterVolumeComponent || !GetOwner())
    {
        return;
    }
    
    // Taille du monde pour le mesh d'eau
    const float WorldSize = 20000.0f; // Ajuster selon la taille de votre niveau
    
    // Mise à jour de la position du mesh d'eau (plan XY)
    FVector MeshLocation = GetOwner()->GetActorLocation();
    MeshLocation.Z = CurrentWaterLevel;
    WaterMeshComponent->SetWorldLocation(MeshLocation);
    
    // Mise à l'échelle pour couvrir toute la carte
    WaterMeshComponent->SetWorldScale3D(FVector(WorldSize / 100.0f, WorldSize / 100.0f, 1.0f)); // Ajuster selon la taille de votre mesh
    
    // Mise à jour du volume de détection (plus grand que la partie visible)
    FVector VolumeOrigin = MeshLocation;
    VolumeOrigin.Z = CurrentWaterLevel - 100.0f; // Légèrement sous la surface pour détecter correctement
    
    WaterVolumeComponent->SetWorldLocation(VolumeOrigin);
    WaterVolumeComponent->SetBoxExtent(FVector(WorldSize, WorldSize, 200.0f)); // Hauteur de détection plus grande
    
    // Mettre à jour les effets visuels de l'eau
    if (WaterDynamicMaterial)
    {
        // Ajouter du temps pour l'animation des vagues
        float Time = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
        WaterDynamicMaterial->SetScalarParameterValue(TEXT("Time"), Time);
        
        // Mettre à jour la couleur en fonction de la profondeur
        float DepthFactor = FMath::GetMappedRangeValueClamped(
            FVector2D(MinWaterLevel, MaxWaterLevel),
            FVector2D(0.2f, 1.0f),
            CurrentWaterLevel
        );
        
        FLinearColor AdjustedColor = WaterColor;
        AdjustedColor.A = FMath::Clamp(WaterColor.A * DepthFactor, 0.5f, 0.9f);
        
        WaterDynamicMaterial->SetVectorParameterValue(TEXT("WaterColor"), AdjustedColor);
    }
}


void UWaterSystem::ApplyUnderwaterEffects(AActor* Actor, bool bIsUnderwater)
{
    // Cette fonction applique des effets visuels/sonores aux personnages sous l'eau
    // À implémenter si vous voulez des effets spéciaux sous-marins
}

void UWaterSystem::SpawnWaterSplashEffect(const FVector& Location)
{
    if (SplashEffect)
    {
        // Créer l'effet à la position donnée, légèrement au-dessus du niveau d'eau
        FVector SplashLocation = Location;
        SplashLocation.Z = CurrentWaterLevel + 10.0f;
        
        UGameplayStatics::SpawnEmitterAtLocation(
            GetWorld(),
            SplashEffect,
            SplashLocation,
            FRotator::ZeroRotator,
            true
        );
    }
    
    // Jouer un son de splash
    if (WaterKillSound)
    {
        UGameplayStatics::PlaySoundAtLocation(
            this,
            WaterKillSound,
            Location
        );
    }
}

void UWaterSystem::CheckForDangerousWaterLevel()
{
    // Trouver tous les personnages
    TArray<AActor*> Characters;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AWormCharacter::StaticClass(), Characters);
    
    // Vérifier la distance entre chaque personnage et l'eau
    for (AActor* Actor : Characters)
    {
        float HeightAboveWater = Actor->GetActorLocation().Z - CurrentWaterLevel;
        
        // Si un personnage est proche du niveau d'eau et que l'eau monte
        if (bIsWaterRising && HeightAboveWater < DangerousWaterLevelThreshold && HeightAboveWater > 0)
        {
            // Jouer le son d'avertissement
            if (WaterWarningSound)
            {
                UGameplayStatics::PlaySound2D(this, WaterWarningSound);
            }
            
            // Déclencher l'événement Blueprint
            OnDangerousWaterLevel();
            
            UE_LOG(LogTemp, Warning, TEXT("Dangerous water level warning: Character %s is %.1f units above water"), 
                *Actor->GetName(), HeightAboveWater);
            
            // Sortir après le premier avertissement pour ne pas spammer
            break;
        }
    }
}

void UWaterSystem::HandleCycleUpdate(float DeltaTime)
{
    // Mettre à jour le timer de cycle
    CycleTimer += DeltaTime;
    
    // Si on a atteint la durée du cycle, recommencer
    if (CycleTimer >= CycleDuration)
    {
        CycleTimer = 0.0f;
        
        // Inverser la direction de l'eau
        if (bIsWaterRising)
        {
            SetWaterLevel(MinWaterLevel);
        }
        else
        {
            SetWaterLevel(MaxWaterLevel);
        }
        
        UE_LOG(LogTemp, Log, TEXT("Water cycle restarting, new direction: %s"), 
            bIsWaterRising ? TEXT("rising") : TEXT("lowering"));
    }
    else if (CurrentWaterLevel == TargetWaterLevel)
    {
        // Si on a atteint la cible, inverser la direction
        if (bIsWaterRising)
        {
            SetWaterLevel(MinWaterLevel);
        }
        else
        {
            SetWaterLevel(MaxWaterLevel);
        }
    }
}


void UWaterSystem::KillCharacterInWater(AActor* Character)
{
    // Vérifier si c'est bien un personnage Worm
    AWormCharacter* WormChar = Cast<AWormCharacter>(Character);
    if (!WormChar)
    {
        return;
    }
    
    // Appliquer des dégâts mortels
    // L'eau est instantanément mortelle - valeur arbitraire élevée
    float LethalDamage = 999.0f;
    
    // Direction de l'impulsion vers le haut pour effet visuel
    FVector ImpulseDirection = FVector(0, 0, 1.0f);
    WormChar->ApplyDamageToWorm(LethalDamage, ImpulseDirection * 500.0f);
    
    // First check if the game is over, to avoid conflicts with turn management
    AWormGameState* GameState = Cast<AWormGameState>(UGameplayStatics::GetGameState(GetWorld()));
    bool bIsGameOver = GameState && GameState->bGameOver;
    
    // Check if it was the character's turn and end it (only if game is not over)
    
    
    // Notify the GameState that a character died (if game not already over)
    if (GameState && !bIsGameOver)
    {
        GameState->CheckGameOverCondition();
    }
}

void UWaterSystem::NotifyTurnEnded_Implementation()
{
    // Handle turn-end water rising if set to rise after each turn
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return;
    }

    AEnvironmentalEventsManager* EventsManager = Cast<AEnvironmentalEventsManager>(Owner);
    if (EventsManager && EventsManager->bIsWaterRisingActive && EventsManager->bRiseAfterEachTurn)
    {
        UE_LOG(LogTemp, Log, TEXT("WaterSystem: Turn ended - raising water"));
        
        // Raise water by the configured amount
        RaiseWaterLevel(EventsManager->WaterRisePerTurn);
    }
}


void UWaterSystem::Multicast_UpdateWaterVisuals_Implementation(float NewWaterLevel)
{
    // Only update visuals on clients
    if (GetOwnerRole() < ROLE_Authority)
    {
        CurrentWaterLevel = NewWaterLevel;
        UpdateWaterMeshAndVolume();
    }
}


void UWaterSystem::AddPersistentWaterEffects()
{
    // Create ambient water sound if specified
    if (WaterAmbientSound && !WaterAmbientSoundComponent)
    {
        WaterAmbientSoundComponent = UGameplayStatics::SpawnSoundAttached(
            WaterAmbientSound,
            WaterMeshComponent,
            NAME_None,
            FVector::ZeroVector,
            EAttachLocation::KeepRelativeOffset,
            true,
            1.0f,  // Volume
            1.0f,  // Pitch
            0.0f,  // Start time
            nullptr,
            nullptr,
            true   // Auto-destroy
        );
        
        // Adjust sound volume based on water level
        if (WaterAmbientSoundComponent)
        {
            // Higher volume as water rises
            float LevelRatio = FMath::GetMappedRangeValueClamped(
                FVector2D(MinWaterLevel, MaxWaterLevel),
                FVector2D(0.2f, 1.0f),
                CurrentWaterLevel
            );
            
            WaterAmbientSoundComponent->SetVolumeMultiplier(LevelRatio);
        }
    }
    
    // Update sound volume when water level changes
    if (WaterAmbientSoundComponent)
    {
        float LevelRatio = FMath::GetMappedRangeValueClamped(
            FVector2D(MinWaterLevel, MaxWaterLevel),
            FVector2D(0.2f, 1.0f),
            CurrentWaterLevel
        );
        
        WaterAmbientSoundComponent->SetVolumeMultiplier(LevelRatio);
    }
    
    // Add periodic ripple effects for visual interest
    if (GetWorld() && RippleEffect)
    {
        // Create a timer to spawn random ripples on the water surface
        if (!GetWorld()->GetTimerManager().IsTimerActive(RippleTimerHandle))
        {
            GetWorld()->GetTimerManager().SetTimer(
                RippleTimerHandle,
                this,
                &UWaterSystem::SpawnRandomRipple,
                2.0f,  // Every 2 seconds
                true   // Looping
            );
        }
    }
}

void UWaterSystem::SpawnRandomRipple()
{
    if (!GetWorld() || !RippleEffect || !WaterMeshComponent)
    {
        return;
    }
    
    // Get the water mesh bounds
    FVector Origin, Extent;
    WaterMeshComponent->GetLocalBounds(Origin, Extent);
    
    // Calculate world bounds
    FVector WorldOrigin = WaterMeshComponent->GetComponentLocation() + WaterMeshComponent->GetComponentTransform().TransformVector(Origin);
    FVector WorldExtent = Extent * WaterMeshComponent->GetComponentScale();
    
    // Generate a random position on the water surface
    float RandomX = FMath::RandRange(WorldOrigin.X - WorldExtent.X * 0.8f, WorldOrigin.X + WorldExtent.X * 0.8f);
    float RandomY = FMath::RandRange(WorldOrigin.Y - WorldExtent.Y * 0.8f, WorldOrigin.Y + WorldExtent.Y * 0.8f);
    
    // Create ripple at water surface
    FVector RippleLocation = FVector(RandomX, RandomY, CurrentWaterLevel + 1.0f);
    
    // Random scale for variety
    float RippleScale = FMath::RandRange(0.5f, 1.5f);
    
    // Spawn the ripple effect
    UGameplayStatics::SpawnEmitterAtLocation(
        GetWorld(),
        RippleEffect,
        RippleLocation,
        FRotator(90.0f, 0.0f, 0.0f),  // Face upward
        FVector(RippleScale),
        true
    );
}
