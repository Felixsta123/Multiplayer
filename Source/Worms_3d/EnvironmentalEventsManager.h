#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WaterSystem.h"
#include "EnvironmentalEventsManager.generated.h"




// Types d'événements environnementaux
UENUM(BlueprintType, meta = (Bitflags))
enum class EEventType : uint8
{
    None        = 0 UMETA(DisplayName = "None"),
    Water       = 1 << 0 UMETA(DisplayName = "Rising Water"),
    Earthquake  = 1 << 1 UMETA(DisplayName = "Earthquake"),
    // Possibilité d'ajouter d'autres types d'événements à l'avenir:
    // Storm      = 1 << 2 UMETA(DisplayName = "Storm"),
    // Meteor     = 1 << 3 UMETA(DisplayName = "Meteor Shower"),
    // Volcano    = 1 << 4 UMETA(DisplayName = "Volcanic Eruption"),
};
ENUM_CLASS_FLAGS(EEventType);

// Structure pour stocker les données d'événement
USTRUCT(BlueprintType)
struct FEnvironmentalEventData
{
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event")
    EEventType EventType = EEventType::None;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event")
    float Intensity = 1.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event")
    FVector Location = FVector::ZeroVector;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event")
    FString Description = TEXT("");
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event")
    float Duration = 5.0f;
    
    FEnvironmentalEventData() {}
    
    FEnvironmentalEventData(EEventType InType, float InIntensity, FVector InLocation, FString InDesc, float InDuration)
        : EventType(InType), Intensity(InIntensity), Location(InLocation), Description(InDesc), Duration(InDuration)
    {}
};

/**
 * Gestionnaire d'événements environnementaux qui s'intègre au système de tours
 * Permet de déclencher divers événements (eau, tremblements de terre, etc.)
 */
UCLASS()
class WORMS_3D_API AEnvironmentalEventsManager : public AActor
{
    GENERATED_BODY()
    
public:    
    AEnvironmentalEventsManager();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // ===== PROPRIÉTÉS GÉNÉRALES =====
    UWaterSystem* WaterSystem;

    UPROPERTY(Replicated, EditDefaultsOnly, BlueprintReadOnly, Category = "Events")
    EEventType ActiveEventTypes;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Events")
    bool bEnableRandomEvents = true;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Events")
    int32 MinTurnsBetweenEvents = 1;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Events")
    int32 MaxTurnsBetweenEvents = 3;
    
    // ===== PROPRIÉTÉS DE L'EAU =====
    UPROPERTY(Replicated, EditDefaultsOnly, BlueprintReadOnly, Category = "Events|Water")
    bool bEnableWaterEvents = true;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Events|Water")
    bool bIsWaterRisingActive;

    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Events|Water")
    bool bRiseAfterEachTurn = true;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Events|Water")
    bool bWaterRiseFaster = false;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Events|Water")
    float WaterRisePerTurn = 30.0f;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Events|Water")
    float WaterRiseInterval = 60.0f;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Events|Water")
    float InitialWaterDelay = 120.0f;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Events|Water")
    float WaterEventProbability = 100.0f;

    // ===== PROPRIÉTÉS DU TREMBLEMENT DE TERRE =====
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Events|Earthquake")
    bool bEnableEarthquakes = false;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Events|Earthquake")
    float EarthquakeIntensityMin = 0.5f;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Events|Earthquake")
    float EarthquakeIntensityMax = 3.0f;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Events|Earthquake")
    float EarthquakeDuration = 5.0f;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Events|Earthquake")
    float EarthquakeInterval = 180.0f;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Events|Earthquake")
    float InitialEarthquakeDelay = 300.0f;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Events|Earthquake")
    float EarthquakeEventProbability = 0.0f;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Events|Earthquake")
    UParticleSystem* EarthquakeParticleEffect;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Events|Earthquake")
    USoundBase* EarthquakeSound;

    // ===== ACTIONS PUBLIQUES =====
    
    // --- Actions générales ---
    UFUNCTION(BlueprintCallable, Category = "Events")
    void TriggerRandomEvent();
    
