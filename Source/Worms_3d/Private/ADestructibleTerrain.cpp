#include "ADestructibleTerrain.h"
#include "MaterialDomain.h"
#include "Engine/World.h"
#include "TimerManager.h"

ADestructibleTerrain::ADestructibleTerrain()
{
    PrimaryActorTick.bCanEverTick = false;
    
    // Création du mesh procédural
    TerrainMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("TerrainMesh"));
    RootComponent = TerrainMesh;
    
    // Configuration de la réplication
    bReplicates = true;
    bAlwaysRelevant = true;
    SetReplicatingMovement(true);
    
    // Pour le mesh procédural - NE PAS répliquer le composant lui-même
    // C'est important : nous allons gérer manuellement la réplication des données de mesh
    TerrainMesh->SetIsReplicated(false);
    
    // Configuration des collisions
    TerrainMesh->SetCollisionProfileName(TEXT("BlockAll"));
    TerrainMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    
    // Valeurs par défaut
    TerrainWidth = 2000.0f;
    TerrainHeight = 2000.0f;
    TerrainDepth = 1000.0f;
    HorizontalResolution = 25; // 50 subdivisions en largeur
    VerticalResolution = 25; // 50 subdivisions en hauteur
    bIsInitialized = false;
    bModificationsApplied = false;
    
    // Augmenter la fréquence de mise à jour réseau
    NetUpdateFrequency = 10.0f;
    MinNetUpdateFrequency = 5.0f;
    
    // Rendre le terrain visible par défaut
    TerrainMesh->SetVisibility(true);
    TerrainMesh->SetCastShadow(true);
    TerrainMesh->bCastDynamicShadow = true;
}

void ADestructibleTerrain::BeginPlay()
{
    Super::BeginPlay();
    
    // Générer le terrain initial après un court délai
    if (HasAuthority())
    {
        // Retarder légèrement la génération du terrain pour s'assurer que tous les systèmes sont prêts
        FTimerHandle TimerHandle;
        GetWorld()->GetTimerManager().SetTimer(
            TimerHandle, 
            [this]() { 
                if (!bIsInitialized) {
                    UE_LOG(LogTemp, Warning, TEXT("Initializing terrain in BeginPlay with delayed timer"));
                    InitializeTerrain(TerrainWidth, TerrainHeight, TerrainDepth);
                }
            }, 
            0.5f, 
            false
        );
    }
}

void ADestructibleTerrain::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    
    // Dans l'éditeur, générer le terrain pour pouvoir le visualiser
    if (GIsEditor && !HasAuthority())
    {
        GenerateTerrain();
    }
}

void ADestructibleTerrain::PostInitializeComponents()
{
    Super::PostInitializeComponents();
    
    // S'assurer que le matériau est correctement appliqué
    if (TerrainMaterial && TerrainMesh)
    {
        TerrainMesh->SetMaterial(0, TerrainMaterial);
    }
}

void ADestructibleTerrain::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    
    // Répliquer les propriétés importantes
    DOREPLIFETIME(ADestructibleTerrain, TerrainModifications);
    DOREPLIFETIME(ADestructibleTerrain, TerrainWidth);
    DOREPLIFETIME(ADestructibleTerrain, TerrainHeight);
    DOREPLIFETIME(ADestructibleTerrain, TerrainDepth);
    DOREPLIFETIME(ADestructibleTerrain, bIsInitialized);
    DOREPLIFETIME(ADestructibleTerrain, bModificationsApplied);
    DOREPLIFETIME(ADestructibleTerrain, MeshData);
}

void ADestructibleTerrain::InitializeTerrain(float Width, float Height, float Depth)
{
    if (!HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("InitializeTerrain called on client, ignoring"));
        return;
    }
    
    UE_LOG(LogTemp, Warning, TEXT("Initializing terrain on server: %fx%fx%f"), Width, Height, Depth);
    
    TerrainWidth = Width;
    TerrainHeight = Height;
    TerrainDepth = Depth;
    
    // Marquer comme initialisé
    bIsInitialized = true;
    
    // Générer le terrain
    GenerateTerrain();
    
    // Informer tous les clients
    Multicast_NotifyInitialized(Width, Height, Depth);
}

