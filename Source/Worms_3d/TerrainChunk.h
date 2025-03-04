#pragma once
#include "CoreMinimal.h"
#include "VoxelData.h"
#include "ProceduralMeshComponent.h"
#include "TerrainChunk.generated.h"
// Custom serialization operators for basic types

/**
 * État d'un chunk de terrain
 */
UENUM(BlueprintType)
enum class EChunkState : uint8
{
    Uninitialized,     // Chunk pas encore initialisé
    Initialized,       // Chunk initialisé mais pas encore généré
    Generated,         // Chunk avec mesh généré
    Modified,          // Chunk modifié et nécessitant une mise à jour
    Generating,        // Chunk en cours de génération
    Error              // Erreur lors de la génération
};

/**
 * Structure pour les données du mesh d'un chunk
 */
USTRUCT(BlueprintType)
struct WORMS_3D_API FChunkMeshData
{
    GENERATED_BODY()

    UPROPERTY()
    TArray<FVector> Vertices;

    UPROPERTY()
    TArray<int32> Triangles;

    UPROPERTY()
    TArray<FVector> Normals;

    UPROPERTY()
    TArray<FVector2D> UVs;

    UPROPERTY()
    TArray<FColor> VertexColors;

    UPROPERTY()
    TArray<FProcMeshTangent> Tangents;

    void Clear()
    {
        Vertices.Empty();
        Triangles.Empty();
        Normals.Empty();
        UVs.Empty();
        VertexColors.Empty();
        Tangents.Empty();
    }
};

/**
 * Classe pour gérer un chunk (section) du terrain
 */
UCLASS()
class WORMS_3D_API UTerrainChunk : public UObject
{
    GENERATED_BODY()
public:
    UFUNCTION()//    FVoxel& Voxel = GetVoxel(x, y, z);
    FVoxel& GetVoxel(int32 x, int32 y, int32 z)
    {
        // Vérifier les limites
        if (x < 0 || x >= ChunkSize.X || y < 0 || y >= ChunkSize.Y || z < 0 || z >= ChunkSize.Z)
        {
            static FVoxel EmptyVoxel;
            return EmptyVoxel;
        }

        return VoxelData.GetVoxel(x, y, z);
    }
    friend FArchive& operator<<(FArchive& Ar, UTerrainChunk& Chunk)
    {
        // Serialize essential chunk data
        Ar << Chunk.ChunkPosition;
        Ar << Chunk.ChunkSize;
        Ar << Chunk.State;
        Ar << Chunk.ModificationCount;

        // Serialize voxel data
        TArray<FVoxel>& Voxels = Chunk.VoxelData.GetRawVoxels();
        int32 VoxelCount = Voxels.Num();
        Ar << VoxelCount;

        for (FVoxel& Voxel : Voxels)
        {
            // Serialize each voxel's properties
            uint8 StateValue = static_cast<uint8>(Voxel.State);
            uint8 MaterialValue = static_cast<uint8>(Voxel.Material);
            
            Ar << StateValue;
            Ar << MaterialValue;
            Ar << Voxel.Density;
            Ar << Voxel.Color;
        }

        return Ar;
    }

    virtual void Serialize(FArchive& Ar) override
    {
        // Call the parent class Serialize method first
        Super::Serialize(Ar);

        // Serialize essential chunk data
        Ar << ChunkPosition;
        Ar << ChunkSize;
        Ar << State;
        Ar << ModificationCount;
        Ar << bIsDirty;
        Ar << bNeedsReplication;

        // Serialize voxel data
        TArray<FVoxel>& Voxels = VoxelData.GetRawVoxels();
        int32 VoxelCount = Voxels.Num();
        Ar << VoxelCount;

        if (Ar.IsLoading())
        {
            // When loading, reinitialize the voxel grid
            VoxelData = FVoxelGrid(ChunkSize);
            Voxels = VoxelData.GetRawVoxels();
        }

        for (FVoxel& Voxel : Voxels)
        {
            // Serialize each voxel's properties
            uint8 StateValue = static_cast<uint8>(Voxel.State);
            uint8 MaterialValue = static_cast<uint8>(Voxel.Material);
        
            Ar << StateValue;
            Ar << MaterialValue;
            Ar << Voxel.Density;
            Ar << Voxel.Color;

            if (Ar.IsLoading())
            {
                Voxel.State = static_cast<EVoxelState>(StateValue);
                Voxel.Material = static_cast<EVoxelMaterial>(MaterialValue);
            }
        }
    }
    
private:
    // Position du chunk dans la grille de chunks (en coordonnées de chunk)
    UPROPERTY()
    FIntVector ChunkPosition;

    // Taille du chunk en voxels
    UPROPERTY()
    FIntVector ChunkSize;

    // Données de voxels pour ce chunk
    UPROPERTY()
    FVoxelGrid VoxelData;

    // Données du mesh
    UPROPERTY()
    FChunkMeshData MeshData;

    // État actuel du chunk
    UPROPERTY()
    EChunkState State;

    // Indique si ce chunk a été modifié depuis la dernière génération
    UPROPERTY()
    bool bIsDirty;

    // Indique si ce chunk a besoin d'être répliqué
    UPROPERTY()
    bool bNeedsReplication;

    // Nombre de modifications appliquées à ce chunk
    UPROPERTY()
    int32 ModificationCount;

