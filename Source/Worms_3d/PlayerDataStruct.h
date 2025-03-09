#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "PlayerDataStruct.generated.h"

USTRUCT(BlueprintType)
struct FPlayerData
{
	GENERATED_BODY()

	// Nom du joueur
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	FText MyPlayerName;

	// Image du joueur
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	UTexture2D* MyPlayerImage;

	// Référence vers le personnage du joueur
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	TSubclassOf<ACharacter> MyPlayerCharacter;

	// Image du personnage
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	UTexture2D* MyCharacterImage;

	// Statut du joueur
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	FText MyPlayerStatus;
};
