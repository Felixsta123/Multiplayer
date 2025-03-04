#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "ADestructibleTerrain.generated.h"





USTRUCT(BlueprintType)
struct FTerrainModification
{
    GENERATED_BODY()
    
    UPROPERTY(BlueprintReadWrite)
    FVector2D Position;
    
    UPROPERTY(BlueprintReadWrite)
    FVector2D Size;
    
    // Ajout pour les destructions circulaires
    UPROPERTY(BlueprintReadWrite)
    bool bIsCircular;
    
    UPROPERTY(BlueprintReadWrite)
    FVector2D CircleCenter;
    
    UPROPERTY(BlueprintReadWrite)
    float CircleRadius;
    
    FTerrainModification()
    {
        Position = FVector2D::ZeroVector;
        Size = FVector2D(100.0f, 100.0f);
        bIsCircular = false;
        CircleCenter = FVector2D::ZeroVector;
        CircleRadius = 50.0f;
    }
    
    FTerrainModification(FVector2D InPosition, FVector2D InSize)
    {
        Position = InPosition;
        Size = InSize;
        bIsCircular = false;
        CircleCenter = FVector2D::ZeroVector;
        CircleRadius = 50.0f;
    }
    
    // Ajouter un constructeur pour les modifications circulaires
    static FTerrainModification MakeCircular(FVector2D Center, float Radius)
    {
        FTerrainModification Mod;
        Mod.Position = Center - FVector2D(Radius, Radius);
        Mod.Size = FVector2D(Radius * 2.0f, Radius * 2.0f);
        Mod.bIsCircular = true;
        Mod.CircleCenter = Center;
        Mod.CircleRadius = Radius;
        return Mod;
    }
    
    // Surcharger l'opérateur == pour pouvoir utiliser Contains()
    bool operator==(const FTerrainModification& Other) const
    {
        return Position == Other.Position && Size == Other.Size &&
               bIsCircular == Other.bIsCircular && CircleCenter == Other.CircleCenter &&
               CircleRadius == Other.CircleRadius;
    }
};


USTRUCT(BlueprintType)
struct FTerrainModificationArray
{
    GENERATED_BODY()
    
    UPROPERTY()
    TArray<FTerrainModification> Modifications;
};

// Structure pour stocker les données du mesh nécessaires à la réplication
USTRUCT(BlueprintType)
struct FTerrainMeshData
{
    GENERATED_BODY()
    
    UPROPERTY()
    TArray<FVector> Vertices;
    
    UPROPERTY()
    TArray<int32> Triangles;
    
    UPROPERTY()
    TArray<FVector2D> UVs;
    
    UPROPERTY()
    TArray<FVector> Normals;
    
    UPROPERTY()
    TArray<FColor> VertexColors;
    
    UPROPERTY()
    bool bIsValid = false;
};


USTRUCT(BlueprintType)
struct FTerrainCell
{
    GENERATED_BODY()
    
    UPROPERTY(BlueprintReadWrite)
    FVector Min;
    
    UPROPERTY(BlueprintReadWrite)
    FVector Max;
    
    UPROPERTY(BlueprintReadWrite)
    bool bIsDestroyed;
    
    // Coordonnées de la cellule dans la grille
    UPROPERTY(BlueprintReadWrite)
    int32 GridX;
    
    UPROPERTY(BlueprintReadWrite)
    int32 GridY;
    
    UPROPERTY(BlueprintReadWrite)
    int32 GridZ;
    
    // Constructeur par défaut
    FTerrainCell()
    {
        Min = FVector::ZeroVector;
        Max = FVector::ZeroVector;
        bIsDestroyed = false;
        GridX = 0;
        GridY = 0;
        GridZ = 0;
    }
    
    // Constructeur pour initialiser une cellule
    FTerrainCell(const FVector& InMin, const FVector& InMax, int32 InGridX, int32 InGridY, int32 InGridZ)
    {
        Min = InMin;
        Max = InMax;
        bIsDestroyed = false;
        GridX = InGridX;
        GridY = InGridY;
        GridZ = InGridZ;
    }
    
    // Vérifier si un point est dans cette cellule
    bool ContainsPoint(const FVector& Point) const
    {
        return Point.X >= Min.X && Point.X <= Max.X &&
               Point.Y >= Min.Y && Point.Y <= Max.Y &&
               Point.Z >= Min.Z && Point.Z <= Max.Z;
    }
    
