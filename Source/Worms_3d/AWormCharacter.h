#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "WormWeapon.h"
#include "Net/UnrealNetwork.h"
#include "InputActionValue.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Camera/CameraComponent.h" // Include the header for UCameraComponent
#include "GameFramework/PlayerController.h"
#include "AWormCharacter.generated.h"


UENUM(BlueprintType)
enum class ECameraMode : uint8
{
    ThirdPerson UMETA(DisplayName = "Third Person"),
    FirstPerson UMETA(DisplayName = "First Person")
};

UCLASS()
class WORMS_3D_API AWormCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    AWormCharacter();

    UPROPERTY()
    UCameraComponent* TempCamera;
    
    UFUNCTION(BlueprintCallable, Category = "Camera")
    void SwitchToFirstPersonView();

    UFUNCTION(BlueprintCallable, Category = "Camera")
    void SwitchToThirdPersonView();

    UFUNCTION(BlueprintPure, Category = "Camera")
    bool IsViewingFromFirstPerson() const;

    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
    TSubclassOf<UUserWidget> AimingWidgetClass;

    UPROPERTY(BlueprintReadOnly, Category = "UI")
    UUserWidget* AimingWidget;

    // Paramètres de caméra
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    float DefaultCameraDistance;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    float MinCameraDistance;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    float MaxCameraDistance;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    float CameraZoomSpeed;

    UPROPERTY(BlueprintReadOnly, Category = "Camera")
    float CurrentCameraDistance;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    bool bUseFirstPersonViewWhenAiming;


    // Fonctions appelables depuis les Blueprints
    UFUNCTION(BlueprintCallable, Category = "Worm")
    void FireWeapon();
    
    UFUNCTION(BlueprintCallable, Category = "Worm")
    void SwitchWeapon(int32 WeaponIndex);
    
    UFUNCTION(BlueprintCallable, Category = "Worm")
    void SetIsMyTurn(bool bNewTurn);
    
    UFUNCTION(BlueprintPure, Category = "Worm")
    bool IsMyTurn() const { return bIsMyTurn; }
    
    UFUNCTION(BlueprintPure, Category = "Worm")
    float GetHealth() const { return Health; }
    
    UFUNCTION(BlueprintCallable, Category = "Worm")
    void ApplyDamageToWorm(float DamageAmount, FVector ImpactDirection);
        
    UFUNCTION(BlueprintCallable, Category = "Worm")
    void ApplyMovementImpulse(FVector Direction, float Strength);

    bool IsPendingKill() const;

    // Une fonction BlueprintNativeEvent pour permettre une réaction au changement de tour
    UFUNCTION(BlueprintNativeEvent, Category = "Game")
    void OnTurnChanged(bool bIsTurn);
    // Et ajoutez l'input action:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
    class UInputAction* AimAction;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
    class UInputAction* TestCameraAction;
    // Fonction Override
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void PossessedBy(AController* NewController) override;
    // Dans AWormCharacter.h, ajoutez :
    UFUNCTION(NetMulticast, Reliable)
    void SetAvailableWeapons(const TArray<TSubclassOf<AWormWeapon>>& WeaponTypes);

    UFUNCTION(BlueprintCallable, Category = "Debug")
    void DiagnoseWeapons();
    UFUNCTION(BlueprintCallable, Category = "Worm")
    void SetAiming(bool bIsAiming);

    // Composants de caméra
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class USpringArmComponent* CameraBoom;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class UCameraComponent* FollowCamera;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class UCameraComponent* FPSCamera;

    // Fonctions pour la caméra
    UFUNCTION(BlueprintCallable, Category = "Camera")
    void ZoomCamera(float Amount);

    UFUNCTION(BlueprintCallable, Category = "Camera")
    void ToggleCameraMode(bool bFirstPerson);

    // Propriétés pour les inputs
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
    class UInputAction* ZoomAction;

    // Fonctions pour les nouveaux inputs
    void OnZoomAction(const FInputActionValue& Value);
    // Liste des armes disponibles
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Worm")
    TArray<TSubclassOf<AWormWeapon>> AvailableWeapons;
    
    // Arme actuellement spawned
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Worm")
    AWormWeapon* CurrentWeapon;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", Replicated)
    USceneComponent* WeaponPivotComponent;
    
    // Fonction pour adjuster la puissance du tir
    UFUNCTION(BlueprintCallable, Category = "Worm")
    void AdjustPower(float Delta);
    // Ajoutez également cette fonction Multicast
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_UpdateWeaponRotation(FRotator NewRotation);

    UFUNCTION(Server, Reliable, WithValidation)
    void Server_UpdateWeaponRotation(FRotator NewRotation);
    // RPC Multicast pour propager la rotation aux clients

