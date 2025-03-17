#include "TutorialWaterTrigger.h"
#include "../AWormCharacter.h"
#include "WormTutorialGameMode.h"
#include "Kismet/GameplayStatics.h"

ATutorialWaterTrigger::ATutorialWaterTrigger()
{
	PrimaryActorTick.bCanEverTick = false;
    
	GetCollisionComponent()->SetGenerateOverlapEvents(true);
	GetCollisionComponent()->SetCollisionProfileName(TEXT("Trigger"));
}

void ATutorialWaterTrigger::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);
    
	// Check if the overlapping actor is the player character
	AWormCharacter* Character = Cast<AWormCharacter>(OtherActor);
	if (Character && Character->IsPlayerControlled())
	{
		UE_LOG(LogTemp, Warning, TEXT("Player has reached water observation point"));
        
		// Broadcast the event
		OnWaterObserved.Broadcast();
        
		// Notify the game mode
		AWormTutorialGameMode* TutorialGameMode = Cast<AWormTutorialGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
		if (TutorialGameMode)
		{
			TutorialGameMode->OnWaterObserved();
		}
	}
}