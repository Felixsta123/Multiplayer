#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "WormWeapon.h"
#include "Net/UnrealNetwork.h"
#include "InputActionValue.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "AWormCharacter.generated.h"

UCLASS()
class WORMS_3D_API AWormCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    AWormCharacter();

    // === COMPOSANTS DE CAMÉRA ===
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class USpringArmComponent* CameraBoom;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class UCameraComponent* FollowCamera;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class UCameraComponent* FPSCamera;

    // === SYSTÈME DE CAMÉRA ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    float DefaultCameraDistance = 400.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    float MinCameraDistance = 200.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    float MaxCameraDistance = 800.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    float CameraZoomSpeed = 20.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Camera")
    float CurrentCameraDistance;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    bool bUseFirstPersonViewWhenAiming = true;
    
    // Position sauvegardée de la caméra
    float SavedCameraDistance;
    
    UPROPERTY(EditDefaultsOnly, Category = "Camera")
    FName HeadSocketName;
    
    // Fonctions de caméra
    UFUNCTION(BlueprintCallable, Category = "Camera")
    void ToggleCameraMode(bool bUseFPSCamera);
    
    UFUNCTION(BlueprintCallable, Category = "Camera")
    void ZoomCamera(float Amount);

    // === INTERFACE UTILISATEUR ===
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
    TSubclassOf<UUserWidget> AimingWidgetClass;

    UPROPERTY(BlueprintReadOnly, Category = "UI")
    UUserWidget* AimingWidget;

    // === FONCTIONS DE JEU PRINCIPALES ===
    
    // Armes
    UFUNCTION(BlueprintCallable, Category = "Worm")
    void FireWeapon();
    
    UFUNCTION(BlueprintCallable, Category = "Worm")
    void SwitchWeapon(int32 WeaponIndex);

    // Rotation de l'arme par défaut en mode TPS
    UPROPERTY()
    FRotator DefaultWeaponRotation;
    
    // Système de tours
    UFUNCTION(BlueprintCallable, Category = "Worm")
    void SetIsMyTurn(bool bNewTurn);
    
    UFUNCTION(BlueprintPure, Category = "Worm")
    bool IsMyTurn() const { return bIsMyTurn; }
    
    // État du personnage
    UFUNCTION(BlueprintPure, Category = "Worm")
    float GetHealth() const { return Health; }
    
    // Gestion des dégâts et impulsions
    UFUNCTION(BlueprintCallable, Category = "Worm")
    void ApplyDamageToWorm(float DamageAmount, FVector ImpactDirection);
        
    UFUNCTION(BlueprintCallable, Category = "Worm")
    void ApplyMovementImpulse(FVector Direction, float Strength);

    // Système de visée
    UFUNCTION(BlueprintCallable, Category = "Worm")
    void SetAiming(bool bIsAiming);

    UFUNCTION(BlueprintCallable, Category = "Worm")
    void AdjustPower(float PowerLevel);

    bool IsPendingKill() const;

    // Événement de changement de tour
    UFUNCTION(BlueprintNativeEvent, Category = "Game")
    void OnTurnChanged(bool bIsTurn);

    // === FONCTIONS DE CYCLE DE VIE ===
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void PossessedBy(AController* NewController) override;

    // === SYSTÈME D'ARMES ===
    UFUNCTION(NetMulticast, Reliable)
    void SetAvailableWeapons(const TArray<TSubclassOf<AWormWeapon>>& WeaponTypes);

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Worm")
    TArray<TSubclassOf<AWormWeapon>> AvailableWeapons;
    
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Worm")
    AWormWeapon* CurrentWeapon;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", Replicated)
    USceneComponent* WeaponPivotComponent;

    // === RÉPLICATION RÉSEAU ===
    UFUNCTION(Server, Reliable, WithValidation)
    void Server_UpdateWeaponRotation(FRotator NewRotation);

    UFUNCTION(NetMulticast, Reliable)
    void Multicast_UpdateWeaponRotation(FRotator NewRotation);

    // === FONCTIONS DE DÉBOGAGE ===
    UFUNCTION(BlueprintCallable, Category = "Debug")
    void DiagnoseWeapons();
    
