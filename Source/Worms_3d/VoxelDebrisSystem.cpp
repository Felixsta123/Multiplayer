#include "VoxelDebrisSystem.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "EngineUtils.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "Engine/World.h"

// ============================
// VoxelDebrisActor Implementation
// ============================

AVoxelDebrisActor::AVoxelDebrisActor()
{
    PrimaryActorTick.bCanEverTick = true;
    
    // Create mesh component with physics
    MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DebrisMesh"));
    RootComponent = MeshComponent;
    
    // Configure physics
    MeshComponent->SetSimulatePhysics(true);
    MeshComponent->SetEnableGravity(true);
    MeshComponent->SetCollisionProfileName(TEXT("DebrisProfile"));
    
    // Disable collision with players and projectiles
    MeshComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
    MeshComponent->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Ignore); // Projectiles
    
    // Make debris kinematic at the end of life for smooth fadeout
    MeshComponent->SetGenerateOverlapEvents(false);
    
    // Don't replicate - this is client-side only
    bReplicates = false;
    
    // Default lifetime
    RemainingLifetime = 3.0f;
}

void AVoxelDebrisActor::BeginPlay()
{
    Super::BeginPlay();
}

void AVoxelDebrisActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    
    // Update lifetime and handle fadeout
    RemainingLifetime -= DeltaTime;
    
    // Start fading out when approaching end of life
    if (RemainingLifetime <= FadeOutDuration)
    {
        float FadeAlpha = RemainingLifetime / FadeOutDuration;
        
        // Apply fade through material or direct opacity change
        if (DynamicMaterial)
        {
            DynamicMaterial->SetScalarParameterValue(TEXT("Opacity"), FadeAlpha);
        }
        
        // Make debris float up while fading for a more interesting visual effect
        if (RemainingLifetime < FadeOutDuration * 0.5f)
        {
            // Slow down physics gradually and add upward motion
            MeshComponent->SetLinearDamping(MeshComponent->GetLinearDamping() + DeltaTime * 2.0f);
            MeshComponent->AddForce(FVector(0, 0, 50.0f), NAME_None, true);
        }
        
        // When lifetime is up, destroy the actor
        if (RemainingLifetime <= 0)
        {
            Destroy();
        }
    }
}

void AVoxelDebrisActor::Initialize(UStaticMesh* Mesh, UMaterialInterface* Material, FColor Color, float Size, float Lifetime)
{
    // Set mesh and scale
    if (Mesh)
    {
        MeshComponent->SetStaticMesh(Mesh);
        MeshComponent->SetWorldScale3D(FVector(Size));
    }
    
    // Create dynamic material instance
    if (Material)
    {
        DynamicMaterial = MeshComponent->CreateAndSetMaterialInstanceDynamic(0);
        if (DynamicMaterial)
        {
            // Set color parameter if available
            DynamicMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(Color));
        }
    }
    
    // Set lifetime
    RemainingLifetime = Lifetime;
    
    // Small random rotations for visual variation
    FRotator RandomRotation(FMath::RandRange(-180.0f, 180.0f), 
                           FMath::RandRange(-180.0f, 180.0f), 
                           FMath::RandRange(-180.0f, 180.0f));
    MeshComponent->SetRelativeRotation(RandomRotation);
    
    // Setup physics
    MeshComponent->SetMassScale(NAME_None, 0.5f);
    MeshComponent->SetAngularDamping(0.1f);
    MeshComponent->SetLinearDamping(0.05f);
}

void AVoxelDebrisActor::ApplyImpulse(FVector Direction, float Strength, float VerticalBoost)
{
    if (!MeshComponent)
        return;
    
    // Add more randomness to the direction (increased range for more variety)
    FVector RandomDir = Direction + FVector(
        FMath::RandRange(-0.4f, 0.4f),
        FMath::RandRange(-0.4f, 0.4f),
        FMath::RandRange(0.1f, 0.6f)  // More upward bias for better visual effect
    );
    RandomDir.Normalize();
    
    // Apply vertical boost - improved formula
    RandomDir.Z += VerticalBoost / (Strength + 100.0f); // Prevent division by small numbers
    RandomDir.Normalize();
    
    // Calculate final impulse strength with more randomness
    float FinalStrength = Strength * FMath::RandRange(0.7f, 1.3f);
    
    // Apply the impulse
    MeshComponent->AddImpulse(RandomDir * FinalStrength, NAME_None, true);
    
    // Add some random torque for more realistic spinning
    FVector RandomTorque(
        FMath::RandRange(-1000.0f, 1000.0f),
        FMath::RandRange(-1000.0f, 1000.0f),
        FMath::RandRange(-1000.0f, 1000.0f)
    );
    MeshComponent->AddTorqueInRadians(RandomTorque, NAME_None, true);
}
// ============================
// VoxelDebrisSystem Implementation
// ============================

