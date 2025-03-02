#include "AWormCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "WormGameMode.h"
#include "WormWeapon.h"
#include "Blueprint/UserWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/SceneComponent.h"

// Conserve le constructeur existant sans modifications mais améliore la lisibilité
AWormCharacter::AWormCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    
    // Configuration réseau
    bReplicates = true;
    
    // Valeurs par défaut - Propriétés générales
    Health = 100.0f;
    bIsMyTurn = false;
    CurrentWeaponIndex = 0;
    WeaponSocketName = "WeaponSocket";
    bAutoEndTurnTimerActive = false;
    HeadSocketName = "head";
    
    // Valeurs de mouvement et combat
    MaxMovementPoints = 100.0f;
    MovementPoints = MaxMovementPoints;
    WeaponCooldown = 0.5f;
    LastWeaponUseTime = 0.0f;
    
    // Initialiser la rotation par défaut de l'arme
    DefaultWeaponRotation = FRotator::ZeroRotator;

    // Configuration du Character Movement Component
    GetCharacterMovement()->GravityScale = 1.5f;
    GetCharacterMovement()->AirControl = 0.8f;
    GetCharacterMovement()->JumpZVelocity = 600.0f;
    GetCharacterMovement()->BrakingDecelerationWalking = 2000.0f;
    
    // Configuration du système de caméra
    InitializeCameraSystem();
}

void AWormCharacter::InitializeCameraSystem()
{
    // Configuration du CameraBoom (spring arm)
    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    if (CameraBoom)
    {
        CameraBoom->SetupAttachment(RootComponent);
        CameraBoom->TargetArmLength = DefaultCameraDistance;
        CameraBoom->bUsePawnControlRotation = true;
        CameraBoom->bEnableCameraLag = true;
        CameraBoom->CameraLagSpeed = 3.0f;
        CameraBoom->bEnableCameraRotationLag = true;
        CameraBoom->CameraRotationLagSpeed = 3.0f;
    }

    // Configuration de la caméra TPS
    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    if (FollowCamera)
    {
        FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
        FollowCamera->bUsePawnControlRotation = false;
    }
    
    // Configuration de la caméra FPS
    FPSCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FPSCamera"));
    if (FPSCamera && GetMesh())
    {
        FPSCamera->SetupAttachment(GetMesh(), HeadSocketName);
        FPSCamera->bUsePawnControlRotation = true;
        FPSCamera->SetActive(false); // Désactivée par défaut
    }
    
    // Valeurs par défaut pour la caméra
    bIsInFirstPersonMode = false;
    bUseFirstPersonViewWhenAiming = true;
}

void AWormCharacter::BeginPlay()
{
    Super::BeginPlay();
    
    // Initialisation des valeurs par défaut
    LastPosition = GetActorLocation();
    LastWeaponUseTime = UGameplayStatics::GetTimeSeconds(this) - (WeaponCooldown * 2);
    
    // Initialiser la rotation par défaut de l'arme (alignée avec la direction du personnage)
    // Utiliser une rotation qui pointe "vers l'avant" du personnage
    DefaultWeaponRotation = GetActorRotation();
    
    // S'assurer que l'arme est parfaitement horizontale en mode TPS
    DefaultWeaponRotation.Pitch = 0.0f;
    DefaultWeaponRotation.Roll = 0.0f;
    
    // Configuration initiale de la caméra
    if (CameraBoom)
    {
        CurrentCameraDistance = DefaultCameraDistance;
        CameraBoom->TargetArmLength = CurrentCameraDistance;
    }
    
    // Configuration de l'input pour le joueur local
    if (IsLocallyControlled())
    {
        // Setup Enhanced Input
        if (APlayerController* PC = Cast<APlayerController>(GetController()))
        {
            SetupEnhancedInput(PC);
        }
        
        // Diagnostic d'armes pour client
        SetupWeaponDiagnostic();
    }
}

void AWormCharacter::SetupWeaponDiagnostic()
{
    // Ne configurer que pour les clients
    if (!HasAuthority())
    {
        FTimerHandle DiagnosticTimerHandle;
        GetWorld()->GetTimerManager().SetTimer(
            DiagnosticTimerHandle,
            [this]() {
                static int32 DiagnosticCount = 0;
                if (DiagnosticCount < 5) // Limite à 5 diagnostics
                {
                    DiagnoseWeapons();
                    DiagnosticCount++;
                    
                    // Force la création d'arme si nécessaire après 3 tentatives
                    if (DiagnosticCount >= 3 && !CurrentWeapon && AvailableWeapons.Num() > 0)
                    {
                        OnRep_CurrentWeaponIndex();
                    }
                }
            },
            2.0f,  // Premier diagnostic après 2 secondes
            true,  // Répéter
            1.0f   // Puis toutes les secondes
        );
    }
}

void AWormCharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);
    
    // Configure l'Enhanced Input lors de la possession par un controlleur
    if (IsLocallyControlled())
    {
        if (APlayerController* PC = Cast<APlayerController>(NewController))
        {
            SetupEnhancedInput(PC);
        }
    }
}

void AWormCharacter::SetupEnhancedInput(APlayerController* PlayerController)
{
    if (!PlayerController || !InputMappingContext)
    {
        return;
    }
    
    if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
    {
        // Clear existing mappings and add our mapping context
        Subsystem->ClearAllMappings();
        Subsystem->AddMappingContext(InputMappingContext, 0);
    }
}

void AWormCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    
    if (UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(PlayerInputComponent))
    {
        // Binding pour le mouvement
        if (MoveAction)
            EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AWormCharacter::OnMoveAction);
            
        // Binding pour le saut
        if (JumpAction)
        {
            EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Triggered, this, &AWormCharacter::OnJumpAction);
            EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &AWormCharacter::OnJumpActionReleased);
        }
        
        // Binding pour le zoom
        if (ZoomAction)
            EnhancedInputComponent->BindAction(ZoomAction, ETriggerEvent::Triggered, this, &AWormCharacter::OnZoomAction);
            
        // Binding pour le tir
        if (FireAction)
            EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Triggered, this, &AWormCharacter::OnFireAction);
            
        // Binding pour le système de puissance
        if (PowerUpAction)
            EnhancedInputComponent->BindAction(PowerUpAction, ETriggerEvent::Triggered, this, &AWormCharacter::OnPowerUpAction);
            
        if (PowerDownAction)
            EnhancedInputComponent->BindAction(PowerDownAction, ETriggerEvent::Triggered, this, &AWormCharacter::OnPowerDownAction);
            
        // Binding pour changer d'arme
        if (NextWeaponAction)
            EnhancedInputComponent->BindAction(NextWeaponAction, ETriggerEvent::Triggered, this, &AWormCharacter::OnNextWeaponAction);
            
        if (PrevWeaponAction)
            EnhancedInputComponent->BindAction(PrevWeaponAction, ETriggerEvent::Triggered, this, &AWormCharacter::OnPrevWeaponAction);
            
        // Binding pour la visée et l'orientation
        if (LookAction)
            EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AWormCharacter::OnLookAction);
            
        if (EndTurnAction)
            EnhancedInputComponent->BindAction(EndTurnAction, ETriggerEvent::Triggered, this, &AWormCharacter::OnEndTurnAction);
            
        if (AimAction)
        {
            EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Started, this, &AWormCharacter::OnAimActionStarted);
            EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Completed, this, &AWormCharacter::OnAimActionEnded);
        }
    }
    else
    {
        // Fallback à l'input legacy - moins de logs et simplification de la structure
        SetupLegacyInputBindings(PlayerInputComponent);
    }
}

void AWormCharacter::SetupLegacyInputBindings(UInputComponent* PlayerInputComponent)
{
    // Fallback au système d'input legacy
    PlayerInputComponent->BindAxis("MoveForward", this, &AWormCharacter::MoveForward);
    PlayerInputComponent->BindAxis("MoveRight", this, &AWormCharacter::MoveRight);
    
    PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
    PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);
    
    PlayerInputComponent->BindAction("Fire", IE_Pressed, this, &AWormCharacter::FireWeapon);
    
    PlayerInputComponent->BindAction("NextWeapon", IE_Pressed, this, &AWormCharacter::NextWeapon);
    PlayerInputComponent->BindAction("PrevWeapon", IE_Pressed, this, &AWormCharacter::PrevWeapon);

    PlayerInputComponent->BindAction("EndTurn", IE_Pressed, this, &AWormCharacter::EndTurn);
}

// Handlers d'inputs améliorés
void AWormCharacter::OnMoveAction(const FInputActionValue& Value)
{
    // Ne rien faire si ce n'est pas notre tour ou si on n'a plus de points de mouvement
    if (!bIsMyTurn || MovementPoints <= 0 || !Controller)
    {
        return;
    }
    
    // Get movement vector from the Input Action
    FVector2D MovementVector = Value.Get<FVector2D>();
    
    if (MovementVector.SizeSquared() > 0.0f)
    {
        // Find orientation
        const FRotator Rotation = Controller->GetControlRotation();
        const FRotator YawRotation(0, Rotation.Yaw, 0);
        
        // Forward/Backward direction
        const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
        
        // Right/Left direction
        const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
        
        // Add movement in those directions
        AddMovementInput(ForwardDirection, MovementVector.Y);
        AddMovementInput(RightDirection, MovementVector.X);
    }
}

void AWormCharacter::OnZoomAction(const FInputActionValue& Value)
{
    float ZoomValue = Value.Get<float>();
    ZoomCamera(ZoomValue);
}