    // Vérifier si cette cellule est touchée par une modification
    bool IsAffectedByModification(const FTerrainModification& Modification) const
    {
        if (bIsDestroyed)
        {
            // La cellule est déjà détruite, pas besoin de la modifier davantage
            return false;
        }
    
        // Pour les modifications circulaires
        if (Modification.bIsCircular)
        {
            // 1. Convertir le centre du cercle en 3D (en utilisant Y=Min.Y pour le plan de référence)
            FVector CircleCenter3D(Modification.CircleCenter.X, Min.Y, Modification.CircleCenter.Y);
            float CircleRadius = Modification.CircleRadius;
        
            // 2. Test de collision entre boîte et sphère - méthode améliorée
        
            // a. Calculer le point le plus proche de la boîte au centre du cercle
            FVector ClosestPoint(
                FMath::Clamp(CircleCenter3D.X, Min.X, Max.X),
                FMath::Clamp(CircleCenter3D.Y, Min.Y, Max.Y),
                FMath::Clamp(CircleCenter3D.Z, Min.Z, Max.Z)
            );
        
            // b. Calculer la distance carrée entre ce point et le centre du cercle
            float DistanceSquared = FVector::DistSquared(ClosestPoint, CircleCenter3D);
        
            // c. Si cette distance est inférieure au carré du rayon, alors il y a collision
            return DistanceSquared <= (CircleRadius * CircleRadius);
        }
        else // Pour les modifications rectangulaires
        {
            // Convertir la position et la taille en 3D (en considérant la profondeur complète)
            FVector ModMin(Modification.Position.X, Min.Y, Modification.Position.Y);
            FVector ModMax(
                Modification.Position.X + Modification.Size.X,
                Max.Y, // Utiliser toute la profondeur Y
                Modification.Position.Y + Modification.Size.Y
            );
        
            // Test de collision entre deux boîtes alignées sur les axes
            return !(Max.X < ModMin.X || Min.X > ModMax.X ||
                    Max.Y < ModMin.Y || Min.Y > ModMax.Y ||
                    Max.Z < ModMin.Z || Min.Z > ModMax.Z);
        }
    }
};


UCLASS()
class WORMS_3D_API ADestructibleTerrain : public AActor
{
    GENERATED_BODY()
    
public:    
    ADestructibleTerrain();
    
    // Initialise le terrain avec une taille et hauteur spécifiques
    UFUNCTION(BlueprintCallable, Category = "Terrain")
    void InitializeTerrain(float Width, float Height, float Depth);
    
    // Fonction appelée par le client pour demander une destruction
    UFUNCTION(BlueprintCallable, Category = "Terrain")
    void RequestDestroyTerrainAt(FVector2D Position, FVector2D Size);
    
    // Fonction serveur pour valider et appliquer la destruction
    UFUNCTION(Server, Reliable, WithValidation)
    void Server_DestroyTerrainAt(FVector2D Position, FVector2D Size);

    void CalculateInternalNormals();
    FLinearColor GetInternalLayerColor(int32 LayerIndex);
    // Génère le mesh procédural du terrain
    UFUNCTION(BlueprintCallable, Category = "Terrain")
    void GenerateTerrain();

        // Applique les modifications de terrain (appelé après réplication)
    UFUNCTION(BlueprintCallable, Category = "Terrain")
    void ApplyTerrainModifications();
    
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    
    // RPC Multicast pour informer tous les clients de l'initialisation
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_NotifyInitialized(float Width, float Height, float Depth);

