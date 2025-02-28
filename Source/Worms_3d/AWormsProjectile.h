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

protected:
    virtual void BeginPlay() override;
    
    // Composant pour la collision et le mouvement
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class USphereComponent* CollisionComp;
    
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
    void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
    
    // Fonction pour l'explosion
    UFUNCTION(BlueprintCallable)
    void Explode();
    
    // Timer pour l'explosion auto
    FTimerHandle DetonationTimerHandle;
    
    // RPC pour l'explosion (Serveur -> Tous)
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_Explode(FVector Location);
};