UVoxelDebrisSystem::UVoxelDebrisSystem()
{
    PrimaryComponentTick.bCanEverTick = true;
    
    // Default debris meshes (cubes, fragments)
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeMeshAsset.Succeeded())
    {
        DebrisMeshes.Add(CubeMeshAsset.Object);
    }
    
    // Default material (created in BeginPlay if not set)
    static ConstructorHelpers::FObjectFinder<UMaterial> DefaultDebrisMaterialAsset(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (DefaultDebrisMaterialAsset.Succeeded())
    {
        DebrisMaterial = DefaultDebrisMaterialAsset.Object;
    }
    
    // Set improved default parameters for FVoxelDebrisParams
    DebrisParams.DebrisSize = 0.3f;               // Slightly smaller debris
    DebrisParams.DebrisCountPerVoxel = 4;         // Increased from 3
    DebrisParams.MaxDebrisCount = 300;            // Keep the same
    DebrisParams.DebrisLifetime = 3.5f;           // Slightly longer lifetime
    DebrisParams.ExplosionForce = 700.0f;         // Increased from 500
    DebrisParams.ExplosionRadius = 250.0f;        // Increased from 200
    DebrisParams.VerticalBoost = 250.0f;          // Higher vertical boost for better visuals
    DebrisParams.bEnableParticleEffects = true;
}

void UVoxelDebrisSystem::BeginPlay()
{
    Super::BeginPlay();
    
    // Setup instanced mesh components if using that approach
    if (DebrisParams.bUseInstancedMeshes)
    {
        SetupInstancedMeshComponents();
    }
    
    // Create dynamic material instance
    SetupDynamicMaterial();
}

void UVoxelDebrisSystem::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    
    // Update debris lifetimes
    UpdateDebrisLifetimes(DeltaTime);
}

void UVoxelDebrisSystem::SpawnDebrisAtLocation(FVector Location, FVector ImpactNormal, FColor VoxelColor, int32 Count)
{
    // If no count specified, use the default per-voxel count
    if (Count <= 0)
    {
        Count = DebrisParams.DebrisCountPerVoxel;
    }
    
    // Enforce the max debris count limit
    int32 SpawnCount = FMath::Min(Count, DebrisParams.MaxDebrisCount - ActiveDebris.Num());
    
    // Spawn dust/smoke particle effect
    if (DebrisParams.bEnableParticleEffects)
    {
        SpawnDustEffect(Location, ImpactNormal);
    }
    
    // Spawn the specified number of debris pieces
    for (int32 i = 0; i < SpawnCount; i++)
    {
        // Add a small random offset from the center point
        FVector Offset = FVector(
            FMath::RandRange(-15.0f, 15.0f),
            FMath::RandRange(-15.0f, 15.0f),
            FMath::RandRange(-15.0f, 15.0f)
        );
        
        // Spawn a single debris piece
        SpawnSingleDebris(Location + Offset, ImpactNormal, VoxelColor);
    }
}

void UVoxelDebrisSystem::SpawnDebrisInVolume(FVector Center, FVector Extent, FVector ImpactNormal, TArray<FColor> Colors)
{
    if (Colors.Num() == 0)
    {
        // Default to light gray if no colors specified
        Colors.Add(FColor(200, 200, 200));
    }
    
    // Calculate how many debris pieces to spawn in this volume
    // Base it on volume size relative to a single voxel
    float VolumeInVoxels = (Extent.X * 2.0f) * (Extent.Y * 2.0f) * (Extent.Z * 2.0f) / (100.0f * 100.0f * 100.0f);
    int32 TotalDebris = FMath::CeilToInt(VolumeInVoxels * DebrisParams.DebrisCountPerVoxel);
    
    // Limit the maximum number of debris for performance
    TotalDebris = FMath::Min(TotalDebris, 50);
    
    // Enforce the max debris count limit
    int32 SpawnCount = FMath::Min(TotalDebris, DebrisParams.MaxDebrisCount - ActiveDebris.Num());
    
    // Spawn a larger dust cloud at the center
    if (DebrisParams.bEnableParticleEffects)
    {
        SpawnDustEffect(Center, ImpactNormal);
        
        // Maybe spawn some additional smaller dust clouds at random points in the volume
        int32 ExtraDustClouds = FMath::Min(3, FMath::FloorToInt(VolumeInVoxels));
        for (int32 i = 0; i < ExtraDustClouds; i++)
        {
            FVector DustLocation = Center + FVector(
                FMath::RandRange(-Extent.X * 0.7f, Extent.X * 0.7f),
                FMath::RandRange(-Extent.Y * 0.7f, Extent.Y * 0.7f),
                FMath::RandRange(-Extent.Z * 0.7f, Extent.Z * 0.7f)
            );
            
            SpawnDustEffect(DustLocation, ImpactNormal);
        }
    }
    
    // Spawn the debris pieces throughout the volume
    for (int32 i = 0; i < SpawnCount; i++)
    {
        // Pick a random point within the volume
        FVector RandomPoint = Center + FVector(
            FMath::RandRange(-Extent.X, Extent.X),
            FMath::RandRange(-Extent.Y, Extent.Y),
            FMath::RandRange(-Extent.Z, Extent.Z)
        );
        
        // Pick a random color from the provided colors
        FColor DebrisColor = Colors[FMath::RandRange(0, Colors.Num() - 1)];
        
        // Spawn a single debris piece
        SpawnSingleDebris(RandomPoint, ImpactNormal, DebrisColor);
    }
}