    // RPC Multicast pour mettre à jour la géométrie du mesh sur tous les clients
    UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = "Terrain")
    void Multicast_UpdateTerrainMesh(const FTerrainMeshData& InMeshData);

    UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = "Terrain")
    void Multicast_ForceVisualUpdate();

    UPROPERTY(EditDefaultsOnly, Category = "Terrain")
    UMaterialInterface* TerrainMaterial;

    //Field for Explosion
   //DestructionEffect
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain")
    UParticleSystem* DestructionEffect;
    //DestructionSound
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain")
    USoundBase* DestructionSound;

    //class for the Debris and a parameter for the number of debris
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain")
    TSubclassOf<AActor> DebrisClass;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain")
    int32 NumDebrisPerCell = 10;
    
    // Fonction helper pour créer le mesh à partir des données
    void CreateMeshFromData(const FTerrainMeshData& MeshData);
    
    // Fonction helper pour passer des FColor aux FLinearColor
    TArray<FLinearColor> ConvertColorsToLinear(const TArray<FColor>& Colors);
    
    // Hauteur du terrain
    UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Terrain")
    float TerrainWidth;
    
    // Largeur du terrain
    UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Terrain")
    float TerrainHeight;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Internal", meta = (EditCondition = "bGenerateInternalStructure"))
    float CellSizeX = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Internal", meta = (EditCondition = "bGenerateInternalStructure"))
    float CellSizeY = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Internal", meta = (EditCondition = "bGenerateInternalStructure"))
    float CellSizeZ = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Internal", meta = (EditCondition = "bGenerateInternalStructure"))
    float WallThickness = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Internal", meta = (EditCondition = "bGenerateInternalStructure"))
    bool bAddNoiseToStructure = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Internal", meta = (EditCondition = "bAddNoiseToStructure"))
    float StructureNoiseAmount = 0.15f;



    // Profondeur du terrain
    UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Terrain")
    float TerrainDepth;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain")
    int32 HorizontalResolution;

    // Résolution verticale du terrain (nombre de subdivisions en hauteur)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain")
    int32 VerticalResolution;

    // Pour garder une trace des modifications déjà appliquées
    UPROPERTY()
    TArray<FTerrainModification> AppliedModifications;
    
    // Configuration de la structure interne
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Internal")
    bool bGenerateInternalStructure;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Internal", meta = (EditCondition = "bGenerateInternalStructure"))
    int32 InternalLayerCount;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Internal", meta = (EditCondition = "bGenerateInternalStructure"))
    float InternalLayerThickness;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Internal", meta = (EditCondition = "bGenerateInternalStructure"))
    TArray<FLinearColor> InternalLayerColors;

    // Matériaux avancés pour différentes parties du terrain
    UPROPERTY(EditDefaultsOnly, Category = "Terrain|Materials")
    UMaterialInterface* SurfaceMaterial;

    UPROPERTY(EditDefaultsOnly, Category = "Terrain|Materials")
    UMaterialInterface* InternalMaterial;

    UPROPERTY(EditDefaultsOnly, Category = "Terrain|Materials")
    UMaterialInterface* EdgeMaterial;

    // Instance de matériau dynamique pour recevoir des paramètres
    UPROPERTY()
    UMaterialInstanceDynamic* TerrainMaterialInstance;

    // Méthode pour configurer les matériaux
    void SetupMaterials();

    // Méthode pour mettre à jour les paramètres du matériau
    void UpdateMaterialParameters();
    
    // Méthode pour générer la structure interne
    void GenerateInternalStructure();
    void GenerateInternalWall(const FVector& BottomLeft, const FVector& BottomRight, const FVector& TopLeft,
                              const FVector& TopRight, float Thickness, const FLinearColor& Color,
                              FRandomStream& Random);

    // Fonction Tick pour les mises à jour périodiques
    virtual void Tick(float DeltaTime) override;
    void CreateCircularCutFaces(
    const FTerrainModification& Mod,
    TArray<FVector>& Vertices,
    TArray<int32>& Triangles,
    TArray<FVector2D>& UVs,
    TArray<FColor>& VertexColors);

    void CreateCleanCutFaces(
    const TArray<FTerrainModification>& Modifications,
    TArray<FVector>& Vertices,
    TArray<int32>& Triangles,
    TArray<FVector2D>& UVs,
    TArray<FColor>& VertexColors);

    void CreateRectangularCutFaces(const FTerrainModification& Mod,
    TArray<FVector>& Vertices,
    TArray<int32>& Triangles,
    TArray<FVector2D>& UVs,
    TArray<FColor>& VertexColors);
    void CreateCutFace(
        const FVector& Corner1,
        const FVector& Corner2,
        const FVector& Corner3,
        const FVector& Corner4,
        TArray<FVector>& Vertices,
        TArray<int32>& Triangles,
        TArray<FVector2D>& UVs,
        TArray<FColor>& VertexColors,
        const FColor& Color);


    UFUNCTION(BlueprintCallable, Category = "Terrain")
    void RequestDestroyTerrainCircle(FVector2D Center, float Radius);

    void Server_DestroyTerrainCircle(FVector2D Center, float Radius);
    bool Server_DestroyTerrainCircle_Validate(FVector2D Center, float Radius);
    // Fonction pour ajuster les paramètres de la structure interne à l'exécution
    UFUNCTION(BlueprintCallable, Category = "Terrain|Internal")
    void SetInternalStructureParameters(float NewCellSizeX, float NewCellSizeY, float NewCellSizeZ, float NewWallThickness, bool bRegenerate);

