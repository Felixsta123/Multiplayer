// WormPlayerController.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/UserWidget.h"
#include "../PlayerDataStruct.h"
#include "WormPlayerController.generated.h"

UCLASS()
class WORMS_3D_API AWormPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    AWormPlayerController();

    virtual void Tick(float DeltaTime) override;
    virtual void BeginPlay() override;
    // Player settings variable
    UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Player Settings")
    FPlayerData PlayerSettings;
protected:
    // La classe du widget UI à créer
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "UI")
    TSubclassOf<UUserWidget> GameUIClass;
    
    // L'instance du widget UI
    UPROPERTY(BlueprintReadOnly, Category = "UI")
    UUserWidget* GameUIWidget;
    
    // La classe du widget UI du joueur
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "UI")
    TSubclassOf<class UUserWidget> PlayerUIWidgetClass;

    // L'instance du widget UI du joueur
    UPROPERTY(BlueprintReadOnly, Category = "UI")
    class UUserWidget* PlayerUIWidget;

    // Fonction pour créer l'UI du joueur
    UFUNCTION(BlueprintCallable, Category = "UI")
    void CreatePlayerUI();
private:
    FTimerHandle UICheckTimerHandle;
    FTimerHandle PlayerUITimerHandle;

    void CheckAndCreateUI();
};
