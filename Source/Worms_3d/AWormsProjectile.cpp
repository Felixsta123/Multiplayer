#include "AWormsProjectile.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "AWormCharacter.h"
#include "Net/UnrealNetwork.h"

AWormProjectile::AWormProjectile()
{
    PrimaryActorTick.bCanEverTick = true;
    
    // Configurer la réplication
    bReplicates = true;
    
    // Créer et configurer le composant de collision
    CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComp"));
    CollisionComp->InitSphereRadius(5.0f);
    CollisionComp->SetCollisionProfileName("Projectile");
    
    // Configuration optimale pour la collision
    CollisionComp->SetSimulatePhysics(false);  // Sera activé plus tard
    CollisionComp->SetEnableGravity(true);
    CollisionComp->SetNotifyRigidBodyCollision(true);  // Hit Events
    
    // Désactiver initialement les collisions - elles seront activées plus tard
    CollisionComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    
    RootComponent = CollisionComp;
    
    // Ajouter callback pour les collisions
    CollisionComp->OnComponentHit.AddDynamic(this, &AWormProjectile::OnHit);
    
    // Créer le composant de mesh avec les collisions désactivées
    ProjectileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileMesh"));
    ProjectileMesh->SetupAttachment(RootComponent);
    ProjectileMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ProjectileMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
    
    // Créer le composant de mouvement projectile
    ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileComp"));
    ProjectileMovement->UpdatedComponent = CollisionComp;
    ProjectileMovement->InitialSpeed = 3000.0f;
    ProjectileMovement->MaxSpeed = 3000.0f;
    ProjectileMovement->bRotationFollowsVelocity = true;
    ProjectileMovement->bShouldBounce = true;
    ProjectileMovement->Bounciness = 0.3f;
    ProjectileMovement->ProjectileGravityScale = 1.0f;
    
    // Configurer les propriétés du ProjectileMovementComponent pour un meilleur comportement
    ProjectileMovement->bInitialVelocityInLocalSpace = true;
    ProjectileMovement->bSimulationEnabled = true;
    ProjectileMovement->bSweepCollision = true;
    
    // Configurer l'explosion
    ExplosionRadius = 200.0f;
    ExplosionDamage = 25.0f;
    DetonationDelay = 3.0f;
    
    // Définir la durée de vie automatique
    InitialLifeSpan = 10.0f;
    
    // Délai avant activation des collisions
    CollisionActivationDelay = 0.3f;
    
    // Initialiser la puissance et la vélocité
    FirePower = 3000.0f;
    InitialVelocity = FVector::ForwardVector * FirePower;
}

void AWormProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    
    // Répliquer ces propriétés
    DOREPLIFETIME(AWormProjectile, InitialVelocity);
    DOREPLIFETIME(AWormProjectile, FirePower);
}

void AWormProjectile::InitializeProjectile(FVector Direction, float Power)
{
    if (HasAuthority())
    {
        // Stocker la direction et la puissance pour la réplication
        InitialVelocity = Direction * Power;
        FirePower = Power;
        
        // Configurer le mouvement du projectile
        if (ProjectileMovement)
        {
            ProjectileMovement->Velocity = InitialVelocity;
            ProjectileMovement->InitialSpeed = Power;
            ProjectileMovement->MaxSpeed = Power * 1.5f;
        }
        
        UE_LOG(LogTemp, Warning, TEXT("Projectile initialized: Direction=%s, Power=%.1f, Velocity=%s"),
               *Direction.ToString(), Power, *InitialVelocity.ToString());
    }
}

void AWormProjectile::BeginPlay()
{
    Super::BeginPlay();
    
    // Configurer le mouvement du projectile avec la vélocité initiale
    if (ProjectileMovement && !InitialVelocity.IsNearlyZero())
    {
        ProjectileMovement->Velocity = InitialVelocity;
        ProjectileMovement->SetVelocityInLocalSpace(InitialVelocity.GetSafeNormal() * FirePower);
        
        UE_LOG(LogTemp, Warning, TEXT("Projectile BeginPlay - Velocity set: %s (magnitude: %.1f)"),
               *ProjectileMovement->Velocity.ToString(),
               ProjectileMovement->Velocity.Size());
    }
    
    // Configurer les acteurs à ignorer
    SetupIgnoredActors();
    
    // Activer les collisions après un délai
    FTimerHandle CollisionTimerHandle;
    GetWorldTimerManager().SetTimer(
        CollisionTimerHandle,
        this,
        &AWormProjectile::EnableCollisions,
        CollisionActivationDelay,
        false
    );
    
    // Démarrer le timer pour l'explosion automatique
    if (HasAuthority())
    {
        GetWorldTimerManager().SetTimer(
            DetonationTimerHandle,
            this,
            &AWormProjectile::Explode,
            DetonationDelay,
            false
        );
    }
}

