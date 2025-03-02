// WormWeapon.cpp
#include "WormWeapon.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "AWormCharacter.h"
#include "AWormsProjectile.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "Materials/MaterialInstanceDynamic.h"


AWormWeapon::AWormWeapon()
{
    PrimaryActorTick.bCanEverTick = true;
    
    // Créer le composant mesh
    WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
    RootComponent = WeaponMesh;
    
    // Créer le composant de spline pour la trajectoire
    TrajectorySpline = CreateDefaultSubobject<USplineComponent>(TEXT("TrajectorySpline"));
    TrajectorySpline->SetupAttachment(RootComponent);
    TrajectorySpline->SetVisibility(false);
    
    // *** Ajouter cette ligne pour que la spline ne soit pas répliquée ***
    TrajectorySpline->SetIsReplicated(false);
    
    // Configurer la réplication
    bReplicates = true;
    WeaponMesh->SetIsReplicated(true);
    
    // Valeurs par défaut
    MuzzleSocketName = "MuzzleSocket";
    // Initialiser les limites de puissance
    MinFirePower = 1000.0f;
    MaxFirePower = 5000.0f;
    FirePower = 3000.0f; // Valeur initiale
    PowerAdjustmentStep = 100.0f;
    ReloadTime = 2.0f;
    MaxAmmo = 5;
    AmmoCount = MaxAmmo;
    bIsReloading = false;
    TrajectoryPointCount = 30;
    SimulationTimeStep = 0.1f;
    SimulationDuration = 3.0f;
}

void AWormWeapon::BeginPlay()
{
    Super::BeginPlay();

    APawn* OwnerPawn = Cast<APawn>(GetOwner());
    
    // Ne créer les points de trajectoire que pour le client qui contrôle le personnage
    if (OwnerPawn && OwnerPawn->IsLocallyControlled())
    {
        // Initialiser les points de trajectoire
        for (int32 i = 0; i < TrajectoryPointCount; ++i)
        {
            UStaticMeshComponent* PointMesh = NewObject<UStaticMeshComponent>(this);
            PointMesh->SetupAttachment(TrajectorySpline);

            if (TrajectoryPointMesh)
            {
                PointMesh->SetStaticMesh(TrajectoryPointMesh);
            }

            PointMesh->SetRelativeScale3D(FVector(0.5f, 0.5f, 0.5f));
            PointMesh->SetVisibility(false);
            
            // Désactiver les collisions pour les points de trajectoire
            PointMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            PointMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
            
            PointMesh->RegisterComponent();
            TrajectoryPoints.Add(PointMesh);
        }

        // Créer l'effet d'aide à la visée
        if (AimingEffect)
        {
            AimingEffectComponent = UGameplayStatics::SpawnEmitterAttached(
                AimingEffect,
                WeaponMesh,
                MuzzleSocketName,
                FVector::ZeroVector,
                FRotator::ZeroRotator,
                FVector(1.0f),
                EAttachLocation::SnapToTarget,
                false,
                EPSCPoolMethod::AutoRelease
            );

            if (AimingEffectComponent)
            {
                AimingEffectComponent->SetVisibility(false);
            }
        }

        // Créer un matériau dynamique pour la trajectoire
        if (TrajectoryMaterial)
        {
            TrajectoryMaterialInstance = UMaterialInstanceDynamic::Create(TrajectoryMaterial, this);

            // Appliquer le matériau à tous les points de trajectoire
            for (UStaticMeshComponent* Point : TrajectoryPoints)
            {
                if (Point && TrajectoryMaterialInstance)
                {
                    Point->SetMaterial(0, TrajectoryMaterialInstance);
                }
            }
        }

        // Créer le composant de prévisualisation d'impact sans collision
        if (ImpactPreviewMesh)
        {
            ImpactPreviewComponent = NewObject<UStaticMeshComponent>(this);
            ImpactPreviewComponent->SetStaticMesh(ImpactPreviewMesh);
            ImpactPreviewComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            ImpactPreviewComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
            ImpactPreviewComponent->SetVisibility(false);
            ImpactPreviewComponent->RegisterComponent();
        }
    }

    // Cacher la trajectoire au début (pour tous les clients)
    ShowTrajectory(false);
}
void AWormWeapon::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    
    // Mettre à jour la trajectoire si elle est visible
    if (TrajectorySpline && TrajectorySpline->IsVisible())
    {
        UpdateTrajectory();
    }
}

