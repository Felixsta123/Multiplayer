#include "AVoxelBuilding.h"

#include "VoxelDebrisSystem.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

AImprovedVoxelBuilding::AImprovedVoxelBuilding()
{
    PrimaryActorTick.bCanEverTick = false;
    
    // Create procedural mesh component
    BuildingMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("BuildingMesh"));
    RootComponent = BuildingMesh;
    BuildingMesh->bUseAsyncCooking = true;
    DebrisSystem = CreateDefaultSubobject<UVoxelDebrisSystem>(TEXT("DebrisSystem"));
    DebrisSystem->SetupAttachment(RootComponent);
    
    // Default values for debris
    bSpawnDebrisOnDestruction = true;
    DebrisAmountMultiplier = 1.0f;
    bSpawnImpactCloud = true;
    // Default values
    GridSizeX = 10;
    GridSizeY = 10;
    GridSizeZ = 10;
    VoxelSize = 100.0f;
    SmoothingFactor = 0.01f;
    bUseRandomColors = false;
    BuildingColor = FLinearColor(0.5f, 0.5f, 1.0f, 1.0f);
    bGenerateOnBeginPlay = true;
    bUseDoubleSidedGeometry = false; // Changed to false for better rendering
    bEnableCollision = true;
    CubeMargin = 0.01f; // Reduced margin for tighter fitting
    LastProcessedDestructionCount = 0;

    // Make actor replicable
    bReplicates = true;
    BuildingMesh->SetIsReplicated(true);
}

void AImprovedVoxelBuilding::BeginPlay()
{
    Super::BeginPlay();
    
    if (bGenerateOnBeginPlay)
    {
        GenerateBuilding();
    }
}

void AImprovedVoxelBuilding::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    
    // Replicate the destruction history
    DOREPLIFETIME(AImprovedVoxelBuilding, DestructionHistory);
}

void AImprovedVoxelBuilding::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    
    // Generate building in editor for preview
    #if WITH_EDITOR
        GenerateBuilding();
    #endif
}

void AImprovedVoxelBuilding::GenerateBuilding()
{
    // Initialize voxel grid
    InitializeVoxelGrid();
    
    // Create building mesh
    CreateMesh();
}

void AImprovedVoxelBuilding::InitializeVoxelGrid()
{
    VoxelGrid.Empty();
    VoxelGrid.SetNum(GridSizeX);
    
    for (int32 X = 0; X < GridSizeX; X++)
    {
        VoxelGrid[X].SetNum(GridSizeY);
        
        for (int32 Y = 0; Y < GridSizeY; Y++)
        {
            VoxelGrid[X][Y].SetNum(GridSizeZ);
            
            for (int32 Z = 0; Z < GridSizeZ; Z++)
            {
                FVoxelData& Voxel = VoxelGrid[X][Y][Z];
                
                // All voxels are active by default to create a full cube
                Voxel.bIsActive = true;
                
                // Set voxel color
                if (bUseRandomColors)
                {
                    Voxel.Color = GetRandomColor();
                }
                else
                {
                    Voxel.Color = BuildingColor.ToFColor(true);
                }
                
                // Add slight variation for materials
                Voxel.MaterialIndex = FMath::RandRange(0, FMath::Max(0, Materials.Num() - 1));
            }
        }
    }
}

