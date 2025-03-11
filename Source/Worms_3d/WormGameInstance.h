#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "WormGameState.h" // Include for FPlayerDamageInfo
#include "WormGameInstance.generated.h"

UCLASS()
class WORMS_3D_API UWormGameInstance : public UGameInstance
{
	GENERATED_BODY()
    
public:
	UWormGameInstance();
    
	// Game state data that needs to persist across level transitions
	UPROPERTY(BlueprintReadWrite, Category = "Game")
	FString WinnerName;
    
	UPROPERTY(BlueprintReadWrite, Category = "Game")
	TArray<FString> PlayerNames;
    
	UPROPERTY(BlueprintReadWrite, Category = "Game")
	TArray<FPlayerDamageInfo> PlayerDamageDealt;
    
	// Functions to pass data between levels
	UFUNCTION(BlueprintCallable, Category = "Game")
	void StoreGameResults(const FString& Winner, const TArray<FString>& Players, const TArray<FPlayerDamageInfo>& DamageStats);
    
	UFUNCTION(BlueprintCallable, Category = "Game")
	void ClearGameResults();
};