    UFUNCTION(BlueprintCallable, Category = "Events")
    void SetActiveEventTypes(EEventType NewActiveTypes);
    
    // --- Eau ---
    UFUNCTION(BlueprintCallable, Category = "Events|Water")
    void StartWaterRising();
    
    UFUNCTION(BlueprintCallable, Category = "Events|Water")
    void StopWaterRising();
    
    UFUNCTION(BlueprintCallable, Category = "Events|Water")
    void RaiseWater(float Amount = 0.0f);
    
    UFUNCTION(BlueprintCallable, Category = "Events|Water")
    void LowerWater(float Amount = 0.0f);

    // --- Tremblement de terre ---
    UFUNCTION(BlueprintCallable, Category = "Events|Earthquake")
    void TriggerEarthquake(float Intensity = 1.0f, FVector Location = FVector::ZeroVector);
    
    UFUNCTION(BlueprintCallable, Category = "Events|Earthquake")
    void TriggerRandomEarthquake();
    
    // ===== ÉVÉNEMENTS DÉCLENCHÉS =====
    UFUNCTION(BlueprintImplementableEvent, Category = "Events")
    void OnEventTriggered(const FEnvironmentalEventData& EventData);
    
    UFUNCTION(BlueprintImplementableEvent, Category = "Events|Water")
    void OnWaterStartsRising();
    
    UFUNCTION(BlueprintImplementableEvent, Category = "Events|Water")
    void OnWaterRisesTooHigh();
    
    UFUNCTION(BlueprintImplementableEvent, Category = "Events|Earthquake")
    void OnEarthquakeStart(float Intensity, FVector Location);
    
    UFUNCTION(BlueprintImplementableEvent, Category = "Events|Earthquake")
    void OnEarthquakeEnd();
    
    // ===== LIAISON AVEC LE SYSTÈME DE TOURS =====
    UFUNCTION(BlueprintNativeEvent, Category = "Water")
    void NotifyTurnEnded();
    
    // ===== OBTENTION D'UNE RÉFÉRENCE =====
    UFUNCTION(BlueprintCallable, Category = "Events", meta = (WorldContext = "WorldContextObject"))
    static AEnvironmentalEventsManager* GetEventsManager(const UObject* WorldContextObject);


    
protected:
    // ===== COMPOSANTS =====
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    
    // ===== VARIABLES INTERNES =====
    
    // --- Générales ---
    int32 TurnCounter;
    int32 TurnsUntilNextEvent;
    
    // --- Eau ---
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Events|Water")
    float TimeUntilNextWaterEvent;

    int32 TurnsUntilNextWaterRise;
    
    // --- Tremblement de terre ---
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Events|Earthquake")
    bool bIsEarthquakeActive;

    float TimeUntilNextEarthquake;
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Events|Earthquake")
    float CurrentEarthquakeIntensity;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Events|Earthquake")
    FVector CurrentEarthquakeLocation;
    
    // ===== TIMERS =====
    FTimerHandle WaterEventTimerHandle;
    FTimerHandle EarthquakeTimerHandle;
    FTimerHandle EarthquakeDurationTimerHandle;
    
    // ===== FONCTIONS INTERNES =====
    void InitializeWaterLevel();
    void PeriodicWaterRise();
    void CheckForDangerousWaterLevel();
    void UpdateUI();
    
    void ProcessEarthquake(float Intensity, FVector Location);
    void EndEarthquake();
    void ApplyEarthquakeEffectToBuildings(float Intensity, FVector Location);
    void ShakeCamera(float Intensity);
    
    void ChooseNextEvent();
    EEventType SelectRandomEventType();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // ===== FONCTIONS BLUEPRINT =====
    UFUNCTION(BlueprintImplementableEvent, Category = "Events")
    void CreateEnvironmentalEventIndicators();
    
    // ===== UTILITAIRES =====
    float CalculateAverageTerrainHeight() const;
    float GetRandomValueBetween(float Min, float Max) const;
    bool ShouldTriggerEventWithProbability(float Probability) const;
};