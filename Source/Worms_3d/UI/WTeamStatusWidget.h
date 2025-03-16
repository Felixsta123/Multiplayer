#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/VerticalBox.h"
#include "WTeamStatusWidget.generated.h"

/**
 * Widget qui affiche la liste des personnages de chaque équipe avec leurs barres de vie
 */
UCLASS()
class WORMS_3D_API UWTeamStatusWidget : public UUserWidget
{
	GENERATED_BODY()
    
public:
	UWTeamStatusWidget(const FObjectInitializer& ObjectInitializer);
    
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
    
	// Appelée pour mettre à jour l'affichage des équipes
	UFUNCTION()
	void UpdateTeamStatus();
    
protected:
	// Conteneur pour les équipes
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UVerticalBox* TeamsContainer;
    
	// Widget à dupliquer pour chaque personnage
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<class UWCharacterStatusWidget> CharacterStatusWidgetClass;
    
	// Référence au GameState
	UPROPERTY()
	class AWormGameState* WormGameState;
    
private:
	// Fonction interne pour créer un widget de personnage
	class UWCharacterStatusWidget* CreateCharacterStatusWidget(class AWormCharacter* Character);
};