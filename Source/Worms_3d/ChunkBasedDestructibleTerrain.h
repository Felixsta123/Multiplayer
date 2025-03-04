#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TerrainChunk.h"
#include "VoxelData.h"
#include "Net/UnrealNetwork.h"
#include "ChunkBasedDestructibleTerrain.generated.h"

/**
 * Type de terrain prédéfini
 */
UENUM(BlueprintType)
enum class ETerrainType : uint8
{
    Flat,          // Terrain plat avec hauteur constante
    Hills,         // Terrain avec collines
    Mountains,     // Terrain montagneux
    Islands,       // Îles flottantes
    Custom         // Terrain personnalisé
};

/**
 * Informations sur une modification du terrain
 */
USTRUCT(BlueprintType)
struct WORMS_3D_API FTerrainModificationInfo
{
    GENERATED_BODY()

    // Type de modification
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain")
    FString Type = TEXT("Explosion");

    // Position dans le monde
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain")
    FVector Position = FVector::ZeroVector;

    // Rayon d'effet
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain")
    float Radius = 200.0f;

    // Falloff (transition douce)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain")
    float Falloff = 0.3f;

    // Horodatage
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain")
    float Timestamp = 0.0f;

    // ID unique
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain")
    FString ID;

    // ID du chunk
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain")
    TArray<FString> AffectedChunkIDs;

    // Constructeur par défaut
    FTerrainModificationInfo() = default;

    // Constructeur avec paramètres
    FTerrainModificationInfo(const FString& InType, const FVector& InPosition, float InRadius, float InFalloff = 0.3f)
        : Type(InType), Position(InPosition), Radius(InRadius), Falloff(InFalloff), Timestamp(0.0f)
    {
        // Générer un ID unique
        ID = FGuid::NewGuid().ToString();
    }

    // Génération d'un vecteur de sérialisation pour la réplication
    TArray<uint8> Serialize() const
    {
        TArray<uint8> Data;
        FMemoryWriter Writer(Data);

        // Créer une copie non-const pour la sérialisation
        FTerrainModificationInfo Copy = *this;
        
        // Utiliser l'opérateur personnalisé pour sérialiser
        Writer << Copy;

        return Data;
    }

    // Désérialisation
    static FTerrainModificationInfo Deserialize(const TArray<uint8>& Data)
    {
        FTerrainModificationInfo Info;
        FMemoryReader Reader(Data);

        // Utiliser l'opérateur personnalisé pour désérialiser
        Reader << Info;

        return Info;
    }

    // Définir l'opérateur de sérialisation personnalisé
    friend FArchive& operator<<(FArchive& Ar, FTerrainModificationInfo& Info)
    {
        Ar << Info.Type;
        Ar << Info.Position;
        Ar << Info.Radius;
        Ar << Info.Falloff;
        Ar << Info.Timestamp;
        Ar << Info.ID;
        Ar << Info.AffectedChunkIDs;
        return Ar;
    }
};


/**
 * Structure pour la réplication des modifications de chunk
 */
/**
 * Structure pour la réplication des modifications de chunk
 */
USTRUCT(BlueprintType)
struct WORMS_3D_API FChunkModificationPacket
{
    GENERATED_BODY()

    // ID du chunk
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain")
    FString ChunkID;

    // Données de modification
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain")
    TArray<uint8> ModificationData;

    // Version de modification
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain")
    int32 ModificationVersion = 0;

    // Constructeur par défaut
    FChunkModificationPacket() = default;

    // Constructeur avec paramètres
    FChunkModificationPacket(const FString& InChunkID, const TArray<uint8>& InData, int32 InVersion)
        : ChunkID(InChunkID), ModificationData(InData), ModificationVersion(InVersion)
    {
    }
    
    // Définir l'opérateur de sérialisation
    friend FArchive& operator<<(FArchive& Ar, FChunkModificationPacket& Packet)
    {
        Ar << Packet.ChunkID;
        Ar << Packet.ModificationData;
        Ar << Packet.ModificationVersion;
        return Ar;
    }
};
/**
 * Classe principale pour le système de terrain destructible basé sur des chunks
 */
UCLASS()
class WORMS_3D_API AChunkBasedDestructibleTerrain : public AActor
{
    GENERATED_BODY()

public:
    // Constructeur
    AChunkBasedDestructibleTerrain();

    // Configuration du terrain
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain")
    FVector TerrainSize = FVector(5000.0f, 5000.0f, 2000.0f);

    // Taille de voxel
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain")
    float VoxelSize = 50.0f;

    // Taille des chunks
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain")
    FIntVector ChunkSize = FIntVector(16, 16, 16);

    // Doit générer le terrain au début du jeu
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain")
    bool bGenerateOnBeginPlay = true;

    // Distance de visibilité des chunks (optimisation)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Optimization")
    float ChunkVisibilityDistance = 3000.0f;

    // Type de terrain à générer
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Generation")
    ETerrainType TerrainType = ETerrainType::Flat;

    // Échelle de hauteur pour la génération
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Generation")
    float HeightScale = 1000.0f;

    // Échelle de bruit pour la génération
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Generation")
    float NoiseScale = 0.01f;

    // Décalage aléatoire pour la génération (permet de varier les terrains)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Generation")
    int32 RandomSeed = 0;

