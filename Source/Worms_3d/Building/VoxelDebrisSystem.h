#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "VoxelDebrisSystem.generated.h"

// Small struct to store debris parameters
USTRUCT(BlueprintType)
struct FVoxelDebrisParams
{
    GENERATED_BODY()
    
    // Size of individual debris pieces
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debris")
    float DebrisSize = 0.35f;
    
    // Number of debris pieces to spawn per voxel
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debris")
    int32 DebrisCountPerVoxel = 3;
    
    // Maximum number of debris pieces active at once
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debris")
    int32 MaxDebrisCount = 300;
    
    // How long debris persists before being destroyed (seconds)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debris")
    float DebrisLifetime = 3.0f;
    
    // Force applied to debris at spawning
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debris")
    float ExplosionForce = 500.0f;
    
    // Radius of the explosion force
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debris")
    float ExplosionRadius = 200.0f;
    
    // Whether to apply random rotation to debris
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debris")
    bool bApplyRandomRotation = true;
    
    // Vertical boost to help debris fly upward
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debris")
    float VerticalBoost = 200.0f;
    
    // Whether to use instanced static meshes for debris (more efficient)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debris")
    bool bUseInstancedMeshes = true;
    
    // Whether to enable dust/smoke particles
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects")
    bool bEnableParticleEffects = true;
};

/**
 * Component that handles spawning and managing voxel debris
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class WORMS_3D_API UVoxelDebrisSystem : public USceneComponent
{
    GENERATED_BODY()

public:    
    UVoxelDebrisSystem();
    
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    
    // Spawn debris at the given location with the given color
    UFUNCTION(BlueprintCallable, Category = "Debris")
    void SpawnDebrisAtLocation(FVector Location, FVector ImpactNormal, FColor VoxelColor, int32 Count = 0);
    
    // Spawn multiple debris pieces in a volume
    UFUNCTION(BlueprintCallable, Category = "Debris")
    void SpawnDebrisInVolume(FVector Center, FVector Extent, FVector ImpactNormal, TArray<FColor> Colors);
    
    // Configure debris parameters
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debris")
    FVoxelDebrisParams DebrisParams;
    
    // Mesh to use for debris
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debris")
    TArray<UStaticMesh*> DebrisMeshes;
    
    // Materials to apply to debris
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debris")
    UMaterialInterface* DebrisMaterial;
    
    // Material instance for color manipulation
    UPROPERTY()
    UMaterialInstanceDynamic* DebrisDynamicMaterial;
    
    // Particle system for dust/smoke effects
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects")
    UParticleSystem* DustParticleSystem;
    
    // Alternative Niagara system for more advanced effects
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects")
    UNiagaraSystem* DebrisNiagaraSystem;
    
protected:
    // Manage instanced static mesh components for efficient rendering
    UPROPERTY()
    TArray<class UInstancedStaticMeshComponent*> InstancedMeshComponents;
    
    // Pool of active debris actors
    UPROPERTY()
    TArray<class AVoxelDebrisActor*> ActiveDebris;
    
    // Manage debris lifetimes and cleanup
    void UpdateDebrisLifetimes(float DeltaTime);
    
    // Initialize instanced mesh components if using that rendering approach
    void SetupInstancedMeshComponents();
    
    // Spawn a single debris piece
    void SpawnSingleDebris(FVector Location, FVector ImpactNormal, FColor Color);
    
    // Spawn a dust/smoke particle effect
    void SpawnDustEffect(FVector Location, FVector ImpactNormal);
    
    // Create a dynamic material instance if needed
    void SetupDynamicMaterial();
};

/**
 * Actor class for individual debris pieces
 */
UCLASS()
class WORMS_3D_API AVoxelDebrisActor : public AActor
{
    GENERATED_BODY()
    
public:
    AVoxelDebrisActor();
    
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    
    // Initialize the debris with properties
    UFUNCTION(BlueprintCallable, Category = "Debris")
    void Initialize(UStaticMesh* Mesh, UMaterialInterface* Material, FColor Color, float Size, float Lifetime);
    
    // Apply physics impulse to the debris
    UFUNCTION(BlueprintCallable, Category = "Debris")
    void ApplyImpulse(FVector Direction, float Strength, float VerticalBoost);
    
    // Static mesh component for the debris
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* MeshComponent;
    
    // How long this debris should exist
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debris")
    float RemainingLifetime;
    
    // Fade out effect near end of lifetime
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debris")
    float FadeOutDuration = 0.5f;
    
    // Dynamic material for visual effects
    UPROPERTY()
    UMaterialInstanceDynamic* DynamicMaterial;
};