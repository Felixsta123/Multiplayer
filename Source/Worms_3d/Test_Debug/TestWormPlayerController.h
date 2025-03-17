#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "TestWormPlayerController.generated.h"

/**
 * Controller de joueur pour le mode test
 * Simplifié pour faciliter les tests en solo
 */
UCLASS()
class WORMS_3D_API ATestWormPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ATestWormPlayerController();
    
	// Override des fonctions de base
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
    
	// Classe du widget d'interface utilisateur test
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<class UUserWidget> TestUIWidgetClass;
    
	// Instance du widget d'interface
	UPROPERTY(BlueprintReadOnly, Category = "UI")
	class UUserWidget* TestUIWidget;
    
	// Fonction pour créer l'interface de test
	UFUNCTION(BlueprintCallable, Category = "UI")
	void CreateTestUI();
    
	// Fonction pour terminer le tour (dans le mode test, cela réinitialise le tour)
	UFUNCTION(BlueprintCallable, Category = "Game")
	void EndTurn();
};