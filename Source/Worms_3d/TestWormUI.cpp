#include "TestWormUI.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "TestWormPlayerController.h"
#include "TestWormGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "AWormCharacter.h"

void UTestWormUI::NativeConstruct()
{
    Super::NativeConstruct();
    
    // Lier les événements des boutons
    if (EndTurnButton)
    {
        EndTurnButton->OnClicked.AddDynamic(this, &UTestWormUI::OnEndTurnClicked);
    }
    
    if (TestExplosionButton)
    {
        TestExplosionButton->OnClicked.AddDynamic(this, &UTestWormUI::OnTestExplosionClicked);
    }
    
    if (ResetTerrainButton)
    {
        ResetTerrainButton->OnClicked.AddDynamic(this, &UTestWormUI::OnResetTerrainClicked);
    }
    
    // Désactiver ou supprimer le bouton de toggle si non nécessaire
    if (ToggleDestructionSystemButton)
    {
        ToggleDestructionSystemButton->SetVisibility(ESlateVisibility::Collapsed);
    }
    
    // S'assurer que le widget est initialisé correctement
    UE_LOG(LogTemp, Log, TEXT("Test UI Widget construit avec succès"));
}

void UTestWormUI::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    
    // Mettre à jour les informations de test
    UpdateTestInfo();
}

void UTestWormUI::UpdateTestInfo()
{
    // Récupérer le GameMode
    ATestWormGameMode* TestGameMode = Cast<ATestWormGameMode>(UGameplayStatics::GetGameMode(this));
    if (!TestGameMode)
    {
        return;
    }
    
    // Mettre à jour le temps restant
    if (TimeRemainingText)
    {
        int32 Minutes = FMath::FloorToInt(TestGameMode->RemainingTurnTime / 60.0f);
        int32 Seconds = FMath::FloorToInt(TestGameMode->RemainingTurnTime) % 60;
        FString TimeString = FString::Printf(TEXT("Temps: %02d:%02d"), Minutes, Seconds);
        TimeRemainingText->SetText(FText::FromString(TimeString));
    }
    
    // Supprimer la mise à jour du système de destruction
    if (DestructionSystemText)
    {
        DestructionSystemText->SetText(FText::FromString(TEXT("Terrain Destructible")));
    }
    
    // Récupérer le personnage
    APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
    if (PC)
    {
        AWormCharacter* Character = Cast<AWormCharacter>(PC->GetPawn());
        if (Character && PlayerInfoText)
        {
            FString InfoString = FString::Printf(TEXT("Santé: %.0f\nMouvement: %.0f/%.0f"), 
                Character->GetHealth(),
                Character->MovementPoints,
                Character->MaxMovementPoints);
            
            PlayerInfoText->SetText(FText::FromString(InfoString));
        }
    }
}

void UTestWormUI::OnEndTurnClicked()
{
    // Récupérer le PlayerController
    ATestWormPlayerController* PC = Cast<ATestWormPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
    if (PC)
    {
        PC->EndTurn();
    }
}

void UTestWormUI::OnTestExplosionClicked()
{
    // Récupérer le GameMode
    ATestWormGameMode* TestGameMode = Cast<ATestWormGameMode>(UGameplayStatics::GetGameMode(this));
    if (!TestGameMode || !TestGameMode->DestructibleTerrain)
    {
        UE_LOG(LogTemp, Error, TEXT("GameMode or Terrain not found"));
        return;
    }
    
    // Exemple de destruction de terrain
    TestGameMode->DestructibleTerrain->RequestDestroyTerrainAt(
        FVector2D(500.0f, 500.0f),  // Position de test
        FVector2D(100.0f, 100.0f)   // Taille de test
    );
    
    UE_LOG(LogTemp, Warning, TEXT("Test explosion triggered on destructible terrain"));
}

void UTestWormUI::OnResetTerrainClicked()
{
    // Récupérer le GameMode
    ATestWormGameMode* TestGameMode = Cast<ATestWormGameMode>(UGameplayStatics::GetGameMode(this));
    if (!TestGameMode)
    {
        return;
    }
    
    // Recréer le terrain
    TestGameMode->SpawnDestructibleTerrain();
    
    UE_LOG(LogTemp, Warning, TEXT("Terrain réinitialisé"));
}

void UTestWormUI::OnToggleDestructionSystemClicked()
{
    // Désactivé car plus nécessaire
    UE_LOG(LogTemp, Warning, TEXT("Toggle destruction system no longer supported"));
}