protected:
    // === ÉTAT DU PERSONNAGE ===
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Worm")
    bool bIsMyTurn;
    
    UPROPERTY(ReplicatedUsing = OnRep_Health, BlueprintReadOnly, Category = "Worm")
    float Health;
    
    UPROPERTY(ReplicatedUsing = OnRep_CurrentWeaponIndex, BlueprintReadOnly, Category = "Worm")
    int32 CurrentWeaponIndex;
    
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Worm")
    float MovementPoints;
    
    UPROPERTY(EditDefaultsOnly, Category = "Worm")
    float MaxMovementPoints;
    
    UPROPERTY()
    FVector LastPosition;

    UPROPERTY()
    bool bIsInFirstPersonMode;

    // === CONFIGURATION DES ARMES ===
    UPROPERTY(EditDefaultsOnly, Category = "Worm")
    FName WeaponSocketName;
    
    UPROPERTY(EditDefaultsOnly, Category = "Worm")
    float WeaponCooldown;
    
    float LastWeaponUseTime;

    // === SYSTÈME DE TOUR DE JEU === 
    FTimerHandle AutoEndTurnTimerHandle;
    bool bAutoEndTurnTimerActive;
    
    UFUNCTION()
    void OnAutoEndTurnTimerExpired();

    // === ANIMATIONS ET EFFETS ===
    UPROPERTY(EditDefaultsOnly, Category = "Effects")
    UParticleSystem* FireEffect;
    
    UPROPERTY(EditDefaultsOnly, Category = "Animations")
    UAnimMontage* FireMontage;
    
    UPROPERTY(EditDefaultsOnly, Category = "Animations")
    UAnimMontage* HitReactMontage;
    
    UPROPERTY(EditDefaultsOnly, Category = "Animations")
    UAnimMontage* DeathMontage;

    // === ENHANCED INPUT SYSTEM ===
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

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
    class UInputAction* EndTurnAction;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
    class UInputAction* AimAction;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
    class UInputAction* TestCameraAction;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
    class UInputAction* ZoomAction;

    // === HANDLERS D'ENTRÉE ===
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
    void OnZoomAction(const FInputActionValue& Value);
    void OnAimActionStarted(const FInputActionValue& Value);
    void OnAimActionEnded(const FInputActionValue& Value);

    // === FONCTIONS DE MOUVEMENT ===
    void MoveForward(float Value);
    void MoveRight(float Value);
    void LimitMovementWhenNotMyTurn();
    
    // === FONCTIONS LIÉES AUX ARMES ===
    void NextWeapon();
    void PrevWeapon();
    void SpawnCurrentWeapon();
    void UpdateAimingWidget();
    
    // === CALLBACKS DE RÉPLICATION ===
    UFUNCTION()
    void OnRep_CurrentWeaponIndex();
    
    UFUNCTION()
    void OnRep_Health();
    
    // === FONCTIONS RPC ===
    UFUNCTION(Server, Reliable, WithValidation)
    void Server_FireWeapon();
    
    UFUNCTION(Server, Reliable, WithValidation)
    void Server_SwitchWeapon(int32 WeaponIndex);
    
    UFUNCTION(Server, Reliable, WithValidation)
    void Server_EndTurn();
    
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_ApplyImpulse(FVector Direction, float Strength);
    
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_WeaponChanged();

    // === FONCTIONS UTILITAIRES ===
    void SetupEnhancedInput(APlayerController* PlayerController);
    void SetupLegacyInputBindings(UInputComponent* PlayerInputComponent);
    void EndTurn();
    void CreateWeaponPivot();
    FTransform CalculateWeaponSpawnTransform();
    void InitializeCameraSystem();
    void SetupWeaponDiagnostic();
    void UpdateWeaponRotation();
    void UpdateMovementPoints();
};