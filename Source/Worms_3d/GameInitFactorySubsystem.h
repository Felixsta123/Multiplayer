#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameInitManager.h"
#include "GameInitFactorySubsystem.generated.h"

/**
 * Subsystem providing utilities to create and access game initialization components
 */
UCLASS()
class WORMS_3D_API UGameInitFactorySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
    
public:
	// Initialize the subsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    
	// Deinitialize the subsystem
	virtual void Deinitialize() override;
    
	// Get the current GameInitManager instance or create one if it doesn't exist
	UFUNCTION(BlueprintCallable, Category = "Game Initialization")
	AGameInitManager* GetOrCreateGameInitManager(UObject* WorldContextObject);
    
	// Get the loading widget class to use
	UFUNCTION(BlueprintCallable, Category = "Game Initialization")
	TSubclassOf<UGameLoadingWidget> GetLoadingWidgetClass() const;
    
	// Set the loading widget class to use
	UFUNCTION(BlueprintCallable, Category = "Game Initialization")
	void SetLoadingWidgetClass(TSubclassOf<UGameLoadingWidget> NewWidgetClass);
    
private:
	// The class to use for loading widgets
	UPROPERTY()
	TSubclassOf<UGameLoadingWidget> LoadingWidgetClass;
    
	// Current game init manager (weak pointer to avoid reference loops)
	TWeakObjectPtr<AGameInitManager> GameInitManagerInstance;
};