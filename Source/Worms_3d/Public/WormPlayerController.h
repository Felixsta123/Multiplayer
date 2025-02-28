// WormPlayerController.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/UserWidget.h"
#include "WormPlayerController.generated.h"

UCLASS()
class WORMS_3D_API AWormPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    AWormPlayerController();

    virtual void Tick(float DeltaTime) override;
    virtual void BeginPlay() override;

protected:
    // La classe du widget UI à créer
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "UI")
    TSubclassOf<UUserWidget> GameUIClass;
    
    // L'instance du widget UI
    UPROPERTY(BlueprintReadOnly, Category = "UI")
    UUserWidget* GameUIWidget;
    
private:
    FTimerHandle UICheckTimerHandle;
    void CheckAndCreateUI();
};