void ADestructibleTerrain::Multicast_NotifyInitialized_Implementation(float Width, float Height, float Depth)
{
    // Ne pas exécuter sur le serveur, il a déjà fait cette opération
    if (HasAuthority())
    {
        return;
    }
    
    UE_LOG(LogTemp, Warning, TEXT("Client received terrain initialization: %fx%fx%f"), Width, Height, Depth);
    
    TerrainWidth = Width;
    TerrainHeight = Height;
    TerrainDepth = Depth;
    
    // Sur les clients, on ne génère pas le terrain ici, car les données viendront via la réplication de MeshData
    
    // Mais on peut préparer le matériau
    if (TerrainMaterial && TerrainMesh)
    {
        TerrainMesh->SetMaterial(0, TerrainMaterial);
    }
    
    // Marquer comme initialisé
    bIsInitialized = true;
}

void ADestructibleTerrain::GenerateTerrain()
{
    // Vider les tableaux
    MeshData.Vertices.Empty();
    MeshData.Triangles.Empty();
    MeshData.UVs.Empty();
    MeshData.Normals.Empty();
    MeshData.VertexColors.Empty();
    
    // Initialiser les tangentes une seule fois
    if (Tangents.Num() == 0)
    {
        InitializeTangents();
    }
    
    // Vérifier que les résolutions sont valides
    HorizontalResolution = FMath::Max(HorizontalResolution, 2);
    VerticalResolution = FMath::Max(VerticalResolution, 2);
    
    // Calculer le pas entre chaque point
    float HStep = TerrainWidth / (HorizontalResolution - 1);
    float VStep = TerrainHeight / (VerticalResolution - 1);
    
    // 1. Génération de la face avant (vue principale du terrain)
    for (int32 y = 0; y < VerticalResolution; ++y)
    {
        for (int32 x = 0; x < HorizontalResolution; ++x)
        {
            // Calculer la position de ce vertex
            float PosX = x * HStep;
            float PosZ = y * VStep;
            
            // Ajouter le vertex
            MeshData.Vertices.Add(FVector(PosX, 0.0f, PosZ));
            
            // Ajouter les UV correspondants (normalisés de 0 à 1)
            MeshData.UVs.Add(FVector2D(
                static_cast<float>(x) / (HorizontalResolution - 1), 
                static_cast<float>(y) / (VerticalResolution - 1)
            ));
            
            // Couleur verte pour le terrain
            MeshData.VertexColors.Add(FColor(75, 150, 75, 255));
        }
    }
    
    // 2. Génération de la face arrière (derrière le terrain)
    for (int32 y = 0; y < VerticalResolution; ++y)
    {
        for (int32 x = 0; x < HorizontalResolution; ++x)
        {
            // Calculer la position de ce vertex
            float PosX = x * HStep;
            float PosZ = y * VStep;
            
            // Ajouter le vertex
            MeshData.Vertices.Add(FVector(PosX, TerrainDepth, PosZ));
            
            // Ajouter les UV correspondants (normalisés de 0 à 1)
            MeshData.UVs.Add(FVector2D(
                static_cast<float>(x) / (HorizontalResolution - 1), 
                static_cast<float>(y) / (VerticalResolution - 1)
            ));
            
            // Couleur verte pour le terrain
            MeshData.VertexColors.Add(FColor(75, 150, 75, 255));
        }
    }
    
    // 3. Création des triangles pour la face avant
    int32 VerticesPerFace = HorizontalResolution * VerticalResolution;
    for (int32 y = 0; y < VerticalResolution - 1; ++y)
    {
        for (int32 x = 0; x < HorizontalResolution - 1; ++x)
        {
            int32 Current = y * HorizontalResolution + x;
            int32 Next = Current + 1;
            int32 Bottom = Current + HorizontalResolution;
            int32 BottomNext = Bottom + 1;
            
            // Premier triangle
            MeshData.Triangles.Add(Current);
            MeshData.Triangles.Add(Bottom);
            MeshData.Triangles.Add(Next);
            
            // Second triangle
            MeshData.Triangles.Add(Next);
            MeshData.Triangles.Add(Bottom);
            MeshData.Triangles.Add(BottomNext);
        }
    }
    
    // 4. Création des triangles pour la face arrière (inversés)
    for (int32 y = 0; y < VerticalResolution - 1; ++y)
    {
        for (int32 x = 0; x < HorizontalResolution - 1; ++x)
        {
            int32 Current = VerticesPerFace + y * HorizontalResolution + x;
            int32 Next = Current + 1;
            int32 Bottom = Current + HorizontalResolution;
            int32 BottomNext = Bottom + 1;
            
            // Premier triangle (inversé)
            MeshData.Triangles.Add(Next);
            MeshData.Triangles.Add(Bottom);
            MeshData.Triangles.Add(Current);
            
            // Second triangle (inversé)
            MeshData.Triangles.Add(BottomNext);
            MeshData.Triangles.Add(Bottom);
            MeshData.Triangles.Add(Next);
        }
    }
    
    // 5. Ajouter les triangles pour les faces latérales
    // Face inférieure (bas)
    for (int32 x = 0; x < HorizontalResolution - 1; ++x)
    {
        int32 FrontLeft = x;
        int32 FrontRight = x + 1;
        int32 BackLeft = VerticesPerFace + x;
        int32 BackRight = VerticesPerFace + x + 1;
        
        MeshData.Triangles.Add(FrontLeft);
        MeshData.Triangles.Add(FrontRight);
        MeshData.Triangles.Add(BackLeft);
        
        MeshData.Triangles.Add(BackLeft);
        MeshData.Triangles.Add(FrontRight);
        MeshData.Triangles.Add(BackRight);
    }
    
    // Face supérieure (haut)
    for (int32 x = 0; x < HorizontalResolution - 1; ++x)
    {
        int32 FrontLeft = (VerticalResolution - 1) * HorizontalResolution + x;
        int32 FrontRight = FrontLeft + 1;
        int32 BackLeft = VerticesPerFace + (VerticalResolution - 1) * HorizontalResolution + x;
        int32 BackRight = BackLeft + 1;
        
        MeshData.Triangles.Add(FrontRight);
        MeshData.Triangles.Add(FrontLeft);
        MeshData.Triangles.Add(BackLeft);
        
        MeshData.Triangles.Add(BackRight);
        MeshData.Triangles.Add(FrontRight);
        MeshData.Triangles.Add(BackLeft);
    }
    
    // Face gauche
    for (int32 y = 0; y < VerticalResolution - 1; ++y)
    {
        int32 FrontBottom = y * HorizontalResolution;
        int32 FrontTop = FrontBottom + HorizontalResolution;
        int32 BackBottom = VerticesPerFace + y * HorizontalResolution;
        int32 BackTop = BackBottom + HorizontalResolution;
        
        MeshData.Triangles.Add(FrontBottom);
        MeshData.Triangles.Add(BackBottom);
        MeshData.Triangles.Add(FrontTop);
        
        MeshData.Triangles.Add(FrontTop);
        MeshData.Triangles.Add(BackBottom);
        MeshData.Triangles.Add(BackTop);
    }
    
    // Face droite
    for (int32 y = 0; y < VerticalResolution - 1; ++y)
    {
        int32 FrontBottom = y * HorizontalResolution + (HorizontalResolution - 1);
        int32 FrontTop = FrontBottom + HorizontalResolution;
        int32 BackBottom = VerticesPerFace + y * HorizontalResolution + (HorizontalResolution - 1);
        int32 BackTop = BackBottom + HorizontalResolution;
        
        MeshData.Triangles.Add(BackBottom);
        MeshData.Triangles.Add(FrontBottom);
        MeshData.Triangles.Add(FrontTop);
        
        MeshData.Triangles.Add(BackTop);
        MeshData.Triangles.Add(BackBottom);
        MeshData.Triangles.Add(FrontTop);
    }
    
    // Initialiser les normales
    MeshData.Normals.Init(FVector::ZeroVector, MeshData.Vertices.Num());
    
    // Calculer les normales pour chaque triangle et les ajouter aux normales des vertices
    for (int32 i = 0; i < MeshData.Triangles.Num(); i += 3)
    {
        // Obtenir les indices des vertices du triangle
        int32 Index0 = MeshData.Triangles[i];
        int32 Index1 = MeshData.Triangles[i + 1];
        int32 Index2 = MeshData.Triangles[i + 2];
        
        // Calculer les vecteurs des côtés du triangle
        FVector Side1 = MeshData.Vertices[Index1] - MeshData.Vertices[Index0];
        FVector Side2 = MeshData.Vertices[Index2] - MeshData.Vertices[Index0];
        
        // Calculer la normale du triangle (produit vectoriel)
        FVector Normal = FVector::CrossProduct(Side1, Side2).GetSafeNormal();
        
        // Ajouter la normale à chaque vertex du triangle
        MeshData.Normals[Index0] += Normal;
        MeshData.Normals[Index1] += Normal;
        MeshData.Normals[Index2] += Normal;
    }
    
    // Normaliser toutes les normales
    for (int32 i = 0; i < MeshData.Normals.Num(); i++)
    {
        MeshData.Normals[i] = MeshData.Normals[i].GetSafeNormal();
    }
    
    // Élargir le tableau des tangentes si nécessaire
    if (Tangents.Num() < MeshData.Vertices.Num())
    {
        int32 OldSize = Tangents.Num();
        Tangents.AddDefaulted(MeshData.Vertices.Num() - OldSize);
        
        for (int32 i = OldSize; i < Tangents.Num(); ++i)
        {
            Tangents[i] = FProcMeshTangent(1.0f, 0.0f, 0.0f);
        }
    }
    
    // Marquer les données comme valides
    MeshData.bIsValid = true;
    
    // Créer le mesh à partir des données
    CreateMeshFromData(MeshData);
    
    // Si nous sommes sur le serveur, répliquer ces données vers tous les clients
    if (HasAuthority())
    {
        Multicast_UpdateTerrainMesh(MeshData);
    }
    
    // Log pour débogage
    UE_LOG(LogTemp, Log, TEXT("%s: Terrain généré avec %d vertices et %d triangles"), 
        HasAuthority() ? TEXT("Server") : TEXT("Client"),
        MeshData.Vertices.Num(), 
        MeshData.Triangles.Num() / 3);
}

