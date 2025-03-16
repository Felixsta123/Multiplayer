// WormPlayerController.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/UserWidget.h"
#include "Worms_3d/Misc//PlayerDataStruct.h"
#include "WormPlayerController.generated.h"

UCLASS()
class WORMS_3D_API AWormPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    AWormPlayerController();

    virtual void Tick(float DeltaTime) override;
    virtual void BeginPlay() override;
    void LoadPlayerSettings();
    // Player settings variable
    UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Player Settings")
    FPlayerData PlayerSettings;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "UI")
    TSubclassOf<class UWMainHUDWidget> MainHUDWidgetClass;

    // Instance du widget HUD
    UPROPERTY(BlueprintReadOnly, Category = "UI")
    class UWMainHUDWidget* MainHUDWidget;

    // Fonction pour créer le HUD principal
    UFUNCTION(BlueprintCallable, Category = "UI")
    void CreateMainHUD();
    void RefreshMainHUD();

protected:
    
    // ADD THIS to the public or protected section
    UFUNCTION(Server, Reliable)
    void ServerSetPlayerSettings(const FPlayerData& NewSettings);

    // Fonction pour créer l'UI du joueur
    UFUNCTION(BlueprintCallable, Category = "UI")
    void CreatePlayerUI();
    bool ServerSetPlayerSettings_Validate(const FPlayerData& NewSettings);
    virtual void OnNetCleanup(class UNetConnection* Connection) override;
    virtual void AcknowledgePossession(class APawn* P) override;

    UPROPERTY(ReplicatedUsing = OnRep_IsReady)
    bool bIsReady;
    
    UFUNCTION()
    void OnRep_IsReady();
    
    UFUNCTION(Server, Reliable, WithValidation)
    void ServerSignalReady();

    void CheckAndCreateUI();
private:
    FTimerHandle UIRefreshTimerHandle; // New timer for periodic refreshes
    FTimerHandle UICheckTimerHandle;
    FTimerHandle PlayerUITimerHandle;
    bool bIsFullyInitialized;

};
