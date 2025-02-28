#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SplineComponent.h" // Ajout de l'include manquant
#include "WormWeapon.generated.h"

UCLASS()
class WORMS_3D_API AWormWeapon : public AActor
{
    GENERATED_BODY()

public:
    AWormWeapon();

    // Fonction de tir principale
    UFUNCTION(BlueprintCallable, Category = "Weapon")
    virtual void Fire();
    
    // Fonctions pour les animations et effets
    UFUNCTION(BlueprintImplementableEvent, Category = "Weapon")
    void OnFire();
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPowerChangedSignature, float, NewPower, float, NormalizedPower);
    
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    // Dans la section public:
    UPROPERTY(BlueprintAssignable, Category = "Weapon|Aiming")
    FOnPowerChangedSignature OnPowerChangedDelegate;   
    // Ajouter les déclarations des fonctions virtuelles manquantes
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    UFUNCTION(BlueprintCallable, Category = "Weapon")
    void ShowTrajectory(bool bShow);

    UFUNCTION(BlueprintCallable, Category = "Weapon")
    void UpdateTrajectory();

    // Fonctions supplémentaires pour le système de visée
    UFUNCTION(BlueprintCallable, Category = "Weapon|Aiming")
    void AdjustPower(float Delta);

    UFUNCTION(BlueprintCallable, Category = "Weapon|Aiming")
    float GetCurrentPower() const { return FirePower; }

    UFUNCTION(BlueprintCallable, Category = "Weapon|Aiming")
    float GetMinPower() const { return MinFirePower; }

    UFUNCTION(BlueprintCallable, Category = "Weapon|Aiming")
    float GetMaxPower() const { return MaxFirePower; }

    UFUNCTION(BlueprintCallable, Category = "Weapon|Aiming")
    float GetNormalizedPower() const { return (FirePower - MinFirePower) / (MaxFirePower - MinFirePower); }

    // Événement Blueprint appelé lors des changements de puissance
    UFUNCTION(BlueprintImplementableEvent, Category = "Weapon|Aiming")
    void OnPowerChanged(float NewPower, float NormalizedPower);
    UPROPERTY(EditDefaultsOnly, Category = "Weapon|Aiming")
    float PowerAdjustmentStep;

protected:
    UPROPERTY()
    USplineComponent* TrajectorySpline;

    UPROPERTY()
    TArray<UStaticMeshComponent*> TrajectoryPoints;

    UPROPERTY(EditDefaultsOnly, Category = "Weapon")
    UStaticMesh* TrajectoryPointMesh;

    UPROPERTY(EditDefaultsOnly, Category = "Weapon")
    int32 TrajectoryPointCount;

    UPROPERTY(EditDefaultsOnly, Category = "Weapon")
    float SimulationTimeStep;

    UPROPERTY(EditDefaultsOnly, Category = "Weapon")
    float SimulationDuration;
    // Composant mesh pour l'arme
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USkeletalMeshComponent* WeaponMesh;
    
    // Position de départ du projectile
    UPROPERTY(EditDefaultsOnly, Category = "Weapon")
    FName MuzzleSocketName;
    
    // Classe du projectile à spawner
    UPROPERTY(EditDefaultsOnly, Category = "Weapon")
    TSubclassOf<class AActor> ProjectileClass;
    
    // Puissance du tir (vitesse du projectile)
    UPROPERTY(EditDefaultsOnly, Category = "Weapon")
    float FirePower;
    
    // Délai de rechargement
    UPROPERTY(EditDefaultsOnly, Category = "Weapon")
    float ReloadTime;
    
    // Est-ce que l'arme est en train de recharger
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Weapon")
    bool bIsReloading;
    
    // Munitions restantes
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Weapon")
    int32 AmmoCount;
    
    // Munitions max
    UPROPERTY(EditDefaultsOnly, Category = "Weapon")
    int32 MaxAmmo;
    
    // Effet visuel du tir
    UPROPERTY(EditDefaultsOnly, Category = "Effects")
    UParticleSystem* MuzzleEffect;
    
    // Son du tir
    UPROPERTY(EditDefaultsOnly, Category = "Effects")
    USoundBase* FireSound;
    
    // RPC pour le tir (Serveur -> Tous)
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_OnFire();
    
    // Timer pour le rechargement
    FTimerHandle ReloadTimerHandle;
    
    // Fonction appelée après le rechargement
    UFUNCTION()
    void OnReloadComplete();

    UPROPERTY(EditDefaultsOnly, Category = "Weapon|Aiming")
    float MinFirePower;

    UPROPERTY(EditDefaultsOnly, Category = "Weapon|Aiming")
    float MaxFirePower;


    // Feedback visuel supplémentaire
    UPROPERTY(EditDefaultsOnly, Category = "Weapon|Effects")
    UParticleSystem* AimingEffect;

    UPROPERTY()
    UParticleSystemComponent* AimingEffectComponent;

    // Matériau dynamique pour la trajectoire
    UPROPERTY(EditDefaultsOnly, Category = "Weapon|Aiming")
    UMaterialInterface* TrajectoryMaterial;

    UPROPERTY()
    UMaterialInstanceDynamic* TrajectoryMaterialInstance;

    // Pour une prévisualisation améliorée
    UPROPERTY(EditDefaultsOnly, Category = "Weapon|Aiming")
    UStaticMesh* ImpactPreviewMesh;

    UPROPERTY()
    UStaticMeshComponent* ImpactPreviewComponent;
};