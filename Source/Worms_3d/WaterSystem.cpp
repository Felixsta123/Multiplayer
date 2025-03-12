#include "WaterSystem.h"
#include "Kismet/GameplayStatics.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundCue.h"
#include "Particles/ParticleSystemComponent.h"
#include "AWormCharacter.h"
#include "WormGameState.h"
#include "WormGameMode.h"
#include "Engine/World.h"
#include "TimerManager.h"

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
    
    // Créer le mesh d'eau
    if (!WaterMeshComponent)
    {
        WaterMeshComponent = NewObject<UStaticMeshComponent>(GetOwner(), TEXT("WaterMesh"));
        WaterMeshComponent->SetupAttachment(GetOwner()->GetRootComponent());
        WaterMeshComponent->RegisterComponent();
        
        if (WaterMesh)
        {
            WaterMeshComponent->SetStaticMesh(WaterMesh);
        }
        else 
        {
            UE_LOG(LogTemp, Warning, TEXT("WaterSystem: Aucun mesh d'eau assigné, utilisation d'un plan par défaut"));
            // On pourrait charger un mesh de plan par défaut ici
        }
        
        // Configurer la physique et les collisions
        WaterMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision); // L'eau n'a pas de collisions physiques
        WaterMeshComponent->SetGenerateOverlapEvents(false);
        WaterMeshComponent->SetCastShadow(false);
        
        // Créer et appliquer le matériau dynamique
        if (WaterMaterial)
        {
            WaterDynamicMaterial = UMaterialInstanceDynamic::Create(WaterMaterial, this);
            WaterDynamicMaterial->SetVectorParameterValue(TEXT("WaterColor"), WaterColor);
            WaterMeshComponent->SetMaterial(0, WaterDynamicMaterial);
        }
    }
    
    // Créer le volume de détection
    if (!WaterVolumeComponent)
    {
        WaterVolumeComponent = NewObject<UBoxComponent>(GetOwner(), TEXT("WaterVolume"));
        WaterVolumeComponent->SetupAttachment(GetOwner()->GetRootComponent());
        WaterVolumeComponent->RegisterComponent();
        
        // Configurer la détection de collision
        WaterVolumeComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        WaterVolumeComponent->SetCollisionResponseToAllChannels(ECR_Overlap);
        WaterVolumeComponent->SetGenerateOverlapEvents(true);
        
        // Lier le callback d'overlap
        WaterVolumeComponent->OnComponentBeginOverlap.AddDynamic(this, &UWaterSystem::OnWaterOverlap);
    }
    
    // Configurer le son d'ambiance
    if (WaterAmbientSound)
    {
        WaterAmbientSoundComponent = UGameplayStatics::SpawnSoundAttached(
            WaterAmbientSound,
            WaterMeshComponent,
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
    
    // Démarrer le cycle si nécessaire
    if (bAutomaticCycleActive)
    {
        StartAutomaticCycle(CycleDuration);
    }
    
    UE_LOG(LogTemp, Log, TEXT("WaterSystem initialized at level %.1f"), CurrentWaterLevel);
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
    // Limiter la hauteur à la plage configurée
    NewLevel = FMath::Clamp(NewLevel, MinWaterLevel, MaxWaterLevel);
    
    // Définir si l'eau monte ou descend
    bIsWaterRising = (NewLevel > CurrentWaterLevel);
    
    // Stocker la cible
    TargetWaterLevel = NewLevel;
    
    // Si changement immédiat, mettre à jour directement
    if (bImmediate)
    {
        CurrentWaterLevel = TargetWaterLevel;
        UpdateWaterMeshAndVolume();
    }
    
    UE_LOG(LogTemp, Verbose, TEXT("Water level changing to %.1f (currently: %.1f, %s)"), 
        TargetWaterLevel, CurrentWaterLevel, bIsWaterRising ? TEXT("rising") : TEXT("lowering"));
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
    AWormCharacter* Character = Cast<AWormCharacter>(OtherActor);
    if (Character)
    {
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
    WaterMeshComponent->SetWorldScale3D(FVector(WorldSize / 100.0f)); // Ajuster selon la taille de votre mesh
    
    // Mise à jour du volume de détection
    FVector VolumeOrigin = MeshLocation;
    VolumeOrigin.Z = CurrentWaterLevel - 100.0f; // Légèrement sous la surface pour détecter correctement
    
    WaterVolumeComponent->SetWorldLocation(VolumeOrigin);
    WaterVolumeComponent->SetBoxExtent(FVector(WorldSize, WorldSize, 100.0f)); // Hauteur de détection arbitraire
    
    // Mettre à jour les effets visuels de l'eau
    if (WaterDynamicMaterial)
    {
        // Ajouter du temps pour l'animation des vagues
        float Time = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
        WaterDynamicMaterial->SetScalarParameterValue(TEXT("Time"), Time);
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
    
    // Notifier le GameState qu'un personnage est mort
    AWormGameState* GameState = Cast<AWormGameState>(UGameplayStatics::GetGameState(GetWorld()));
    if (GameState)
    {
        GameState->CheckGameOverCondition();
    }
}

TArray<AActor*> UWaterSystem::GetActorsInWater() const
{
    TArray<AActor*> Result;
    
    if (WaterVolumeComponent)
    {
        WaterVolumeComponent->GetOverlappingActors(Result);
    }
    
    return Result;
}