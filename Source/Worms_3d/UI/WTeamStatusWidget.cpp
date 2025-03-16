#include "Worms_3d/UI/WTeamStatusWidget.h"
#include "Worms_3d/UI/WCharacterStatusWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Components/VerticalBox.h"
#include "WormGameState.h"

UWTeamStatusWidget::UWTeamStatusWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // Rien à initialiser ici
}

void UWTeamStatusWidget::NativeConstruct()
{
    Super::NativeConstruct();
    
    // Récupérer le GameState
    WormGameState = Cast<AWormGameState>(UGameplayStatics::GetGameState(GetWorld()));
    
    if (WormGameState)
    {
        // S'abonner à l'événement de mise à jour des équipes
        WormGameState->OnTeamStatusUpdated.AddDynamic(this, &UWTeamStatusWidget::UpdateTeamStatus);
        
        // Mise à jour initiale
        UpdateTeamStatus();
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("WTeamStatusWidget: GameState non trouvé"));
    }
}

void UWTeamStatusWidget::NativeDestruct()
{
    // Se désabonner des événements
    if (WormGameState)
    {
        WormGameState->OnTeamStatusUpdated.RemoveDynamic(this, &UWTeamStatusWidget::UpdateTeamStatus);
    }
    
    Super::NativeDestruct();
}

void UWTeamStatusWidget::UpdateTeamStatus()
{
    if (!WormGameState || !TeamsContainer || !CharacterStatusWidgetClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("WTeamStatusWidget: Ressources manquantes pour UpdateTeamStatus"));
        return;
    }
    
    // Vider le conteneur actuel
    TeamsContainer->ClearChildren();
    
    // Parcourir toutes les équipes
    for (const FTeamInfo& Team : WormGameState->Teams)
    {
        // Créer un widget pour chaque personnage de l'équipe
        for (AWormCharacter* Character : Team.TeamMembers)
        {
            if (Character)
            {
                UWCharacterStatusWidget* CharWidget = CreateCharacterStatusWidget(Character);
                if (CharWidget)
                {
                    TeamsContainer->AddChild(CharWidget);
                }
            }
        }
    }
}

UWCharacterStatusWidget* UWTeamStatusWidget::CreateCharacterStatusWidget(AWormCharacter* Character)
{
    if (!Character || !CharacterStatusWidgetClass)
    {
        return nullptr;
    }
    
    // Créer une instance du widget
    UWCharacterStatusWidget* Widget = CreateWidget<UWCharacterStatusWidget>(this, CharacterStatusWidgetClass);
    if (Widget)
    {
        // Initialiser le widget avec le personnage
        Widget->SetCharacter(Character);
    }
    
    return Widget;
}