void AWormCharacter::ZoomCamera(float Amount)
{
    if (CameraBoom && !bIsInFirstPersonMode) // Ne zoomer qu'en mode TPS
    {
        // Calcul et application du zoom comme actuellement
        CurrentCameraDistance = FMath::Clamp(
            CurrentCameraDistance - (Amount * CameraZoomSpeed),
            MinCameraDistance,
            MaxCameraDistance
        );
        
        CameraBoom->TargetArmLength = CurrentCameraDistance;
    }
}

void AWormCharacter::OnLookAction(const FInputActionValue& Value)
{
    FVector2D LookAxisVector = Value.Get<FVector2D>();

    if (Controller != nullptr)
    {
        // Add yaw and pitch input to the controller
        AddControllerYawInput(LookAxisVector.X);
        AddControllerPitchInput(LookAxisVector.Y);
    }
}

void AWormCharacter::OnJumpAction(const FInputActionValue& Value)
{
    // Ne rien faire si ce n'est pas notre tour ou si on n'a plus de points de mouvement
    if (bIsMyTurn && MovementPoints > 0)
    {
        Jump();
    }
}

void AWormCharacter::OnJumpActionReleased(const FInputActionValue& Value)
{
    StopJumping();
}

void AWormCharacter::OnFireAction(const FInputActionValue& Value)
{
    FireWeapon();
}

void AWormCharacter::OnNextWeaponAction(const FInputActionValue& Value)
{
    NextWeapon();
}

void AWormCharacter::OnPrevWeaponAction(const FInputActionValue& Value)
{
    PrevWeapon();
}

void AWormCharacter::OnEndTurnAction(const FInputActionValue& Value)
{
    EndTurn();
}

void AWormCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Mise à jour de la rotation de l'arme
    UpdateWeaponRotation();
    
    // Gestion du mouvement et des points de mouvement
    if (bIsMyTurn && HasAuthority())
    {
        UpdateMovementPoints();
    }
    
    // Interface utilisateur de visée
    if (AimingWidget && CurrentWeapon)
    {
        UpdateAimingWidget();
    }
    
    // Limiter le mouvement quand ce n'est pas le tour du personnage
    LimitMovementWhenNotMyTurn();
}

void AWormCharacter::UpdateWeaponRotation()
{
    // Mise à jour de l'arme seulement si on est contrôlé localement et qu'on a une arme
    if (IsLocallyControlled() && WeaponPivotComponent && CurrentWeapon)
    {
        FRotator NewWeaponRotation;
        
        // En mode FPS: l'arme suit la rotation de la caméra
        // En mode TPS: l'arme conserve une orientation fixe par rapport au personnage
        if (bIsInFirstPersonMode)
        {
            // En mode FPS, permettre la rotation complète (y compris la hauteur)
            FRotator ControlRotation = GetControlRotation();
            NewWeaponRotation = FRotator(ControlRotation.Pitch, ControlRotation.Yaw, 0.0f);
            
            // Appliquer la nouvelle rotation
            WeaponPivotComponent->SetWorldRotation(NewWeaponRotation);
        
            // Envoyer au serveur si c'est un client et que c'est notre tour
            if (GetLocalRole() < ROLE_Authority && bIsMyTurn)
            {
                // Envoyer moins souvent pour réduire le trafic réseau
                static int32 FrameCounter = 0;
                if (++FrameCounter >= 5)
                {
                    // Envoyer la rotation calculée
                    Server_UpdateWeaponRotation(NewWeaponRotation);
                    FrameCounter = 0;
                }
            }
        }
        else
        {
            // En mode TPS, l'arme reste fixe par rapport au personnage
            // On n'applique aucune mise à jour de rotation ici
            // L'arme conserve sa rotation DefaultWeaponRotation définie à l'initialisation 
            // ou lors du passage de FPS à TPS
        }
    }
}

void AWormCharacter::UpdateMovementPoints()
{
    FVector CurrentPosition = GetActorLocation();
    float DistanceMoved = FVector::Dist2D(LastPosition, CurrentPosition);
    
    if (DistanceMoved > 0)
    {
        // Consommer les points de mouvement
        float PointsToConsume = DistanceMoved * 0.1f;
        float PreviousMovementPoints = MovementPoints;
        MovementPoints = FMath::Max(0.0f, MovementPoints - PointsToConsume);
        
        // Limiter le mouvement si tous les points sont consommés
        if (MovementPoints <= 0 && PreviousMovementPoints > 0)
        {
            GetCharacterMovement()->MaxWalkSpeed = 0;
            
            // Démarrer le timer de fin automatique de tour si les points sont épuisés
            if (!bAutoEndTurnTimerActive)
            {
                GetWorldTimerManager().SetTimer(
                    AutoEndTurnTimerHandle, 
                    this, 
                    &AWormCharacter::OnAutoEndTurnTimerExpired, 
                    3.0f, 
                    false
                );
                bAutoEndTurnTimerActive = true;
            }
        }
    }
    
    LastPosition = CurrentPosition;
}

void AWormCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    
    // Répliquer les variables essentielles
    DOREPLIFETIME(AWormCharacter, bIsMyTurn);
    DOREPLIFETIME(AWormCharacter, Health);
    DOREPLIFETIME(AWormCharacter, CurrentWeaponIndex);
    DOREPLIFETIME(AWormCharacter, CurrentWeapon);
    DOREPLIFETIME(AWormCharacter, MovementPoints);
    DOREPLIFETIME(AWormCharacter, AvailableWeapons);
    DOREPLIFETIME(AWormCharacter, WeaponPivotComponent);
}

// Fonctions de mouvement legacy
void AWormCharacter::MoveForward(float Value)
{
    if ((Controller != nullptr) && (Value != 0.0f) && bIsMyTurn && MovementPoints > 0)
    {
        const FRotator Rotation = Controller->GetControlRotation();
        const FRotator YawRotation(0, Rotation.Yaw, 0);
        const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
        AddMovementInput(Direction, Value);
    }
}

void AWormCharacter::MoveRight(float Value)
{
    if ((Controller != nullptr) && (Value != 0.0f) && bIsMyTurn && MovementPoints > 0)
    {
        const FRotator Rotation = Controller->GetControlRotation();
        const FRotator YawRotation(0, Rotation.Yaw, 0);
        const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
        AddMovementInput(Direction, Value);
    }
}

void AWormCharacter::NextWeapon()
{
    if (AvailableWeapons.Num() > 1)
    {
        int32 NewIndex = (CurrentWeaponIndex + 1) % AvailableWeapons.Num();
        SwitchWeapon(NewIndex);
    }
}

void AWormCharacter::PrevWeapon()
{
    if (AvailableWeapons.Num() > 1)
    {
        int32 NewIndex = (CurrentWeaponIndex - 1 + AvailableWeapons.Num()) % AvailableWeapons.Num();
        SwitchWeapon(NewIndex);
    }
}

void AWormCharacter::LimitMovementWhenNotMyTurn()
{
    if (!bIsMyTurn)
    {
        GetCharacterMovement()->Velocity = FVector::ZeroVector;
        GetCharacterMovement()->MaxWalkSpeed = 0;
    }
    else
    {
        if (MovementPoints > 0)
        {
            GetCharacterMovement()->MaxWalkSpeed = 600.0f;
        }
    }
}

void AWormCharacter::FireWeapon()
{
    float CurrentTime = UGameplayStatics::GetTimeSeconds(this);
    
    // Vérifier si les conditions sont réunies pour tirer
    if (bIsMyTurn && CurrentWeapon && (CurrentTime - LastWeaponUseTime >= WeaponCooldown))
    {
        // Cacher la trajectoire AVANT de tirer
        if (CurrentWeapon)
        {
            CurrentWeapon->ShowTrajectory(false);
        }
        
        // Mettre à jour le timestamp
        LastWeaponUseTime = CurrentTime;
        
        // Appeler le RPC serveur si on est sur un client
        if (GetLocalRole() < ROLE_Authority)
        {
            Server_FireWeapon();
        }
        else
        {
            // Sur le serveur, appeler directement
            CurrentWeapon->Fire();
            
            // Effets visuels
            if (FireEffect)
            {
                UGameplayStatics::SpawnEmitterAtLocation(
                    GetWorld(),
                    FireEffect,
                    GetActorLocation() + FVector(0, 0, 100),
                    FRotator::ZeroRotator,
                    FVector(3, 3, 3)
                );
            }
            
            // Jouer l'animation de tir
            if (FireMontage)
            {
                PlayAnimMontage(FireMontage);
            }
        }
    }
}

bool AWormCharacter::Server_FireWeapon_Validate()
{
    return true;
}

void AWormCharacter::Server_FireWeapon_Implementation()
{
    FireWeapon();
}

void AWormCharacter::SwitchWeapon(int32 WeaponIndex)
{
    if (WeaponIndex >= 0 && WeaponIndex < AvailableWeapons.Num())
    {
        if (GetLocalRole() < ROLE_Authority)
        {
            Server_SwitchWeapon(WeaponIndex);
        }
        else
        {
            CurrentWeaponIndex = WeaponIndex;
            
            // Détruire l'arme actuelle si elle existe
            if (CurrentWeapon)
            {
                CurrentWeapon->Destroy();
                CurrentWeapon = nullptr;
            }
            
            // Spawner la nouvelle arme
            SpawnCurrentWeapon();
        }
    }
}

bool AWormCharacter::Server_SwitchWeapon_Validate(int32 WeaponIndex)
{
    return WeaponIndex >= 0 && WeaponIndex < AvailableWeapons.Num();
}

void AWormCharacter::Server_SwitchWeapon_Implementation(int32 WeaponIndex)
{
    SwitchWeapon(WeaponIndex);
}

