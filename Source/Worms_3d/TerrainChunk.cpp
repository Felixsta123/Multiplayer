#include "TerrainChunk.h"
#include "TerrainChunk.h"
#include "MarchingCubes.h"
#include "ProceduralMeshComponent.h"
inline FArchive& operator<<(FArchive& Ar, const FIntVector& Vec)
{
    Ar << const_cast<int32&>(Vec.X);
    Ar << const_cast<int32&>(Vec.Y);
    Ar << const_cast<int32&>(Vec.Z);
    return Ar;
}

inline FArchive& operator<<(FArchive& Ar, const float& Value)
{
    return Ar << const_cast<float&>(Value);
}

inline FArchive& operator<<(FArchive& Ar, const int32& Value)
{
    return Ar << const_cast<int32&>(Value);
}

inline FArchive& operator<<(FArchive& Ar, const bool& Value)
{
    return Ar << const_cast<bool&>(Value);
}

inline FArchive& operator<<(FArchive& Ar, const FColor& Color)
{
    uint8 R = Color.R;
    uint8 G = Color.G;
    uint8 B = Color.B;
    uint8 A = Color.A;

    Ar << R;
    Ar << G;
    Ar << B;
    Ar << A;

    if (Ar.IsLoading())
    {
        const_cast<FColor&>(Color) = FColor(R, G, B, A);
    }

    return Ar;
}
UTerrainChunk::UTerrainChunk()
{
    ChunkPosition = FIntVector::ZeroValue;
    ChunkSize = FIntVector(16, 16, 16);
    State = EChunkState::Uninitialized;
    bIsDirty = false;
    bNeedsReplication = false;
    ModificationCount = 0;
    MeshComponent = nullptr;
    bIsVisible = true;
    WorldOrigin = FVector::ZeroVector;
}

void UTerrainChunk::Initialize(const FIntVector& InChunkPosition, const FIntVector& InChunkSize, 
                             UProceduralMeshComponent* InMeshComponent, const FVector& InWorldOrigin)
{
    ChunkPosition = InChunkPosition;
    ChunkSize = InChunkSize;
    MeshComponent = InMeshComponent;
    WorldOrigin = InWorldOrigin;
    State = EChunkState::Initialized;
    
    // Initialiser la grille de voxels
    VoxelData = FVoxelGrid(ChunkSize);
    
    // Générer l'ID du chunk
    GenerateChunkID();
}

void UTerrainChunk::GenerateChunkID()
{
    // Format: Chunk_X_Y_Z
    ChunkID = FString::Printf(TEXT("Chunk_%d_%d_%d"), ChunkPosition.X, ChunkPosition.Y, ChunkPosition.Z);
}

void UTerrainChunk::SetVisibility(bool bVisible)
{
    if (bIsVisible != bVisible)
    {
        bIsVisible = bVisible;
        
        if (MeshComponent)
        {
            MeshComponent->SetVisibility(bVisible);
        }
    }
}

FIntVector UTerrainChunk::LocalToGlobal(const FIntVector& LocalCoord) const
{
    return FIntVector(
        LocalCoord.X + ChunkPosition.X * ChunkSize.X,
        LocalCoord.Y + ChunkPosition.Y * ChunkSize.Y,
        LocalCoord.Z + ChunkPosition.Z * ChunkSize.Z
    );
}

FIntVector UTerrainChunk::GlobalToLocal(const FIntVector& GlobalCoord) const
{
    return FIntVector(
        GlobalCoord.X - ChunkPosition.X * ChunkSize.X,
        GlobalCoord.Y - ChunkPosition.Y * ChunkSize.Y,
        GlobalCoord.Z - ChunkPosition.Z * ChunkSize.Z
    );
}

bool UTerrainChunk::ContainsGlobalCoord(const FIntVector& GlobalCoord) const
{
    FIntVector LocalCoord = GlobalToLocal(GlobalCoord);
    return LocalCoord.X >= 0 && LocalCoord.X < ChunkSize.X &&
           LocalCoord.Y >= 0 && LocalCoord.Y < ChunkSize.Y &&
           LocalCoord.Z >= 0 && LocalCoord.Z < ChunkSize.Z;
}

