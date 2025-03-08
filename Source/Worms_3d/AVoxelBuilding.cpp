#include "AVoxelBuilding.h"

#include "Kismet/GameplayStatics.h"

AImprovedVoxelBuilding::AImprovedVoxelBuilding()
{
    PrimaryActorTick.bCanEverTick = false;
    
    // Create procedural mesh component
    BuildingMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("BuildingMesh"));
    RootComponent = BuildingMesh;
    BuildingMesh->bUseAsyncCooking = true;
    
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
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;

    
    TArray<FColor> Colors;
    TArray<FProcMeshTangent> Tangents;
    
    // Clear all existing mesh sections
    BuildingMesh->ClearAllMeshSections();
    
    // Loop through all voxels and add visible ones
    for (int32 X = 0; X < GridSizeX; X++)
    {
        for (int32 Y = 0; Y < GridSizeY; Y++)
        {
            for (int32 Z = 0; Z < GridSizeZ; Z++)
            {
                if (VoxelGrid[X][Y][Z].bIsActive)
                {
                    // Only add faces that are exposed (not completely surrounded by other voxels)
                    AddVisibleFacesToMesh(X, Y, Z, Vertices, Triangles, Normals, UVs, Colors, Tangents);
                }
            }
        }
    }
    
    // Apply smoothing if needed
    if (SmoothingFactor > 0.0f)
    {
        SmoothVertices(Vertices, Triangles);
    }
    
    // Check if we have data to add
    if (Vertices.Num() > 0 && Triangles.Num() > 0)
    {
        // MODIFICATION : Utiliser la collision complexe pour une précision maximale
        BuildingMesh->bUseComplexAsSimpleCollision = true;
        BuildingMesh->bReceivesDecals = true;
        
        // Créer la section de mesh avec collision activée
        BuildingMesh->CreateMeshSection_LinearColor(
            0,                  // Section index
            Vertices,           // Vertices
            Triangles,          // Triangles
            Normals,            // Normals
            UVs,                // UV0
            TArray<FLinearColor>(), // Vertex colors
            Tangents,           // Tangents
            true               // FORCÉ à true pour toujours activer les collisions
        );
        
        // Assurer que la section est visible
        BuildingMesh->SetMeshSectionVisible(0, true);
        
        // Appliquer le matériau
        if (Materials.Num() > 0 && Materials[0] != nullptr)
        {
            BuildingMesh->SetMaterial(0, Materials[0]);
        }
        
        // MODIFICATION : Configuration explicite des collisions
        BuildingMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        BuildingMesh->SetCollisionObjectType(ECC_WorldStatic);
        BuildingMesh->SetCollisionResponseToAllChannels(ECR_Block);
        
        // AJOUT : Configuration du canal de collision spécifique pour les projectiles
        BuildingMesh->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block); // Assurez-vous que ce canal est configuré pour vos projectiles
        
        // AJOUT : Forcer la mise à jour des données de collision
        BuildingMesh->ContainsPhysicsTriMeshData(true);
    }
}