void ADestructibleTerrain::InitializeTangents()
{
    // Initialiser les tangentes (une seule fois)
    Tangents.Empty();
    
    // Nous utilisons une tangente par défaut
    for (int32 i = 0; i < 8; i++)  // 8 vertices pour notre cube
    {
        Tangents.Add(FProcMeshTangent(1.0f, 0.0f, 0.0f));
    }
}

void ADestructibleTerrain::CreateMeshFromData(const FTerrainMeshData& InMeshData)
{
    if (!InMeshData.bIsValid)
    {
        UE_LOG(LogTemp, Error, TEXT("CreateMeshFromData called with invalid mesh data"));
        return;
    }
    
    // Convertir les couleurs
    TArray<FLinearColor> LinearColors = ConvertColorsToLinear(InMeshData.VertexColors);
    
    // S'assurer que toutes les propriétés sont correctement définies
    TArray<FVector> EmptyVertices;
    TArray<int32> EmptyTriangles;
    
    // Créer la section de mesh
    // IMPORTANT: Nous utilisons ClearMeshSection pour éviter tout problème
    TerrainMesh->ClearMeshSection(0);
    
    // Vérification supplémentaire pour éviter des crashs
    if (InMeshData.Vertices.Num() > 0 && InMeshData.Triangles.Num() > 0 && 
        InMeshData.Normals.Num() == InMeshData.Vertices.Num() && 
        InMeshData.UVs.Num() == InMeshData.Vertices.Num() &&
        LinearColors.Num() == InMeshData.Vertices.Num())
    {
        // Créer la section avec les données fournies
        TerrainMesh->CreateMeshSection_LinearColor(
            0, 
            InMeshData.Vertices, 
            InMeshData.Triangles, 
            InMeshData.Normals, 
            InMeshData.UVs, 
            LinearColors, 
            Tangents, 
            true  // Génère une collision
        );
        
        // Activer les collisions
        TerrainMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid mesh data dimensions"));
        return;
    }
    
    // Forcer l'application du matériau
    if (TerrainMaterial)
    {
        TerrainMesh->SetMaterial(0, TerrainMaterial);
    }
    else
    {
        // Utiliser un matériau par défaut si aucun n'est assigné
        UE_LOG(LogTemp, Warning, TEXT("Aucun matériau assigné au terrain. Utilisation d'un matériau par défaut."));
        UMaterial* DefaultMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
        TerrainMesh->SetMaterial(0, DefaultMaterial);
    }
    
    // S'assurer que le mesh est visible
    TerrainMesh->SetVisibility(true);
    
    // Forcer une mise à jour du rendu
    TerrainMesh->MarkRenderStateDirty();
}