protected:
    UPROPERTY()
    TArray<FTerrainCell> TerrainCells;

    UPROPERTY()
    int32 CellCountX;

    UPROPERTY()
    int32 CellCountY;

    UPROPERTY()
    int32 CellCountZ;

    // Vérifier si une cellule est détruite
    bool IsCellDestroyed(int32 GridX, int32 GridY, int32 GridZ);

    // Obtenir les cellules affectées par une modification
    TArray<FTerrainCell*> GetAffectedCells(const FTerrainModification& Modification);

    void SpawnDestructionEffects(const TArray<FTerrainCell*>& DestroyedCells);

    // Nettoyer les vertices orphelins
    void CleanupOrphanVertices(
        TArray<FVector>& Vertices,
        TArray<int32>& Triangles,
        TArray<FVector2D>& UVs,
        TArray<FColor>& VertexColors,
        TArray<FVector>& Normals);

    // Créer des faces aux bords des cellules détruites
    void CreateCleanCellCutFaces(
        const TArray<FTerrainCell*>& AffectedCells,
        TArray<FVector>& Vertices,
        TArray<int32>& Triangles,
        TArray<FVector2D>& UVs,
        TArray<FColor>& VertexColors);

    // Générer les parois d'une cellule spécifique
    void GenerateCellWalls(const FTerrainCell& Cell, FRandomStream& Random);
    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void PostInitializeComponents() override;
    
    // Le mesh procédural représentant le terrain
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UProceduralMeshComponent* TerrainMesh;
    
    // Données de mesh répliquées (nouvelle structure)
    UPROPERTY(ReplicatedUsing = OnRep_MeshData)
    FTerrainMeshData MeshData;
    
    // Callback pour quand les données de mesh sont répliquées
    UFUNCTION()
    void OnRep_MeshData();
    
    // Liste des modifications apportées au terrain
    UPROPERTY(ReplicatedUsing = OnRep_TerrainModifications)
    TArray<FTerrainModification> TerrainModifications;
    
    // Fonction appelée quand TerrainModifications est répliqué
    UFUNCTION()
    void OnRep_TerrainModifications();
    
    // Vérifie si un vertex est dans une zone rectangulaire
    bool IsVertexInModification(const FVector& Vertex, const FTerrainModification& Modification);
    
    // Variable pour savoir si le terrain a été initialisé
    UPROPERTY(Replicated)
    bool bIsInitialized;
    
    // Initialisation des tangentes (méthode séparée pour être appelée une seule fois)
    void InitializeTangents();
    
    // Tableau des tangentes du terrain (pas besoin de répliquer)
    UPROPERTY()
    TArray<FProcMeshTangent> Tangents;
    
    // Flag pour indiquer que les modifications de terrain ont été appliquées
    UPROPERTY(Replicated)
    bool bModificationsApplied;

    // Données supplémentaires pour la structure interne
    UPROPERTY()
    TArray<FVector> InternalVertices;

    UPROPERTY()
    TArray<int32> InternalTriangles;

    UPROPERTY()
    TArray<FVector2D> InternalUVs;

    UPROPERTY()
    TArray<FVector> InternalNormals;

    UPROPERTY()
    TArray<FColor> InternalVertexColors;
    
    // Subdivision du terrain en sections pour optimisation
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain|Optimization")
    bool bUseTerrainSections;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain|Optimization", meta = (EditCondition = "bUseTerrainSections"))
    float SectionSizeX;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain|Optimization", meta = (EditCondition = "bUseTerrainSections"))
    float SectionSizeY;

    // Array pour stocker les modifications par section
    UPROPERTY()
    TMap<FIntPoint, FTerrainModificationArray> SectionModifications;
    
    // Méthodes pour la gestion des sections
    void InitializeSections();
    void AssignModificationToSections(const FTerrainModification& Modification);
    TArray<FIntPoint> GetAffectedSections(const FTerrainModification& Modification);
    void RegenerateSections(const TArray<FIntPoint>& SectionCoords);
    bool IsVertexInSection(const FVector& Vertex, const FIntPoint& SectionCoord);
    
    // Configuration du LOD
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain|LOD")
    bool bUseLOD;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain|LOD", meta = (EditCondition = "bUseLOD"))
    float LODDistanceThreshold;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain|LOD", meta = (EditCondition = "bUseLOD"))
    int32 LODHorizontalResolution;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain|LOD", meta = (EditCondition = "bUseLOD"))
    int32 LODVerticalResolution;

    // Méthodes pour la gestion du LOD
    UFUNCTION()
    void UpdateLOD();

    UFUNCTION()
    float GetDistanceToNearestPlayer();

    UPROPERTY()
    bool bIsUsingLOD;

    // Méthode pour basculer entre les résolutions
    void SwitchResolution(bool bUseLowResolution);
};