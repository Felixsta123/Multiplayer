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
    
    if (ToggleDestructionSystemButton)
    {
        ToggleDestructionSystemButton->OnClicked.AddDynamic(this, &UTestWormUI::OnToggleDestructionSystemClicked);
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
    
    // Mettre à jour l'information sur le système de destruction actif
    if (DestructionSystemText)
    {
        FString SystemName = TestGameMode->GetActiveDestructionSystemName();
        FString SystemInfo = FString::Printf(TEXT("Système de destruction: %s"), *SystemName);
        DestructionSystemText->SetText(FText::FromString(SystemInfo));
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
    if (!TestGameMode)
    {
        UE_LOG(LogTemp, Error, TEXT("GameMode not found"));
        return;
    }
    
    // Vérifier si le terrain à base de chunks existe
    if (TestGameMode->ChunkBasedTerrain)
    {
        UE_LOG(LogTemp, Warning, TEXT("Calling TestExplosion on chunk-based terrain"));
        
        // Appeler la fonction via reflection (car on ne peut pas caster directement)
        UFunction* TestExplosionFunc = TestGameMode->ChunkBasedTerrain->FindFunction(FName("TestExplosion"));
        if (TestExplosionFunc)
        {
            TestGameMode->ChunkBasedTerrain->ProcessEvent(TestExplosionFunc, nullptr);
            UE_LOG(LogTemp, Warning, TEXT("TestExplosion called successfully"));
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("TestExplosion function not found"));
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Chunk-based terrain not found"));
    }
}
void UTestWormUI::OnResetTerrainClicked()
{
    // Récupérer le GameMode
    ATestWormGameMode* TestGameMode = Cast<ATestWormGameMode>(UGameplayStatics::GetGameMode(this));
    if (!TestGameMode)
    {
        return;
    }
    
    // Recréer le terrain avec le système actif
    TestGameMode->SpawnDestructibleTerrain();
    
    UE_LOG(LogTemp, Warning, TEXT("Terrain réinitialisé avec le système %s"), 
        *TestGameMode->GetActiveDestructionSystemName());
}

void UTestWormUI::OnToggleDestructionSystemClicked()
{
    // Récupérer le GameMode
    ATestWormGameMode* TestGameMode = Cast<ATestWormGameMode>(UGameplayStatics::GetGameMode(this));
    if (!TestGameMode)
    {
        return;
    }
    
    // Appeler la fonction de basculement
    //TestGameMode->ToggleDestructionSystem();
    
    // Mettre à jour l'UI immédiatement
    if (DestructionSystemText)
    {
        FString SystemName = TestGameMode->GetActiveDestructionSystemName();
        FString SystemInfo = FString::Printf(TEXT("Système de destruction: %s"), *SystemName);
        DestructionSystemText->SetText(FText::FromString(SystemInfo));
    }
    
    UE_LOG(LogTemp, Warning, TEXT("Tentative de basculement vers le système de destruction: %s"), 
        *TestGameMode->GetActiveDestructionSystemName());
}