void UVoxelDebrisSystem::SpawnSingleDebris(FVector Location, FVector ImpactNormal, FColor Color)
{
    if (DebrisMeshes.Num() == 0 || !GetWorld())
        return;
    
    // Add small random offset for each debris piece (reduced range for more precise placement)
    FVector Offset = FVector(
        FMath::RandRange(-8.0f, 8.0f),
        FMath::RandRange(-8.0f, 8.0f),
        FMath::RandRange(-5.0f, 10.0f)  // More upward bias
    );
    
    // Update location with offset
    FVector FinalLocation = Location + Offset;
    
    // Pick a random mesh from the available options
    UStaticMesh* SelectedMesh = DebrisMeshes[FMath::RandRange(0, DebrisMeshes.Num() - 1)];
    
    // Spawn debris actor
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    
    AVoxelDebrisActor* Debris = GetWorld()->SpawnActor<AVoxelDebrisActor>(
        FinalLocation,
        FRotator::ZeroRotator, 
        SpawnParams
    );
    
    if (Debris)
    {
        // Slightly vary the color for more realism
        FLinearColor ModifiedColor = FLinearColor(Color);
        float ColorVariation = FMath::RandRange(-0.07f, 0.07f);
        ModifiedColor.R = FMath::Clamp(ModifiedColor.R + ColorVariation, 0.0f, 1.0f);
        ModifiedColor.G = FMath::Clamp(ModifiedColor.G + ColorVariation, 0.0f, 1.0f);
        ModifiedColor.B = FMath::Clamp(ModifiedColor.B + ColorVariation, 0.0f, 1.0f);
        
        // Initialize with more varied size and lifetime
        Debris->Initialize(
            SelectedMesh,
            DebrisMaterial,
            ModifiedColor.ToFColor(true),
            DebrisParams.DebrisSize * FMath::RandRange(0.7f, 1.3f),  // More varied size
            DebrisParams.DebrisLifetime * FMath::RandRange(0.8f, 1.3f)  // More varied lifetime
        );
        
        // Apply physics impulse
        Debris->ApplyImpulse(
            -ImpactNormal,  // Direction away from impact
            DebrisParams.ExplosionForce,
            DebrisParams.VerticalBoost
        );
        
        // Add to active debris list for management
        ActiveDebris.Add(Debris);
    }
}
void UVoxelDebrisSystem::SpawnDustEffect(FVector Location, FVector ImpactNormal)
{
    // Try Niagara system first if available
    if (DebrisNiagaraSystem)
    {
        UNiagaraComponent* NiagaraComp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            this,
            DebrisNiagaraSystem,
            Location,
            ImpactNormal.Rotation(),
            FVector(1.0f),
            true,
            true,
            ENCPoolMethod::AutoRelease
        );
        
        if (NiagaraComp)
        {
            // Can set parameters on the Niagara system if needed
            // NiagaraComp->SetFloatParameter(TEXT("ExplosionSize"), ExplosionRadius);
            return;
        }
    }
    
    // Fall back to regular particle system if Niagara not available
    if (DustParticleSystem)
    {
        UGameplayStatics::SpawnEmitterAtLocation(
            GetWorld(),
            DustParticleSystem,
            Location,
            ImpactNormal.Rotation(),
            FVector(1.0f),
            true,
            EPSCPoolMethod::AutoRelease
        );
    }
}

void UVoxelDebrisSystem::UpdateDebrisLifetimes(float DeltaTime)
{
    // Remove any null entries from the ActiveDebris array
    for (int32 i = ActiveDebris.Num() - 1; i >= 0; i--)
    {
        if (!ActiveDebris[i] || !IsValid(ActiveDebris[i]))
        {
            ActiveDebris.RemoveAt(i);
        }
    }
}

void UVoxelDebrisSystem::SetupInstancedMeshComponents()
{
    // This function would initialize instanced static mesh components
    // for more efficient rendering if that option is enabled
    // For simplicity, we're using individual actors in this implementation
}

void UVoxelDebrisSystem::SetupDynamicMaterial()
{
    // Create a dynamic material instance if needed
    if (DebrisMaterial && !DebrisDynamicMaterial)
    {
        DebrisDynamicMaterial = UMaterialInstanceDynamic::Create(DebrisMaterial, this);
    }
}