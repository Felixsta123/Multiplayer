#include "AWormsProjectile.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "AWormCharacter.h"

// Variables pour stocker la puissance initiale et la direction
float AWormProjectile::InitialFirePower = 3000.0f;
FVector AWormProjectile::FiringDirection = FVector::ForwardVector;

AWormProjectile::AWormProjectile()
{
    PrimaryActorTick.bCanEverTick = true;
    
    // Configurer la réplication
    bReplicates = true;
    
    // Créer et configurer le composant de collision
    CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComp"));
    CollisionComp->InitSphereRadius(5.0f);
    CollisionComp->SetCollisionProfileName("Projectile");
    
    // Définir les canaux de collision - Ignorer les acteurs Pawn pour éviter de bloquer le tir
    CollisionComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
    
    // IMPORTANT: TEMPORAIREMENT désactiver les collisions - elles seront activées plus tard
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
    ProjectileMovement->ProjectileGravityScale = 1.0f;  // S'assurer que la gravité est activée
    
    // Configurer l'explosion
    ExplosionRadius = 200.0f;
    ExplosionDamage = 25.0f;
    DetonationDelay = 3.0f;
    
    // Définir la durée de vie automatique
    InitialLifeSpan = 10.0f;
}

void AWormProjectile::BeginPlay()
{
    Super::BeginPlay();
    
    // Obtenir la puissance et la direction initiales
    if (ProjectileMovement)
    {
        // Tenter d'ajuster la vitesse et la direction si nécessaire
        if (InitialFirePower > 0.0f && !FiringDirection.IsZero())
        {
            ProjectileMovement->Velocity = FiringDirection * InitialFirePower;
            ProjectileMovement->InitialSpeed = InitialFirePower;
            ProjectileMovement->MaxSpeed = InitialFirePower * 1.5f;
            
            // Forcer une mise à jour immédiate de la vélocité
            ProjectileMovement->SetVelocityInLocalSpace(FVector(1.0f, 0.0f, 0.0f) * InitialFirePower);
            
            UE_LOG(LogTemp, Warning, TEXT("Projectile BeginPlay - Velocity reset: %s (magnitude: %.1f)"),
                *ProjectileMovement->Velocity.ToString(),
                ProjectileMovement->Velocity.Size());
        }
    }
    
    // Appliquer une impulsion additionnelle au démarrage
    if (CollisionComp)
    {
        CollisionComp->AddImpulse(FiringDirection * InitialFirePower * 2.0f, NAME_None, true);
        UE_LOG(LogTemp, Warning, TEXT("Applied additional impulse: %s"), 
               *(FiringDirection * InitialFirePower * 2.0f).ToString());
    }
    
    // Activer les collisions après un court délai pour éviter les collisions avec le tireur
    FTimerHandle CollisionTimerHandle;
    GetWorldTimerManager().SetTimer(
        CollisionTimerHandle,
        [this]() {
            if (CollisionComp)
            {
                CollisionComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
                
                // Log pour confirmer l'activation des collisions
                UE_LOG(LogTemp, Warning, TEXT("Projectile collisions activated"));
                
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
                        ProjectileMovement->Velocity = FiringDirection * InitialFirePower;
                        CollisionComp->AddImpulse(FiringDirection * InitialFirePower * 5.0f, NAME_None, true);
                    }
                }
            }
        },
        0.2f, // délai augmenté à 0.2 seconde
        false // ne pas répéter
    );
    
    // Vérifier le propriétaire et ignorer les collisions avec lui
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
    
    // Ignorer tous les Worms et leurs armes
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
    
    // Log la position de départ et la vélocité
    UE_LOG(LogTemp, Warning, TEXT("Projectile BeginPlay - Start Position: %s, Velocity: %s"),
        *GetActorLocation().ToString(),
        ProjectileMovement ? *ProjectileMovement->Velocity.ToString() : TEXT("No ProjectileMovement"));
    
    // Démarrer le timer pour l'explosion automatique
    if (HasAuthority())
    {
        GetWorldTimerManager().SetTimer(DetonationTimerHandle, this, &AWormProjectile::Explode, DetonationDelay, false);
    }
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
                UE_LOG(LogTemp, Warning, TEXT("Projectile appears stuck - velocity: %s (magnitude: %.1f), position: %s"),
                    *ProjectileMovement->Velocity.ToString(),
                    ProjectileMovement->Velocity.Size(),
                    *GetActorLocation().ToString());
                
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
        
        // Créer un effet de débogage pour visualiser le point d'impact
        DrawDebugSphere(GetWorld(), ExplosionLocation, 50.0f, 12, FColor::Red, false, 5.0f);
        
        // Chercher les Worms dans le rayon d'explosion en utilisant SphereOverlapActors
        TArray<AActor*> OverlappingActors;
        TArray<AActor*> ActorsToIgnore;
        ActorsToIgnore.Add(this);
        
        // Utiliser la fonction correcte pour trouver les acteurs dans une sphère
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
                
                UE_LOG(LogTemp, Warning, TEXT("Explosion at world pos (%f, %f, %f), CONVERTED to local terrain pos (%f, %f, %f)"),
                    ExplosionLocation.X, ExplosionLocation.Y, ExplosionLocation.Z,
                    LocalExplosion.X, LocalExplosion.Y, LocalExplosion.Z);
                
                // CORRECTION: Utiliser les bonnes coordonnées pour le terrain 2D
                // Dans ce cas, on utilise X et Z pour les coordonnées 2D du terrain
                float SafeX = FMath::Clamp(LocalExplosion.X, ExplosionRadius, Terrain->TerrainWidth - ExplosionRadius);
                float SafeZ = FMath::Clamp(LocalExplosion.Z, ExplosionRadius, Terrain->TerrainHeight - ExplosionRadius);
                
                // Créer la zone de destruction centrée sur l'explosion
                FVector2D Position2D(SafeX - ExplosionRadius, SafeZ - ExplosionRadius);
                FVector2D Size2D(ExplosionRadius * 2.0f, ExplosionRadius * 2.0f);
                
                // Dessiner un debug box pour visualiser la zone de destruction
                FVector BoxCenter = Terrain->GetActorTransform().TransformPosition(
                    FVector(SafeX, LocalExplosion.Y, SafeZ));
                FVector BoxExtent = FVector(ExplosionRadius, ExplosionRadius, ExplosionRadius);
                DrawDebugBox(GetWorld(), BoxCenter, BoxExtent, FColor::Green, false, 5.0f);
                
                UE_LOG(LogTemp, Warning, TEXT("Creating destruction zone at (%f, %f) with size (%f, %f)"),
                    Position2D.X, Position2D.Y, Size2D.X, Size2D.Y);
                
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