void AWormCharacter::SpawnCurrentWeapon()
{
    // Cette fonction ne devrait s'exécuter que sur le serveur
    if (!HasAuthority())
    {
        return;
    }
    
    // Vérifier que l'index est valide
    if (!AvailableWeapons.IsValidIndex(CurrentWeaponIndex))
    {
        return;
    }
    
    // Nettoyage de l'arme existante
    if (CurrentWeapon)
    {
        CurrentWeapon->Destroy();
        CurrentWeapon = nullptr;
    }
    
    // Créer le pivot de l'arme
    CreateWeaponPivot();
    
    // Calculer la transformation de spawn
    FTransform SpawnTransform = CalculateWeaponSpawnTransform();
    
    // Paramètres de spawn
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.Instigator = GetInstigator();
    
    // Spawner l'arme
    CurrentWeapon = GetWorld()->SpawnActor<AWormWeapon>(
        AvailableWeapons[CurrentWeaponIndex], 
        SpawnTransform, 
        SpawnParams
    );
    
    if (CurrentWeapon)
    {
        // Attacher l'arme au pivot
        CurrentWeapon->AttachToComponent(WeaponPivotComponent, 
            FAttachmentTransformRules::SnapToTargetNotIncludingScale);
        
        CurrentWeapon->SetOwner(this);
    }
}

void AWormCharacter::ApplyDamageToWorm(float DamageAmount, FVector ImpactDirection)
{
    if (HasAuthority())
    {
        // Appliquer les dégâts
        Health = FMath::Max(0.0f, Health - DamageAmount);
        
        // Appliquer une impulsion basée sur la direction d'impact
        ApplyMovementImpulse(ImpactDirection.GetSafeNormal(), DamageAmount * 10.0f);
        
        // Vérifier si le personnage est mort
        if (Health <= 0)
        {
            // Désactiver les collisions et le mouvement
            GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            GetCharacterMovement()->DisableMovement();
        }
    }
}

void AWormCharacter::ApplyMovementImpulse(FVector Direction, float Strength)
{
    if (HasAuthority())
    {
        // Appliquer l'impulsion au mouvement
        GetCharacterMovement()->AddImpulse(Direction * Strength, true);
        
        // Multicast RPC pour la synchronisation visuelle
        Multicast_ApplyImpulse(Direction, Strength);
    }
}

void AWormCharacter::Multicast_ApplyImpulse_Implementation(FVector Direction, float Strength)
{
    // Cette fonction est appelée sur tous les clients et le serveur
    if (!HasAuthority())
    {
        // Appliquer uniquement l'effet visuel sur les clients
        GetCharacterMovement()->AddImpulse(Direction * Strength, true);
    }
}

void AWormCharacter::SetIsMyTurn(bool bNewTurn)
{
    // Cette fonction ne devrait être appelée que par le serveur
    if (HasAuthority())
    {
        // Stocker l'ancien état pour détecter les changements
        bool bOldTurn = bIsMyTurn;
        
        // Si le tour se termine, annuler le timer d'auto-fin de tour
        if (bOldTurn && !bNewTurn && bAutoEndTurnTimerActive)
        {
            GetWorldTimerManager().ClearTimer(AutoEndTurnTimerHandle);
            bAutoEndTurnTimerActive = false;
        }
        
        // Mettre à jour l'état
        bIsMyTurn = bNewTurn;
        
        if (bIsMyTurn)
        {
            // Réinitialiser les points de mouvement au début du tour
            MovementPoints = MaxMovementPoints;
            
            // Réinitialiser la vitesse de marche
            GetCharacterMovement()->MaxWalkSpeed = 600.0f;
            
            // Réinitialiser la position pour le calcul de la distance
            LastPosition = GetActorLocation();
        }
        else
        {
            // Arrêter le mouvement à la fin du tour
            GetCharacterMovement()->Velocity = FVector::ZeroVector;
            GetCharacterMovement()->MaxWalkSpeed = 0;
        }
        
        // Appeler l'événement BlueprintNativeEvent seulement si l'état a changé
        if (bOldTurn != bIsMyTurn)
        {
            OnTurnChanged(bIsMyTurn);
        }
    }
}

void AWormCharacter::OnTurnChanged_Implementation(bool bIsTurn)
{
    // Cette fonction peut être surchargée en Blueprint
    // Le code C++ par défaut est minimal
}

bool AWormCharacter::IsPendingKill() const
{
    return IsPendingKillPending();
}

void AWormCharacter::OnAutoEndTurnTimerExpired()
{
    // Cette fonction est appelée après le délai de 3 secondes
    if (HasAuthority() && bIsMyTurn)
    {
        // Trouver le GameMode pour terminer le tour
        AGameModeBase* GameMode = GetWorld()->GetAuthGameMode();
        AWormGameMode* WormGameMode = Cast<AWormGameMode>(GameMode);
        
        if (WormGameMode)
        {
            WormGameMode->EndCurrentTurn();
        }
    }
    
    bAutoEndTurnTimerActive = false;
}

