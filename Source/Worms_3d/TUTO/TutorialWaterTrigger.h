#pragma once

#include "CoreMinimal.h"
#include "Engine/TriggerBox.h"
#include "TutorialWaterTrigger.generated.h"

UCLASS()
class WORMS_3D_API ATutorialWaterTrigger : public ATriggerBox
{
	GENERATED_BODY()
    
public:
	ATutorialWaterTrigger();
    
	virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;
    
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWaterObservedSignature);
	UPROPERTY(BlueprintAssignable, Category = "Tutorial")
	FOnWaterObservedSignature OnWaterObserved;
};