void AWormProjectile::EnableCollisions()
{
    if (CollisionComp)
    {
        // Activer la simulation physique ET les collisions en même temps
        CollisionComp->SetSimulatePhysics(true);
        CollisionComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        
        // S'assurer que les paramètres de collision sont corrects
        CollisionComp->SetCollisionResponseToAllChannels(ECR_Block);
        
        // Exceptions pour les canaux spécifiques si nécessaire
        CollisionComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
        
        UE_LOG(LogTemp, Warning, TEXT("Projectile collisions and physics activated"));
        
        // Vérifier la vélocité actuelle
        if (ProjectileMovement)
        {
            UE_LOG(LogTemp, Warning, TEXT("Current projectile velocity: %s (magnitude: %.1f)"), 
                *ProjectileMovement->Velocity.ToString(), 
                ProjectileMovement->Velocity.Size());
            
            // Si la vélocité est trop faible, réappliquer une impulsion
            if (ProjectileMovement->Velocity.Size() < 1000.0f)
            {
                UE_LOG(LogTemp, Warning, TEXT("Velocity too low, reapplying!"));
                ProjectileMovement->Velocity = InitialVelocity;
                CollisionComp->AddImpulse(InitialVelocity, NAME_None, true);
            }
        }
    }
}

void AWormProjectile::SetupIgnoredActors()
{
    // Ignorer l'instigateur (le tireur)
    if (GetInstigator())
    {
        CollisionComp->IgnoreActorWhenMoving(GetInstigator(), true);
        
        // Si l'instigateur est un AWormCharacter, ignorer aussi son arme
        AWormCharacter* WormChar = Cast<AWormCharacter>(GetInstigator());
        if (WormChar && WormChar->CurrentWeapon)
        {
            CollisionComp->IgnoreActorWhenMoving(WormChar->CurrentWeapon, true);
        }
    }
    
    // Ignorer tous les Worms et leurs armes pendant un court instant
    TArray<AActor*> AllWormCharacters;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AWormCharacter::StaticClass(), AllWormCharacters);
    for (AActor* Actor : AllWormCharacters)
    {
        CollisionComp->IgnoreActorWhenMoving(Actor, true);
        
        // Ignorer aussi leurs armes
        AWormCharacter* Character = Cast<AWormCharacter>(Actor);
        if (Character && Character->CurrentWeapon)
        {
            CollisionComp->IgnoreActorWhenMoving(Character->CurrentWeapon, true);
        }
    }
    
    // Après un certain délai, ne plus ignorer les autres Worms (seulement le tireur)
    FTimerHandle ResetIgnoreTimerHandle;
    GetWorldTimerManager().SetTimer(
        ResetIgnoreTimerHandle,
        [this, AllWormCharacters]() {
            // Ne plus ignorer les autres Worms sauf l'instigateur
            for (AActor* Actor : AllWormCharacters)
            {
                if (Actor != GetInstigator())
                {
                    CollisionComp->IgnoreActorWhenMoving(Actor, false);
                }
            }
        },
        1.0f, // Après 1 seconde
        false
    );
}

void AWormProjectile::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    
    // Vérifier si le projectile se déplace correctement
    static float TimeSinceLastCheck = 0.0f;
    TimeSinceLastCheck += DeltaTime;
    
    // Ne vérifier que toutes les 0.5 secondes pour réduire les logs
    if (TimeSinceLastCheck >= 0.5f)
    {
        TimeSinceLastCheck = 0.0f;
        
        if (ProjectileMovement)
        {
            // Si le projectile a une vélocité très faible et est proche du sol, il est probablement bloqué
            if (ProjectileMovement->Velocity.Size() < 100.0f && GetActorLocation().Z < 100.0f)
            {
                // Si bloqué, tenter de réappliquer une impulsion ou exploser
                if (HasAuthority())
                {
                    UE_LOG(LogTemp, Warning, TEXT("Stuck projectile: forcing explosion"));
                    Explode();
                }
            }
        }
    }
}

