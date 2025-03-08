#include "AWormsProjectile.h"

#include "AVoxelBuilding.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Field/FieldSystemComponent.h"
#include "Field/FieldSystemActor.h"
#include "Field/FieldSystemTypes.h"
#include "Field/FieldSystemObjects.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
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
    //CollisionComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    
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
    
    // Initialiser le tableau des positions du tracé
    TrailPositions.Empty();
    
    // Si le débogage du tracé est activé, commencer à enregistrer les positions
    if (bDebugTrail)
    {
        // Enregistrer la position initiale
        TrailPositions.Add(GetActorLocation());
        
        // Démarrer le timer pour enregistrer les positions suivantes
        GetWorldTimerManager().SetTimer(
            TrailTimerHandle,
            this,
            &AWormProjectile::RecordTrailPosition,
            TrailRecordInterval,
            true // Répéter
        );
    }
    // Activer les collisions après un délai
  
}
void AWormProjectile::RecordTrailPosition()
{
    if (!bDebugTrail)
        return;
    
    // Ajouter la position actuelle au tableau
    TrailPositions.Add(GetActorLocation());
    
    // Limiter le nombre de points pour éviter une consommation excessive de mémoire
    if (TrailPositions.Num() > MaxTrailPoints)
    {
        TrailPositions.RemoveAt(0);
    }
    
    // Dessiner le tracé
    DrawDebugTrail();
}