void AImprovedVoxelBuilding::CreateMesh()
{
    // Optimization 1: Pre-allocate arrays with estimated capacity
    const int32 EstimatedFaces = GridSizeX * GridSizeY * GridSizeZ * 3; // Conservative estimate
    const int32 EstimatedVertices = EstimatedFaces * 4;
    const int32 EstimatedIndices = EstimatedFaces * 6;
    
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TArray<FColor> Colors;
    TArray<FProcMeshTangent> Tangents;
    
    // Reserve memory upfront to avoid reallocations
    Vertices.Reserve(EstimatedVertices);
    Triangles.Reserve(EstimatedIndices);
    Normals.Reserve(EstimatedVertices);
    UVs.Reserve(EstimatedVertices);
    Colors.Reserve(EstimatedVertices);
    Tangents.Reserve(EstimatedVertices);
    
    // Optimization 2: Chunked mesh processing
    // If the building is large, process it in chunks to avoid single frame spikes
    BuildingMesh->ClearAllMeshSections();
    
    // Optimization 3: Calculate visible faces more efficiently
    // Precompute face visibility using neighbor lookup instead of checking each face
    
    // Create a 3D bool array to track active voxels
    TArray<TArray<TArray<bool>>> ActiveVoxels;
    ActiveVoxels.SetNum(GridSizeX + 2); // +2 for boundary padding
    
    for (int32 X = 0; X < GridSizeX + 2; X++)
    {
        ActiveVoxels[X].SetNum(GridSizeY + 2);
        for (int32 Y = 0; Y < GridSizeY + 2; Y++)
        {
            ActiveVoxels[X][Y].SetNum(GridSizeZ + 2);
            for (int32 Z = 0; Z < GridSizeZ + 2; Z++)
            {
                // Default to inactive for boundary padding
                ActiveVoxels[X][Y][Z] = false;
            }
        }
    }
    
    // Fill active status for actual voxels
    for (int32 X = 0; X < GridSizeX; X++)
    {
        for (int32 Y = 0; Y < GridSizeY; Y++)
        {
            for (int32 Z = 0; Z < GridSizeZ; Z++)
            {
                // X+1, Y+1, Z+1 to account for the boundary padding
                ActiveVoxels[X+1][Y+1][Z+1] = VoxelGrid[X][Y][Z].bIsActive;
            }
        }
    }
    
    // Loop through all voxels and add visible ones
    for (int32 X = 0; X < GridSizeX; X++)
    {
        for (int32 Y = 0; Y < GridSizeY; Y++)
        {
            for (int32 Z = 0; Z < GridSizeZ; Z++)
            {
                if (VoxelGrid[X][Y][Z].bIsActive)
                {
                    // Check visibility against padded grid (x+1,y+1,z+1)
                    bool bBottomFaceVisible = !ActiveVoxels[X+1][Y+1][Z];
                    bool bTopFaceVisible = !ActiveVoxels[X+1][Y+1][Z+2];
                    bool bLeftFaceVisible = !ActiveVoxels[X][Y+1][Z+1];
                    bool bRightFaceVisible = !ActiveVoxels[X+2][Y+1][Z+1];
                    bool bBackFaceVisible = !ActiveVoxels[X+1][Y][Z+1];
                    bool bFrontFaceVisible = !ActiveVoxels[X+1][Y+2][Z+1];
                    
                    // Only add faces that are exposed
                    AddVisibleFacesToMesh(X, Y, Z, Vertices, Triangles, Normals, UVs, Colors, Tangents,
                        bBottomFaceVisible, bTopFaceVisible, bLeftFaceVisible, bRightFaceVisible, 
                        bBackFaceVisible, bFrontFaceVisible);
                }
            }
        }
    }
    
    // Optimization 4: Disable smoothing for faster rebuilds during gameplay
    if (SmoothingFactor > 0.0f)
    {
        SmoothVertices(Vertices, Triangles);
    }
    
    // Check if we have data to add
    if (Vertices.Num() > 0 && Triangles.Num() > 0)
    {
        // Optimization 5: Configure collision settings more efficiently
        BuildingMesh->bUseComplexAsSimpleCollision = true;
        BuildingMesh->bReceivesDecals = true;
        
        // Create the mesh section with appropriate collision settings
        BuildingMesh->CreateMeshSection_LinearColor(
            0,                   // Section index
            Vertices,            // Vertices
            Triangles,           // Triangles
            Normals,             // Normals
            UVs,                 // UV0
            TArray<FLinearColor>(), // Vertex colors (using default)
            Tangents,            // Tangents
            true                // Enable collision
        );
        
        // Ensure section is visible
        BuildingMesh->SetMeshSectionVisible(0, true);
        
        // Apply material
        if (Materials.Num() > 0 && Materials[0] != nullptr)
        {
            BuildingMesh->SetMaterial(0, Materials[0]);
        }
        
        // Configure collision settings
        BuildingMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        BuildingMesh->SetCollisionObjectType(ECC_WorldStatic);
        BuildingMesh->SetCollisionResponseToAllChannels(ECR_Block);
        
        // Configure specific collision for projectiles
        BuildingMesh->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
        
        // Force collision data update
        BuildingMesh->ContainsPhysicsTriMeshData(true);
    }

    FVector LocalTopCenter = FVector(
    GridSizeX * VoxelSize * 0.5f,
    GridSizeY * VoxelSize * 0.5f,
    GridSizeZ * VoxelSize + 100.0f  // 100 unités au-dessus
    );
    
    // Transformer en coordonnées monde (prend en compte rotation/scale/position)
    TopSpawnPoint = GetActorTransform().TransformPosition(LocalTopCenter);
    
    UE_LOG(LogTemp, Log, TEXT("Building %s: Top spawn point calculated at %s"), 
        *GetName(), *TopSpawnPoint.ToString());
}

