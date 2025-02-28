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
protected:
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
};