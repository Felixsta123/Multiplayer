#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "WormGameState.h"
#include "UWormGameUI.generated.h"

UCLASS()
class WORMS_3D_API UWormGameUI : public UUserWidget
{
    GENERATED_BODY()
    
public:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    // Nouvelle fonction pour vérifier et rétablir la liaison du delegate si nécessaire
    UFUNCTION(BlueprintCallable)
    void EnsureDelegateBinding();
protected:
    // Référence vers le GameState
    UPROPERTY(BlueprintReadOnly, Category = "Game")
    AWormGameState* WormGameState;
    
    // Widget pour afficher le nom du joueur actif
    UPROPERTY(meta = (BindWidget))
    UTextBlock* CurrentPlayerName;
    
    // Widget pour afficher le temps restant
    UPROPERTY(meta = (BindWidget))
    UTextBlock* RemainingTime;
    
    // Barre de progression pour le temps
    UPROPERTY(meta = (BindWidget))
    UProgressBar* TimeProgressBar;
    
    // Texte pour afficher le nombre de joueurs restants
    UPROPERTY(meta = (BindWidget))
    UTextBlock* PlayersRemaining;
    
    UPROPERTY(meta = (BindWidget))
    UTextBlock* YourTurnText;
    
    UFUNCTION()
    void OnCurrentPlayerChangedCallback(int32 NewPlayerIndex);
    
    // Fonction pour mettre à jour l'UI
    UFUNCTION(BlueprintCallable)
    void UpdateUI();
};