// Modified function signature to accept face visibility flags
// Modified function signature to accept face visibility flags
void AImprovedVoxelBuilding::AddVisibleFacesToMesh(int32 X, int32 Y, int32 Z, TArray<FVector>& Vertices, TArray<int32>& Triangles,
                          TArray<FVector>& Normals, TArray<FVector2D>& UVs, TArray<FColor>& Colors, 
                          TArray<FProcMeshTangent>& Tangents,
                          bool bBottomFaceVisible, bool bTopFaceVisible, 
                          bool bLeftFaceVisible, bool bRightFaceVisible,
                          bool bBackFaceVisible, bool bFrontFaceVisible)
{
    // Position of voxel center
    FVector Center = FVector(X * VoxelSize, Y * VoxelSize, Z * VoxelSize);
    
    // Half size of voxel - slight reduction to create visual separation between cubes
    float HalfSize = VoxelSize * (0.5f - CubeMargin);
    
    // Voxel color
    FColor VoxelColor = VoxelGrid[X][Y][Z].Color;
    
    // Base index for this voxel
    int32 BaseIndex = Vertices.Num();
    
    // Bottom face (Z-)
    if (bBottomFaceVisible)
    {
        Vertices.Add(Center + FVector(-HalfSize, -HalfSize, -HalfSize)); // 0
        Vertices.Add(Center + FVector(HalfSize, -HalfSize, -HalfSize));  // 1
        Vertices.Add(Center + FVector(HalfSize, HalfSize, -HalfSize));   // 2
        Vertices.Add(Center + FVector(-HalfSize, HalfSize, -HalfSize));  // 3
        
        AddFaceTriangles(Triangles, BaseIndex, false);
        AddFaceNormals(Normals, FVector(0, 0, -1), 4);
        AddFaceUVs(UVs);
        AddFaceColors(Colors, VoxelColor, 4);
        AddFaceTangents(Tangents, FVector(1, 0, 0), 4);
        
        BaseIndex += 4;
    }
    
    // Top face (Z+)
    if (bTopFaceVisible)
    {
        Vertices.Add(Center + FVector(-HalfSize, -HalfSize, HalfSize)); // 4
        Vertices.Add(Center + FVector(HalfSize, -HalfSize, HalfSize));  // 5
        Vertices.Add(Center + FVector(HalfSize, HalfSize, HalfSize));   // 6
        Vertices.Add(Center + FVector(-HalfSize, HalfSize, HalfSize));  // 7
        
        AddFaceTriangles(Triangles, BaseIndex, true);
        AddFaceNormals(Normals, FVector(0, 0, 1), 4);
        AddFaceUVs(UVs);
        AddFaceColors(Colors, VoxelColor, 4);
        AddFaceTangents(Tangents, FVector(1, 0, 0), 4);
        
        BaseIndex += 4;
    }
    
    // Left face (X-)
    if (bLeftFaceVisible)
    {
        Vertices.Add(Center + FVector(-HalfSize, -HalfSize, -HalfSize)); // 0
        Vertices.Add(Center + FVector(-HalfSize, HalfSize, -HalfSize));  // 3
        Vertices.Add(Center + FVector(-HalfSize, HalfSize, HalfSize));   // 7
        Vertices.Add(Center + FVector(-HalfSize, -HalfSize, HalfSize));  // 4
        
        AddFaceTriangles(Triangles, BaseIndex, false);
        AddFaceNormals(Normals, FVector(-1, 0, 0), 4);
        AddFaceUVs(UVs);
        AddFaceColors(Colors, VoxelColor, 4);
        AddFaceTangents(Tangents, FVector(0, 1, 0), 4);
        
        BaseIndex += 4;
    }
    
    // Right face (X+)
    if (bRightFaceVisible)
    {
        Vertices.Add(Center + FVector(HalfSize, -HalfSize, -HalfSize)); // 1
        Vertices.Add(Center + FVector(HalfSize, -HalfSize, HalfSize));  // 5
        Vertices.Add(Center + FVector(HalfSize, HalfSize, HalfSize));   // 6
        Vertices.Add(Center + FVector(HalfSize, HalfSize, -HalfSize));  // 2
        
        AddFaceTriangles(Triangles, BaseIndex, false);
        AddFaceNormals(Normals, FVector(1, 0, 0), 4);
        AddFaceUVs(UVs);
        AddFaceColors(Colors, VoxelColor, 4);
        AddFaceTangents(Tangents, FVector(0, -1, 0), 4);
        
        BaseIndex += 4;
    }
    
    // Back face (Y-)
    if (bBackFaceVisible)
    {
        Vertices.Add(Center + FVector(-HalfSize, -HalfSize, -HalfSize)); // 0
        Vertices.Add(Center + FVector(-HalfSize, -HalfSize, HalfSize));  // 4
        Vertices.Add(Center + FVector(HalfSize, -HalfSize, HalfSize));   // 5
        Vertices.Add(Center + FVector(HalfSize, -HalfSize, -HalfSize));  // 1
        
        AddFaceTriangles(Triangles, BaseIndex, false);
        AddFaceNormals(Normals, FVector(0, -1, 0), 4);
        AddFaceUVs(UVs);
        AddFaceColors(Colors, VoxelColor, 4);
        AddFaceTangents(Tangents, FVector(1, 0, 0), 4);
        
        BaseIndex += 4;
    }
    
    // Front face (Y+)
    if (bFrontFaceVisible)
    {
        Vertices.Add(Center + FVector(-HalfSize, HalfSize, -HalfSize)); // 3
        Vertices.Add(Center + FVector(HalfSize, HalfSize, -HalfSize));  // 2
        Vertices.Add(Center + FVector(HalfSize, HalfSize, HalfSize));   // 6
        Vertices.Add(Center + FVector(-HalfSize, HalfSize, HalfSize));  // 7
        
        AddFaceTriangles(Triangles, BaseIndex, false);
        AddFaceNormals(Normals, FVector(0, 1, 0), 4);
        AddFaceUVs(UVs);
        AddFaceColors(Colors, VoxelColor, 4);
        AddFaceTangents(Tangents, FVector(-1, 0, 0), 4);
    }
}
void AImprovedVoxelBuilding::AddFaceTriangles(TArray<int32>& Triangles, int32 BaseIndex, bool bReversed)
{
    if (!bReversed)
    {
        // First triangle (0,1,2)
        Triangles.Add(BaseIndex);
        Triangles.Add(BaseIndex + 1);
        Triangles.Add(BaseIndex + 2);
        
        // Second triangle (0,2,3)
        Triangles.Add(BaseIndex);
        Triangles.Add(BaseIndex + 2);
        Triangles.Add(BaseIndex + 3);
    }
    else
    {
        // First triangle (reversed: 0,2,1)
        Triangles.Add(BaseIndex);
        Triangles.Add(BaseIndex + 2);
        Triangles.Add(BaseIndex + 1);
        
        // Second triangle (reversed: 0,3,2)
        Triangles.Add(BaseIndex);
        Triangles.Add(BaseIndex + 3);
        Triangles.Add(BaseIndex + 2);
    }
}