bool UTerrainChunk::ContainsWorldPoint(const FVector& WorldPoint) const
{
    // Convertir en coordonnées locales du chunk
    FVector LocalPoint = WorldPoint - WorldOrigin;
    
    // Convertir en indices de voxel
    FIntVector VoxelCoord = FIntVector(
        FMath::FloorToInt(LocalPoint.X / VoxelData.GetVoxelSize()),
        FMath::FloorToInt(LocalPoint.Y / VoxelData.GetVoxelSize()),
        FMath::FloorToInt(LocalPoint.Z / VoxelData.GetVoxelSize())
    );
    
    // Vérifier si les coordonnées sont dans le chunk
    return VoxelCoord.X >= 0 && VoxelCoord.X < ChunkSize.X &&
           VoxelCoord.Y >= 0 && VoxelCoord.Y < ChunkSize.Y &&
           VoxelCoord.Z >= 0 && VoxelCoord.Z < ChunkSize.Z;
}

bool UTerrainChunk::CarveSphere(const FVector& WorldCenter, float Radius, float Falloff)
{
    // Vérifier si la sphère intersecte ce chunk
    FVector ChunkMin = WorldOrigin;
    FVector ChunkMax = WorldOrigin + FVector(
        ChunkSize.X * VoxelData.GetVoxelSize(),
        ChunkSize.Y * VoxelData.GetVoxelSize(),
        ChunkSize.Z * VoxelData.GetVoxelSize()
    );
    
    UE_LOG(LogTemp, Warning, TEXT("CarveSphere called on chunk %s: Center=%s, Radius=%.1f"), 
        *ChunkID, *WorldCenter.ToString(), Radius);
    UE_LOG(LogTemp, Warning, TEXT("Chunk bounds: Min=%s, Max=%s"), 
        *ChunkMin.ToString(), *ChunkMax.ToString());
    
    // Calculer le point le plus proche du chunk à partir du centre de la sphère
    FVector ClosestPoint(
        FMath::Clamp(WorldCenter.X, ChunkMin.X, ChunkMax.X),
        FMath::Clamp(WorldCenter.Y, ChunkMin.Y, ChunkMax.Y),
        FMath::Clamp(WorldCenter.Z, ChunkMin.Z, ChunkMax.Z)
    );
    
    // Calculer la distance entre le point le plus proche et le centre de la sphère
    float Distance = FVector::Dist(ClosestPoint, WorldCenter);
    
    UE_LOG(LogTemp, Warning, TEXT("Closest point: %s, Distance: %.1f"), 
        *ClosestPoint.ToString(), Distance);
    
    // Si la distance est supérieure au rayon, la sphère n'intersecte pas le chunk
    if (Distance > Radius)
    {
        UE_LOG(LogTemp, Warning, TEXT("Sphere does not intersect chunk"));
        return false;
    }
    
    // La sphère intersecte le chunk, appliquer la modification
    UE_LOG(LogTemp, Warning, TEXT("Sphere intersects chunk, applying modification"));
    
    bool bAnyModification = false;
    
    // Accéder directement à la fonction de la grille de voxels
    VoxelData.CarveSphere(WorldCenter, Radius, Falloff, WorldOrigin);
    
    // Compter combien de voxels ont été modifiés
    int32 ModifiedCount = 0;
    const TArray<FVoxel>& Voxels = VoxelData.GetRawVoxels();
    for (const FVoxel& Voxel : Voxels)
    {
        if (Voxel.State != EVoxelState::Full || Voxel.Density < 0.99f)
        {
            ModifiedCount++;
            bAnyModification = true;
        }
    }
    
    UE_LOG(LogTemp, Warning, TEXT("Modified %d voxels out of %d"), 
        ModifiedCount, Voxels.Num());
    
    if (bAnyModification)
    {
        // Appliquer un lissage léger
        VoxelData.SmoothVolume(1);
        
        // Marquer le chunk comme sale
        MarkDirty();
        IncrementModificationCount();
        
        UE_LOG(LogTemp, Warning, TEXT("Chunk marked dirty for update"));
    }
    
    return bAnyModification;
}

