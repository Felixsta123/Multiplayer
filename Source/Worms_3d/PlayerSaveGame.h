#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "PlayerDataStruct.h" // Assurez-vous que le fichier contenant FPlayerData est inclus
#include "PlayerSaveGame.generated.h"

UCLASS()
class WORMS_3D_API UPlayerSaveGame : public USaveGame
{
	GENERATED_BODY()

public:

	// Constructeur
	UPlayerSaveGame();

	// Informations du joueur sauvegardées
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SaveData", Replicated)
	FPlayerData SavedPlayerInfo;
};
