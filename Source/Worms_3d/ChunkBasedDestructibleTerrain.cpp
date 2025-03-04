#include "ChunkBasedDestructibleTerrain.h"

#include "MaterialDomain.h"
#include "TerrainChunk.h"
#include "Kismet/GameplayStatics.h"
#include "Components/SceneComponent.h"
#include "ProceduralMeshComponent.h"
#include "Net/UnrealNetwork.h"

AChunkBasedDestructibleTerrain::AChunkBasedDestructibleTerrain()
{
    PrimaryActorTick.bCanEverTick = true;

    // Configure network properties
    bReplicates = true;
    bAlwaysRelevant = true;
    NetUpdateFrequency = 5.0f;

    // Create root scene component
    RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootSceneComponent"));
    RootComponent = RootSceneComponent;

    // Default values
    TerrainSize = FVector(5000.0f, 5000.0f, 2000.0f);
    VoxelSize = 50.0f;
    ChunkSize = FIntVector(16, 16, 16);
    bGenerateOnBeginPlay = true;
    ChunkVisibilityDistance = 3000.0f;
    TerrainType = ETerrainType::Flat;
    HeightScale = 1000.0f;
    NoiseScale = 0.01f;
    RandomSeed = 0;
    FlatTerrainHeight = 0.5f;
    bUseLOD = true;
    SmoothingIterations = 1;
    bIsGenerating = false;
    ChunksPerFrame = 2; // Generate 2 chunks per frame during async generation

    // Use FObjectFinder in the constructor
    static ConstructorHelpers::FObjectFinder<UStaticMesh> MeshAsset(TEXT("StaticMesh'/Engine/BasicShapes/Cube.Cube'"));
    if (MeshAsset.Succeeded())
    {
        DebugMesh = MeshAsset.Object;
    }
}

void AChunkBasedDestructibleTerrain::GenerateTerrain()
{
    // Reset existing terrain
    ResetTerrain();

    // Placer le terrain à une position visible
    SetActorLocation(FVector(0, 0, 0));

    // Créer un simple mesh visible pour test
    UStaticMeshComponent* TestMesh = NewObject<UStaticMeshComponent>(this);
    TestMesh->RegisterComponent();
    TestMesh->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);

    // Set the static mesh
    if (DebugMesh)
    {
        TestMesh->SetStaticMesh(DebugMesh);
        TestMesh->SetRelativeLocation(FVector(0, 0, 0));
        TestMesh->SetWorldScale3D(FVector(10.0f)); // Grand cube pour test

        // Matériau de base
        UMaterialInstanceDynamic* DynMat = TestMesh->CreateAndSetMaterialInstanceDynamic(0);
        if (DynMat)
        {
            DynMat->SetVectorParameterValue("Color", FLinearColor(1.0f, 0.5f, 0.0f)); // Orange
        }

        // Activer la collision
        TestMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

        UE_LOG(LogTemp, Error, TEXT("TEST CUBE CRÉÉ"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("IMPOSSIBLE DE TROUVER LE MESH CUBE"));
    }
}

void AChunkBasedDestructibleTerrain::BeginPlay()
{
    Super::BeginPlay();
    
    // Only initialize on server
    if (HasAuthority() && bGenerateOnBeginPlay)
    {
        FTimerHandle Timer;
        GetWorld()->GetTimerManager().SetTimer(
            Timer, 
            [this]() { GenerateTerrain(); }, 
            0.5f, // Delay generation to allow other components to initialize
            false
        );
    }
}

void AChunkBasedDestructibleTerrain::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    
    // In editor, we might want to visualize the terrain bounds
    if (GIsEditor && !HasAuthority())
    {
        // Debug visualization of terrain bounds could be added here
    }
}

void AChunkBasedDestructibleTerrain::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    
    // Process any pending modifications
    ProcessPendingModifications();
    
    // Continue async chunk generation if needed
    if (bIsGenerating)
    {
        GenerateChunksAsync();
    }
    
    // Update chunk visibility based on player position(s)
    UpdateChunkVisibility();
    
    // Replicate modified chunks if needed
    if (HasAuthority())
    {
        ReplicateModifiedChunks();
    }
}

void AChunkBasedDestructibleTerrain::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    
    // Replicate the terrain modifications
    DOREPLIFETIME(AChunkBasedDestructibleTerrain, ReplicatedChunkModifications);
}

void AChunkBasedDestructibleTerrain::OnRep_ModifiedChunks()
{
    // Apply modifications that were received from the server
    ApplyReplicatedModifications(ReplicatedChunkModifications);
}