void AImprovedVoxelBuilding::AddFaceNormals(TArray<FVector>& Normals, FVector Normal, int32 Count)
{
    for (int32 i = 0; i < Count; ++i)
    {
        Normals.Add(Normal);
    }
}

void AImprovedVoxelBuilding::AddFaceUVs(TArray<FVector2D>& UVs)
{
    // Standard UV mapping for a quad
    UVs.Add(FVector2D(0, 0)); // Bottom-left
    UVs.Add(FVector2D(1, 0)); // Bottom-right
    UVs.Add(FVector2D(1, 1)); // Top-right
    UVs.Add(FVector2D(0, 1)); // Top-left
}

void AImprovedVoxelBuilding::AddFaceColors(TArray<FColor>& Colors, FColor Color, int32 Count)
{
    for (int32 i = 0; i < Count; ++i)
    {
        Colors.Add(Color);
    }
}

void AImprovedVoxelBuilding::AddFaceTangents(TArray<FProcMeshTangent>& Tangents, FVector Tangent, int32 Count)
{
    for (int32 i = 0; i < Count; ++i)
    {
        Tangents.Add(FProcMeshTangent(Tangent.X, Tangent.Y, Tangent.Z));
    }
}

FColor AImprovedVoxelBuilding::GetRandomColor()
{
    // Generate a more vibrant random color
    float Hue = FMath::FRand() * 360.0f;
    float Saturation = 0.8f + FMath::FRand() * 0.2f; // More saturated
    float Value = 0.7f + FMath::FRand() * 0.3f;      // Brighter
    
    // Convert HSV to RGB
    FLinearColor LinearColor = FLinearColor::MakeFromHSV8(Hue, Saturation * 255.0f, Value * 255.0f);
    
    // Ensure full opacity
    LinearColor.A = 1.0f;
    
    // Convert to FColor with full opacity
    return LinearColor.ToFColor(true);
}

void AImprovedVoxelBuilding::SmoothVertices(TArray<FVector>& Vertices, TArray<int32>& Triangles)
{
    // Create a copy of original vertices
    TArray<FVector> OriginalVertices = Vertices;
    
    // Create a structure to store connected vertices
    TArray<TArray<int32>> VertexConnections;
    VertexConnections.SetNum(Vertices.Num());
    
    // Loop through all triangles and build connections
    for (int32 i = 0; i < Triangles.Num(); i += 3)
    {
        int32 V1 = Triangles[i];
        int32 V2 = Triangles[i + 1];
        int32 V3 = Triangles[i + 2];
        
        VertexConnections[V1].AddUnique(V2);
        VertexConnections[V1].AddUnique(V3);
        
        VertexConnections[V2].AddUnique(V1);
        VertexConnections[V2].AddUnique(V3);
        
        VertexConnections[V3].AddUnique(V1);
        VertexConnections[V3].AddUnique(V2);
    }
    
    // Apply smoothing
    for (int32 i = 0; i < Vertices.Num(); i++)
    {
        if (VertexConnections[i].Num() > 0)
        {
            // Calculate average position of connected vertices
            FVector AveragePosition = FVector::ZeroVector;
            for (int32 j = 0; j < VertexConnections[i].Num(); j++)
            {
                AveragePosition += OriginalVertices[VertexConnections[i][j]];
            }
            AveragePosition /= VertexConnections[i].Num();
            
            // Apply smoothing with weighting factor
            Vertices[i] = FMath::Lerp(OriginalVertices[i], AveragePosition, SmoothingFactor);
        }
    }
}
void AImprovedVoxelBuilding::DestroyVoxelsAt(FVector Location, FVector ImpactNormal, float Radius)
{
    // This implementation doesn't actually destroy anything immediately
    // It just prepares the destruction data and forwards it to the authoritative function
    
    if (HasAuthority())
    {
        // Generate a new random seed
        int32 RandomSeed = FMath::Rand();
        
        // Create destruction data
        FVoxelDestructionData DestructionData(Location, ImpactNormal, Radius, RandomSeed);
        
        // Add to history (this will trigger replication to clients)
        DestructionHistory.Add(DestructionData);

        // Ajoutez cette ligne pour forcer la mise à jour réseau :
        ForceNetUpdate();

        // Puis continuez avec l'application de la destruction
        ApplyDeterministicDestruction(DestructionData);        
    }
    else
    {
        // On client, just request the server to destroy
        Server_DestroyVoxelsAt(Location, ImpactNormal, Radius);
    }
}