void AWormWeapon::ShowTrajectory(bool bShow)
{
    // Vérification que ce code s'exécute uniquement sur le client local qui est actuellement en train de jouer
    APawn* OwnerPawn = Cast<APawn>(GetOwner());
    AWormCharacter* OwnerChar = Cast<AWormCharacter>(OwnerPawn);
    
    // Log détaillé pour vérifier les conditions
    UE_LOG(LogTemp, Warning, TEXT("ShowTrajectory(%s): OwnerPawn=%s, LocallyControlled=%s, OwnerChar=%s, IsMyTurn=%s"),
        bShow ? TEXT("true") : TEXT("false"),
        OwnerPawn ? *OwnerPawn->GetName() : TEXT("NULL"),
        OwnerPawn && OwnerPawn->IsLocallyControlled() ? TEXT("Yes") : TEXT("No"),
        OwnerChar ? *OwnerChar->GetName() : TEXT("NULL"),
        OwnerChar && OwnerChar->IsMyTurn() ? TEXT("Yes") : TEXT("No"));
    
    // Vérifier trois conditions:
    // 1. L'arme appartient à un Pawn
    // 2. C'est le Pawn contrôlé localement
    // 3. C'est son tour de jouer
    if (!OwnerPawn || !OwnerPawn->IsLocallyControlled() || (OwnerChar && !OwnerChar->IsMyTurn()))
    {
        // Si non, ne pas montrer/cacher la trajectoire
        UE_LOG(LogTemp, Warning, TEXT("Not showing trajectory - conditions not met"));
        return;
    }

    if (!TrajectorySpline)
    {
        UE_LOG(LogTemp, Error, TEXT("Cannot show trajectory: TrajectorySpline is null!"));
        return;
    }
    
    // Le reste du code reste inchangé
    UE_LOG(LogTemp, Warning, TEXT("Setting trajectory visibility to: %s"), bShow ? TEXT("Visible") : TEXT("Hidden"));
    
    // Définir la visibilité de tous les composants
    TrajectorySpline->SetVisibility(bShow);
    
    if (AimingEffectComponent)
    {
        AimingEffectComponent->SetVisibility(bShow);
    }

    if (ImpactPreviewComponent)
    {
        ImpactPreviewComponent->SetVisibility(bShow);
        // S'assurer que les collisions sont désactivées
        ImpactPreviewComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
    
    for (UStaticMeshComponent* Point : TrajectoryPoints)
    {
        if (Point)
        {
            Point->SetVisibility(bShow);
            // S'assurer que les collisions sont désactivées
            Point->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        }
    }
    
    // Si la trajectoire doit être visible, la mettre à jour immédiatement
    if (bShow)
    {
        UpdateTrajectory();
    }
}

void AWormWeapon::UpdateTrajectory()
{
    APawn* OwnerPawn = Cast<APawn>(GetOwner());
    AWormCharacter* OwnerChar = Cast<AWormCharacter>(OwnerPawn);
    
    if (!OwnerPawn || !OwnerPawn->IsLocallyControlled() || (OwnerChar && !OwnerChar->IsMyTurn()))
    {
        return;
    }

    if (!TrajectorySpline)
    {
        UE_LOG(LogTemp, Error, TEXT("Cannot update trajectory: TrajectorySpline is null!"));
        return;
    }
    
    // Calculer la position de départ et la vélocité initiale
    FVector StartLocation = WeaponMesh->GetSocketLocation(MuzzleSocketName);
    FRotator MuzzleRotation = WeaponMesh->GetSocketRotation(MuzzleSocketName);
    FVector LaunchVelocity = MuzzleRotation.Vector() * FirePower;
    
    // Log pour débogage
    UE_LOG(LogTemp, Verbose, TEXT("Updating trajectory: Start=%s, Direction=%s, Power=%.1f"),
        *StartLocation.ToString(), *MuzzleRotation.Vector().ToString(), FirePower);
    
    // Paramètres de simulation
    float TimeStep = SimulationTimeStep;
    float MaxSimTime = SimulationDuration;
    
    // Vider les points précédents
    TrajectorySpline->ClearSplinePoints();
    
    // Simuler la trajectoire
    TArray<FVector> SimulatedTrajectoryPoints;
    FVector CurrentLocation = StartLocation;
    FVector CurrentVelocity = LaunchVelocity;
    
    // Ajouter le point initial
    SimulatedTrajectoryPoints.Add(CurrentLocation);
    
    // Préparer les paramètres de collision
    FCollisionQueryParams CollisionParams;
    CollisionParams.AddIgnoredActor(this);
    CollisionParams.AddIgnoredActor(GetOwner());
    
    // IMPORTANT: Ignorer tous les personnages Worm et leurs armes
    TArray<AActor*> AllWormCharacters;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AWormCharacter::StaticClass(), AllWormCharacters);
    for (AActor* Actor : AllWormCharacters)
    {
        CollisionParams.AddIgnoredActor(Actor);
        
        // Ignorer aussi les armes attachées
        AWormCharacter* Character = Cast<AWormCharacter>(Actor);
        if (Character && Character->CurrentWeapon)
        {
            CollisionParams.AddIgnoredActor(Character->CurrentWeapon);
        }
    }
    
    // Ignorer aussi toutes les armes
    TArray<AActor*> AllWeapons;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AWormWeapon::StaticClass(), AllWeapons);
    for (AActor* Weapon : AllWeapons)
    {
        CollisionParams.AddIgnoredActor(Weapon);
    }
    
    // Configuration de la collision (plus de détails et d'informations)
    CollisionParams.bTraceComplex = true;
    CollisionParams.bReturnPhysicalMaterial = true;
    
    // Simuler chaque étape
    bool bHitSomething = false;
    for (float CurrentTime = 0.0f; CurrentTime < MaxSimTime && SimulatedTrajectoryPoints.Num() < TrajectoryPointCount; CurrentTime += TimeStep)
    {
        // Mise à jour de la position avec la vélocité actuelle
        CurrentLocation += CurrentVelocity * TimeStep;
        
        // Ajouter la gravité
        FVector GravityVector = FVector(0, 0, GetWorld()->GetGravityZ());
        CurrentVelocity += GravityVector * TimeStep;
        
        // Ajouter ce point à la trajectoire
        SimulatedTrajectoryPoints.Add(CurrentLocation);
        
        // Vérifier si le point a touché quelque chose
        FHitResult HitResult;
        FVector EndTrace = CurrentLocation + CurrentVelocity * TimeStep;
        
        // Tracer la ligne pour détecter les collisions (utiliser ECC_Visibility au lieu de ECC_WorldStatic)
        if (GetWorld()->LineTraceSingleByChannel(HitResult, CurrentLocation, EndTrace, ECC_Visibility, CollisionParams))
        {
            // Debug
            UE_LOG(LogTemp, Verbose, TEXT("Trajectory hit: %s at %s"), 
                *HitResult.GetActor()->GetName(), *HitResult.Location.ToString());
            
            // Dessiner un point de débug à l'emplacement de l'impact (visible en PIE)
            DrawDebugSphere(GetWorld(), HitResult.Location, 20.0f, 8, FColor::Red, false, 0.1f);
            
            // Ajouter le point d'impact et arrêter la simulation
            SimulatedTrajectoryPoints.Add(HitResult.Location);
            bHitSomething = true;
            break;
        }
    }
    
    // S'assurer qu'il y a au moins deux points pour la spline
    if (SimulatedTrajectoryPoints.Num() < 2)
    {
        // Ajouter un point de plus s'il n'y en a qu'un
        if (SimulatedTrajectoryPoints.Num() == 1)
        {
            SimulatedTrajectoryPoints.Add(SimulatedTrajectoryPoints[0] + FVector(100.0f, 0.0f, 0.0f));
        }
        else
        {
            // Si aucun point, en ajouter deux par défaut
            SimulatedTrajectoryPoints.Add(StartLocation);
            SimulatedTrajectoryPoints.Add(StartLocation + MuzzleRotation.Vector() * 100.0f);
        }
    }
    
    // Mettre à jour la spline avec les points calculés
    for (int32 i = 0; i < SimulatedTrajectoryPoints.Num(); ++i)
    {
        TrajectorySpline->AddSplinePoint(SimulatedTrajectoryPoints[i], ESplineCoordinateSpace::World);
    }
    
    // Mettre à jour la spline
    TrajectorySpline->UpdateSpline();
    
    // Positionner les composants visuels et s'assurer qu'ils sont visibles
    for (int32 i = 0; i < TrajectoryPoints.Num() && i < SimulatedTrajectoryPoints.Num(); ++i)
    {
        if (TrajectoryPoints[i])
        {
            TrajectoryPoints[i]->SetWorldLocation(SimulatedTrajectoryPoints[i]);
            TrajectoryPoints[i]->SetVisibility(true);
        }
    }

    // S'assurer que tous les composants de trajectoire sont visibles
    TrajectorySpline->SetVisibility(true);
    
    if (TrajectoryMaterialInstance)
    {
        float NormalizedPower = GetNormalizedPower();
        TrajectoryMaterialInstance->SetScalarParameterValue(TEXT("Power"), NormalizedPower);
        
        // Optionnel: Changer la couleur en fonction de la puissance
        FLinearColor TrajectoryColor = FMath::Lerp(
            FLinearColor(0.0f, 0.5f, 1.0f), // Bleu pour faible puissance
            FLinearColor(1.0f, 0.0f, 0.0f), // Rouge pour haute puissance
            NormalizedPower
        );
        TrajectoryMaterialInstance->SetVectorParameterValue(TEXT("Color"), TrajectoryColor);
    }
    
    // Si on a détecté un point d'impact lors de la simulation, mettre à jour la prévisualisation
    if (ImpactPreviewComponent && SimulatedTrajectoryPoints.Num() > 1)
    {
        // Utiliser le dernier point comme position d'impact
        FVector ImpactLocation = SimulatedTrajectoryPoints.Last();
        ImpactPreviewComponent->SetWorldLocation(ImpactLocation);
        ImpactPreviewComponent->SetVisibility(true);
        
        // Faire tourner lentement la prévisualisation pour attirer l'attention
        float Time = GetWorld()->GetTimeSeconds();
        ImpactPreviewComponent->SetWorldRotation(FRotator(0.0f, Time * 30.0f, 0.0f));
        
        // Faire pulser la taille pour un effet visuel intéressant
        float PulseScale = 1.0f + 0.2f * FMath::Sin(Time * 5.0f);
        ImpactPreviewComponent->SetWorldScale3D(FVector(PulseScale));
    }
    
    UE_LOG(LogTemp, Verbose, TEXT("Trajectory simulation complete: %d points, hit something: %s"),
     SimulatedTrajectoryPoints.Num(), bHitSomething ? TEXT("Yes") : TEXT("No"));
}