void UTerrainChunk::GenerateRandomTerrain(float HeightScale, float NoiseScale)
{
    // Générer le terrain avec la fonction de la classe VoxelData
    VoxelData.GenerateRandomTerrain(HeightScale, NoiseScale, 
        ChunkPosition.X * 100 + ChunkPosition.Y * 10 + ChunkPosition.Z); // Décalage par position
    
    // Marquer le chunk comme généré
    State = EChunkState::Generated;
    
    // Marquer comme sale pour régénérer le mesh
    MarkDirty(false); // Ne pas répliquer la génération initiale
}

void UTerrainChunk::GenerateTerrainWithSurface(int32 SurfaceHeight)
{
    // Limiter SurfaceHeight aux limites du volume
    SurfaceHeight = FMath::Clamp(SurfaceHeight, 0, ChunkSize.Z - 1);
    
    UE_LOG(LogTemp, Error, TEXT("Generating chunk %s with surface at height %d"), *ChunkID, SurfaceHeight);
    
    // Remplir tous les voxels jusqu'à SurfaceHeight
    for (int32 z = 0; z < ChunkSize.Z; ++z)
    {
        for (int32 y = 0; y < ChunkSize.Y; ++y)
        {
            for (int32 x = 0; x < ChunkSize.X; ++x)
            {
                FVoxel& Voxel = GetVoxel(x, y, z);
                
                // Vider au-dessus de la surface
                if (z > SurfaceHeight)
                {
                    Voxel.State = EVoxelState::Empty;
                    Voxel.Density = 0.0f; // Valeur cruciale pour Marching Cubes
                }
                // Remplir sous la surface
                else
                {
                    Voxel.State = EVoxelState::Full;
                    Voxel.Density = 1.0f; // Valeur cruciale pour Marching Cubes
                    
                    // Définir le matériau en fonction de la profondeur
                    if (z == SurfaceHeight)
                    {
                        Voxel.Material = EVoxelMaterial::Grass;
                    }
                    else if (z > SurfaceHeight - 3)
                    {
                        Voxel.Material = EVoxelMaterial::Dirt;
                    }
                    else
                    {
                        Voxel.Material = EVoxelMaterial::Stone;
                    }
                    
                    Voxel.UpdateColorFromMaterial();
                }
            }
        }
    }
    
    // Important: Logs pour vérifier que les densités sont correctes
    UE_LOG(LogTemp, Error, TEXT("Surface voxel densities: Top=%.1f, Middle=%.1f, Bottom=%.1f"),
        GetVoxel(ChunkSize.X/2, ChunkSize.Y/2, SurfaceHeight+1).Density,  // devrait être 0.0
        GetVoxel(ChunkSize.X/2, ChunkSize.Y/2, SurfaceHeight).Density,    // devrait être 1.0
        GetVoxel(ChunkSize.X/2, ChunkSize.Y/2, 0).Density);               // devrait être 1.0
}

void UTerrainChunk::GenerateFullVolume()
{
    // Remplir tout le volume avec des voxels solides
    for (int32 z = 0; z < ChunkSize.Z; z++)
    {
        for (int32 y = 0; y < ChunkSize.Y; y++)
        {
            for (int32 x = 0; x < ChunkSize.X; x++)
            {
                VoxelData.SetVoxel(x, y, z, FVoxel(EVoxelState::Full, EVoxelMaterial::Dirt, 1.0f));
            }
        }
    }
    
    // Marquer le chunk comme généré
    State = EChunkState::Generated;
    
    // Marquer comme sale pour générer le mesh
    MarkDirty(false);
}

void UTerrainChunk::GenerateEmptyVolume()
{
    // Remplir tout le volume avec des voxels vides
    for (int32 z = 0; z < ChunkSize.Z; z++)
    {
        for (int32 y = 0; y < ChunkSize.Y; y++)
        {
            for (int32 x = 0; x < ChunkSize.X; x++)
            {
                VoxelData.SetVoxel(x, y, z, FVoxel(EVoxelState::Empty, EVoxelMaterial::Dirt, 0.0f));
            }
        }
    }
    
    // Marquer le chunk comme généré
    State = EChunkState::Generated;
    
    // Marquer comme sale pour générer le mesh
    MarkDirty(false);
}

void UTerrainChunk::SmoothVolume(int32 Iterations)
{
    // Appliquer le lissage au volume de voxels
    VoxelData.SmoothVolume(Iterations);
    
    // Marquer comme sale pour régénérer le mesh
    MarkDirty();
}

