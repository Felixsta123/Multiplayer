// WormWeapon.cpp
#include "WormWeapon.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Particles/ParticleSystemComponent.h"
#include "Materials/MaterialInstanceDynamic.h"


AWormWeapon::AWormWeapon()
{
    PrimaryActorTick.bCanEverTick = true; // Activé pour mettre à jour la trajectoire
    
    // Créer le composant mesh
    WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
    RootComponent = WeaponMesh;
    
    // Créer le composant de spline pour la trajectoire
    TrajectorySpline = CreateDefaultSubobject<USplineComponent>(TEXT("TrajectorySpline"));
    TrajectorySpline->SetupAttachment(RootComponent);
    TrajectorySpline->SetVisibility(false);
    
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

    // Initialiser les points de trajectoire
    for (int32 i = 0; i < TrajectoryPointCount; ++i)
    {
        UStaticMeshComponent* PointMesh = NewObject<UStaticMeshComponent>(this);
        PointMesh->SetupAttachment(TrajectorySpline);

        // Vérifier si le mesh de point est assigné
        if (TrajectoryPointMesh)
        {
            PointMesh->SetStaticMesh(TrajectoryPointMesh);
        }

        PointMesh->SetRelativeScale3D(FVector(0.5f, 0.5f, 0.5f));
        PointMesh->SetVisibility(false);
        PointMesh->RegisterComponent();
        TrajectoryPoints.Add(PointMesh);
    }

    APawn* OwnerPawn = Cast<APawn>(GetOwner());
    if (AimingEffect && OwnerPawn && OwnerPawn->IsLocallyControlled())
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

    // Créer le composant de prévisualisation d'impact
    if (ImpactPreviewMesh && OwnerPawn && OwnerPawn->IsLocallyControlled())
    {
        ImpactPreviewComponent = NewObject<UStaticMeshComponent>(this);
        ImpactPreviewComponent->SetStaticMesh(ImpactPreviewMesh);
        ImpactPreviewComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        ImpactPreviewComponent->SetVisibility(false);
        ImpactPreviewComponent->RegisterComponent();
    }

    // Cacher la trajectoire au début
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
    if (!TrajectorySpline)
    {
        UE_LOG(LogTemp, Error, TEXT("Cannot show trajectory: TrajectorySpline is null!"));
        return;
    }
    
    // Activer/désactiver la visibilité de la spline et des points
    TrajectorySpline->SetVisibility(bShow);
    if (AimingEffectComponent)
    {
        AimingEffectComponent->SetVisibility(bShow);
    }

    if (ImpactPreviewComponent)
    {
        ImpactPreviewComponent->SetVisibility(bShow);
    }
    for (UStaticMeshComponent* Point : TrajectoryPoints)
    {
        if (Point)
        {
            Point->SetVisibility(bShow);
        }
    }
    
    // Mettre à jour la trajectoire si elle devient visible
    if (bShow)
    {
        UpdateTrajectory();
    }
}

void AWormWeapon::UpdateTrajectory()
{
    if (!TrajectorySpline)
    {
        UE_LOG(LogTemp, Error, TEXT("Cannot update trajectory: TrajectorySpline is null!"));
        return;
    }
    
    // Uniquement pour les clients contrôlés localement
    APawn* OwnerPawn = Cast<APawn>(GetOwner());
    if (!OwnerPawn || !OwnerPawn->IsLocallyControlled())
    {
        return;
    }
    
    // Calculer la position de départ et la vélocité initiale
    FVector StartLocation = WeaponMesh->GetSocketLocation(MuzzleSocketName);
    FRotator MuzzleRotation = WeaponMesh->GetSocketRotation(MuzzleSocketName);
    FVector LaunchVelocity = MuzzleRotation.Vector() * FirePower;
    
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
    
    // Simuler chaque étape
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
        FCollisionQueryParams CollisionParams;
        CollisionParams.AddIgnoredActor(this);
        CollisionParams.AddIgnoredActor(GetOwner());
        
        if (GetWorld()->LineTraceSingleByChannel(HitResult, CurrentLocation, EndTrace, ECC_Visibility, CollisionParams))
        {
            // Dessiner un point de débug à l'emplacement de l'impact (visible en PIE)
            DrawDebugSphere(GetWorld(), HitResult.Location, 20.0f, 8, FColor::Red, false, 0.1f);
            
            // Ajouter le point d'impact et arrêter la simulation
            SimulatedTrajectoryPoints.Add(HitResult.Location);
            break;
        }
    }
    
    // Mettre à jour la spline avec les points calculés
    for (int32 i = 0; i < SimulatedTrajectoryPoints.Num(); ++i)
    {
        TrajectorySpline->AddSplinePoint(SimulatedTrajectoryPoints[i], ESplineCoordinateSpace::World);
    }
    
    // Mettre à jour la spline
    TrajectorySpline->UpdateSpline();
    
    // Positionner les composants visuels
    for (int32 i = 0; i < TrajectoryPoints.Num() && i < SimulatedTrajectoryPoints.Num(); ++i)
    {
        if (TrajectoryPoints[i])
        {
            TrajectoryPoints[i]->SetWorldLocation(SimulatedTrajectoryPoints[i]);
        }
    }

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
        
        // Faire tourner lentement la prévisualisation pour attirer l'attention
        float Time = GetWorld()->GetTimeSeconds();
        ImpactPreviewComponent->SetWorldRotation(FRotator(0.0f, Time * 30.0f, 0.0f));
        
        // Faire pulser la taille pour un effet visuel intéressant
        float PulseScale = 1.0f + 0.2f * FMath::Sin(Time * 5.0f);
        ImpactPreviewComponent->SetWorldScale3D(FVector(PulseScale));
    }
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
        
        // Paramètres de spawn
        FActorSpawnParameters SpawnParams;
        SpawnParams.Owner = GetOwner();
        SpawnParams.Instigator = Cast<APawn>(GetOwner());
        ShowTrajectory(false);

        // Spawner le projectile
        AActor* Projectile = GetWorld()->SpawnActor<AActor>(ProjectileClass, MuzzleLocation, MuzzleRotation, SpawnParams);
        
        if (Projectile)
        {
            // Trouver le composant de mouvement du projectile
            UPrimitiveComponent* ProjectileComp = Cast<UPrimitiveComponent>(Projectile->GetComponentByClass(UPrimitiveComponent::StaticClass()));
            
            if (ProjectileComp)
            {
                // Ajouter une impulsion initiale
                ProjectileComp->AddImpulse(MuzzleRotation.Vector() * FirePower, NAME_None, true);
            }
        }
        
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