// Implement the modified Server RPC
bool AImprovedVoxelBuilding::Server_DestroyVoxelsAt_Validate(FVector Location, FVector ImpactNormal, float Radius)
{
    return Radius > 0.0f;
}

void AImprovedVoxelBuilding::Server_DestroyVoxelsAt_Implementation(FVector Location, FVector ImpactNormal, float Radius)
{
    // Call the local method which will use authority to actually destroy
    DestroyVoxelsAt(Location, ImpactNormal, Radius);
    
    // No need to call Multicast - the replication of DestructionHistory will handle this
}

// Remove the Multicast_DestroyVoxelsAt implementation - we won't be using it anymore

// Add new function for deterministic destruction
void AImprovedVoxelBuilding::ApplyDeterministicDestruction(const FVoxelDestructionData& DestructionData)
{
    // Initialize the random stream with the provided seed for deterministic results
    RandomStream.Initialize(DestructionData.RandomSeed);
    
    UE_LOG(LogTemp, Warning, TEXT("Applying deterministic destruction with seed: %d"), DestructionData.RandomSeed);
    
    // Local variables for easier access
    FVector Location = DestructionData.Location;
    FVector ImpactNormal = DestructionData.Normal;
    float Radius = DestructionData.Radius;
    
    // Ensure normal is normalized
    if (ImpactNormal.IsNearlyZero())
    {
        ImpactNormal = FVector(0, 0, 1); // Default to up if no valid normal
    }
    else
    {
        ImpactNormal = ImpactNormal.GetSafeNormal();
    }
    
    // Use a more robust grid calculation for high-angle shots
    // Add a small offset in the direction of the normal to ensure we're inside the block
    Location -= ImpactNormal * VoxelSize * 0.1f;
    
    // Convert to grid coordinates with safeguards
    int32 GridX = FMath::Clamp(FMath::Floor(Location.X / VoxelSize), 0, GridSizeX - 1);
    int32 GridY = FMath::Clamp(FMath::Floor(Location.Y / VoxelSize), 0, GridSizeY - 1);
    int32 GridZ = FMath::Clamp(FMath::Floor(Location.Z / VoxelSize), 0, GridSizeZ - 1);
    
    // Grid radius - increase for more reliable high-angle hits
    int32 GridRadius = FMath::CeilToInt(Radius / VoxelSize) + 1;
    
    // Store voxels to destroy with their destruction priority
    TArray<TPair<FIntVector, float>> VoxelsToDestroy;
    
    // ENHANCEMENT 1: Improved ray distribution
    const int32 RayCount = 48; // Increased from 32 for better coverage
    const float MaxRayLength = Radius * 2.5f; // Increased from 2.0f for better penetration

    // Use the enhanced ray distribution algorithm
    TArray<FVector> RayDirections;
    GenerateOptimizedRayDirections(RayDirections, ImpactNormal, RayCount - 8); // Reserve 8 for special rays

    // Add special penetration rays for different scenarios
    // Extra ray directly backward for glancing hits
    RayDirections.Add(ImpactNormal); 

    // Extra downward ray for high hits
    if (ImpactNormal.Z < -0.5f)
    {
        RayDirections.Add(FVector(0, 0, -1));
    }

    // Extra ray in horizontal plane for side hits
    if (FMath::Abs(ImpactNormal.Z) < 0.5f)
    {
        FVector HorizontalNormal = ImpactNormal;
        HorizontalNormal.Z = 0;
        if (!HorizontalNormal.IsNearlyZero())
        {
            HorizontalNormal.Normalize();
            RayDirections.Add(-HorizontalNormal);
        }
    }

    // Now trace each ray - using our deterministic random stream
    for (int32 RayIndex = 0; RayIndex < RayDirections.Num(); RayIndex++)
    {
        FVector RayDir = RayDirections[RayIndex];
        
        // ENHANCEMENT 3: Variable ray length and attenuation
        float RayLength = MaxRayLength;
        if (RayIndex > 0) // Keep first ray at max length
        {
            // Use our deterministic random stream instead of FMath::FRand()
            RayLength *= (0.7f + 0.3f * RandomStream.GetFraction());
        }
        
        // Trace the ray through the voxels
        FVector RayStart = Location;
        // Smaller step size for better accuracy
        float StepSize = VoxelSize * 0.15f; // Smaller than before for more precision
        FVector CurrentPos = RayStart;
        
        // Store which voxels this ray has already affected
        TSet<FIntVector> AffectedVoxels;
        
        // ENHANCEMENT 4: Ray attenuation - rays lose energy as they travel
        float RayEnergy = 1.0f;
        float EnergyCostPerStep = 1.0f / (RayLength / StepSize);
        float EnergyCostPerHit = 0.1f;
        
        for (float Distance = 0.0f; Distance <= RayLength && RayEnergy > 0.05f; Distance += StepSize)
        {
            int32 X = FMath::Clamp(FMath::Floor(CurrentPos.X / VoxelSize), 0, GridSizeX - 1);
            int32 Y = FMath::Clamp(FMath::Floor(CurrentPos.Y / VoxelSize), 0, GridSizeY - 1);
            int32 Z = FMath::Clamp(FMath::Floor(CurrentPos.Z / VoxelSize), 0, GridSizeZ - 1);
            
            FIntVector VoxelCoord(X, Y, Z);
            
            // Check if this voxel was already affected by this ray
            if (!AffectedVoxels.Contains(VoxelCoord))
            {
                // Mark this voxel as affected by this ray
                AffectedVoxels.Add(VoxelCoord);
                
                if (X >= 0 && X < GridSizeX && Y >= 0 && Y < GridSizeY && Z >= 0 && Z < GridSizeZ &&
                    VoxelGrid[X][Y][Z].bIsActive)
                {
                    // Ray loses energy when it hits an active voxel
                    RayEnergy -= EnergyCostPerHit;
                    
                    // Ray might destroy the voxel based on energy and distance
                    float DistanceFromImpact = FVector::Dist(CurrentPos, Location);
                    
                    // Destruction priority is based on ray energy and distance from impact
                    float DestructionPriority = RayEnergy * (1.0f - FMath::Min(1.0f, DistanceFromImpact / RayLength));
                    
                    // ENHANCEMENT 5: Fuzzy edges - more randomness at edges of explosion
                    float EdgeRandomness = FMath::Lerp(0.1f, 0.7f, DistanceFromImpact / RayLength);
                    // Use our deterministic random stream
                    DestructionPriority *= (1.0f - RandomStream.GetFraction() * EdgeRandomness);
                    
                    // Boost priority for first ray (direct hit direction)
                    if (RayIndex == 0)
                    {
                        DestructionPriority *= 1.5f;
                    }
                    
                    // Check if this voxel is already queued for destruction with a higher priority
                    bool bAlreadyAdded = false;
                    
                    for (int32 i = 0; i < VoxelsToDestroy.Num(); i++)
                    {
                        if (VoxelsToDestroy[i].Key == VoxelCoord)
                        {
                            bAlreadyAdded = true;
                            // Update priority if this is higher
                            VoxelsToDestroy[i].Value = FMath::Max(VoxelsToDestroy[i].Value, DestructionPriority);
                            break;
                        }
                    }
                    
                    if (!bAlreadyAdded)
                    {
                        VoxelsToDestroy.Add(TPair<FIntVector, float>(VoxelCoord, DestructionPriority));
                    }
                }
            }
            
            // Advance along the ray
            CurrentPos += RayDir * StepSize;
            // Reduce ray energy with each step
            RayEnergy -= EnergyCostPerStep;
        }
    }
    
    // ENHANCEMENT 6: Structural weakening - make voxels more likely to be destroyed if they've lost neighbors
    TArray<TPair<FIntVector, float>> StructuralWeakening;
    
    // Find hanging voxels or weakened structures after primary destruction
    for (const TPair<FIntVector, float>& VoxelData : VoxelsToDestroy)
    {
        int32 X = VoxelData.Key.X;
        int32 Y = VoxelData.Key.Y;
        int32 Z = VoxelData.Key.Z;
        
        // Check six neighboring voxels
        const int32 NeighborCount = 6;
        const int32 NeighborOffsets[NeighborCount][3] = {
            {-1, 0, 0}, {1, 0, 0},   // Left, Right
            {0, -1, 0}, {0, 1, 0},   // Front, Back
            {0, 0, -1}, {0, 0, 1}    // Bottom, Top
        };
        
        for (int32 i = 0; i < NeighborCount; i++)
        {
            int32 NX = X + NeighborOffsets[i][0];
            int32 NY = Y + NeighborOffsets[i][1];
            int32 NZ = Z + NeighborOffsets[i][2];
            
            if (NX >= 0 && NX < GridSizeX && NY >= 0 && NY < GridSizeY && NZ >= 0 && NZ < GridSizeZ)
            {
                if (VoxelGrid[NX][NY][NZ].bIsActive)
                {
                    FIntVector NeighborCoord(NX, NY, NZ);
                    
                    // Check if this neighbor is not already in the destruction list
                    bool bAlreadyQueued = false;
                    for (const TPair<FIntVector, float>& QueuedVoxel : VoxelsToDestroy)
                    {
                        if (QueuedVoxel.Key == NeighborCoord)
                        {
                            bAlreadyQueued = true;
                            break;
                        }
                    }
                    
                    if (!bAlreadyQueued)
                    {
                        // Add neighbor to the structural weakening list with lower priority
                        float WeakeningPriority = VoxelData.Value * 0.4f; // 40% of original priority
                        
                        // Extra weakening for voxels above destroyed voxels (gravity effect)
                        if (NeighborOffsets[i][2] == 1)
                        {
                            WeakeningPriority *= 1.5f;
                        }
                        
                        StructuralWeakening.Add(TPair<FIntVector, float>(NeighborCoord, WeakeningPriority));
                    }
                }
            }
        }
    }
    
    // Add the structural weakening voxels to the main destruction list
    for (const TPair<FIntVector, float>& WeakVoxel : StructuralWeakening)
    {
        bool bAlreadyExists = false;
        
        for (TPair<FIntVector, float>& ExistingVoxel : VoxelsToDestroy)
        {
            if (ExistingVoxel.Key == WeakVoxel.Key)
            {
                ExistingVoxel.Value = FMath::Max(ExistingVoxel.Value, WeakVoxel.Value);
                bAlreadyExists = true;
                break;
            }
        }
        
        if (!bAlreadyExists)
        {
            VoxelsToDestroy.Add(WeakVoxel);
        }
    }
    
    // Sort voxels by destruction priority (highest to lowest)
    VoxelsToDestroy.Sort([](const TPair<FIntVector, float>& A, const TPair<FIntVector, float>& B) {
        return A.Value > B.Value;
    });
    
    // Store list of destroyed voxels and their colors for debris creation
    TArray<TPair<FIntVector, FColor>> DestroyedVoxels;
    bool bAnyVoxelDestroyed = false;
    
    // Apply destruction with probability based on priority
    float BaseDestructionThreshold = FMath::Lerp(0.3f, 0.1f, FMath::Min(1.0f, Radius / 500.0f));
    
    for (const TPair<FIntVector, float>& VoxelData : VoxelsToDestroy)
    {
        int32 X = VoxelData.Key.X;
        int32 Y = VoxelData.Key.Y;
        int32 Z = VoxelData.Key.Z;
        float Priority = VoxelData.Value;
        
        // Higher priority voxels are more likely to be destroyed
        float DestructionThreshold = BaseDestructionThreshold * (1.0f - (Priority * 0.8f));
        
        // Use our deterministic random stream
        if (RandomStream.GetFraction() < (1.0f - DestructionThreshold))
        {
            // Store the voxel color before destroying it
            if (bSpawnDebrisOnDestruction && DebrisSystem)
            {
                DestroyedVoxels.Add(TPair<FIntVector, FColor>(VoxelData.Key, VoxelGrid[X][Y][Z].Color));
            }
            
            // Destroy the voxel
            VoxelGrid[X][Y][Z].bIsActive = false;
            bAnyVoxelDestroyed = true;
        }
    }
    
    // Spawn debris for destroyed voxels
    if (bAnyVoxelDestroyed && bSpawnDebrisOnDestruction && DebrisSystem)
    {
        // First, spawn a debris cloud at the impact point if enabled
        if (bSpawnImpactCloud)
        {
            // Convert Location to world space
            FVector WorldImpactLocation = GetTransform().TransformPosition(Location);    
            // Get average color of destroyed voxels
            TArray<FColor> VoxelColors;
            for (const TPair<FIntVector, FColor>& DestroyedVoxel : DestroyedVoxels)
            {
                VoxelColors.Add(DestroyedVoxel.Value);
            }
    
            // Spawn a debris cloud at impact point
            float ImpactVolume = FMath::Min(Radius * 0.5f, 200.0f);
            DebrisSystem->SpawnDebrisInVolume(
                WorldImpactLocation, 
                FVector(ImpactVolume, ImpactVolume, ImpactVolume), 
                ImpactNormal,
                VoxelColors
            );
        }
        
        // Then spawn individual debris for each destroyed voxel
        int32 MaxIndividualDebris = FMath::Min(DestroyedVoxels.Num(), 20); // Limit for performance
        
        for (int32 i = 0; i < MaxIndividualDebris; i++)
        {
            int32 Index = i;
            if (DestroyedVoxels.Num() > MaxIndividualDebris)
            {
                // Use our deterministic random stream
                Index = RandomStream.RandRange(0, DestroyedVoxels.Num() - 1);
            }
    
            FIntVector VoxelCoord = DestroyedVoxels[Index].Key;
            FColor VoxelColor = DestroyedVoxels[Index].Value;
    
            // Calculate the local space position of the voxel center
            FVector LocalVoxelPos = FVector(
                (VoxelCoord.X + 0.5f) * VoxelSize,
                (VoxelCoord.Y + 0.5f) * VoxelSize,
                (VoxelCoord.Z + 0.5f) * VoxelSize
            );
    
            // Transform this position to world space
            FVector VoxelWorldLocation = GetTransform().TransformPosition(LocalVoxelPos);
    
            // Calculate debris count based on distance from impact
            float DistanceFromImpact = FVector::Dist(VoxelWorldLocation, GetTransform().TransformPosition(Location));
            float DistanceFactor = FMath::Clamp(1.0f - (DistanceFromImpact / (Radius * 1.5f)), 0.2f, 1.0f);
    
            // Spawn debris for this voxel
            int32 DebrisCount = FMath::RoundToInt(DebrisSystem->DebrisParams.DebrisCountPerVoxel * 
                                                 DistanceFactor * DebrisAmountMultiplier);
                                         
            if (DebrisCount > 0)
            {
                DebrisSystem->SpawnDebrisAtLocation(VoxelWorldLocation, ImpactNormal, VoxelColor, DebrisCount);
            }
        }
    }
    
    // ENHANCEMENT 8: Batch mesh recreation for performance
    if (bAnyVoxelDestroyed)
    {
        // Recreate the mesh with updated voxels
        CreateMesh();
    }
}