// Ajouter la fonction pour ajuster la puissance
void AWormWeapon::AdjustPower(float Delta)
{
    // Calculer la nouvelle puissance
    float NewPower = FMath::Clamp(
        FirePower + (Delta * PowerAdjustmentStep),
        MinFirePower,
        MaxFirePower
    );
    
    // Mettre à jour la puissance si elle a changé
    if (NewPower != FirePower)
    {
        FirePower = NewPower;
        
        // Mettre à jour la trajectoire si elle est visible
        if (TrajectorySpline && TrajectorySpline->IsVisible())
        {
            UpdateTrajectory();
        }
        
        // Appeler l'événement pour notification Blueprint
        OnPowerChanged(FirePower, GetNormalizedPower());
        OnPowerChangedDelegate.Broadcast(FirePower, GetNormalizedPower());
    }
    
}

void AWormWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    
    DOREPLIFETIME(AWormWeapon, bIsReloading);
    DOREPLIFETIME(AWormWeapon, AmmoCount);

}

void AWormWeapon::Fire()
{
    // Vérifier si on peut tirer
    if (HasAuthority() && ProjectileClass && AmmoCount > 0 && !bIsReloading)
    {
        // Réduire les munitions
        AmmoCount--;
        
        // Obtenir la position et direction de tir
        FVector MuzzleLocation = WeaponMesh->GetSocketLocation(MuzzleSocketName);
        FRotator MuzzleRotation = WeaponMesh->GetSocketRotation(MuzzleSocketName);
        FVector LaunchDirection = MuzzleRotation.Vector();
        
        // Calculer une position de spawn en avant de l'arme pour éviter les collisions
        FVector SpawnLocation = MuzzleLocation + (LaunchDirection * 100.0f);
        
        // Log de débogage
        UE_LOG(LogTemp, Warning, TEXT("=== FIRE DEBUG ==="));
        UE_LOG(LogTemp, Warning, TEXT("MuzzleLocation: %s"), *MuzzleLocation.ToString());
        UE_LOG(LogTemp, Warning, TEXT("SpawnLocation: %s"), *SpawnLocation.ToString());
        UE_LOG(LogTemp, Warning, TEXT("Direction: %s"), *LaunchDirection.ToString());
        UE_LOG(LogTemp, Warning, TEXT("FirePower: %.1f"), FirePower);
        
        // Paramètres de spawn avec ignorance des collisions
        FActorSpawnParameters SpawnParams;
        SpawnParams.Owner = GetOwner();
        SpawnParams.Instigator = Cast<APawn>(GetOwner());
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        
        // Spawner le projectile
        AWormProjectile* Projectile = GetWorld()->SpawnActor<AWormProjectile>(
            ProjectileClass,
            SpawnLocation,
            MuzzleRotation,
            SpawnParams
        );
        
        if (Projectile)
        {
            UE_LOG(LogTemp, Warning, TEXT("Projectile spawned successfully: %s"), *Projectile->GetName());
            
            // Initialiser le projectile avec la direction et puissance de tir
            Projectile->InitializeProjectile(LaunchDirection, FirePower);
            
            UE_LOG(LogTemp, Warning, TEXT("Projectile initialized with velocity: %s (Power: %.1f)"),
                  *LaunchDirection.ToString(), FirePower);
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("Failed to spawn projectile!"));
        }
        
        // Maintenant que le projectile est lancé, masquer la trajectoire
        ShowTrajectory(false);
        
        // Déclencher les effets sur tous les clients
        Multicast_OnFire();
        
        // Démarrer le rechargement si nécessaire
        if (AmmoCount <= 0)
        {
            bIsReloading = true;
            GetWorldTimerManager().SetTimer(ReloadTimerHandle, this, &AWormWeapon::OnReloadComplete, ReloadTime, false);
        }
    }
}

void AWormWeapon::Multicast_OnFire_Implementation()
{
    // Jouer les effets visuels
    if (MuzzleEffect)
    {
        UGameplayStatics::SpawnEmitterAttached(MuzzleEffect, WeaponMesh, MuzzleSocketName);
    }
    
    // Jouer le son
    if (FireSound)
    {
        UGameplayStatics::PlaySoundAtLocation(this, FireSound, GetActorLocation());
    }
    
    // Cacher la trajectoire lors du tir
    ShowTrajectory(false);
    
    // Appeler l'événement Blueprint
    OnFire();
}

void AWormWeapon::OnReloadComplete()
{
    if (HasAuthority())
    {
        // Recharger l'arme
        AmmoCount = MaxAmmo;
        bIsReloading = false;
    }
}