#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WCharacterStatusWidget.generated.h"

/**
 * Widget qui affiche les informations d'un personnage (nom, barre de vie, etc.)
 */
UCLASS()
class WORMS_3D_API UWCharacterStatusWidget : public UUserWidget
{
	GENERATED_BODY()
    
public:
	UWCharacterStatusWidget(const FObjectInitializer& ObjectInitializer);
    
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    
	// Définir le personnage à afficher
	UFUNCTION(BlueprintCallable, Category = "UI")
	void SetCharacter(class AWormCharacter* InCharacter);
    
protected:
	// Éléments UI
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	class UTextBlock* CharacterNameText;
    
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	class UProgressBar* HealthBar;
    
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	class UImage* TeamColorImage;
    
	// Personnage référencé
	UPROPERTY(BlueprintReadOnly, Category = "UI")
	class AWormCharacter* Character;
    
	// Mettre à jour les informations UI
	UFUNCTION()
	void UpdateHealth();
};