void AChunkBasedDestructibleTerrain::ResetTerrain()
{
    // Clear all chunks
    for (auto& Pair : Chunks)
    {
        if (Pair.Value)
        {
            // Remove the mesh component
            if (Pair.Value->GetMeshComponent())
            {
                Pair.Value->GetMeshComponent()->DestroyComponent();
            }
            
            // Remove the chunk object
            Pair.Value->ConditionalBeginDestroy();
        }
    }
    
    // Clear the collections
    Chunks.Empty();
    ChunkIDsByPosition.Empty();
    PendingModifications.Empty();
    AppliedModifications.Empty();
    
    // Reset generation state
    bIsGenerating = false;
}

void AChunkBasedDestructibleTerrain::CreateChunkMeshComponents()
{
    // Calculer combien de chunks nous avons dans chaque dimension
    int32 ChunksX = FMath::CeilToInt(TerrainSize.X / (ChunkSize.X * VoxelSize));
    int32 ChunksY = FMath::CeilToInt(TerrainSize.Y / (ChunkSize.Y * VoxelSize));
    int32 ChunksZ = FMath::CeilToInt(TerrainSize.Z / (ChunkSize.Z * VoxelSize));
    
    UE_LOG(LogTemp, Error, TEXT("Creating %d x %d x %d chunks with size %d x %d x %d (voxel size: %.1f)"), 
        ChunksX, ChunksY, ChunksZ, ChunkSize.X, ChunkSize.Y, ChunkSize.Z, VoxelSize);
    
    // Detach all existing mesh components
    TArray<USceneComponent*> ChildComponents;
    RootComponent->GetChildrenComponents(false, ChildComponents);
    for (USceneComponent* Child : ChildComponents)
    {
        Child->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
        Child->DestroyComponent();
    }
    
    // Déterminer l'origine du terrain
    FVector Origin = GetActorLocation() - TerrainSize * 0.5f;
    UE_LOG(LogTemp, Error, TEXT("Terrain origin at: %s"), *Origin.ToString());
    
    // Pour le test, créons seulement un petit nombre de chunks centraux
    ChunksX = FMath::Min(ChunksX, 3);
    ChunksY = FMath::Min(ChunksY, 3);
    ChunksZ = FMath::Min(ChunksZ, 3);
    
    // Créer mesh components pour chaque chunk
    for (int32 z = 0; z < ChunksZ; z++)
    {
        for (int32 y = 0; y < ChunksY; y++)
        {
            for (int32 x = 0; x < ChunksX; x++)
            {
                FIntVector ChunkPos(x, y, z);
                FString ComponentName = FString::Printf(TEXT("TerrainMesh_%d_%d_%d"), x, y, z);
                
                // Créer procedural mesh component
                UProceduralMeshComponent* MeshComponent = NewObject<UProceduralMeshComponent>(this, *ComponentName);
                MeshComponent->RegisterComponent();
                MeshComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
                
                // Set position
                FVector ChunkOrigin = Origin + FVector(
                    x * ChunkSize.X * VoxelSize,
                    y * ChunkSize.Y * VoxelSize,
                    z * ChunkSize.Z * VoxelSize
                );
                MeshComponent->SetRelativeLocation(ChunkOrigin);
                
                // Assigner le matériau si spécifié
                if (TerrainMaterial)
                {
                    MeshComponent->SetMaterial(0, TerrainMaterial);
                }
                else
                {
                    // Créer un matériau de base pour le test
                    UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(
                        UMaterial::GetDefaultMaterial(MD_Surface), this);
                    if (DynMat)
                    {
                        DynMat->SetVectorParameterValue("Color", FLinearColor(0.5f, 0.3f, 0.1f));
                        MeshComponent->SetMaterial(0, DynMat);
                    }
                }
                
                // Configurer la collision
                MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
                MeshComponent->SetCollisionObjectType(ECollisionChannel::ECC_WorldStatic);
                
                // Créer une instance de chunk
                UTerrainChunk* Chunk = NewObject<UTerrainChunk>(this);
                Chunk->Initialize(ChunkPos, ChunkSize, MeshComponent, ChunkOrigin);
                
                // Stocker le chunk
                Chunks.Add(Chunk->GetChunkID(), Chunk);
                ChunkIDsByPosition.Add(ChunkPos, Chunk->GetChunkID());
                
                UE_LOG(LogTemp, Error, TEXT("Created chunk %s at position %s"), 
                    *Chunk->GetChunkID(), *ChunkOrigin.ToString());
            }
        }
    }
    
    UE_LOG(LogTemp, Error, TEXT("Created %d mesh components"), Chunks.Num());
}