void AWormCharacter::EndTurn()
{
    // Vérifier si c'est notre tour
    if (bIsMyTurn)
    {
        // Appeler le RPC serveur si on est sur un client
        if (GetLocalRole() < ROLE_Authority)
        {
            Server_EndTurn();
        }
        else
        {
            // Sur le serveur, terminer le tour directement via le GameMode
            AGameModeBase* GameMode = GetWorld()->GetAuthGameMode();
            AWormGameMode* WormGameMode = Cast<AWormGameMode>(GameMode);
            
            if (WormGameMode)
            {
                WormGameMode->EndCurrentTurn();
            }
        }
    }
}

bool AWormCharacter::Server_EndTurn_Validate()
{
    return true;
}

void AWormCharacter::Server_EndTurn_Implementation()
{
    // Vérifier à nouveau si c'est notre tour (sécurité côté serveur)
    if (bIsMyTurn)
    {
        EndTurn();
    }
}

void AWormCharacter::SetAvailableWeapons_Implementation(const TArray<TSubclassOf<AWormWeapon>>& WeaponTypes)
{
    // Stocker les armes disponibles
    AvailableWeapons = WeaponTypes;
    
    // Sur le serveur, nous allons créer l'arme
    if (HasAuthority() && AvailableWeapons.Num() > 0)
    {
        // S'assurer que l'index est valide
        if (CurrentWeaponIndex >= AvailableWeapons.Num())
        {
            CurrentWeaponIndex = 0;
        }
        
        // Petit délai pour s'assurer que tout est initialisé
        FTimerHandle WeaponSpawnTimerHandle;
        GetWorld()->GetTimerManager().SetTimer(
            WeaponSpawnTimerHandle,
            [this]() {
                SpawnCurrentWeapon();
                // Forcer la réplication du CurrentWeaponIndex vers les clients
                ForceNetUpdate();
            },
            0.5f,
            false
        );
    }
    else if (!HasAuthority() && AvailableWeapons.Num() > 0)
    {
        // Si c'est un client et que nous avons des armes, vérifier si nous devons créer notre arme
        FTimerHandle ClientWeaponCheckTimer;
        GetWorld()->GetTimerManager().SetTimer(
            ClientWeaponCheckTimer,
            [this]() {
                if (!CurrentWeapon)
                {
                    OnRep_CurrentWeaponIndex();
                }
            },
            1.0f,
            false
        );
    }
}

void AWormCharacter::DiagnoseWeapons()
{
    // Diagnostic minimal avec les informations essentielles uniquement
    if (!HasAuthority() && IsLocallyControlled() && !CurrentWeapon && AvailableWeapons.Num() > 0)
    {
        OnRep_CurrentWeaponIndex();
    }
}

void AWormCharacter::SetAiming(bool bIsAiming)
{
    // Si on a une arme, activer/désactiver la prévisualisation
    if (CurrentWeapon)
    {
        CurrentWeapon->ShowTrajectory(bIsAiming);
    }
    
    // Widget de visée
    if (bIsAiming)
    {
        if (IsLocallyControlled() && AimingWidgetClass && !AimingWidget)
        {
            AimingWidget = CreateWidget<UUserWidget>(GetWorld(), AimingWidgetClass);
            if (AimingWidget)
            {
                AimingWidget->AddToViewport();
                UpdateAimingWidget();
            }
        }
    }
    else
    {
        if (AimingWidget)
        {
            AimingWidget->RemoveFromParent();
            AimingWidget = nullptr;
        }
    }
}

void AWormCharacter::OnAimActionStarted(const FInputActionValue& Value)
{
    if (bUseFirstPersonViewWhenAiming)
    {
        ToggleCameraMode(true); // Passer en FPS
    }
    
    SetAiming(true);
}

void AWormCharacter::OnAimActionEnded(const FInputActionValue& Value)
{
    if (bUseFirstPersonViewWhenAiming)
    {
        ToggleCameraMode(false); // Revenir en TPS
    }
    
    SetAiming(false);
}

void AWormCharacter::OnPowerUpAction(const FInputActionValue& Value)
{
    if (bIsMyTurn && CurrentWeapon && IsLocallyControlled())
    {
        CurrentWeapon->AdjustPower(1.0f);
    }
}

void AWormCharacter::OnPowerDownAction(const FInputActionValue& Value)
{
    if (bIsMyTurn && CurrentWeapon && IsLocallyControlled())
    {
        CurrentWeapon->AdjustPower(-1.0f);
    }
}

