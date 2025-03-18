#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "AWormsProjectile.generated.h"

UCLASS()
class WORMS_3D_API AWormProjectile : public AActor
{
    GENERATED_BODY()

public:
    AWormProjectile();

    // Tick function pour surveiller le mouvement
    virtual void Tick(float DeltaTime) override;

    UPROPERTY(EditDefaultsOnly, Category = "Projectile", BlueprintReadWrite)
    bool bIsSniperProjectile = false;

    UPROPERTY(EditDefaultsOnly, Category = "Projectile")
    float DamageFalloffExponent = 1.5f; // Default for regular weapons
    // Rendre le composant de collision accessible en public
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class USphereComponent* CollisionComp;
    FTimerHandle TrailTimerHandle;
    
    // Rayon d'explosion
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
    float ExplosionRadius;
    
    // Dégâts de l'explosion
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
    float ExplosionDamage;


    // Fonction pour l'explosion
    UFUNCTION(BlueprintCallable)
    void Explode();
    void DebugDamageCalculation(const FVector& ExplosionLocation, float DynamicExplosionRadius);

    // Initialiser le projectile avec une direction et une puissance
    UFUNCTION(BlueprintCallable, Category = "Projectile")
    void InitializeProjectile(FVector Direction, float Power);
       // Dans la section public:
    UFUNCTION()
    void RecordTrailPosition();

    UFUNCTION()
    void DrawDebugTrail();
protected:

    static TArray<AWormProjectile*> ActiveProjectiles;
    
    UPROPERTY()
    TArray<FVector> TrailPositions;

    UPROPERTY(EditDefaultsOnly, Category = "Debug")
    bool bDebugTrail = true;

    UPROPERTY(EditDefaultsOnly, Category = "Debug")
    float TrailRecordInterval = 0.05f;

    UPROPERTY(EditDefaultsOnly, Category = "Debug")
    int32 MaxTrailPoints = 100;

    UPROPERTY(EditDefaultsOnly, Category = "Debug")
    FLinearColor ServerTrailColor = FLinearColor(1.0f, 0.0f, 0.0f, 1.0f); // Rouge pour le serveur

    UPROPERTY(EditDefaultsOnly, Category = "Debug")
    FLinearColor ClientTrailColor = FLinearColor(0.0f, 0.0f, 1.0f, 1.0f); // Bleu pour le client

    virtual void BeginPlay() override;

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    
    // Composant de mouvement projectile
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class UProjectileMovementComponent* ProjectileMovement;
    
    // Mesh du projectile
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* ProjectileMesh;
    
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
    virtual void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, 
               FVector NormalImpulse, const FHitResult& Hit);

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
    virtual void EnableCollisions();
    
    // Fonction pour configurer les acteurs à ignorer
    void SetupIgnoredActors();
    FVector LastHitLocation;
    FVector LastHitNormal;
    UPROPERTY()
    AActor* LastHitActor;

    UPROPERTY()
    UPrimitiveComponent* LastHitComponent;

    // Enhanced direct hit detection
    UPROPERTY()
    bool bDirectVoxelHit;

    UPROPERTY()
    FVector DirectHitLocation;

    UPROPERTY()
    FVector DirectHitNormal;

    UPROPERTY()
    class AImprovedVoxelBuilding* DirectHitBuilding;

    // Helper function for applying character damage
    void ApplyDamageToCharacters(const FVector& ExplosionLocation, float DynamicExplosionRadius);
};