void AWormProjectile::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
    // Vérifier si on est sur le serveur
    if (HasAuthority())
    {
        // Exploser au contact si le projectile touche le terrain ou un personnage
        if (OtherActor != GetInstigator())
        {
            // Log pour voir ce qui a été touché
            UE_LOG(LogTemp, Warning, TEXT("Projectile hit: %s at location %s"), 
                OtherActor ? *OtherActor->GetName() : TEXT("Unknown Actor"),
                *Hit.Location.ToString());
                
            Explode();
        }
    }
}

void AWormProjectile::Explode()
{
    if (HasAuthority())
    {
        // Position de l'explosion
        FVector ExplosionLocation = GetActorLocation();
        
        // Chercher les Worms dans le rayon d'explosion
        TArray<AActor*> OverlappingActors;
        TArray<AActor*> ActorsToIgnore;
        ActorsToIgnore.Add(this);
        
        UKismetSystemLibrary::SphereOverlapActors(
            GetWorld(),
            ExplosionLocation,
            ExplosionRadius,
            TArray<TEnumAsByte<EObjectTypeQuery>>(),
            AWormCharacter::StaticClass(),
            ActorsToIgnore,
            OverlappingActors
        );
        
        // Appliquer les dégâts aux personnages touchés
        for (AActor* Actor : OverlappingActors)
        {
            AWormCharacter* WormChar = Cast<AWormCharacter>(Actor);
            if (WormChar)
            {
                // Calculer la direction de l'impact
                FVector ImpactDirection = WormChar->GetActorLocation() - ExplosionLocation;
                float Distance = ImpactDirection.Size();
                
                // Calculer les dégâts basés sur la distance
                float DamageToApply = ExplosionDamage * (1.0f - FMath::Min(Distance / ExplosionRadius, 1.0f));
                
                // Appliquer les dégâts
                WormChar->ApplyDamageToWorm(DamageToApply, ImpactDirection);
            }
        }
        
        // Détruire le terrain dans la zone d'explosion
        TArray<AActor*> TerrainActors;
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), ADestructibleTerrain::StaticClass(), TerrainActors);
        
        for (AActor* Actor : TerrainActors)
        {
            ADestructibleTerrain* Terrain = Cast<ADestructibleTerrain>(Actor);
            if (Terrain)
            {
                // Transformation pour convertir les coordonnées mondiales en coordonnées locales
                FVector LocalExplosion = Terrain->GetActorTransform().InverseTransformPosition(ExplosionLocation);
                
                // Utiliser les coordonnées appropriées pour le terrain 2D
                float SafeX = FMath::Clamp(LocalExplosion.X, ExplosionRadius, Terrain->TerrainWidth - ExplosionRadius);
                float SafeZ = FMath::Clamp(LocalExplosion.Z, ExplosionRadius, Terrain->TerrainHeight - ExplosionRadius);
                
                // Créer la zone de destruction centrée sur l'explosion
                FVector2D Position2D(SafeX - ExplosionRadius, SafeZ - ExplosionRadius);
                FVector2D Size2D(ExplosionRadius * 2.0f, ExplosionRadius * 2.0f);
                
                // Demander la destruction du terrain
                Terrain->RequestDestroyTerrainAt(Position2D, Size2D);
            }
        }
        
        // Effets multicast d'explosion
        Multicast_Explode(ExplosionLocation);
        
        // Détruire le projectile
        Destroy();
    }
}

void AWormProjectile::Multicast_Explode_Implementation(FVector Location)
{
    // Jouer l'effet d'explosion
    if (ExplosionEffect)
    {
        UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ExplosionEffect, Location);
    }
    
    // Jouer le son d'explosion
    if (ExplosionSound)
    {
        UGameplayStatics::PlaySoundAtLocation(GetWorld(), ExplosionSound, Location);
    }
}