void AImprovedVoxelBuilding::Multicast_DestroyVoxelsAt_Implementation(FVector Location, FVector ImpactNormal, float Radius)
{
    // Ne pas exécuter à nouveau sur le serveur, uniquement sur les clients
    if (!HasAuthority())
    {
        DestroyVoxelsAt(Location, ImpactNormal, Radius);
    }
}
void AImprovedVoxelBuilding::OnRep_DestructionHistory()
{
    if (!HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("OnRep_DestructionHistory: Client received %d operations (previously processed %d)"), 
            DestructionHistory.Num(), LastProcessedDestructionCount);
            
        // Process only new operations
        for (int32 i = LastProcessedDestructionCount; i < DestructionHistory.Num(); i++)
        {
            ApplyDeterministicDestruction(DestructionHistory[i]);
        }
        
        // Update our processed count
        LastProcessedDestructionCount = DestructionHistory.Num();
    }
}

TArray<AImprovedVoxelBuilding*> AImprovedVoxelBuilding::FindAllVoxelBuildings(const UObject* WorldContextObject)
{
    TArray<AImprovedVoxelBuilding*> Result;
    if (!WorldContextObject)
        return Result;
        
    UWorld* World = WorldContextObject->GetWorld();
    if (!World)
        return Result;
        
    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsOfClass(World, AImprovedVoxelBuilding::StaticClass(), FoundActors);
    
    for (AActor* Actor : FoundActors)
    {
        AImprovedVoxelBuilding* Building = Cast<AImprovedVoxelBuilding>(Actor);
        if (Building)
        {
            Result.Add(Building);
        }
    }
    
    return Result;
}
void AImprovedVoxelBuilding::GenerateOptimizedRayDirections(TArray<FVector>& RayDirections, const FVector& ImpactNormal, int32 NumRays)
{
    RayDirections.Empty(NumRays);
    
    // Bias the ray distribution toward the impact normal
    FVector BiasedDirection = -ImpactNormal * 3.0f;
    
    // First ray always goes directly into the building along the normal with extra length
    RayDirections.Add(-ImpactNormal);
    
    // Create a coordinate system based on the impact normal
    FVector UpVector = FVector(0, 0, 1);
    FVector RightVector = FVector::CrossProduct(ImpactNormal, UpVector);
    
    // Handle case where normal is parallel to up vector
    if (RightVector.IsNearlyZero())
    {
        RightVector = FVector(1, 0, 0);
    }
    
    RightVector.Normalize();
    FVector ForwardVector = FVector::CrossProduct(RightVector, ImpactNormal).GetSafeNormal();
    
    // Add additional rays with high angle concentration around impact normal
    // and increasing spread based on distance from initial ray
    for (int32 i = 1; i < NumRays; i++)
    {
        float NormalizedIndex = float(i) / float(NumRays - 1);
        
        // Golden spiral distribution for better coverage
        float Phi = 2.0f * PI * fmod(i * 0.618033988749895f, 1.0f); // Golden ratio spiral
        
        // More rays concentrated near the center (impact point)
        float Radius = FMath::Sqrt(NormalizedIndex);
        
        // Calculate position on unit circle
        float X = FMath::Cos(Phi) * Radius;
        float Y = FMath::Sin(Phi) * Radius;
        
        // Transform to ray direction in 3D
        FVector RayDir = (-ImpactNormal) + (RightVector * X * 0.9f) + (ForwardVector * Y * 0.9f);
        RayDir.Normalize();
        
        // Add bias based on distance from center ray
        float BiasStrength = FMath::Lerp(0.9f, 0.3f, Radius);
        RayDir = (RayDir + BiasedDirection * BiasStrength).GetSafeNormal();
        
        RayDirections.Add(RayDir);
    }
    
    // Add additional rays specifically for high-angle impacts
    if (FMath::Abs(ImpactNormal.Z) > 0.7f)
    {
        // For high angle impacts (coming from above/below), add more horizontal penetration
        for (int32 i = 0; i < 8; i++)
        {
            float Angle = (float)i * (2.0f * PI / 8.0f);
            FVector HorizontalDir = RightVector * FMath::Cos(Angle) + ForwardVector * FMath::Sin(Angle);
            
            // Mix horizontal direction with a bit of normal direction
            FVector MixedDir = (HorizontalDir * 0.9f - ImpactNormal * 0.1f).GetSafeNormal();
            RayDirections.Add(MixedDir);
        }
    }
}