void UTerrainChunk::UpdateMeshData()
{
    // Réinitialiser les données de mesh
    ClearMeshData();
    
    // Définir l'état comme en cours de génération
    State = EChunkState::Generating;
    
    UE_LOG(LogTemp, Warning, TEXT("Generating Marching Cubes mesh for chunk %s"), *ChunkID);
    
    // Lambda pour obtenir la densité à une position
    auto GetDensity = [this](float X, float Y, float Z) -> float {
        int32 IX = FMath::FloorToInt(X);
        int32 IY = FMath::FloorToInt(Y);
        int32 IZ = FMath::FloorToInt(Z);
        
        if (IX < 0 || IX >= ChunkSize.X ||
            IY < 0 || IY >= ChunkSize.Y ||
            IZ < 0 || IZ >= ChunkSize.Z)
        {
            return 1.0f; // Considérer hors limites comme solide
        }
        
        return VoxelData.GetVoxel(IX, IY, IZ).Density;
    };
    
    // Lambda pour calculer le gradient (pour la normale)
    auto GetGradient = [this, &GetDensity](float X, float Y, float Z) -> FVector {
        const float Delta = 0.5f;
        
        float DX1 = GetDensity(X + Delta, Y, Z);
        float DX2 = GetDensity(X - Delta, Y, Z);
        float DY1 = GetDensity(X, Y + Delta, Z);
        float DY2 = GetDensity(X, Y - Delta, Z);
        float DZ1 = GetDensity(X, Y, Z + Delta);
        float DZ2 = GetDensity(X, Y, Z - Delta);
        
        return FVector(DX2 - DX1, DY2 - DY1, DZ2 - DZ1).GetSafeNormal();
    };
    
    // Lambda pour obtenir la couleur à une position
    auto GetColor = [this](float X, float Y, float Z) -> FColor {
        int32 IX = FMath::FloorToInt(X);
        int32 IY = FMath::FloorToInt(Y);
        int32 IZ = FMath::FloorToInt(Z);
        
        if (IX < 0 || IX >= ChunkSize.X ||
            IY < 0 || IY >= ChunkSize.Y ||
            IZ < 0 || IZ >= ChunkSize.Z)
        {
            return FColor(120, 80, 40, 255); // Couleur par défaut
        }
        
        FVoxel Voxel = VoxelData.GetVoxel(IX, IY, IZ);
        return Voxel.Color != FColor::Black ? Voxel.Color : FColor(120, 80, 40, 255);
    };
    
    // Générer le maillage d'isosurface
    try {
        UE_LOG(LogTemp, Warning, TEXT("Calling GenerateIsosurfaceMesh for chunk %s"), *ChunkID);
        
        UMarchingCubesUtils::GenerateIsosurfaceMesh(
            MeshData.Vertices,
            MeshData.Triangles,
            MeshData.Normals,
            MeshData.VertexColors,
            GetDensity,
            GetGradient,
            GetColor,
            FIntVector(0, 0, 0),
            ChunkSize,
            0.5f
        );
        
        UE_LOG(LogTemp, Warning, TEXT("GenerateIsosurfaceMesh completed with %d vertices and %d triangles"), 
            MeshData.Vertices.Num(), MeshData.Triangles.Num() / 3);
    }
    catch (const std::exception& e) {
        UE_LOG(LogTemp, Error, TEXT("Exception in GenerateIsosurfaceMesh: %s"), UTF8_TO_TCHAR(e.what()));
        
        // Fallback to cube if marching cubes fails
        CreateFallbackCube();
    }
    catch (...) {
        UE_LOG(LogTemp, Error, TEXT("Unknown exception in GenerateIsosurfaceMesh"));
        
        // Fallback to cube if marching cubes fails
        CreateFallbackCube();
    }
    
    // Si aucun vertex n'a été généré, créer un cube de secours
    if (MeshData.Vertices.Num() == 0) {
        UE_LOG(LogTemp, Warning, TEXT("No vertices generated, creating fallback cube"));
        CreateFallbackCube();
    }
    
    // Générer les coordonnées UV
    if (MeshData.UVs.Num() < MeshData.Vertices.Num()) {
        MeshData.UVs.SetNum(MeshData.Vertices.Num());
        for (int32 i = 0; i < MeshData.Vertices.Num(); i++)
        {
            // UV simples basées sur les coordonnées XY
            MeshData.UVs[i] = FVector2D(
                MeshData.Vertices[i].X / (ChunkSize.X * VoxelData.GetVoxelSize()),
                MeshData.Vertices[i].Y / (ChunkSize.Y * VoxelData.GetVoxelSize())
            );
        }
    }
    
    // Générer les tangentes
    if (MeshData.Tangents.Num() < MeshData.Vertices.Num()) {
        MeshData.Tangents.SetNum(MeshData.Vertices.Num());
        for (int32 i = 0; i < MeshData.Vertices.Num(); i++)
        {
            // Tangente par défaut (X+)
            MeshData.Tangents[i] = FProcMeshTangent(1.0f, 0.0f, 0.0f);
        }
    }
    
    // Définir l'état comme généré
    State = EChunkState::Generated;
    
    // Indiquer que le chunk n'est plus sale
    bIsDirty = false;
    
    UE_LOG(LogTemp, Warning, TEXT("Mesh generation completed for chunk %s"), *ChunkID);
}