void AWormCharacter::UpdateAimingWidget()
{
    if (AimingWidget && CurrentWeapon)
    {
        // Accès aux propriétés du widget via UMG
        UProgressBar* PowerBar = Cast<UProgressBar>(AimingWidget->GetWidgetFromName(TEXT("PowerBar")));
        UTextBlock* PowerText = Cast<UTextBlock>(AimingWidget->GetWidgetFromName(TEXT("PowerText")));
        
        if (PowerBar)
        {
            PowerBar->SetPercent(CurrentWeapon->GetNormalizedPower());
        }
        
        if (PowerText)
        {
            float PowerPercentage = CurrentWeapon->GetNormalizedPower() * 100.0f;
            PowerText->SetText(FText::FromString(FString::Printf(TEXT("%.0f%%"), PowerPercentage)));
        }
    }
}

void AWormCharacter::AdjustPower(float PowerLevel)
{
    if (bIsMyTurn && CurrentWeapon)
    {
        // Convertir 0-1 en valeur entre min et max
        float ActualPower = FMath::Lerp(
            CurrentWeapon->GetMinPower(),
            CurrentWeapon->GetMaxPower(),
            FMath::Clamp(PowerLevel, 0.0f, 1.0f)
        );
        
        // Calculer la différence nécessaire
        float CurrentPower = CurrentWeapon->GetCurrentPower();
        float Delta = (ActualPower - CurrentPower) / CurrentWeapon->PowerAdjustmentStep;
        
        CurrentWeapon->AdjustPower(Delta);
    }
}

bool AWormCharacter::Server_UpdateWeaponRotation_Validate(FRotator NewRotation)
{
    return true;
}

void AWormCharacter::Server_UpdateWeaponRotation_Implementation(FRotator NewRotation)
{
    if (WeaponPivotComponent)
    {
        // Appliquer la rotation reçue - on respecte également le mode de caméra
        // Utiliser directement la rotation envoyée par le client (qui a déjà géré le mode)
        WeaponPivotComponent->SetWorldRotation(NewRotation);
        
        // Propager aux clients via Multicast
        Multicast_UpdateWeaponRotation(NewRotation);
    }
}

void AWormCharacter::Multicast_UpdateWeaponRotation_Implementation(FRotator NewRotation)
{
    // Ne pas exécuter sur le client qui a envoyé la rotation (pour éviter les doublons)
    if (!IsLocallyControlled() && WeaponPivotComponent)
    {
        // Appliquer la rotation telle qu'elle est reçue, sans modifier le pitch
        // ce qui permet de conserver l'orientation en mode FPS
        WeaponPivotComponent->SetWorldRotation(NewRotation);
    }
}

void AWormCharacter::ToggleCameraMode(bool bUseFPSCamera)
{
    if (FollowCamera && FPSCamera)
    {
        FollowCamera->SetActive(!bUseFPSCamera);
        FPSCamera->SetActive(bUseFPSCamera);
        
        // Mettre à jour l'indicateur de mode caméra
        bool bPreviousMode = bIsInFirstPersonMode;
        bIsInFirstPersonMode = bUseFPSCamera;
        
        // Gestion de la position de caméra
        if (bUseFPSCamera)
        {
            // Sauvegarder la position actuelle du CameraBoom
            SavedCameraDistance = CameraBoom->TargetArmLength;
            
            // Si on passe du mode TPS au mode FPS, on applique la rotation actuelle de la caméra
            if (WeaponPivotComponent)
            {
                FRotator ControlRotation = GetControlRotation();
                WeaponPivotComponent->SetWorldRotation(FRotator(ControlRotation.Pitch, ControlRotation.Yaw, 0.0f));
            }
        }
        else
        {
            // Restaurer la position du CameraBoom
            CameraBoom->TargetArmLength = SavedCameraDistance;
            
            // Si on retourne en mode TPS, remettre l'arme dans la rotation par défaut
            if (WeaponPivotComponent)
            {
                WeaponPivotComponent->SetWorldRotation(DefaultWeaponRotation);
            }
        }
    }
}
  
void AWormCharacter::CreateWeaponPivot()
{
    // Nettoyage du pivot existant
    if (WeaponPivotComponent)
    {
        WeaponPivotComponent->DestroyComponent();
    }
    
    // Créer le nouveau pivot
    WeaponPivotComponent = NewObject<USceneComponent>(this, TEXT("WeaponPivot"));
    WeaponPivotComponent->RegisterComponent();
    
    // Attacher au bon socket si disponible, sinon au root
    if (GetMesh()->DoesSocketExist(WeaponSocketName))
    {
        WeaponPivotComponent->AttachToComponent(GetMesh(), 
            FAttachmentTransformRules::SnapToTargetNotIncludingScale, 
            WeaponSocketName);
            
        // En mode TPS, appliquer la rotation fixe par défaut,
        // sinon utiliser la rotation de contrôle en mode FPS
        if (!bIsInFirstPersonMode)
        {
            WeaponPivotComponent->SetWorldRotation(DefaultWeaponRotation);
        }
        else
        {
            FRotator ControlRotation = GetControlRotation();
            WeaponPivotComponent->SetWorldRotation(FRotator(ControlRotation.Pitch, ControlRotation.Yaw, 0.0f));
        }
    }
    else
    {
        WeaponPivotComponent->AttachToComponent(GetRootComponent(), 
            FAttachmentTransformRules::SnapToTargetNotIncludingScale);
        
        // Appliquer également la rotation appropriée
        if (!bIsInFirstPersonMode)
        {
            WeaponPivotComponent->SetWorldRotation(DefaultWeaponRotation);
        }
    }
}