TArray<FLinearColor> ADestructibleTerrain::ConvertColorsToLinear(const TArray<FColor>& Colors)
{
    TArray<FLinearColor> LinearColors;
    LinearColors.Reserve(Colors.Num());
    
    for (const FColor& Color : Colors)
    {
        LinearColors.Add(FLinearColor(Color));
    }
    
    return LinearColors;
}

void ADestructibleTerrain::Multicast_UpdateTerrainMesh_Implementation(const FTerrainMeshData& InMeshData)
{
    // Ne pas exécuter sur le serveur, il a déjà fait cette opération
    if (HasAuthority())
    {
        return;
    }
    
    UE_LOG(LogTemp, Log, TEXT("Client received terrain mesh update with %d vertices and %d triangles"), 
        InMeshData.Vertices.Num(), InMeshData.Triangles.Num() / 3);
    
    // Mettre à jour les données locales
    this->MeshData = InMeshData;
    
    // Créer le mesh à partir des données
    CreateMeshFromData(InMeshData);
}

void ADestructibleTerrain::OnRep_MeshData()
{
    UE_LOG(LogTemp, Log, TEXT("OnRep_MeshData called on client"));
    
    // Lorsque MeshData est répliqué, recréer le mesh
    if (MeshData.bIsValid)
    {
        CreateMeshFromData(MeshData);
    }
}

