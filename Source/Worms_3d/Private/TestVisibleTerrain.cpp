#include "TestVisibleTerrain.h"

ATestVisibleTerrain::ATestVisibleTerrain()
{
	PrimaryActorTick.bCanEverTick = false;
    
	// Création du mesh statique
	TerrainMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TerrainMesh"));
	RootComponent = TerrainMesh;
    
	// Charger un cube par défaut
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMeshAsset.Succeeded())
	{
		TerrainMesh->SetStaticMesh(CubeMeshAsset.Object);
	}
    
	// Charger un matériau de base rouge vif pour test
	static ConstructorHelpers::FObjectFinder<UMaterial> RedMaterialAsset(TEXT("/Engine/BasicShapes/BasicShapeMaterial"));
	if (RedMaterialAsset.Succeeded())
	{
		TerrainMaterial = RedMaterialAsset.Object;
		TerrainMesh->SetMaterial(0, TerrainMaterial);
	}
    
	// Configuration réseau
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(true);
}

void ATestVisibleTerrain::BeginPlay()
{
	Super::BeginPlay();
    
	UE_LOG(LogTemp, Warning, TEXT("TestVisibleTerrain: BeginPlay sur %s"), 
		HasAuthority() ? TEXT("Serveur") : TEXT("Client"));
}

void ATestVisibleTerrain::SetDimensions(float Width, float Height, float Depth)
{
	// Mettre à l'échelle le cube pour correspondre aux dimensions
	TerrainMesh->SetRelativeScale3D(FVector(Width/100.0f, Depth/100.0f, Height/100.0f));
    
	UE_LOG(LogTemp, Warning, TEXT("TestVisibleTerrain: Dimensions %fx%fx%f sur %s"), 
		Width, Height, Depth, HasAuthority() ? TEXT("Serveur") : TEXT("Client"));
}