    // Hauteur de la surface pour les terrains plats
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Generation", meta = (EditCondition = "TerrainType == ETerrainType::Flat"))
    float FlatTerrainHeight = 0.5f;

    // Matériau à appliquer aux chunks
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Appearance")
    UMaterialInterface* TerrainMaterial;

    // Niveau de détail pour les chunks éloignés
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Optimization")
    bool bUseLOD = true;

    // Nombre de lissages à appliquer après modification
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Quality")
    int32 SmoothingIterations = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Debug")
    TObjectPtr<UStaticMesh> DebugMesh;
    UFUNCTION(BlueprintCallable, Category = "Terrain|Testing")
    void TestExplosion();
    // === FONCTIONS PUBLIQUES ===

    // Génération du terrain
    UFUNCTION(BlueprintCallable, Category = "Terrain")
    void GenerateTerrain();

    // Réinitialisation du terrain
    UFUNCTION(BlueprintCallable, Category = "Terrain")
    void ResetTerrain();

    // Applique une modification au terrain (explosion, etc.)
    UFUNCTION(BlueprintCallable, Category = "Terrain")
    void ApplyTerrainModification(const FVector& Position, float Radius, float Falloff = 0.3f);

    // Fonction serveur pour les modifications de terrain
    UFUNCTION(Server, Reliable, WithValidation)
    void Server_ApplyTerrainModification(const FVector& Position, float Radius, float Falloff = 0.3f);

    // Fonction pour obtenir la densité à une position du monde
    UFUNCTION(BlueprintCallable, Category = "Terrain")
    float GetDensityAtWorldPosition(const FVector& WorldPosition) const;

    // Fonction pour obtenir le matériau à une position du monde
    UFUNCTION(BlueprintCallable, Category = "Terrain")
    EVoxelMaterial GetMaterialAtWorldPosition(const FVector& WorldPosition) const;

    // Fonction pour vérifier si une position du monde est solide
    UFUNCTION(BlueprintCallable, Category = "Terrain")
    bool IsPositionSolid(const FVector& WorldPosition) const;

    // Fonction pour tracer un rayon et obtenir le premier point d'impact avec le terrain
    UFUNCTION(BlueprintCallable, Category = "Terrain")
    bool TraceTerrain(const FVector& Start, const FVector& End, FVector& HitPoint, FVector& HitNormal) const;

    // Fonction pour obtenir la liste des chunks modifiés
    UFUNCTION(BlueprintCallable, Category = "Terrain")
    TArray<FString> GetModifiedChunkIDs() const;

    // Fonction pour synchroniser manuellement le terrain
    UFUNCTION(BlueprintCallable, Category = "Terrain")
    void SynchronizeTerrain();

protected:
    // Fonctions Unreal standards
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void OnConstruction(const FTransform& Transform) override;

    // Fonction multicast pour synchroniser les chunks modifiés
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_SynchronizeChunks(const TArray<FChunkModificationPacket>& ChunkModifications);

    // Fonction multicast pour notifier d'une modification
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_NotifyTerrainModification(const TArray<uint8>& ModificationData);

    // Gestion des modifications
    TArray<FTerrainModificationInfo> PendingModifications;
    TArray<FTerrainModificationInfo> AppliedModifications;

    // Composant de scène racine
    UPROPERTY(VisibleDefaultsOnly, Category = "Components")
    USceneComponent* RootSceneComponent;

    // Dictionnaire des chunks
    UPROPERTY()
    TMap<FString, UTerrainChunk*> Chunks;

    // Liste des IDs de chunk par position
    UPROPERTY()
    TMap<FIntVector, FString> ChunkIDsByPosition;

    // Tableaux pour les modifications répliquées
    UPROPERTY(ReplicatedUsing = OnRep_ModifiedChunks)
    TArray<FChunkModificationPacket> ReplicatedChunkModifications;

    UFUNCTION()
    void OnRep_ModifiedChunks();

    // Système de génération asynchrone (utile pour les grands terrains)
    bool bIsGenerating;
    FIntVector NextChunkToGenerate;
    int32 ChunksPerFrame;

    // Fonctions internes
    void InitializeChunks();
    void CreateChunkMeshComponents();
    void GenerateChunkGeometry(UTerrainChunk* Chunk);
    void GenerateAllChunks();
    void GenerateChunksAsync();
    UTerrainChunk* GetChunkAt(const FIntVector& ChunkPosition);
    UTerrainChunk* GetChunkAtWorldPosition(const FVector& WorldPosition);
    FIntVector WorldPositionToChunkPosition(const FVector& WorldPosition) const;
    FVector ChunkPositionToWorldOrigin(const FIntVector& ChunkPosition) const;
    TArray<UTerrainChunk*> GetChunksInSphere(const FVector& Center, float Radius);
    void UpdateChunkVisibility();
    void UpdateChunksAroundPlayer();
    void ProcessPendingModifications();
    void CalculateAffectedChunks(FTerrainModificationInfo& Modification);

    // Fonctions de réplication
    void PrepareChunkForReplication(UTerrainChunk* Chunk);
    void ReplicateModifiedChunks();
    void ApplyReplicatedModifications(const TArray<FChunkModificationPacket>& Modifications);
};