void AChunkBasedDestructibleTerrain::InitializeChunks()
{
    // This will be called after CreateChunkMeshComponents
    // Configure the voxel size for all chunks
    for (auto& Pair : Chunks)
    {
        UTerrainChunk* Chunk = Pair.Value;
        if (Chunk)
        {
            // Set the voxel size
            Chunk->GetVoxelData().SetVoxelSize(VoxelSize);
        }
    }
}

void AChunkBasedDestructibleTerrain::GenerateChunksAsync()
{
    // Calculate how many chunks we have in each dimension
    int32 ChunksX = FMath::CeilToInt(TerrainSize.X / (ChunkSize.X * VoxelSize));
    int32 ChunksY = FMath::CeilToInt(TerrainSize.Y / (ChunkSize.Y * VoxelSize));
    int32 ChunksZ = FMath::CeilToInt(TerrainSize.Z / (ChunkSize.Z * VoxelSize));
    
    int32 ChunksProcessed = 0;
    bool bAllChunksGenerated = true;
    
    // Process chunks
    while (ChunksProcessed < ChunksPerFrame)
    {
        // Check if we've reached the end
        if (NextChunkToGenerate.X >= ChunksX)
        {
            bIsGenerating = false;
            break;
        }
        
        // Generate the next chunk
        FIntVector ChunkPos = NextChunkToGenerate;
        
        // Find the chunk
        UTerrainChunk* Chunk = nullptr;
        FString* ChunkIDPtr = ChunkIDsByPosition.Find(ChunkPos);
        if (ChunkIDPtr)
        {
            Chunk = Chunks.FindRef(*ChunkIDPtr);
        }
        
        // If we found the chunk, generate its terrain
        if (Chunk)
        {
            GenerateChunkGeometry(Chunk);
            ChunksProcessed++;
        }
        
        // Move to next chunk position
        NextChunkToGenerate.Y++;
        if (NextChunkToGenerate.Y >= ChunksY)
        {
            NextChunkToGenerate.Y = 0;
            NextChunkToGenerate.Z++;
            if (NextChunkToGenerate.Z >= ChunksZ)
            {
                NextChunkToGenerate.Z = 0;
                NextChunkToGenerate.X++;
                
                if (NextChunkToGenerate.X >= ChunksX)
                {
                    // We've processed all chunks
                    bIsGenerating = false;
                    UE_LOG(LogTemp, Warning, TEXT("All chunks generated successfully"));
                    break;
                }
            }
        }
        
        bAllChunksGenerated = false;
    }
    
    // If we've generated all chunks, update their visibility
    if (bAllChunksGenerated)
    {
        UpdateChunksAroundPlayer();
    }
}

void AChunkBasedDestructibleTerrain::GenerateChunkGeometry(UTerrainChunk* Chunk)
{
    if (!Chunk)
    {
        return;
    }
    
    // Skip if already generated
    if (Chunk->GetState() == EChunkState::Generated)
    {
        return;
    }
    
    // Generate based on the terrain type
    switch (TerrainType)
    {
        case ETerrainType::Flat:
        {
            // Calculate surface height
            int32 SurfaceHeight = FMath::FloorToInt(ChunkSize.Z * FlatTerrainHeight);
            Chunk->GenerateTerrainWithSurface(SurfaceHeight);
            break;
        }
        case ETerrainType::Hills:
        case ETerrainType::Mountains:
        {
            // For hills and mountains, use noise with different scale
            float NoiseMultiplier = (TerrainType == ETerrainType::Mountains) ? 2.0f : 1.0f;
            Chunk->GenerateRandomTerrain(HeightScale, NoiseScale * NoiseMultiplier);
            break;
        }
        case ETerrainType::Islands:
        {
            // For islands, we'll use a special technique (simplified here)
            Chunk->GenerateRandomTerrain(HeightScale * 0.5f, NoiseScale * 1.5f);
            break;
        }
        case ETerrainType::Custom:
        {
            // For custom, we'll just generate a flat terrain for now
            Chunk->GenerateTerrainWithSurface(ChunkSize.Z / 2);
            break;
        }
    }
    
    // Update the mesh from the voxel data
    Chunk->UpdateMeshData();
    Chunk->ApplyMeshData();
}

void AChunkBasedDestructibleTerrain::ApplyTerrainModification(const FVector& Position, float Radius, float Falloff)
{
    // Call server RPC if we're on a client
    if (GetLocalRole() < ROLE_Authority)
    {
        Server_ApplyTerrainModification(Position, Radius, Falloff);
        return;
    }
    
    // Create a modification info
    FTerrainModificationInfo Modification("Explosion", Position, Radius, Falloff);
    Modification.Timestamp = GetWorld()->GetTimeSeconds();
    
    // Calculate which chunks are affected
    CalculateAffectedChunks(Modification);
    
    // Add to pending modifications
    PendingModifications.Add(Modification);
    
    // Notify clients about the modification
    TArray<uint8> SerializedData = Modification.Serialize();
    Multicast_NotifyTerrainModification(SerializedData);
}