FTransform AWormCharacter::CalculateWeaponSpawnTransform()
{
    FTransform SpawnTransform;
    
    // Obtenir la transformation depuis le socket si disponible
    if (GetMesh()->DoesSocketExist(WeaponSocketName))
    {
        SpawnTransform = GetMesh()->GetSocketTransform(WeaponSocketName);
        
        // Ajuster la rotation selon le mode de caméra
        if (bIsInFirstPersonMode)
        {
            // En FPS, suivre la rotation de la caméra
            FRotator ControlRotation = GetControlRotation();
            FRotator NewRotation(ControlRotation.Pitch, ControlRotation.Yaw, 0.0f);
            SpawnTransform.SetRotation(NewRotation.Quaternion());
        }
        else
        {
            // En TPS, utiliser la rotation fixe par défaut
            SpawnTransform.SetRotation(DefaultWeaponRotation.Quaternion());
        }
    }
    else
    {
        // Fallback à la transformation de l'acteur avec un offset
        SpawnTransform = GetActorTransform();
        SpawnTransform.AddToTranslation(FVector(50.0f, 0.0f, 0.0f));
        
        // Utiliser également la rotation appropriée
        if (!bIsInFirstPersonMode)
        {
            SpawnTransform.SetRotation(DefaultWeaponRotation.Quaternion());
        }
    }
    
    return SpawnTransform;
}

void AWormCharacter::Multicast_WeaponChanged_Implementation()
{
    // Ne pas exécuter cette logique sur le serveur
    if (HasAuthority())
    {
        return;
    }
    
    // S'assurer que l'index est valide avant de continuer
    if (AvailableWeapons.Num() > 0 && AvailableWeapons.IsValidIndex(CurrentWeaponIndex))
    {
        // Détruire l'arme actuelle si elle existe
        if (CurrentWeapon)
        {
            CurrentWeapon->Destroy();
            CurrentWeapon = nullptr;
        }
        
        // Créer la nouvelle arme (uniquement visuel)
        OnRep_CurrentWeaponIndex();
    }
}

void AWormCharacter::OnRep_Health()
{
    // Jouer l'animation de réaction aux dégâts
    if (Health > 0 && HitReactMontage)
    {
        PlayAnimMontage(HitReactMontage);
    }
    else if (Health <= 0 && DeathMontage)
    {
        PlayAnimMontage(DeathMontage);
    }
}

void AWormCharacter::OnRep_CurrentWeaponIndex()
{
    // Fonction optimisée d'initialisation d'armes côté client
    if (AvailableWeapons.Num() <= 0 || !AvailableWeapons.IsValidIndex(CurrentWeaponIndex))
    {
        return;
    }
    
    // Nettoyage de l'arme existante
    if (CurrentWeapon)
    {
        CurrentWeapon->Destroy();
        CurrentWeapon = nullptr;
    }
    
    // Créer le pivot pour l'arme
    CreateWeaponPivot();
    
    // Créer l'arme
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.Instigator = GetInstigator();
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    
    // Utiliser la transformation du pivot pour le spawn
    FTransform SpawnTransform = WeaponPivotComponent->GetComponentTransform();
    
    // Spawner l'arme
    CurrentWeapon = GetWorld()->SpawnActor<AWormWeapon>(
        AvailableWeapons[CurrentWeaponIndex], 
        SpawnTransform, 
        SpawnParams
    );
    
    if (CurrentWeapon)
    {
        // Attacher l'arme au pivot
        CurrentWeapon->AttachToComponent(WeaponPivotComponent, 
            FAttachmentTransformRules::SnapToTargetNotIncludingScale);
        
        CurrentWeapon->SetOwner(this);
        
        // IMPORTANT: Désactiver les collisions avec l'arme
        if (CurrentWeapon->GetRootComponent())
        {
            // Désactiver les collisions pour tous les composants de l'arme
            TArray<UPrimitiveComponent*> Components;
            CurrentWeapon->GetComponents<UPrimitiveComponent>(Components);
            for (UPrimitiveComponent* Component : Components)
            {
                if (Component)
                {
                    Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                    Component->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
                    Component->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Ignore);
                    Component->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Ignore);
                }
            }
            
            // Si le composant principal est un SkeletalMeshComponent ou StaticMeshComponent
            if (UMeshComponent* MeshComponent = Cast<UMeshComponent>(CurrentWeapon->GetRootComponent()))
            {
                MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                MeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
            }
        }
    }
}