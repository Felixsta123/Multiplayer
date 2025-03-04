#include "VoxelBuilding.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

// Constructeur - initialise les propriétés par défaut
AVoxelBuilding::AVoxelBuilding()
{
    // Ce composant doit faire un tick à chaque frame
    PrimaryActorTick.bCanEverTick = false;

    // Créer un composant racine
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));

    // Valeurs par défaut
    VoxelSize = 100.0f;
    bEnableVoxelPhysics = false;

    // Trouver un cube standard pour les voxels (si disponible)
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeMeshFinder.Succeeded())
    {
        VoxelMesh = CubeMeshFinder.Object;
    }

    // Trouver un matériau standard (si disponible)
    static ConstructorHelpers::FObjectFinder<UMaterial> DefaultMaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial"));
    if (DefaultMaterialFinder.Succeeded())
    {
        VoxelMaterial = DefaultMaterialFinder.Object;
    }
}

void AVoxelBuilding::BeginPlay()
{
    Super::BeginPlay();
}

void AVoxelBuilding::CreateRectangle(int32 Width, int32 Height, int32 Depth)
{
    // D'abord, nettoyer tous les voxels existants
    ClearVoxels();

    // Vérifier si nous avons un mesh valide
    if (!VoxelMesh)
    {
        UE_LOG(LogTemp, Error, TEXT("VoxelBuilding: Pas de VoxelMesh assigné!"));
        return;
    }

    // Limiter la taille pour éviter les abus de performance
    Width = FMath::Clamp(Width, 1, 100);
    Height = FMath::Clamp(Height, 1, 100);
    Depth = FMath::Clamp(Depth, 1, 100);

    UE_LOG(LogTemp, Log, TEXT("Création d'un rectangle de voxels %d x %d x %d"), Width, Height, Depth);

    // Obtenir le centre de l'acteur comme point de départ
    FVector BuildingCenter = GetActorLocation();
    
    // Calculer le décalage pour centrer le bâtiment autour de l'acteur
    float HalfWidth = (Width - 1) * VoxelSize * 0.5f;
    float HalfDepth = (Depth - 1) * VoxelSize * 0.5f;
    
    // Point de départ (coin inférieur du bâtiment)
    FVector StartLocation = BuildingCenter - FVector(HalfWidth, HalfDepth, 0);

    // Pour chaque position dans la grille 3D, créer un voxel
    for (int32 X = 0; X < Width; X++)
    {
        for (int32 Y = 0; Y < Depth; Y++)
        {
            for (int32 Z = 0; Z < Height; Z++)
            {
                // Calculer la position de ce voxel
                FVector VoxelLocation = StartLocation + FVector(X * VoxelSize, Y * VoxelSize, Z * VoxelSize);
                
                // Créer le voxel
                UStaticMeshComponent* NewVoxel = CreateVoxel(VoxelLocation);
                
                // Ajouter au tableau pour référence future
                if (NewVoxel)
                {
                    VoxelComponents.Add(NewVoxel);
                }
            }
        }
    }

    UE_LOG(LogTemp, Log, TEXT("Rectangle de voxels créé avec %d composants"), VoxelComponents.Num());
}

void AVoxelBuilding::ClearVoxels()
{
    // Supprimer tous les composants de voxel existants
    for (UStaticMeshComponent* Component : VoxelComponents)
    {
        if (Component)
        {
            Component->DestroyComponent();
        }
    }
    
    // Vider le tableau
    VoxelComponents.Empty();
}

UStaticMeshComponent* AVoxelBuilding::CreateVoxel(const FVector& Location)
{
    // Générer un nom unique pour ce composant
    FName ComponentName = *FString::Printf(TEXT("Voxel_%d"), VoxelComponents.Num());
    
    // Créer un nouveau composant StaticMesh
    UStaticMeshComponent* NewVoxelComponent = NewObject<UStaticMeshComponent>(this, ComponentName);
    
    if (NewVoxelComponent)
    {
        // Configurer le composant
        NewVoxelComponent->SetStaticMesh(VoxelMesh);
        
        // Appliquer le matériau si disponible
        if (VoxelMaterial)
        {
            NewVoxelComponent->SetMaterial(0, VoxelMaterial);
        }
        
        // Échelle pour match VoxelSize (le cube par défaut fait 100 unités)
        float Scale = VoxelSize / 100.0f;
        NewVoxelComponent->SetRelativeScale3D(FVector(Scale));
        
        // Position
        NewVoxelComponent->SetWorldLocation(Location);
        
        // Attacher au root
        NewVoxelComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
        
        // Activer la physique si demandé
        if (bEnableVoxelPhysics)
        {
            NewVoxelComponent->SetSimulatePhysics(true);
            NewVoxelComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            // Chaque voxel est indépendant en physique
            NewVoxelComponent->SetMobility(EComponentMobility::Movable);
        }
        else
        {
            // Sans physique, on peut utiliser un composant statique
            NewVoxelComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
            NewVoxelComponent->SetMobility(EComponentMobility::Static);
        }
        
        // Enregistrer et activer le composant
        NewVoxelComponent->RegisterComponent();
        
        return NewVoxelComponent;
    }
    
    return nullptr;
}