bool AChunkBasedDestructibleTerrain::Server_ApplyTerrainModification_Validate(const FVector& Position, float Radius, float Falloff)
{
    // Simple validation - reject extreme values
    return Radius > 0.0f && Radius < 2000.0f && Falloff >= 0.0f && Falloff <= 1.0f;
}

void AChunkBasedDestructibleTerrain::Server_ApplyTerrainModification_Implementation(const FVector& Position, float Radius, float Falloff)
{
    // Call the actual implementation
    ApplyTerrainModification(Position, Radius, Falloff);
}

UTerrainChunk* AChunkBasedDestructibleTerrain::GetChunkAt(const FIntVector& ChunkPosition)
{
    // Find the chunk ID for this position
    FString* IDPtr = ChunkIDsByPosition.Find(ChunkPosition);
    if (!IDPtr)
    {
        return nullptr;
    }
    
    // Find the chunk with this ID
    UTerrainChunk** ChunkPtr = Chunks.Find(*IDPtr);
    if (!ChunkPtr)
    {
        return nullptr;
    }
    
    return *ChunkPtr;
}

UTerrainChunk* AChunkBasedDestructibleTerrain::GetChunkAtWorldPosition(const FVector& WorldPosition)
{
    // Convert world position to chunk position
    FIntVector ChunkPos = WorldPositionToChunkPosition(WorldPosition);
    
    // Get the chunk at that position
    return GetChunkAt(ChunkPos);
}

FIntVector AChunkBasedDestructibleTerrain::WorldPositionToChunkPosition(const FVector& WorldPosition) const
{
    // Determine origin of the terrain
    FVector Origin = GetActorLocation() - TerrainSize * 0.5f;
    
    // Calculate relative position
    FVector RelativePos = WorldPosition - Origin;
    
    // Convert to chunk position
    return FIntVector(
        FMath::FloorToInt(RelativePos.X / (ChunkSize.X * VoxelSize)),
        FMath::FloorToInt(RelativePos.Y / (ChunkSize.Y * VoxelSize)),
        FMath::FloorToInt(RelativePos.Z / (ChunkSize.Z * VoxelSize))
    );
}

FVector AChunkBasedDestructibleTerrain::ChunkPositionToWorldOrigin(const FIntVector& ChunkPosition) const
{
    // Determine origin of the terrain
    FVector Origin = GetActorLocation() - TerrainSize * 0.5f;
    
    // Calculate chunk origin
    return Origin + FVector(
        ChunkPosition.X * ChunkSize.X * VoxelSize,
        ChunkPosition.Y * ChunkSize.Y * VoxelSize,
        ChunkPosition.Z * ChunkSize.Z * VoxelSize
    );
}

TArray<UTerrainChunk*> AChunkBasedDestructibleTerrain::GetChunksInSphere(const FVector& Center, float Radius)
{
    TArray<UTerrainChunk*> AffectedChunks;
    
    // Calculate the chunks that might be affected
    float ChunkDiagonal = FMath::Sqrt(
        FMath::Square(ChunkSize.X * VoxelSize) +
        FMath::Square(ChunkSize.Y * VoxelSize) +
        FMath::Square(ChunkSize.Z * VoxelSize)
    );
    
    // Expand the radius to ensure we get all potentially affected chunks
    float ExpandedRadius = Radius + ChunkDiagonal;
    
    // Calculate affected chunk bounds
    FIntVector CenterChunkPos = WorldPositionToChunkPosition(Center);
    int32 ChunkRadius = FMath::CeilToInt(ExpandedRadius / (ChunkSize.X * VoxelSize)) + 1;
    
    // Calculate how many chunks we have in each dimension
    int32 ChunksX = FMath::CeilToInt(TerrainSize.X / (ChunkSize.X * VoxelSize));
    int32 ChunksY = FMath::CeilToInt(TerrainSize.Y / (ChunkSize.Y * VoxelSize));
    int32 ChunksZ = FMath::CeilToInt(TerrainSize.Z / (ChunkSize.Z * VoxelSize));
    
    // Iterate through potentially affected chunks
    for (int32 z = FMath::Max(0, CenterChunkPos.Z - ChunkRadius); z <= FMath::Min(ChunksZ - 1, CenterChunkPos.Z + ChunkRadius); z++)
    {
        for (int32 y = FMath::Max(0, CenterChunkPos.Y - ChunkRadius); y <= FMath::Min(ChunksY - 1, CenterChunkPos.Y + ChunkRadius); y++)
        {
            for (int32 x = FMath::Max(0, CenterChunkPos.X - ChunkRadius); x <= FMath::Min(ChunksX - 1, CenterChunkPos.X + ChunkRadius); x++)
            {
                FIntVector ChunkPos(x, y, z);
                
                // Check if this chunk actually intersects the sphere
                FVector ChunkOrigin = ChunkPositionToWorldOrigin(ChunkPos);
                FVector ChunkCenter = ChunkOrigin + FVector(
                    ChunkSize.X * VoxelSize * 0.5f,
                    ChunkSize.Y * VoxelSize * 0.5f,
                    ChunkSize.Z * VoxelSize * 0.5f
                );
                
                float ChunkToCenterDist = FVector::Dist(ChunkCenter, Center);
                if (ChunkToCenterDist <= ExpandedRadius)
                {
                    // This chunk might be affected, get the actual chunk
                    UTerrainChunk* Chunk = GetChunkAt(ChunkPos);
                    if (Chunk)
                    {
                        AffectedChunks.Add(Chunk);
                    }
                }
            }
        }
    }
    
    return AffectedChunks;
}