void ADestructibleTerrain::RequestDestroyTerrainAt(FVector2D Position, FVector2D Size)
{
    // Appeler la fonction serveur pour valider et appliquer la destruction
    if (GetLocalRole() < ROLE_Authority)
    {
        Server_DestroyTerrainAt(Position, Size);
    }
    else
    {
        // Si déjà sur le serveur, appliquer directement
        Server_DestroyTerrainAt(Position, Size);
    }
}

bool ADestructibleTerrain::Server_DestroyTerrainAt_Validate(FVector2D Position, FVector2D Size)
{
    // Validation simple
    return true;
}

void ADestructibleTerrain::Server_DestroyTerrainAt_Implementation(FVector2D Position, FVector2D Size)
{
    UE_LOG(LogTemp, Warning, TEXT("Destroying terrain at position (%f, %f) with size (%f, %f)"), 
        Position.X, Position.Y, Size.X, Size.Y);
    
    // Créer une nouvelle modification
    FTerrainModification NewMod(Position, Size);
    
    // Ajouter à la liste des modifications
    TerrainModifications.Add(NewMod);
    
    // Afficher le nombre total de modifications
    UE_LOG(LogTemp, Warning, TEXT("Total modifications: %d"), TerrainModifications.Num());
    
    // Appliquer les modifications sur le serveur
    ApplyTerrainModifications();
}
void ADestructibleTerrain::OnRep_TerrainModifications()
{
    // Appelé sur les clients quand TerrainModifications est répliqué
    ApplyTerrainModifications();
}

