#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "public/ADestructibleTerrain.h"
#include "Field/FieldSystemComponent.h"
#include "Field/FieldSystemActor.h"
#include "Chaos/ChaosSolverActor.h"
#include "Field/FieldSystemTypes.h"
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

    // Rendre le composant de collision accessible en public
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class USphereComponent* CollisionComp;
    FTimerHandle TrailTimerHandle;

    // Initialiser le projectile avec une direction et une puissance
    UFUNCTION(BlueprintCallable, Category = "Projectile")
    void InitializeProjectile(FVector Direction, float Power);
    UFUNCTION(Server, Reliable, WithValidation)
    void Server_SpawnDestructionField(FVector Location);
    
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_SpawnDestructionField(FVector Location);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Field System")
    TSubclassOf<AFieldSystemActor> FieldSystemActorClass;
    // Dans la section public:
    UFUNCTION()
    void RecordTrailPosition();

    UFUNCTION()
    void DrawDebugTrail();
protected:
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
    void Multicast_SpawnDestructionField_Implementation(FVector Location);

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
    FVector LastHitLocation;
    FVector LastHitNormal;
};