void AChunkBasedDestructibleTerrain::UpdateChunkVisibility()
{
    // Only update periodically (e.g., every 0.5 seconds)
    static float LastUpdateTime = 0.0f;
    float CurrentTime = GetWorld()->GetTimeSeconds();
    
    if (CurrentTime - LastUpdateTime < 0.5f)
    {
        return;
    }
    
    LastUpdateTime = CurrentTime;
    
    // Update visibility based on player positions
    UpdateChunksAroundPlayer();
}

void AChunkBasedDestructibleTerrain::UpdateChunksAroundPlayer()
{
    // Get all player controllers
    TArray<APlayerController*> PlayerControllers;
    for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        if (PC)
        {
            PlayerControllers.Add(PC);
        }
    }
    
    if (PlayerControllers.Num() == 0)
    {
        return;
    }
    
    // Hide all chunks first
    /*for (auto& Pair : Chunks)
    {
        if (Pair.Value)
        {
            Pair.Value->SetVisibility(false);
        }
    }
    */
    
    // Show chunks around players
    for (APlayerController* PC : PlayerControllers)
    {
        if (!PC)
        {
            continue;
        }
        
        APawn* Pawn = PC->GetPawn();
        if (!Pawn)
        {
            continue;
        }
        
        FVector PlayerPos = Pawn->GetActorLocation();
        
        // Get chunks near the player
        FIntVector CenterChunkPos = WorldPositionToChunkPosition(PlayerPos);
        int32 ChunkRadius = FMath::CeilToInt(ChunkVisibilityDistance / (ChunkSize.X * VoxelSize)) + 1;
        
        // Calculate how many chunks we have in each dimension
        int32 ChunksX = FMath::CeilToInt(TerrainSize.X / (ChunkSize.X * VoxelSize));
        int32 ChunksY = FMath::CeilToInt(TerrainSize.Y / (ChunkSize.Y * VoxelSize));
        int32 ChunksZ = FMath::CeilToInt(TerrainSize.Z / (ChunkSize.Z * VoxelSize));
        
        // Iterate through potentially visible chunks
        for (int32 z = FMath::Max(0, CenterChunkPos.Z - ChunkRadius); z <= FMath::Min(ChunksZ - 1, CenterChunkPos.Z + ChunkRadius); z++)
        {
            for (int32 y = FMath::Max(0, CenterChunkPos.Y - ChunkRadius); y <= FMath::Min(ChunksY - 1, CenterChunkPos.Y + ChunkRadius); y++)
            {
                for (int32 x = FMath::Max(0, CenterChunkPos.X - ChunkRadius); x <= FMath::Min(ChunksX - 1, CenterChunkPos.X + ChunkRadius); x++)
                {
                    FIntVector ChunkPos(x, y, z);
                    
                    // Check if this chunk is actually within visibility distance
                    FVector ChunkOrigin = ChunkPositionToWorldOrigin(ChunkPos);
                    FVector ChunkCenter = ChunkOrigin + FVector(
                        ChunkSize.X * VoxelSize * 0.5f,
                        ChunkSize.Y * VoxelSize * 0.5f,
                        ChunkSize.Z * VoxelSize * 0.5f
                    );
                    
                    float ChunkToCenterDist = FVector::Dist(ChunkCenter, PlayerPos);
                    if (ChunkToCenterDist <= ChunkVisibilityDistance)
                    {
                        // This chunk is visible, show it
                        UTerrainChunk* Chunk = GetChunkAt(ChunkPos);
                        if (Chunk)
                        {
                            Chunk->SetVisibility(true);
                        }
                    }
                }
            }
        }
    }
}