// Ajouter cette fonction helper
void UTerrainChunk::CreateFallbackCube()
{
    // Créer un cube simple comme fallback
    MeshData.Vertices.Add(FVector(0, 0, 0));
    MeshData.Vertices.Add(FVector(ChunkSize.X * VoxelData.GetVoxelSize(), 0, 0));
    MeshData.Vertices.Add(FVector(0, ChunkSize.Y * VoxelData.GetVoxelSize(), 0));
    MeshData.Vertices.Add(FVector(ChunkSize.X * VoxelData.GetVoxelSize(), ChunkSize.Y * VoxelData.GetVoxelSize(), 0));
    MeshData.Vertices.Add(FVector(0, 0, ChunkSize.Z * VoxelData.GetVoxelSize()));
    MeshData.Vertices.Add(FVector(ChunkSize.X * VoxelData.GetVoxelSize(), 0, ChunkSize.Z * VoxelData.GetVoxelSize()));
    MeshData.Vertices.Add(FVector(0, ChunkSize.Y * VoxelData.GetVoxelSize(), ChunkSize.Z * VoxelData.GetVoxelSize()));
    MeshData.Vertices.Add(FVector(ChunkSize.X * VoxelData.GetVoxelSize(), ChunkSize.Y * VoxelData.GetVoxelSize(), ChunkSize.Z * VoxelData.GetVoxelSize()));
    
    // Face avant
    MeshData.Triangles.Add(0); MeshData.Triangles.Add(2); MeshData.Triangles.Add(1);
    MeshData.Triangles.Add(1); MeshData.Triangles.Add(2); MeshData.Triangles.Add(3);
    // Face arrière
    MeshData.Triangles.Add(4); MeshData.Triangles.Add(5); MeshData.Triangles.Add(6);
    MeshData.Triangles.Add(5); MeshData.Triangles.Add(7); MeshData.Triangles.Add(6);
    // Face gauche
    MeshData.Triangles.Add(0); MeshData.Triangles.Add(4); MeshData.Triangles.Add(2);
    MeshData.Triangles.Add(2); MeshData.Triangles.Add(4); MeshData.Triangles.Add(6);
    // Face droite
    MeshData.Triangles.Add(1); MeshData.Triangles.Add(3); MeshData.Triangles.Add(5);
    MeshData.Triangles.Add(3); MeshData.Triangles.Add(7); MeshData.Triangles.Add(5);
    // Face supérieure
    MeshData.Triangles.Add(2); MeshData.Triangles.Add(6); MeshData.Triangles.Add(3);
    MeshData.Triangles.Add(3); MeshData.Triangles.Add(6); MeshData.Triangles.Add(7);
    // Face inférieure
    MeshData.Triangles.Add(0); MeshData.Triangles.Add(1); MeshData.Triangles.Add(4);
    MeshData.Triangles.Add(1); MeshData.Triangles.Add(5); MeshData.Triangles.Add(4);
    
    // Ajouter des normales, UVs et couleurs pour le cube
    MeshData.Normals.SetNum(8);
    MeshData.UVs.SetNum(8);
    MeshData.VertexColors.SetNum(8);
    for (int32 i = 0; i < 8; i++)
    {
        MeshData.Normals[i] = FVector(0, 0, 1);
        MeshData.UVs[i] = FVector2D(0, 0);
        MeshData.VertexColors[i] = FColor(120, 80, 40, 255); // Marron
    }
    
    // Générer les tangentes
    MeshData.Tangents.SetNum(8);
    for (int32 i = 0; i < 8; i++)
    {
        MeshData.Tangents[i] = FProcMeshTangent(1.0f, 0.0f, 0.0f);
    }
    
    UE_LOG(LogTemp, Warning, TEXT("Created fallback cube with 8 vertices and 12 triangles"));
}