void AWormProjectile::DrawDebugTrail()
{
    if (!bDebugTrail || TrailPositions.Num() < 2)
        return;
    
    // Déterminer la couleur en fonction de si nous sommes sur le serveur ou le client
    FColor TrailColor = HasAuthority() ? 
        FColor(255, 0, 0) :  // Rouge pour le serveur
        FColor(0, 0, 255);   // Bleu pour le client
    
    // Dessiner des lignes entre chaque position enregistrée
    for (int32 i = 0; i < TrailPositions.Num() - 1; i++)
    {
        DrawDebugLine(
            GetWorld(),
            TrailPositions[i],
            TrailPositions[i + 1],
            TrailColor,
            false,   // Persistant
            TrailRecordInterval * 2.0f,  // Durée (un peu plus que l'intervalle d'enregistrement)
            0,       // Priorité
            1.0f     // Épaisseur
        );
    }
    
    // Dessiner une sphère à la position actuelle pour mieux la voir
    DrawDebugSphere(
        GetWorld(),
        GetActorLocation(),
        10.0f,
        8,
        TrailColor,
        false,
        TrailRecordInterval * 2.0f
    );
    
    // Afficher des informations sur la console
    if (TrailPositions.Num() % 10 == 0)  // Ne pas trop spammer la console
    {
        UE_LOG(LogTemp, Verbose, TEXT("%s Projectile Trail: Pos=%s, Vel=%s"),
            HasAuthority() ? TEXT("[SERVER]") : TEXT("[CLIENT]"),
            *GetActorLocation().ToString(),
            ProjectileMovement ? *ProjectileMovement->Velocity.ToString() : TEXT("N/A")
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
    
    // Créer des TWeakObjectPtr pour la sécurité
    TWeakObjectPtr<AWormProjectile> WeakThis(this);
    TWeakObjectPtr<USphereComponent> WeakCollisionComp(CollisionComp);
    TWeakObjectPtr<APawn> WeakInstigator(GetInstigator());
    
    // Tableau de pointeurs faibles
    TArray<TWeakObjectPtr<AActor>> WeakWormCharacters;
    for (AActor* Actor : AllWormCharacters)
    {
        WeakWormCharacters.Add(TWeakObjectPtr<AActor>(Actor));
    }
    
    // Après un certain délai, ne plus ignorer les autres Worms (seulement le tireur)
    FTimerHandle ResetIgnoreTimerHandle;
    GetWorldTimerManager().SetTimer(
        ResetIgnoreTimerHandle,
        [WeakThis, WeakCollisionComp, WeakInstigator, WeakWormCharacters]() {
            // Vérifier que this et CollisionComp sont toujours valides
            if (!WeakThis.IsValid() || !WeakCollisionComp.IsValid())
            {
                return;
            }
            
            // Ne plus ignorer les autres Worms sauf l'instigator
            for (TWeakObjectPtr<AActor> WeakActor : WeakWormCharacters)
            {
                if (WeakActor.IsValid() && WeakActor.Get() != WeakInstigator.Get())
                {
                    WeakCollisionComp->IgnoreActorWhenMoving(WeakActor.Get(), false);
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
            
            // Pour le débogage, afficher la vélocité actuelle
            if (bDebugTrail && TimeSinceLastCheck < 0.01f) // Juste après la réinitialisation
            {
                UE_LOG(LogTemp, Verbose, TEXT("%s Projectile Velocity: %s (magnitude: %.1f)"),
                    HasAuthority() ? TEXT("[SERVER]") : TEXT("[CLIENT]"),
                    *ProjectileMovement->Velocity.ToString(),
                    ProjectileMovement->Velocity.Size()
                );
            }
        }
    }
}


void AWormProjectile::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
                             FVector NormalImpulse, const FHitResult& Hit)
{
    // Arrêter d'enregistrer les positions du tracé
    if (bDebugTrail)
    {
        GetWorldTimerManager().ClearTimer(TrailTimerHandle);
        
        UE_LOG(LogTemp, Warning, TEXT("%s Projectile hit: %s at %s"),
            HasAuthority() ? TEXT("[SERVER]") : TEXT("[CLIENT]"),
            OtherActor ? *OtherActor->GetName() : TEXT("NULL"),
            *Hit.Location.ToString()
        );
    }
    
    // Sauvegarder les informations d'impact pour utilisation dans Explode
    LastHitLocation = Hit.Location;
    LastHitNormal = Hit.ImpactNormal;
    
    // Ne traiter les collisions que sur le serveur
    if (HasAuthority())
    {
        // Déclencher une explosion
        Explode();
    }
}

void AWormProjectile::Explode()
{
    if (!HasAuthority())
    {
        return; // Ne s'exécute que sur le serveur
    }
    
    // Position de l'explosion
    FVector ExplosionLocation = GetActorLocation();
    
    UE_LOG(LogTemp, Warning, TEXT("Explosion at location: %s with radius: %.1f and damage: %.1f"), 
        *ExplosionLocation.ToString(), ExplosionRadius, ExplosionDamage);
    
    // Chercher les personnages dans le rayon d'explosion
    TArray<AActor*> OverlappingActors;
    TArray<AActor*> ActorsToIgnore;
    ActorsToIgnore.Add(this);
    
    // Utiliser SphereOverlapActors pour détecter tous les acteurs touchés
    UKismetSystemLibrary::SphereOverlapActors(
        GetWorld(),
        ExplosionLocation,
        ExplosionRadius,
        TArray<TEnumAsByte<EObjectTypeQuery>>(),
        AWormCharacter::StaticClass(),
        ActorsToIgnore,
        OverlappingActors
    );
    
    UE_LOG(LogTemp, Warning, TEXT("Found %d actors in explosion radius"), OverlappingActors.Num());
    
    // Appliquer les dégâts aux personnages touchés
    for (AActor* Actor : OverlappingActors)
    {
        AWormCharacter* WormChar = Cast<AWormCharacter>(Actor);
        if (WormChar)
        {
            FVector ImpactDirection = WormChar->GetActorLocation() - ExplosionLocation;
            float Distance = ImpactDirection.Size();

            if (Distance <= 0.0f)
            {
                Distance = 1.0f; // Éviter la division par zéro
                ImpactDirection = FVector(0, 0, 1); // Direction par défaut
            }
            else
            {
                // Normaliser AVANT d'ajuster
                ImpactDirection.Normalize();
                
                // Ajuster la direction pour avoir une composante horizontale plus forte
                // Si la distance est très courte, ajouter plus de composante horizontale
                float HorizontalFactor = FMath::Clamp(1.0f - (Distance / ExplosionRadius), 0.3f, 0.8f);
                
                // Déterminer la direction horizontale dominante (vers l'extérieur de l'explosion)
                FVector HorizontalDir = ImpactDirection;
                HorizontalDir.Z = 0;
                
                if (HorizontalDir.IsNearlyZero())
                {
                    // Si la direction horizontale est presque nulle, choisir une direction aléatoire
                    float RandomAngle = FMath::RandRange(0.0f, 2.0f * PI);
                    HorizontalDir.X = FMath::Cos(RandomAngle);
                    HorizontalDir.Y = FMath::Sin(RandomAngle);
                }
                else
                {
                    HorizontalDir.Normalize();
                }
                
                // Mélanger la composante verticale et horizontale
                ImpactDirection = FVector(
                    HorizontalDir.X * HorizontalFactor,
                    HorizontalDir.Y * HorizontalFactor,
                    0.5f + (0.5f * (1.0f - HorizontalFactor))  // Plus proche = plus haut
                ).GetSafeNormal();
            }

            // Calculer les dégâts basés sur la distance
            float DamageRatio = 1.0f - FMath::Min(Distance / ExplosionRadius, 1.0f);
            float DamageToApply = ExplosionDamage * DamageRatio;

            // Force d'impulsion proportionnelle aux dégâts, mais avec une valeur minimum
            float ImpulseStrength = FMath::Max(1500.0f * DamageRatio, 500.0f);

            UE_LOG(LogTemp, Warning, TEXT("Applying %.1f damage to %s with impulse dir: %s, strength: %.1f"), 
                DamageToApply, *WormChar->GetName(), *ImpactDirection.ToString(), ImpulseStrength);

            // Appliquer les dégâts et l'impulsion
            WormChar->ApplyDamageToWorm(DamageToApply, ImpactDirection * ImpulseStrength);
                        
        }
    }
        
    // Détruire le terrain dans la zone d'explosion
    TArray<AImprovedVoxelBuilding*> NearbyBuildings = AImprovedVoxelBuilding::FindAllVoxelBuildings(this);

    // Utiliser la normale d'impact sauvegardée, ou une valeur par défaut si non disponible
    FVector HitNormal = LastHitNormal;
    if (HitNormal.IsZero())
    {
        // Si pas de normale d'impact disponible, utiliser une direction vers le haut par défaut
        HitNormal = FVector(0, 0, 1);
    }

    for (AImprovedVoxelBuilding* Building : NearbyBuildings)
    {
        if (Building)
        {
            // Vérifier si le bâtiment est dans le rayon d'explosion
            float Distance = FVector::Dist(ExplosionLocation, Building->GetActorLocation());
            if (Distance <= ExplosionRadius * 2.0f) // Rayon un peu plus grand pour les vérifications
            {
                // Appeler la fonction serveur pour détruire les voxels
                // Convertir en coordonnées locales du bâtiment
                FVector LocalExplosion = Building->GetActorTransform().InverseTransformPosition(ExplosionLocation);
                Building->Server_DestroyVoxelsAt(LocalExplosion, HitNormal, ExplosionRadius);
            }
        }
    }
    
    // Effets multicast d'explosion
    Multicast_Explode(ExplosionLocation);
    
    // Détruire le projectile
    Destroy();
}
void AWormProjectile::Server_SpawnDestructionField_Implementation(FVector Location)
{
    Multicast_SpawnDestructionField(Location);
}
void AWormProjectile::Multicast_SpawnDestructionField_Implementation(FVector Location)
{
    if (!GetWorld()) return;

    // Spawn un Field System Actor
    AFieldSystemActor* FieldActor = GetWorld()->SpawnActor<AFieldSystemActor>(AFieldSystemActor::StaticClass(), Location, FRotator::ZeroRotator);
    if (!FieldActor) return;

    UFieldSystemComponent* FieldSystem = FieldActor->GetFieldSystemComponent();
    if (!FieldSystem) return;

    // Création d'un Radial Falloff Field
    URadialFalloff* RadialFalloff = NewObject<URadialFalloff>();
    RadialFalloff->Magnitude = -5000.0f;   // Force de destruction (doit être négative)
    RadialFalloff->Radius = 300.0f;       // Rayon d'affectation
    RadialFalloff->Position = Location;
    RadialFalloff->Falloff = EFieldFalloffType::Field_FallOff_None;

    // Création d'une force linéaire
    UUniformVector* LinearForce = NewObject<UUniformVector>();
    LinearForce->Magnitude = 2000.0f;
    LinearForce->Direction = FVector(0, 0, 1); // Force vers le haut

    // Application au Field System
    FieldSystem->ApplyPhysicsField(true, EFieldPhysicsType::Field_LinearForce, nullptr, LinearForce);
    FieldSystem->ApplyPhysicsField(true, EFieldPhysicsType::Field_ExternalClusterStrain, nullptr, RadialFalloff);

    // Détruire après quelques secondes
    FieldActor->SetLifeSpan(2.0f);
}

bool AWormProjectile::Server_SpawnDestructionField_Validate(FVector Location)
{
    return true; // Toujours valide
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