void AChunkBasedDestructibleTerrain::ProcessPendingModifications()
{
    // Only process on authority
    if (!HasAuthority())
    {
        return;
    }
    
    // Move modifications from pending to applied
    for (int32 i = 0; i < PendingModifications.Num(); i++)
    {
        FTerrainModificationInfo& Modification = PendingModifications[i];
        
        // Apply to all affected chunks
        for (const FString& ChunkID : Modification.AffectedChunkIDs)
        {
            UTerrainChunk* Chunk = Chunks.FindRef(ChunkID);
            if (Chunk)
            {
                // Apply modification to this chunk
                bool bModified = Chunk->CarveSphere(
                    Modification.Position, 
                    Modification.Radius, 
                    Modification.Falloff
                );
                
                // If modified, update the mesh
                if (bModified)
                {
                    Chunk->UpdateMeshData();
                    Chunk->ApplyMeshData();
                    
                    // Prepare for replication
                    PrepareChunkForReplication(Chunk);
                }
            }
        }
        
        // Move to applied list
        AppliedModifications.Add(Modification);
    }
    
    // Clear pending list
    PendingModifications.Empty();
}

void AChunkBasedDestructibleTerrain::CalculateAffectedChunks(FTerrainModificationInfo& Modification)
{
    // Get chunks that might be affected
    TArray<UTerrainChunk*> AffectedChunks = GetChunksInSphere(
        Modification.Position, 
        Modification.Radius
    );
    
    // Store their IDs
    Modification.AffectedChunkIDs.Empty();
    for (UTerrainChunk* Chunk : AffectedChunks)
    {
        if (Chunk)
        {
            Modification.AffectedChunkIDs.Add(Chunk->GetChunkID());
        }
    }
}

void AChunkBasedDestructibleTerrain::PrepareChunkForReplication(UTerrainChunk* Chunk)
{
    if (!Chunk || !Chunk->NeedsReplication())
    {
        return;
    }
    
    // Serialize modified voxels for efficient replication
    TArray<uint8> ModifiedVoxels = Chunk->SerializeModifiedVoxels();
    
    // Add to list for replication
    FChunkModificationPacket Packet(
        Chunk->GetChunkID(),
        ModifiedVoxels,
        Chunk->GetModificationCount()
    );
    
    // Add to replication queue
    bool bFound = false;
    for (int32 i = 0; i < ReplicatedChunkModifications.Num(); i++)
    {
        if (ReplicatedChunkModifications[i].ChunkID == Chunk->GetChunkID())
        {
            // Update existing entry if newer
            if (ReplicatedChunkModifications[i].ModificationVersion < Packet.ModificationVersion)
            {
                ReplicatedChunkModifications[i] = Packet;
            }
            bFound = true;
            break;
        }
    }
    
    // Add new entry if not found
    if (!bFound)
    {
        ReplicatedChunkModifications.Add(Packet);
    }
    
    // Mark as replicated
    Chunk->MarkReplicated();
}

void AChunkBasedDestructibleTerrain::ReplicateModifiedChunks()
{
    // Only server should replicate
    if (!HasAuthority() || ReplicatedChunkModifications.Num() == 0)
    {
        return;
    }
    
    // Replicate in batches to avoid network overload
    const int32 MaxBatchSize = 5;
    
    if (ReplicatedChunkModifications.Num() <= MaxBatchSize)
    {
        // Small enough to replicate all at once
        Multicast_SynchronizeChunks(ReplicatedChunkModifications);
        ReplicatedChunkModifications.Empty();
    }
    else
    {
        // Replicate in batches
        TArray<FChunkModificationPacket> Batch;
        for (int32 i = 0; i < MaxBatchSize && ReplicatedChunkModifications.Num() > 0; i++)
        {
            Batch.Add(ReplicatedChunkModifications[0]);
            ReplicatedChunkModifications.RemoveAt(0);
        }
        
        // Send the batch
        Multicast_SynchronizeChunks(Batch);
    }
}

void AChunkBasedDestructibleTerrain::ApplyReplicatedModifications(const TArray<FChunkModificationPacket>& Modifications)
{
    // Apply modifications locally
    for (const FChunkModificationPacket& Packet : Modifications)
    {
        // Find the chunk
        UTerrainChunk* Chunk = Chunks.FindRef(Packet.ChunkID);
        if (Chunk)
        {
            // Apply modification
            bool bModified = Chunk->ApplyModifiedVoxels(Packet.ModificationData);
            
            // Update mesh if needed
            if (bModified)
            {
                Chunk->UpdateMeshData();
                Chunk->ApplyMeshData();
            }
        }
    }
}