void UTerrainChunk::ApplyMeshData()
{
    // Vérifier que le mesh component existe
    if (!MeshComponent)
    {
        UE_LOG(LogTemp, Error, TEXT("UTerrainChunk::ApplyMeshData: MeshComponent is null for chunk %s"), *ChunkID);
        return;
    }
    
    // Si les données de mesh sont vides ou si le chunk est invisible, effacer la section
    if (MeshData.Vertices.Num() == 0 || !bIsVisible)
    {
        MeshComponent->ClearAllMeshSections();
        return;
    }
    
    // Convertir les couleurs en couleurs linéaires
    TArray<FLinearColor> LinearColors;
    LinearColors.Reserve(MeshData.VertexColors.Num());
    for (const FColor& Color : MeshData.VertexColors)
    {
        LinearColors.Add(Color.ReinterpretAsLinear());
    }
    
    // Mettre à jour le mesh procédural
    MeshComponent->CreateMeshSection_LinearColor(
        0,                     // Section index
        MeshData.Vertices,     // Vertices
        MeshData.Triangles,    // Triangles
        MeshData.Normals,      // Normals
        MeshData.UVs,          // UVs
        LinearColors,          // Vertex colors (now linear)
        MeshData.Tangents,     // Tangents
        true                   // Créer une collision
    );
    
    // Activer la collision
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
}

void UTerrainChunk::CalculateMarchingCubesSection(int32 StartZ, int32 EndZ)
{
    // Cette fonction peut être utilisée pour les calculs asynchrones
    // Elle calcule une partie (tranche en Z) du maillage Marching Cubes
    
    // Créer des tampons temporaires pour les données de mesh
    TArray<FVector> TempVertices;
    TArray<int32> TempTriangles;
    TArray<FVector> TempNormals;
    TArray<FColor> TempVertexColors;
    
    // Lambda pour obtenir la densité à une position
    auto GetDensity = [this](float X, float Y, float Z) -> float {
        int32 IX = FMath::FloorToInt(X);
        int32 IY = FMath::FloorToInt(Y);
        int32 IZ = FMath::FloorToInt(Z);
        
        if (IX < 0 || IX >= ChunkSize.X ||
            IY < 0 || IY >= ChunkSize.Y ||
            IZ < 0 || IZ >= ChunkSize.Z)
        {
            return 1.0f; // Considérer hors limites comme solide
        }
        
        return VoxelData.GetVoxel(IX, IY, IZ).Density;
    };
    
    // Lambda pour calculer le gradient (pour la normale)
    auto GetGradient = [this, &GetDensity](float X, float Y, float Z) -> FVector {
        const float Delta = 0.5f;
        
        float DX1 = GetDensity(X + Delta, Y, Z);
        float DX2 = GetDensity(X - Delta, Y, Z);
        float DY1 = GetDensity(X, Y + Delta, Z);
        float DY2 = GetDensity(X, Y - Delta, Z);
        float DZ1 = GetDensity(X, Y, Z + Delta);
        float DZ2 = GetDensity(X, Y, Z - Delta);
        
        return FVector(DX2 - DX1, DY2 - DY1, DZ2 - DZ1).GetSafeNormal();
    };
    
    // Lambda pour obtenir la couleur à une position
    auto GetColor = [this](float X, float Y, float Z) -> FColor {
        int32 IX = FMath::FloorToInt(X);
        int32 IY = FMath::FloorToInt(Y);
        int32 IZ = FMath::FloorToInt(Z);
        
        if (IX < 0 || IX >= ChunkSize.X ||
            IY < 0 || IY >= ChunkSize.Y ||
            IZ < 0 || IZ >= ChunkSize.Z)
        {
            return FColor(120, 80, 40, 255); // Couleur par défaut
        }
        
        return VoxelData.GetVoxel(IX, IY, IZ).Color;
    };
    
    // Générer le maillage pour cette section
    UMarchingCubesUtils::GenerateIsosurfaceMesh(
        TempVertices,
        TempTriangles,
        TempNormals,
        TempVertexColors,
        GetDensity,
        GetGradient,
        GetColor,
        FIntVector(0, 0, StartZ),
        FIntVector(ChunkSize.X, ChunkSize.Y, EndZ),
        0.5f
    );
    
    // Verrouiller les données de mesh pour les mettre à jour
    {
        // Décalage pour les indices
        int32 VertexOffset = MeshData.Vertices.Num();
        
        // Ajouter les vertices et les normales
        MeshData.Vertices.Append(TempVertices);
        MeshData.Normals.Append(TempNormals);
        MeshData.VertexColors.Append(TempVertexColors);
        
        // Ajouter les triangles avec décalage
        for (int32 Index : TempTriangles)
        {
            MeshData.Triangles.Add(Index + VertexOffset);
        }
        
        // Générer les coordonnées UV pour les nouveaux vertices
        for (int32 i = 0; i < TempVertices.Num(); i++)
        {
            // UV simples basées sur les coordonnées XY
            FVector2D UV(
                TempVertices[i].X / (ChunkSize.X * VoxelData.GetVoxelSize()),
                TempVertices[i].Y / (ChunkSize.Y * VoxelData.GetVoxelSize())
            );
            
            MeshData.UVs.Add(UV);
            
            // Tangente par défaut (X+)
            MeshData.Tangents.Add(FProcMeshTangent(1.0f, 0.0f, 0.0f));
        }
    }
}

