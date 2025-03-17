#pragma once

#include "CoreMinimal.h"
#include "WormWeapon.h"
#include "MeleeBatWeapon.generated.h"

/**
 * Melee bat weapon that hits nearby players with a swinging motion
 */
UCLASS()
class WORMS_3D_API AMeleeBatWeapon : public AWormWeapon
{
    GENERATED_BODY()

public:
    AMeleeBatWeapon();

    // Override Fire function to handle melee attack instead of projectiles
    virtual void Fire() override;

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // The distance the bat can hit
    UPROPERTY(EditDefaultsOnly, Category = "Weapon|Melee")
    float HitDistance;

    // The angle in degrees of the hit detection cone
    UPROPERTY(EditDefaultsOnly, Category = "Weapon|Melee")
    float HitAngle;

    // Base damage for a hit
    UPROPERTY(EditDefaultsOnly, Category = "Weapon|Melee")
    float BaseDamage;

    // Knockback force multiplier
    UPROPERTY(EditDefaultsOnly, Category = "Weapon|Melee")
    float KnockbackForce;

    // Sound for swinging the bat
    UPROPERTY(EditDefaultsOnly, Category = "Weapon|Melee")
    USoundBase* SwingSound;

    // Sound for hitting something
    UPROPERTY(EditDefaultsOnly, Category = "Weapon|Melee")
    USoundBase* HitSound;

    // Effect for hitting something
    UPROPERTY(EditDefaultsOnly, Category = "Weapon|Melee")
    UParticleSystem* HitEffect;

    // Trail effect for bat swing
    UPROPERTY(EditDefaultsOnly, Category = "Weapon|Melee")
    UParticleSystem* SwingTrailEffect;

    // Socket name for the bat's hitting point (tip of the bat)
    UPROPERTY(EditDefaultsOnly, Category = "Weapon|Melee")
    FName BatTipSocketName;

    // Current swing state and timing
    bool bIsSwinging;
    float SwingTime;
    
    UPROPERTY(EditDefaultsOnly, Category = "Weapon|Melee|Animation")
    float SwingDuration;
    
    // Animation parameters
    UPROPERTY(EditDefaultsOnly, Category = "Weapon|Melee|Animation")
    float SwingAngle;
    
    UPROPERTY(EditDefaultsOnly, Category = "Weapon|Melee|Animation")
    FVector SwingAxis;
    
    UPROPERTY(EditDefaultsOnly, Category = "Weapon|Melee|Animation")
    float SwingReturnDelay;
    
    // Animation state
    FRotator OriginalRotation;
    FRotator TargetRotation;
    bool bIsReturning;
    FTimerHandle SwingTimerHandle;
    FTimerHandle ReturnTimerHandle;
    
    // Particle component for the swing trail
    UPROPERTY()
    UParticleSystemComponent* TrailParticleComponent;

    // Trace for melee hits
    void PerformMeleeTrace();

    // Process hit results
    void ProcessHit(AActor* HitActor, const FVector& HitLocation, const FVector& HitDirection);

    // RPC for swing animation and effects
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_Swing();
    
    // Animation functions
    void StartSwingAnimation();
    void UpdateSwingAnimation(float DeltaTime);
    void FinishSwingAnimation();
    void ReturnToOriginalPosition();

    // Flag to track if we've already hit someone during this swing
    // Prevents multiple hits from a single swing
    UPROPERTY()
    TArray<AActor*> AlreadyHitActors;

    // Debug drawing for hit detection
    UPROPERTY(EditDefaultsOnly, Category = "Debug")
    bool bDrawDebugTrace;
};