void AChunkBasedDestructibleTerrain::Multicast_SynchronizeChunks_Implementation(const TArray<FChunkModificationPacket>& ChunkModifications)
{
    // Skip on server, only clients need to apply these
    if (HasAuthority())
    {
        return;
    }
    
    // Apply the modifications
    ApplyReplicatedModifications(ChunkModifications);
}

void AChunkBasedDestructibleTerrain::Multicast_NotifyTerrainModification_Implementation(const TArray<uint8>& ModificationData)
{
    // Skip on server, it already has this info
    if (HasAuthority())
    {
        return;
    }
    
    // Deserialize the modification
    FTerrainModificationInfo Modification = FTerrainModificationInfo::Deserialize(ModificationData);
    
    // For visual effects (not actual terrain modification)
    // Find the affected chunks
    for (const FString& ChunkID : Modification.AffectedChunkIDs)
    {
        UTerrainChunk* Chunk = Chunks.FindRef(ChunkID);
        if (Chunk)
        {
            // We could trigger visual effects here
            // e.g., particle effects, sound, etc.
        }
    }
}

void AChunkBasedDestructibleTerrain::SynchronizeTerrain()
{
    if (!HasAuthority())
    {
        return;
    }
    
    // Force replication of all modified chunks
    for (auto& Pair : Chunks)
    {
        UTerrainChunk* Chunk = Pair.Value;
        if (Chunk && Chunk->IsDirty())
        {
            PrepareChunkForReplication(Chunk);
        }
    }
    
    // Send all at once (could be optimized to batch)
    Multicast_SynchronizeChunks(ReplicatedChunkModifications);
    ReplicatedChunkModifications.Empty();
}

float AChunkBasedDestructibleTerrain::GetDensityAtWorldPosition(const FVector& WorldPosition) const
{
    // Find the chunk containing this position
    FIntVector ChunkPos = WorldPositionToChunkPosition(WorldPosition);
    
    // Find the chunk ID
    const FString* ChunkIDPtr = ChunkIDsByPosition.Find(ChunkPos);
    if (!ChunkIDPtr)
    {
        return 1.0f; // Outside terrain bounds, consider solid
    }
    
    // Find the chunk
    const UTerrainChunk* const* ChunkPtr = Chunks.Find(*ChunkIDPtr);
    if (!ChunkPtr || !(*ChunkPtr))
    {
        return 1.0f; // Chunk not found, consider solid
    }
    
    const UTerrainChunk* Chunk = *ChunkPtr;
    
    // Get the local coordinates within the chunk
    FVector LocalPos = WorldPosition - Chunk->GetWorldOrigin();
    FIntVector VoxelCoord = FIntVector(
        FMath::FloorToInt(LocalPos.X / VoxelSize),
        FMath::FloorToInt(LocalPos.Y / VoxelSize),
        FMath::FloorToInt(LocalPos.Z / VoxelSize)
    );
    
    // Check bounds
    if (VoxelCoord.X < 0 || VoxelCoord.X >= ChunkSize.X ||
        VoxelCoord.Y < 0 || VoxelCoord.Y >= ChunkSize.Y ||
        VoxelCoord.Z < 0 || VoxelCoord.Z >= ChunkSize.Z)
    {
        return 1.0f; // Outside chunk bounds, consider solid
    }
    
    // Get the density from the voxel
    return Chunk->GetVoxelData().GetVoxel(VoxelCoord).Density;
}

EVoxelMaterial AChunkBasedDestructibleTerrain::GetMaterialAtWorldPosition(const FVector& WorldPosition) const
{
    // Find the chunk containing this position
    FIntVector ChunkPos = WorldPositionToChunkPosition(WorldPosition);
    
    // Find the chunk ID
    const FString* ChunkIDPtr = ChunkIDsByPosition.Find(ChunkPos);
    if (!ChunkIDPtr)
    {
        return EVoxelMaterial::Dirt; // Default material
    }
    
    // Find the chunk
    const UTerrainChunk* const* ChunkPtr = Chunks.Find(*ChunkIDPtr);
    if (!ChunkPtr || !(*ChunkPtr))
    {
        return EVoxelMaterial::Dirt; // Default material
    }
    
    const UTerrainChunk* Chunk = *ChunkPtr;
    
    // Get the local coordinates within the chunk
    FVector LocalPos = WorldPosition - Chunk->GetWorldOrigin();
    FIntVector VoxelCoord = FIntVector(
        FMath::FloorToInt(LocalPos.X / VoxelSize),
        FMath::FloorToInt(LocalPos.Y / VoxelSize),
        FMath::FloorToInt(LocalPos.Z / VoxelSize)
    );
    
    // Check bounds
    if (VoxelCoord.X < 0 || VoxelCoord.X >= ChunkSize.X ||
        VoxelCoord.Y < 0 || VoxelCoord.Y >= ChunkSize.Y ||
        VoxelCoord.Z < 0 || VoxelCoord.Z >= ChunkSize.Z)
    {
        return EVoxelMaterial::Dirt; // Default material
    }
    
    // Get the material from the voxel
    return Chunk->GetVoxelData().GetVoxel(VoxelCoord).Material;
}