void AImprovedVoxelBuilding::AddVisibleFacesToMesh(int32 X, int32 Y, int32 Z, TArray<FVector>& Vertices, TArray<int32>& Triangles, 
                         TArray<FVector>& Normals, TArray<FVector2D>& UVs, TArray<FColor>& Colors, TArray<FProcMeshTangent>& Tangents)
{
    // Position of voxel center
    FVector Center = FVector(X * VoxelSize, Y * VoxelSize, Z * VoxelSize);
    
    // Half size of voxel - slight reduction to create visual separation between cubes
    float HalfSize = VoxelSize * (0.5f - CubeMargin);
    
    // Voxel color
    FColor VoxelColor = VoxelGrid[X][Y][Z].Color;
    
    // Base index for this voxel
    int32 BaseIndex = Vertices.Num();
    
    // Check if each face is visible (not adjacent to another active voxel)
    bool bBottomFaceVisible = Z == 0 || !VoxelGrid[X][Y][Z-1].bIsActive;
    bool bTopFaceVisible = Z == GridSizeZ-1 || !VoxelGrid[X][Y][Z+1].bIsActive;
    bool bLeftFaceVisible = X == 0 || !VoxelGrid[X-1][Y][Z].bIsActive;
    bool bRightFaceVisible = X == GridSizeX-1 || !VoxelGrid[X+1][Y][Z].bIsActive;
    bool bBackFaceVisible = Y == 0 || !VoxelGrid[X][Y-1][Z].bIsActive;
    bool bFrontFaceVisible = Y == GridSizeY-1 || !VoxelGrid[X][Y+1][Z].bIsActive;
    
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
    // Convertir la position mondiale en coordonnées locales
    FVector LocalPosition = Location; // Déjà en coordonnées locales
    FVector LocalNormal = ImpactNormal; // Considérons que la normale est aussi en coordonnées locales
    
    // Convertir en coordonnées de grille sans décalage
    int32 GridX = FMath::Floor(LocalPosition.X / VoxelSize);
    int32 GridY = FMath::Floor(LocalPosition.Y / VoxelSize);
    int32 GridZ = FMath::Floor(LocalPosition.Z / VoxelSize);
    
    // Log pour le débogage
    UE_LOG(LogTemp, Warning, TEXT("Impact at local position: %s, Normal: %s"), 
           *LocalPosition.ToString(), *LocalNormal.ToString());
    UE_LOG(LogTemp, Warning, TEXT("Grid coordinates: (%d, %d, %d)"), GridX, GridY, GridZ);
    
    // Rayon en unités de grille
    int32 GridRadius = FMath::CeilToInt(Radius / VoxelSize);
    
    // Vérifier chaque voxel dans la zone d'explosion
    bool bAnyVoxelDestroyed = false;
    
    // Créer un tableau pour stocker les voxels à détruire avec leur priorité
    TArray<TPair<FIntVector, float>> VoxelsToDestroy;
    
    // Utiliser un raycasting 3D depuis le point d'impact dans plusieurs directions
    const int32 RayCount = 16; // Nombre de rayons
    const float MaxRayLength = Radius * 1.2f; // Légèrement plus long que le rayon pour toucher tous les voxels
    
    // Générer des rayons dans différentes directions
    for (int32 RayIndex = 0; RayIndex < RayCount; RayIndex++)
    {
        // Calculer une direction aléatoire avec un biais vers la direction de l'impact
        FVector RayDir;
        
        if (RayIndex == 0)
        {
            // Le premier rayon va directement dans la direction opposée à la normale (dans le bâtiment)
            RayDir = -LocalNormal;
        }
        else
        {
            // Les autres rayons sont aléatoires avec un biais vers l'intérieur
            float Pitch = FMath::DegreesToRadians(FMath::RandRange(-80.0f, 80.0f));
            float Yaw = FMath::DegreesToRadians(FMath::RandRange(0.0f, 360.0f));
            
            // Calculer la direction du rayon
            RayDir = FVector(
                FMath::Cos(Pitch) * FMath::Cos(Yaw),
                FMath::Cos(Pitch) * FMath::Sin(Yaw),
                FMath::Sin(Pitch)
            );
            
            // Ajouter un biais vers l'intérieur du bâtiment
            RayDir = (RayDir - LocalNormal).GetSafeNormal();
        }
        
        // Tracer le rayon à travers les voxels
        FVector RayStart = LocalPosition;
        FVector RayEnd = RayStart + RayDir * MaxRayLength;
        
        // Version simplifiée de l'algorithme de Bresenham pour la traversée de voxel 3D
        FVector CurrentPos = RayStart;
        FVector Step = RayDir * (VoxelSize * 0.25f); // Pas plus petit que la taille du voxel pour ne pas en manquer
        
        for (float Distance = 0.0f; Distance <= MaxRayLength; Distance += VoxelSize * 0.25f)
        {
            // Calculer les coordonnées de grille pour cette position
            int32 X = FMath::Floor(CurrentPos.X / VoxelSize);
            int32 Y = FMath::Floor(CurrentPos.Y / VoxelSize);
            int32 Z = FMath::Floor(CurrentPos.Z / VoxelSize);
            
            // Vérifier les limites
            if (X >= 0 && X < GridSizeX && Y >= 0 && Y < GridSizeY && Z >= 0 && Z < GridSizeZ)
            {
                // Si ce voxel est actif, le marquer pour destruction
                if (VoxelGrid[X][Y][Z].bIsActive)
                {
                    // Calculer la priorité de destruction (1.0 = centre de l'explosion, 0.0 = bord)
                    float DistanceFromImpact = FVector::Dist(CurrentPos, LocalPosition);
                    float DestructionPriority = 1.0f - FMath::Min(1.0f, DistanceFromImpact / Radius);
                    
                    // Ajouter à la liste si pas déjà présent ou avec une priorité plus élevée
                    bool bAlreadyAdded = false;
                    FIntVector VoxelCoord(X, Y, Z);
                    
                    for (int32 i = 0; i < VoxelsToDestroy.Num(); i++)
                    {
                        if (VoxelsToDestroy[i].Key.X == VoxelCoord.X && 
                            VoxelsToDestroy[i].Key.Y == VoxelCoord.Y && 
                            VoxelsToDestroy[i].Key.Z == VoxelCoord.Z)
                        {
                            bAlreadyAdded = true;
                            // Mettre à jour la priorité si celle-ci est plus élevée
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
            
            // Avancer le long du rayon
            CurrentPos += Step;
        }
    }
    
    // Trier les voxels par priorité de destruction (du plus prioritaire au moins prioritaire)
    VoxelsToDestroy.Sort([](const TPair<FIntVector, float>& A, const TPair<FIntVector, float>& B) {
        return A.Value > B.Value;
    });
    
    // Appliquer la destruction avec une chance basée sur la priorité
    for (const TPair<FIntVector, float>& VoxelData : VoxelsToDestroy)
    {
        int32 X = VoxelData.Key.X;
        int32 Y = VoxelData.Key.Y;
        int32 Z = VoxelData.Key.Z;
        float Priority = VoxelData.Value;
        
        // Chance de destruction basée sur la priorité
        if (FMath::FRand() < Priority)
        {
            VoxelGrid[X][Y][Z].bIsActive = false;
            bAnyVoxelDestroyed = true;
            
            // Log pour le voxel central (celui directement touché)
            if (X == GridX && Y == GridY && Z == GridZ)
            {
                UE_LOG(LogTemp, Warning, TEXT("Destroying central voxel at grid (%d, %d, %d)"), X, Y, Z);
            }
        }
    }
    
    // Si des voxels ont été détruits, recréer le mesh
    if (bAnyVoxelDestroyed)
    {
        CreateMesh();
    }
}

bool AImprovedVoxelBuilding::Server_DestroyVoxelsAt_Validate(FVector Location, FVector ImpactNormal, float Radius)
{
    // Validation basique : s'assurer que le rayon est positif
    return Radius > 0.0f;
}

void AImprovedVoxelBuilding::Server_DestroyVoxelsAt_Implementation(FVector Location, FVector ImpactNormal, float Radius)
{
    // Appliquer la destruction sur le serveur
    DestroyVoxelsAt(Location, ImpactNormal, Radius);
    
    // Propager à tous les clients
    Multicast_DestroyVoxelsAt(Location, ImpactNormal, Radius);
}

void AImprovedVoxelBuilding::Multicast_DestroyVoxelsAt_Implementation(FVector Location, FVector ImpactNormal, float Radius)
{
    // Ne pas exécuter à nouveau sur le serveur, uniquement sur les clients
    if (!HasAuthority())
    {
        DestroyVoxelsAt(Location, ImpactNormal, Radius);
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