bool ADestructibleTerrain::IsVertexInModification(const FVector& Vertex, const FTerrainModification& Modification)
{
    // Obtenir la position locale du vertex par rapport à l'acteur
    FVector LocalVertex = Vertex;
    
    // Log pour déboguer
    UE_LOG(LogTemp, Log, TEXT("Checking vertex at (%f, %f, %f) against modification at (%f, %f) with size (%f, %f)"),
        LocalVertex.X, LocalVertex.Y, LocalVertex.Z,
        Modification.Position.X, Modification.Position.Y,
        Modification.Size.X, Modification.Size.Y);
    
    // Considérer que le vertex est en 2D (X, Z)
    float VertexX = LocalVertex.X;
    float VertexZ = LocalVertex.Z;
    
    // Vérifier si le vertex est dans le rectangle de modification
    bool isInside = (VertexX >= Modification.Position.X && 
            VertexX <= Modification.Position.X + Modification.Size.X &&
            VertexZ >= Modification.Position.Y && 
            VertexZ <= Modification.Position.Y + Modification.Size.Y);
    
    if (isInside)
    {
        UE_LOG(LogTemp, Log, TEXT("Vertex is inside modification area!"));
    }
    
    return isInside;
}

void ADestructibleTerrain::ApplyTerrainModifications()
{
    // Si aucune modification, ne rien faire
    if (TerrainModifications.Num() == 0)
    {
        return;
    }
    
    UE_LOG(LogTemp, Warning, TEXT("Applying %d terrain modifications"), TerrainModifications.Num());
    
    // On ne traite que les modifications non appliquées
    TArray<FTerrainModification> NewModifications;
    for (const FTerrainModification& Mod : TerrainModifications)
    {
        if (!AppliedModifications.Contains(Mod))
        {
            NewModifications.Add(Mod);
        }
    }
    
    if (NewModifications.Num() == 0)
    {
        return; // Toutes les modifications ont déjà été appliquées
    }
    
    // Nous allons travailler avec des copies des données existantes
    TArray<FVector> NewVertices = MeshData.Vertices;
    TArray<int32> NewTriangles; // Sera rempli avec les triangles conservés et modifiés
    TArray<FVector2D> NewUVs = MeshData.UVs;
    TArray<FColor> NewVertexColors = MeshData.VertexColors;
    
    // Liste des triangles à conserver (non affectés par les modifications)
    TArray<bool> TriangleToKeep;
    TriangleToKeep.Init(true, MeshData.Triangles.Num() / 3);
    
    // 1. Première passe: identifier les triangles touchés par les modifications
    for (int32 TriIdx = 0; TriIdx < MeshData.Triangles.Num() / 3; TriIdx++)
    {
        int32 Index1 = MeshData.Triangles[TriIdx * 3];
        int32 Index2 = MeshData.Triangles[TriIdx * 3 + 1];
        int32 Index3 = MeshData.Triangles[TriIdx * 3 + 2];
        
        // Vérifier si le triangle est affecté par l'une des nouvelles modifications
        for (const FTerrainModification& Mod : NewModifications)
        {
            if (IsVertexInModification(MeshData.Vertices[Index1], Mod) ||
                IsVertexInModification(MeshData.Vertices[Index2], Mod) ||
                IsVertexInModification(MeshData.Vertices[Index3], Mod))
            {
                TriangleToKeep[TriIdx] = false;
                break;
            }
        }
    }
    
    // 2. Construire le nouveau tableau de triangles en ne gardant que ceux qui ne sont pas touchés
    for (int32 TriIdx = 0; TriIdx < MeshData.Triangles.Num() / 3; TriIdx++)
    {
        if (TriangleToKeep[TriIdx])
        {
            // Ajouter les indices du triangle
            NewTriangles.Add(MeshData.Triangles[TriIdx * 3]);
            NewTriangles.Add(MeshData.Triangles[TriIdx * 3 + 1]);
            NewTriangles.Add(MeshData.Triangles[TriIdx * 3 + 2]);
        }
    }
    
    // 3. Mettre à jour les données de mesh
    MeshData.Triangles = NewTriangles;
    
    // 4. Recalculer les normales
    MeshData.Normals.Init(FVector::ZeroVector, NewVertices.Num());
    
    for (int32 i = 0; i < NewTriangles.Num(); i += 3)
    {
        // Obtenir les indices des vertices du triangle
        int32 Index0 = NewTriangles[i];
        int32 Index1 = NewTriangles[i + 1];
        int32 Index2 = NewTriangles[i + 2];
        
        // Vérifier si les indices sont valides
        if (NewVertices.IsValidIndex(Index0) && NewVertices.IsValidIndex(Index1) && NewVertices.IsValidIndex(Index2))
        {
            // Calculer les vecteurs des côtés du triangle
            FVector Side1 = NewVertices[Index1] - NewVertices[Index0];
            FVector Side2 = NewVertices[Index2] - NewVertices[Index0];
            
            // Calculer la normale du triangle (produit vectoriel)
            FVector Normal = FVector::CrossProduct(Side1, Side2).GetSafeNormal();
            
            // Ajouter la normale à chaque vertex du triangle
            MeshData.Normals[Index0] += Normal;
            MeshData.Normals[Index1] += Normal;
            MeshData.Normals[Index2] += Normal;
        }
    }
    
    // Normaliser toutes les normales
    for (int32 i = 0; i < MeshData.Normals.Num(); i++)
    {
        if (!MeshData.Normals[i].IsZero())
        {
            MeshData.Normals[i] = MeshData.Normals[i].GetSafeNormal();
        }
        else
        {
            // Par défaut, on met une normale vers l'extérieur
            MeshData.Normals[i] = FVector(0.0f, -1.0f, 0.0f);
        }
    }
    
    // 5. Appliquer les données au mesh
    CreateMeshFromData(MeshData);
    
    // 6. Ajouter les nouvelles modifications à la liste des modifications appliquées
    for (const FTerrainModification& Mod : NewModifications)
    {
        AppliedModifications.Add(Mod);
    }
    
    // Marquer que les modifications ont été appliquées
    bModificationsApplied = true;
    
    // Si nous sommes sur le serveur, répliquer ces données vers tous les clients
    if (HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("Replicating updated mesh to clients"));
        Multicast_UpdateTerrainMesh(MeshData);
    }
}

void ADestructibleTerrain::Multicast_ForceVisualUpdate_Implementation()
{
    UE_LOG(LogTemp, Log, TEXT("ForceVisualUpdate called on %s"), HasAuthority() ? TEXT("server") : TEXT("client"));
    
    if (MeshData.bIsValid)
    {
        // Recréer le mesh à partir des données actuelles
        CreateMeshFromData(MeshData);
    }
    else if (bIsInitialized)
    {
        // Si les données ne sont pas valides mais que le terrain est initialisé, régénérer
        GenerateTerrain();
    }
}