bool AChunkBasedDestructibleTerrain::IsPositionSolid(const FVector& WorldPosition) const
{
    // Consider a position solid if its density is above 0.5
    return GetDensityAtWorldPosition(WorldPosition) > 0.5f;
}

bool AChunkBasedDestructibleTerrain::TraceTerrain(const FVector& Start, const FVector& End, FVector& HitPoint, FVector& HitNormal) const
{
    // Simple ray marching implementation
    const float StepSize = VoxelSize * 0.5f; // Half voxel size for better accuracy
    const float MaxSteps = FVector::Dist(Start, End) / StepSize;
    
    FVector Direction = (End - Start).GetSafeNormal();
    FVector CurrentPos = Start;
    
    for (int32 i = 0; i < MaxSteps; i++)
    {
        // Check if current position is solid
        if (IsPositionSolid(CurrentPos))
        {
            // Found a hit
            HitPoint = CurrentPos;
            
            // Calculate normal by sampling around the hit point
            float Delta = VoxelSize * 0.1f;
            float DensityX1 = GetDensityAtWorldPosition(CurrentPos + FVector(Delta, 0, 0));
            float DensityX2 = GetDensityAtWorldPosition(CurrentPos - FVector(Delta, 0, 0));
            float DensityY1 = GetDensityAtWorldPosition(CurrentPos + FVector(0, Delta, 0));
            float DensityY2 = GetDensityAtWorldPosition(CurrentPos - FVector(0, Delta, 0));
            float DensityZ1 = GetDensityAtWorldPosition(CurrentPos + FVector(0, 0, Delta));
            float DensityZ2 = GetDensityAtWorldPosition(CurrentPos - FVector(0, 0, Delta));
            
            HitNormal = FVector(DensityX2 - DensityX1, DensityY2 - DensityY1, DensityZ2 - DensityZ1).GetSafeNormal();
            
            return true;
        }
        
        // Step forward
        CurrentPos += Direction * StepSize;
        
        // Check if we've gone past the end
        if (FVector::DotProduct(CurrentPos - Start, End - Start) > FVector::DistSquared(Start, End))
        {
            break;
        }
    }
    
    // No hit found
    return false;
}

TArray<FString> AChunkBasedDestructibleTerrain::GetModifiedChunkIDs() const
{
    TArray<FString> ModifiedIDs;
    
    for (auto& Pair : Chunks)
    {
        if (Pair.Value && Pair.Value->IsDirty())
        {
            ModifiedIDs.Add(Pair.Value->GetChunkID());
        }
    }
    
    return ModifiedIDs;
}

void AChunkBasedDestructibleTerrain::TestExplosion()
{
    // Créer une explosion au centre du terrain
    FVector Center = GetActorLocation();
    float Radius = 500.0f; // Rayon assez grand pour être visible
    
    UE_LOG(LogTemp, Error, TEXT("TEST EXPLOSION at %s with radius %.1f"), *Center.ToString(), Radius);
    
    // Appliquer à tous les chunks
    TArray<UTerrainChunk*> AffectedChunks = GetChunksInSphere(Center, Radius);
    
    for (UTerrainChunk* Chunk : AffectedChunks)
    {
        if (Chunk)
        {
            UE_LOG(LogTemp, Error, TEXT("Applying explosion to chunk %s"), *Chunk->GetChunkID());
            
            bool bModified = Chunk->CarveSphere(Center, Radius, 0.3f);
            
            if (bModified)
            {
                Chunk->UpdateMeshData();
                Chunk->ApplyMeshData();
                UE_LOG(LogTemp, Error, TEXT("Chunk modified successfully"));
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("Chunk was not modified by explosion"));
            }
        }
    }
    
    UE_LOG(LogTemp, Error, TEXT("Test explosion applied to %d chunks"), AffectedChunks.Num());
}