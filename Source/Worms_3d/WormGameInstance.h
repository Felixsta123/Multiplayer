#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "WormGameInstance.generated.h"

UCLASS()
class WORMS_3D_API UWormGameInstance : public UGameInstance
{
	GENERATED_BODY()
    
public:
	UWormGameInstance();
	
	// We're removing all game results functionality from GameInstance
	// to keep it within the GameState/GameMode

	// Add any other game-instance level functionality here that doesn't
	// relate to the game-over system
};