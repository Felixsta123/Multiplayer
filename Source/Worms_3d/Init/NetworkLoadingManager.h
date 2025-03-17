// NetworkLoadingManager.h
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameLoadingWidget.h"
#include "NetworkLoadingManager.generated.h"

/**
 * Component that handles network-synchronized loading screens
 * Attach this to GameState for proper replication to all clients
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class WORMS_3D_API UNetworkLoadingManager : public UActorComponent
{
    GENERATED_BODY()

public:
    UNetworkLoadingManager();
    
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // Functions to control loading screens - call these on server
    UFUNCTION(BlueprintCallable, Category = "Loading")
    void ShowLoadingScreen(float Duration = 0.0f);
    
    UFUNCTION(BlueprintCallable, Category = "Loading")
    void UpdateLoadingProgress(float Progress, const FString& StatusText);
    
    UFUNCTION(BlueprintCallable, Category = "Loading")
    void DismissLoadingScreen();
    
    // Client-side properties
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Loading")
    TSubclassOf<UGameLoadingWidget> LoadingWidgetClass;

protected:
    virtual void BeginPlay() override;
    
    // Replicated properties to sync loading state
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Loading")
    bool bIsLoadingActive;
    
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Loading")
    float LoadingProgress;
    
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Loading")
    FString LoadingStatusText;
    
    // Event called when loading state changes
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_ShowLoadingScreen(float Duration);
    
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_UpdateLoadingProgress(float Progress, const FString& StatusText);
    
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_DismissLoadingScreen();
    
    // Client-side loading widget management
    UPROPERTY()
    UGameLoadingWidget* LoadingWidget;
    
    // Creates the loading widget if it doesn't exist
    UFUNCTION()
    void EnsureLoadingWidgetExists();
};