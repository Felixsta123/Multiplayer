#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/PostProcessVolume.h"
#include "WaterSystem.generated.h"

/**
 * Système de gestion de l'eau montante/descendante
 * Permet de créer une marée qui monte progressivement et tue les joueurs qui la touchent
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class WORMS_3D_API UWaterSystem : public UActorComponent
{
	GENERATED_BODY()

public:
	UWaterSystem();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    
    // Accesseurs
    UFUNCTION(BlueprintCallable, Category = "Water")
    float GetCurrentWaterLevel() const { return CurrentWaterLevel; }
    
    UFUNCTION(BlueprintCallable, Category = "Water")
    float GetTargetWaterLevel() const { return TargetWaterLevel; }
    
    UFUNCTION(BlueprintCallable, Category = "Water")
    bool IsWaterRising() const { return bIsWaterRising; }
    
    // Forcer un changement du niveau d'eau
    UFUNCTION(BlueprintCallable, Category = "Water")
    void SetWaterLevel(float NewLevel, bool bImmediate = false);
    
    // Augmenter/diminuer le niveau d'eau
    UFUNCTION(BlueprintCallable, Category = "Water")
    void RaiseWaterLevel(float AmountToRaise = 50.0f);
    
    UFUNCTION(BlueprintCallable, Category = "Water")
    void LowerWaterLevel(float AmountToLower = 50.0f);
    
    // Fonctions de cycle automatique
    UFUNCTION(BlueprintCallable, Category = "Water")
    void StartAutomaticCycle(float CycleDuration = 300.0f);
    
    UFUNCTION(BlueprintCallable, Category = "Water")
    void StopAutomaticCycle();
    
    // Avertissement de niveau d'eau dangereux
    UFUNCTION(BlueprintImplementableEvent, Category = "Water")
    void OnDangerousWaterLevel();
    
    // Callback lorsqu'un acteur entre dans l'eau
    UFUNCTION()
    void OnWaterOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, 
                         UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, 
                         bool bFromSweep, const FHitResult& SweepResult);

public:
    // Paramètres d'apparence de l'eau
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water|Appearance")
    UStaticMesh* WaterMesh;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water|Appearance")
    UMaterialInterface* WaterMaterial;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water|Appearance")
    FLinearColor WaterColor = FLinearColor(0.1f, 0.5f, 0.8f, 0.8f);
    
    // Paramètres de comportement de l'eau
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water|Behavior")
    float InitialWaterLevel = -500.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water|Behavior")
    float MinWaterLevel = -1000.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water|Behavior")
    float MaxWaterLevel = 500.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water|Behavior")
    float WaterRiseSpeed = 20.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water|Behavior")
    float WaterLowerSpeed = 10.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water|Behavior")
    float DangerousWaterLevelThreshold = 100.0f;
    
    // Effets visuels et sonores
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water|Effects")
    USoundBase* WaterAmbientSound;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water|Effects")
    USoundBase* WaterWarningSound;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water|Effects")
    USoundBase* WaterKillSound;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water|Effects")
    UParticleSystem* SplashEffect;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water|Effects")
    UParticleSystem* RippleEffect;
    
    // Paramètres du post-process subaquatique
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water|Underwater")
    bool bEnableUnderwaterEffects = true;
    
protected:
    // Composants internes
    UPROPERTY()
    UStaticMeshComponent* WaterMeshComponent;
    
    UPROPERTY()
    UBoxComponent* WaterVolumeComponent;
    
    UPROPERTY()
    UAudioComponent* WaterAmbientSoundComponent;
    
    UPROPERTY()
    APostProcessVolume* UnderwaterPostProcess;
    
    // Variables d'état
    float CurrentWaterLevel;
    float TargetWaterLevel;
    bool bIsWaterRising;
    
    // Variables pour cycle automatique
    bool bAutomaticCycleActive;
    float CycleTimer;
    float CycleDuration;
    
    // Matériau dynamique pour effets d'eau
    UPROPERTY()
    UMaterialInstanceDynamic* WaterDynamicMaterial;
    
    // Méthodes internes
    void UpdateWaterMeshAndVolume();
    void ApplyUnderwaterEffects(AActor* Actor, bool bIsUnderwater);
    void SpawnWaterSplashEffect(const FVector& Location);
    void CheckForDangerousWaterLevel();
    void HandleCycleUpdate(float DeltaTime);
    void KillCharacterInWater(AActor* Character);
    
    // Récupérer tous les acteurs actuellement dans l'eau
    TArray<AActor*> GetActorsInWater() const;
};