protected:
    void OnAimActionStarted(const FInputActionValue& Value);
    void OnAimActionEnded(const FInputActionValue& Value);
    void UpdateAimingWidget();
    void ValidateCameraComponents();


    // Système de tour
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Worm")
    bool bIsMyTurn;
    
    // Points de vie
    UPROPERTY(ReplicatedUsing = OnRep_Health, BlueprintReadOnly, Category = "Worm")
    float Health;
    
    // Arme actuellement équipée
    UPROPERTY(ReplicatedUsing = OnRep_CurrentWeaponIndex, BlueprintReadOnly, Category = "Worm")
    int32 CurrentWeaponIndex;
    // Fonction appelée lorsque CurrentWeaponIndex est répliqué
    UFUNCTION()
    void OnRep_CurrentWeaponIndex();
   
    //FireEffect for debugging
    UPROPERTY(EditDefaultsOnly, Category = "Effects")
    UParticleSystem* FireEffect;
    // Enhanced Input System - Propriétés
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
    class UInputMappingContext* InputMappingContext;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
    class UInputAction* MoveAction;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
    class UInputAction* JumpAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
    class UInputAction* FireAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
    class UInputAction* NextWeaponAction;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
    class UInputAction* PrevWeaponAction;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
    class UInputAction* LookAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
    class UInputAction* PowerUpAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
    class UInputAction* PowerDownAction;

    // Ajouter dans la section protected de AWormCharacter.h, avec les autres InputActions
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
    class UInputAction* EndTurnAction;
    // Enhanced Input System - Fonctions de callback
    void OnMoveAction(const FInputActionValue& Value);
    void OnJumpAction(const FInputActionValue& Value);
    void OnJumpActionReleased(const FInputActionValue& Value);
    void OnFireAction(const FInputActionValue& Value);
    void OnNextWeaponAction(const FInputActionValue& Value);
    void OnPrevWeaponAction(const FInputActionValue& Value);
    void OnLookAction(const FInputActionValue& Value);
    void OnEndTurnAction(const FInputActionValue& Value);
    void OnPowerUpAction(const FInputActionValue& Value);
    void OnPowerDownAction(const FInputActionValue& Value);
    // Timer pour gérer la fin automatique du tour
    FTimerHandle AutoEndTurnTimerHandle;
    
    // Fonction qui sera appelée quand le timer expire
    UFUNCTION()
    void OnAutoEndTurnTimerExpired();
    
    // Indicateur pour savoir si le timer de fin automatique est actif
    bool bAutoEndTurnTimerActive;
    // Socket pour attacher l'arme
    UPROPERTY(EditDefaultsOnly, Category = "Worm")
    FName WeaponSocketName;
    
    // Fonction pour limiter le mouvement quand ce n'est pas le tour du personnage
    UFUNCTION()
    void LimitMovementWhenNotMyTurn();
    
    // Fonctions de mouvement legacy (conservées pour référence)
    UFUNCTION()
    void MoveForward(float Value);
    
    UFUNCTION()
    void MoveRight(float Value);
    
    // Fonctions pour changer d'arme
    UFUNCTION()
    void NextWeapon();
    
    UFUNCTION()
    void PrevWeapon();
    
    // Consommation de points de mouvement
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Worm")
    float MovementPoints;
    
    UPROPERTY(EditDefaultsOnly, Category = "Worm")
    float MaxMovementPoints;
    
    // Fonction pour mesurer la distance parcourue et consommer des points
    UPROPERTY()
    FVector LastPosition;
    
    // RepNotify fonctions
    UFUNCTION()
    void OnRep_Health();
    
    // RPC pour tirer (Client -> Serveur)
    UFUNCTION(Server, Reliable, WithValidation)
    void Server_FireWeapon();
    
    // RPC pour changer d'arme (Client -> Serveur)
    UFUNCTION(Server, Reliable, WithValidation)
    void Server_SwitchWeapon(int32 WeaponIndex);
    
    // RPC pour appliquer une impulsion de mouvement (Serveur -> Tous)
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_ApplyImpulse(FVector Direction, float Strength);
    
    // Fonction pour spawner l'arme actuelle
    void SpawnCurrentWeapon();
    // RPC pour notifier du changement d'arme
    
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_WeaponChanged();
    // Animation de tir
    UPROPERTY(EditDefaultsOnly, Category = "Animations")
    UAnimMontage* FireMontage;
    
    // Animation de dégât
    UPROPERTY(EditDefaultsOnly, Category = "Animations")
    UAnimMontage* HitReactMontage;
    
    // Animation de mort
    UPROPERTY(EditDefaultsOnly, Category = "Animations")
    UAnimMontage* DeathMontage;
    
    // Délai entre chaque utilisation d'arme
    UPROPERTY(EditDefaultsOnly, Category = "Worm")
    float WeaponCooldown;
    
    // Timestamp de la dernière utilisation d'arme
    float LastWeaponUseTime;
    // Ajouter une fonction pour terminer le tour manuellement
    UFUNCTION(BlueprintCallable, Category = "Worm")
    void EndTurn();

    // Ajouter le RPC côté serveur
    UFUNCTION(Server, Reliable, WithValidation)
    void Server_EndTurn();
    // Initialisation du Enhanced Input System
    void SetupEnhancedInput(APlayerController* PlayerController);
    UPROPERTY(BlueprintReadOnly, Category = "Camera")
    ECameraMode CurrentCameraMode;

    // Ajoutez une fonction pour définir directement le mode de caméra
    UFUNCTION(BlueprintCallable, Category = "Camera")
    void SetCameraMode(ECameraMode NewMode);

    UFUNCTION(BlueprintCallable, Category = "Camera")
    void ForceToggleCamera();
    UPROPERTY()
    FVector OriginalCameraLocation;
    void OnTestCameraAction(const FInputActionValue& Value);
    

};