    // ID du chunk pour la réplication et le débogage
    UPROPERTY()
    FString ChunkID;

    // Composant de mesh procédural associé à ce chunk
    UPROPERTY()
    UProceduralMeshComponent* MeshComponent;

    // Indique si le chunk est visible
    UPROPERTY()
    bool bIsVisible;

    // Origine du chunk dans le monde
    UPROPERTY()
    FVector WorldOrigin;

public:
    // Constructeur
    UTerrainChunk();

    // Initialiser le chunk
    void Initialize(const FIntVector& InChunkPosition, const FIntVector& InChunkSize, 
                    UProceduralMeshComponent* InMeshComponent, const FVector& InWorldOrigin);

    // Obtenir les données de voxel
    FVoxelGrid& GetVoxelData() { return VoxelData; }
    const FVoxelGrid& GetVoxelData() const { return VoxelData; }

    // Obtenir les données de mesh
    FChunkMeshData& GetMeshData() { return MeshData; }
    const FChunkMeshData& GetMeshData() const { return MeshData; }

    // Obtenir la position du chunk
    const FIntVector& GetChunkPosition() const { return ChunkPosition; }

    // Obtenir la taille du chunk
    const FIntVector& GetChunkSize() const { return ChunkSize; }

    // Obtenir l'état du chunk
    EChunkState GetState() const { return State; }

    // Définir l'état du chunk
    void SetState(EChunkState NewState) { State = NewState; }

    // Vérifier si le chunk est sale (a besoin d'être mis à jour)
    bool IsDirty() const { return bIsDirty; }

    // Marquer le chunk comme sale
    void MarkDirty(bool bReplicateChanges = true)
    {
        bIsDirty = true;
        if (bReplicateChanges)
        {
            bNeedsReplication = true;
        }
        State = EChunkState::Modified;
    }

    // Récupérer le nombre de modifications
    int32 GetModificationCount() const { return ModificationCount; }

    // Incrémenter le compteur de modifications
    void IncrementModificationCount() { ModificationCount++; }

    // Vérifier si le chunk a besoin d'être répliqué
    bool NeedsReplication() const { return bNeedsReplication; }

    // Marquer le chunk comme répliqué
    void MarkReplicated() { bNeedsReplication = false; }

    // Obtenir l'ID du chunk
    const FString& GetChunkID() const { return ChunkID; }

    // Générer l'ID du chunk
    void GenerateChunkID();

    // Obtenir le composant mesh
    UProceduralMeshComponent* GetMeshComponent() const { return MeshComponent; }

    // Définir le composant mesh
    void SetMeshComponent(UProceduralMeshComponent* InMeshComponent) { MeshComponent = InMeshComponent; }

    // Vérifier si le chunk est visible
    bool IsVisible() const { return bIsVisible; }

    // Définir la visibilité du chunk
    void SetVisibility(bool bVisible);

    // Obtenir l'origine du chunk dans le monde
    const FVector& GetWorldOrigin() const { return WorldOrigin; }

    // Définir l'origine du chunk dans le monde
    void SetWorldOrigin(const FVector& InWorldOrigin) { WorldOrigin = InWorldOrigin; }

    // Réinitialiser les données de mesh
    void ClearMeshData()
    {
        MeshData.Clear();
    }

    // Appliquer les données de mesh au composant
    void ApplyMeshData();

    // Mettre à jour les données de mesh (mais ne pas les appliquer)
    void UpdateMeshData();
    void CreateFallbackCube();

    // Convertir des coordonnées locales à globales
    FIntVector LocalToGlobal(const FIntVector& LocalCoord) const;

    // Convertir des coordonnées globales à locales
    FIntVector GlobalToLocal(const FIntVector& GlobalCoord) const;

    // Vérifier si des coordonnées globales sont contenues dans ce chunk
    bool ContainsGlobalCoord(const FIntVector& GlobalCoord) const;

    // Vérifier si un point du monde est contenu dans ce chunk
    bool ContainsWorldPoint(const FVector& WorldPoint) const;

    // Creuser une sphère dans ce chunk
    bool CarveSphere(const FVector& WorldCenter, float Radius, float Falloff = 0.5f);

    // Générer avec du terrain aléatoire
    void GenerateRandomTerrain(float HeightScale = 1000.0f, float NoiseScale = 0.01f);

    // Générer un volume plein avec surface plate
    void GenerateTerrainWithSurface(int32 SurfaceHeight);

    // Générer un volume plein
    void GenerateFullVolume();

    // Générer un volume vide
    void GenerateEmptyVolume();

    // Lisser le volume de voxels
    void SmoothVolume(int32 Iterations = 1);

    // Calculer une tranche de l'algorithme Marching Cubes
    void CalculateMarchingCubesSection(int32 StartZ, int32 EndZ);

    // Serialiser le chunk pour la réplication
    TArray<uint8> SerializeForReplication() const;

    // Désérialiser le chunk à partir des données
    bool DeserializeFromData(const TArray<uint8>& Data);

    // Sérialiser uniquement les voxels modifiés
    TArray<uint8> SerializeModifiedVoxels() const;

    // Désérialiser et appliquer les voxels modifiés
    bool ApplyModifiedVoxels(const TArray<uint8>& Data);
};