TArray<uint8> UTerrainChunk::SerializeForReplication() const
{
    // Sérialiser les données du chunk pour la réplication
    TArray<uint8> Data;
    FMemoryWriter Writer(Data);
    
    // Écrire la position et la taille du chunk
    Writer << ChunkPosition;
    Writer << ChunkSize;
    
    // Écrire le nombre de voxels
    const TArray<FVoxel>& Voxels = VoxelData.GetRawVoxels();
    int32 VoxelCount = Voxels.Num();
    Writer << VoxelCount;
    
    // Écrire chaque voxel
    for (const FVoxel& Voxel : Voxels)
    {
        // Écrire l'état et la densité
        uint8 StateValue = static_cast<uint8>(Voxel.State);
        uint8 MaterialValue = static_cast<uint8>(Voxel.Material);
        Writer << StateValue;
        Writer << MaterialValue;
        Writer << Voxel.Density;
        Writer << Voxel.Color;
    }
    
    // Écrire d'autres propriétés si nécessaire
    Writer << ModificationCount;
    
    return Data;
}

bool UTerrainChunk::DeserializeFromData(const TArray<uint8>& Data)
{
    if (Data.Num() == 0)
    {
        return false;
    }
    
    FMemoryReader Reader(Data);
    
    // Lire la position et la taille du chunk
    FIntVector NewPosition;
    FIntVector NewSize;
    Reader << NewPosition;
    Reader << NewSize;
    
    // Vérifier que les données correspondent au bon chunk
    if (NewPosition != ChunkPosition || NewSize != ChunkSize)
    {
        UE_LOG(LogTemp, Error, TEXT("Chunk deserialize error: Position/Size mismatch"));
        return false;
    }
    
    // Lire le nombre de voxels
    int32 VoxelCount;
    Reader << VoxelCount;
    
    // Vérifier la cohérence
    if (VoxelCount != ChunkSize.X * ChunkSize.Y * ChunkSize.Z)
    {
        UE_LOG(LogTemp, Error, TEXT("Chunk deserialize error: Voxel count mismatch"));
        return false;
    }
    
    // Réinitialiser la grille de voxels
    VoxelData = FVoxelGrid(ChunkSize, VoxelData.GetVoxelSize());
    TArray<FVoxel>& Voxels = VoxelData.GetRawVoxels();
    
    // Lire chaque voxel
    for (int32 i = 0; i < VoxelCount; i++)
    {
        // Lire l'état et la densité
        uint8 StateValue;
        uint8 MaterialValue;
        float Density;
        FColor Color;
        
        Reader << StateValue;
        Reader << MaterialValue;
        Reader << Density;
        Reader << Color;
        
        // Convertir les valeurs lues
        Voxels[i].State = static_cast<EVoxelState>(StateValue);
        Voxels[i].Material = static_cast<EVoxelMaterial>(MaterialValue);
        Voxels[i].Density = Density;
        Voxels[i].Color = Color;
    }
    
    // Lire d'autres propriétés si nécessaire
    Reader << ModificationCount;
    
    // Marquer le chunk comme modifié pour régénérer le mesh
    MarkDirty(false);
    
    return true;
}

