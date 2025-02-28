#include "AWormsProjectile.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "AWormCharacter.h"

AWormProjectile::AWormProjectile()
{
    PrimaryActorTick.bCanEverTick = true;
    
    // Configurer la réplication
    bReplicates = true;
    
    // Créer et configurer le composant de collision
    CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComp"));
    CollisionComp->InitSphereRadius(5.0f);
    CollisionComp->SetCollisionProfileName("Projectile");
    RootComponent = CollisionComp;
    
    // Ajouter callback pour les collisions
    CollisionComp->OnComponentHit.AddDynamic(this, &AWormProjectile::OnHit);
    
    // Créer le composant de mesh
    ProjectileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileMesh"));
    ProjectileMesh->SetupAttachment(RootComponent);
    ProjectileMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    
    // Créer le composant de mouvement projectile
    ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileComp"));
    ProjectileMovement->UpdatedComponent = CollisionComp;
    ProjectileMovement->InitialSpeed = 3000.0f;
    ProjectileMovement->MaxSpeed = 3000.0f;
    ProjectileMovement->bRotationFollowsVelocity = true;
    ProjectileMovement->bShouldBounce = true;
    ProjectileMovement->Bounciness = 0.3f;
    
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
    
    // Démarrer le timer pour l'explosion automatique
    if (HasAuthority())
    {
        GetWorldTimerManager().SetTimer(DetonationTimerHandle, this, &AWormProjectile::Explode, DetonationDelay, false);
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