// WeaponUIData.h
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "WeaponUIData.generated.h"

/**
 * Data structure for weapon UI information
 */
USTRUCT(BlueprintType)
struct WORMS_3D_API FWeaponUIData : public FTableRowBase
{
	GENERATED_BODY()

public:
	// Display name for the weapon
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon UI")
	FString Name;

	// Icon to display on the weapon wheel
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon UI")
	UTexture2D* Icon;

	// Brief description of the weapon
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon UI")
	FString Description;

	// Default constructor
	FWeaponUIData() : Icon(nullptr) {}
};