TArray<uint8> UTerrainChunk::SerializeModifiedVoxels() const
{
    // Sérialiser uniquement les voxels modifiés pour une réplication plus efficace
    TArray<uint8> Data;
    FMemoryWriter Writer(Data);
    
    // Écrire la position du chunk
    Writer << ChunkPosition;
    
    // Écrire le nombre de modifications
    Writer << ModificationCount;
    
    // Obtenir les voxels
    const TArray<FVoxel>& Voxels = VoxelData.GetRawVoxels();
    
    // Écrire uniquement les voxels modifiés (non-pleins)
    TArray<int32> ModifiedIndices;
    for (int32 i = 0; i < Voxels.Num(); i++)
    {
        if (Voxels[i].State != EVoxelState::Full)
        {
            ModifiedIndices.Add(i);
        }
    }
    
    // Écrire le nombre de voxels modifiés
    int32 ModifiedCount = ModifiedIndices.Num();
    Writer << ModifiedCount;
    
    // Écrire chaque voxel modifié
    for (int32 Index : ModifiedIndices)
    {
        // Écrire l'indice
        Writer << Index;
        
        // Écrire les données du voxel
        const FVoxel& Voxel = Voxels[Index];
        uint8 StateValue = static_cast<uint8>(Voxel.State);
        uint8 MaterialValue = static_cast<uint8>(Voxel.Material);
        
        Writer << StateValue;
        Writer << MaterialValue;
        Writer << Voxel.Density;
        Writer << Voxel.Color;
    }
    
    return Data;
}

bool UTerrainChunk::ApplyModifiedVoxels(const TArray<uint8>& Data)
{
    if (Data.Num() == 0)
    {
        return false;
    }
    
    FMemoryReader Reader(Data);
    
    // Lire la position du chunk
    FIntVector SourcePosition;
    Reader << SourcePosition;
    
    // Vérifier que les données correspondent au bon chunk
    if (SourcePosition != ChunkPosition)
    {
        UE_LOG(LogTemp, Error, TEXT("Modified voxels apply error: Position mismatch"));
        return false;
    }
    
    // Lire le nombre de modifications
    int32 SourceModCount;
    Reader << SourceModCount;
    
    // Si le nombre de modifications est inférieur ou égal au nombre actuel, ignorer
    if (SourceModCount <= ModificationCount)
    {
        return false;
    }
    
    // Lire le nombre de voxels modifiés
    int32 ModifiedCount;
    Reader << ModifiedCount;
    
    // Obtenir les voxels
    TArray<FVoxel>& Voxels = VoxelData.GetRawVoxels();
    
    // Appliquer chaque modification
    for (int32 i = 0; i < ModifiedCount; i++)
    {
        // Lire l'indice
        int32 Index;
        Reader << Index;
        
        // Vérifier que l'indice est valide
        if (!Voxels.IsValidIndex(Index))
        {
            UE_LOG(LogTemp, Error, TEXT("Modified voxels apply error: Invalid index"));
            continue;
        }
        
        // Lire les données du voxel
        uint8 StateValue;
        uint8 MaterialValue;
        float Density;
        FColor Color;
        
        Reader << StateValue;
        Reader << MaterialValue;
        Reader << Density;
        Reader << Color;
        
        // Mettre à jour le voxel
        Voxels[Index].State = static_cast<EVoxelState>(StateValue);
        Voxels[Index].Material = static_cast<EVoxelMaterial>(MaterialValue);
        Voxels[Index].Density = Density;
        Voxels[Index].Color = Color;
    }
    
    // Mettre à jour le nombre de modifications
    ModificationCount = SourceModCount;
    
    // Marquer le chunk comme modifié pour régénérer le mesh
    MarkDirty(false);
    
    return true;
}