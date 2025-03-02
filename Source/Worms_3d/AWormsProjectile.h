#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "public/ADestructibleTerrain.h"
#include "AWormsProjectile.generated.h"

UCLASS()
class WORMS_3D_API AWormProjectile : public AActor
{
    GENERATED_BODY()

public:
    AWormProjectile();

    // Tick function pour surveiller le mouvement
    virtual void Tick(float DeltaTime) override;

    // Rendre le composant de collision accessible en public
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class USphereComponent* CollisionComp;
    
    // Initialiser le projectile avec une direction et une puissance
    UFUNCTION(BlueprintCallable, Category = "Projectile")
    void InitializeProjectile(FVector Direction, float Power);

protected:
    virtual void BeginPlay() override;
    
    // Composant de mouvement projectile
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class UProjectileMovementComponent* ProjectileMovement;
    
    // Mesh du projectile
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* ProjectileMesh;
    
    // Rayon d'explosion
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
    float ExplosionRadius;
    
    // Dégâts de l'explosion
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
    float ExplosionDamage;
    
    // Délai avant explosion auto
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
    float DetonationDelay;
    
    // Effet d'explosion
    UPROPERTY(EditDefaultsOnly, Category = "Effects")
    class UParticleSystem* ExplosionEffect;
    
    // Son d'explosion
    UPROPERTY(EditDefaultsOnly, Category = "Effects")
    class USoundBase* ExplosionSound;
    
    // Callback quand le projectile touche quelque chose
    UFUNCTION()
    void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, 
               FVector NormalImpulse, const FHitResult& Hit);
    
    // Fonction pour l'explosion
    UFUNCTION(BlueprintCallable)
    void Explode();
    
    // Timer pour l'explosion auto
    FTimerHandle DetonationTimerHandle;
    
    // RPC pour l'explosion (Serveur -> Tous)
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_Explode(FVector Location);
    
    // Vélocité initiale du projectile
    UPROPERTY(Replicated)
    FVector InitialVelocity;
    
    // Puissance de tir
    UPROPERTY(Replicated)
    float FirePower;
    
    // Délai avant l'activation des collisions
    UPROPERTY(EditDefaultsOnly, Category = "Projectile")
    float CollisionActivationDelay;
    
    // Fonction pour activer les collisions après un délai
    UFUNCTION()
    void EnableCollisions();
    
    // Fonction pour configurer les acteurs à ignorer
    void SetupIgnoredActors();
};