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
//include for   AWormCharacter.cpp(1045): [C2039] 'IsNormalized': is not a member of 'UE::Math::TRotator<double>'
#include "Math/UnrealMathUtility.h"
// Ajouter les includes manquants pour les collisions Cannot resolve symbol 'SetCollisionEnabled'
#include "WeaponWheelWidget.h"
#include "Worms_3d/Env/EnvironmentalEventsManager.h"
#include "WormGameState.h"
#include "GuidedMissileWeapon.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/WidgetComponent.h"

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
    AnimationSpeed = 0.0f;

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
    // Dans le constructeur du personnage qui ne s'oriente pas correctement
    GetCharacterMovement()->bOrientRotationToMovement = true; // Activer cette propriété
    GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f); // Vitesse de rotation
    // Configuration du système de caméra
    // S'assurer que la physique est activée dès le début
    GetCharacterMovement()->UpdateComponentVelocity();
    NameWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("NameWidgetComponent"));
    if (NameWidgetComponent)
    {
        NameWidgetComponent->SetupAttachment(RootComponent);
        NameWidgetComponent->SetRelativeLocation(FVector(0, 0, 120.0f)); // Ajuster la hauteur selon les besoins
        NameWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
        NameWidgetComponent->SetDrawSize(FVector2D(150.0f, 50.0f));
        // Le NameIndicatorWidgetClass sera défini via une propriété UPROPERTY dans l'éditeur
    }
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

    bIsGuidingMissile = false;
    
    // Debug
    UE_LOG(LogTemp, Warning, TEXT("Available weapons count: %d"), AvailableWeapons.Num());
    for (auto Weapon : AvailableWeapons)
    {
        if (Weapon)
        {
            UE_LOG(LogTemp, Warning, TEXT("Weapon class: %s"), *Weapon->GetName());
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("Null weapon class found in AvailableWeapons"));
        }
    }
    
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

    // Diagnostic d'armes pour client
    SetupWeaponDiagnostic();
    FTimerHandle NameWidgetTimerHandle;
    GetWorld()->GetTimerManager().SetTimer(
        NameWidgetTimerHandle,
        [this]() {
            InitializeNameWidget();
        },
        0.5f,
        false
    );
    // Configuration de l'input pour le joueur local
    if (IsLocallyControlled())
    {
        // Setup Enhanced Input
        if (APlayerController* PC = Cast<APlayerController>(GetController()))
        {
            SetupEnhancedInput(PC);
        }
        

    }
    if (!HasAuthority() && IsLocallyControlled())
    {
        FTimerHandle WeaponCheckTimer;
        GetWorld()->GetTimerManager().SetTimer(
            WeaponCheckTimer,
            [this]()
            {
                if (!CurrentWeapon && AvailableWeapons.Num() > 0)
                {
                    OnRep_CurrentWeaponIndex();
                }
            },
            1.0f,
            false
        );
    }
}

void AWormCharacter::SetupWeaponDiagnostic()
{
    // Only setup for clients and limit to 3 attempts max per instance
    if (!HasAuthority() && IsLocallyControlled())
    {
        UE_LOG(LogTemp, Warning, TEXT("[CLIENT] Configuration du diagnostic d'arme pour %s"), *GetName());
        
        // Make this instance-specific rather than static
        DiagnosticCount = 0;
        TWeakObjectPtr<AWormCharacter> WeakThis(this);
        
        FTimerHandle DiagnosticTimerHandle;
        GetWorld()->GetTimerManager().SetTimer(
            DiagnosticTimerHandle,
            [WeakThis]() {
                // Safety check to ensure character still exists
                if (!WeakThis.IsValid())
                    return;
                    
                if (WeakThis->DiagnosticCount < 3) // Limit to 3 diagnostics
                {
                    WeakThis->DiagnoseWeapons();
                    WeakThis->DiagnosticCount++;
                    
                    // Force weapon creation only after the 2nd attempt and only if necessary
                    if (WeakThis->DiagnosticCount >= 2 && !WeakThis->CurrentWeapon && WeakThis->AvailableWeapons.Num() > 0)
                    {
                        WeakThis->OnRep_CurrentWeaponIndex();
                    }
                }
            },
            2.0f,  // First diagnostic after 2 seconds
            false   // Don't repeat
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
    
    // If this character is dead, make it visible again but still non-collidable
    if (IsDead())
    {
        // Make sure it's visible - possessed characters should be visible even if dead
        GetMesh()->SetVisibility(true);
        GetMesh()->SetRenderCustomDepth(false);
        
        // But keep collision disabled
        GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        
        UE_LOG(LogTemp, Warning, TEXT("Possessed dead character %s - adjusted visibility"), *GetName());
    }
}
void AWormCharacter::UnPossessed()
{
    Super::UnPossessed();
    
    // If this character is dead and now unpossessed, apply full death visual state
    if (IsDead())
    {
        // Apply death visual state
        GetMesh()->SetRenderCustomDepth(true);
        GetMesh()->SetCustomDepthStencilValue(2);
        
        UE_LOG(LogTemp, Warning, TEXT("Unpossessed dead character %s - applied death visual state"), *GetName());
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

        // Binding pour le switch de joueur
        if (SwitchTeamMemberAction)
            EnhancedInputComponent->BindAction(SwitchTeamMemberAction, ETriggerEvent::Triggered, this, &AWormCharacter::OnSwitchTeamMemberAction);
        
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
        if (AbortMissileAction)
            EnhancedInputComponent->BindAction(AbortMissileAction, ETriggerEvent::Triggered, this, &AWormCharacter::OnAbortMissileAction);
        // Binding for toggling weapon wheel
        if (ToggleWeaponWheelAction)
            EnhancedInputComponent->BindAction(ToggleWeaponWheelAction, ETriggerEvent::Started, this, &AWormCharacter::OnToggleWeaponWheelAction);
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

    // Log input for debugging
    UE_LOG(LogTemp, Verbose, TEXT("Look input: X=%.2f, Y=%.2f, bIsGuidingMissile=%s"), 
        LookAxisVector.X, LookAxisVector.Y, 
        bIsGuidingMissile ? TEXT("true") : TEXT("false"));

    if (bIsGuidingMissile && CurrentWeapon)
    {
        // Try to cast to missile weapon
        AGuidedMissileWeapon* MissileWeapon = Cast<AGuidedMissileWeapon>(CurrentWeapon);
        if (MissileWeapon)
        {
            // Use raw input values for missile control - no need to negate Y
            UE_LOG(LogTemp, Verbose, TEXT("Sending missile input: Yaw=%.2f, Pitch=%.2f"), 
                LookAxisVector.X, LookAxisVector.Y);
                
            MissileWeapon->ProcessMissileMovementInput(LookAxisVector.X, LookAxisVector.Y);
            return;
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("Expected GuidedMissileWeapon but got %s"), 
                CurrentWeapon ? *CurrentWeapon->GetClass()->GetName() : TEXT("NULL"));
        }
    }

    // Handle normal character looking if not controlling a missile
    if (Controller != nullptr)
    {
        // Add yaw and pitch input to controller
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


void AWormCharacter::HandleMissileControl()
{
    // Previous missile control state
    bool bWasGuidingMissile = bIsGuidingMissile;
    
    // Check if weapon is a missile weapon
    AGuidedMissileWeapon* MissileWeapon = Cast<AGuidedMissileWeapon>(CurrentWeapon);
    if (MissileWeapon)
    {
        // Update guided missile state
        bIsGuidingMissile = MissileWeapon->IsGuidingMissile();
        
        // Log state change for debugging
        if (bIsGuidingMissile != bWasGuidingMissile)
        {
            UE_LOG(LogTemp, Warning, TEXT("Missile control state changed: %s -> %s"), 
                bWasGuidingMissile ? TEXT("Guiding") : TEXT("Not Guiding"),
                bIsGuidingMissile ? TEXT("Guiding") : TEXT("Not Guiding"));
        }
    }
    else
    {
        // Not a missile weapon
        if (bIsGuidingMissile)
        {
            UE_LOG(LogTemp, Warning, TEXT("No longer using a missile weapon"));
            bIsGuidingMissile = false;
        }
    }
}

void AWormCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Mise à jour de la rotation de l'arme
    UpdateWeaponRotation();
    if (GetWorld())
    {
        // Find water manager
        AEnvironmentalEventsManager* WaterManager = AEnvironmentalEventsManager::GetEventsManager(GetWorld());
        if (WaterManager && WaterManager->WaterSystem)
        {
            float WaterLevel = WaterManager->WaterSystem->GetCurrentWaterLevel();
            float CharacterZ = GetActorLocation().Z;
            
            // If underwater, apply damage
            if (CharacterZ < WaterLevel)
            {
                // Let the water system handle the kill
                WaterManager->WaterSystem->KillCharacterInWater(this);
            }
        }
    }
    // Gestion du mouvement et des points de mouvement
    if (bIsMyTurn && HasAuthority())
    {
        UpdateMovementPoints();
    }
    FVector Velocity = GetVelocity();
    AnimationSpeed = FVector2D(Velocity.X, Velocity.Y).Size();
    
    // Interface utilisateur de visée
    if (AimingWidget && CurrentWeapon)
    {
        UpdateAimingWidget();
    }
    
    // Limiter le mouvement quand ce n'est pas le tour du personnage
    LimitMovementWhenNotMyTurn();
    HandleMissileControl();
}

// Optionnel: Ajouter une fonction helper pour vérifier si une rotation est dans les limites
bool AWormCharacter::IsRotationWithinLimits(const FRotator& TestRotation) const
{
    FRotator ActorRotation = GetActorRotation();
    float DeltaYaw = FMath::FindDeltaAngleDegrees(ActorRotation.Yaw, TestRotation.Yaw);
    
    return FMath::Abs(DeltaYaw) <= MaxYawAngle && 
           FMath::Abs(TestRotation.Pitch) <= MaxPitchAngle;
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
    DOREPLIFETIME(AWormCharacter, CharacterIndexInTeam);
    DOREPLIFETIME(AWormCharacter, InGameName);
    DOREPLIFETIME(AWormCharacter, TeamId);
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
        // Stopper les mouvements horizontaux mais conserver la gravité
        FVector CurrentVelocity = GetCharacterMovement()->Velocity;
        // Conserver uniquement la composante verticale (Z) pour la gravité
        GetCharacterMovement()->Velocity = FVector(0, 0, CurrentVelocity.Z);
        
        // Empêcher de marcher mais permettre la chute
        GetCharacterMovement()->MaxWalkSpeed = 0;
        
        // Augmenter légèrement la gravité pour retomber plus vite après une explosion
        GetCharacterMovement()->GravityScale = 2.0f; // Augmenté de 1.5f à 2.0f
    }
    else
    {
        if (MovementPoints > 0)
        {
            GetCharacterMovement()->MaxWalkSpeed = 600.0f;
        }
        
        // Rétablir la gravité normale pendant son tour
        GetCharacterMovement()->GravityScale = 1.5f;
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
    // Assurez-vous que le CurrentWeapon est valide
    if (CurrentWeapon)
    {
        CurrentWeapon->Fire();
    }
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
    if (!HasAuthority()) return;
    
    if (!AvailableWeapons.IsValidIndex(CurrentWeaponIndex))
    {
        return;
    }

    if (CurrentWeapon)
    {
        CurrentWeapon->Destroy();
        CurrentWeapon = nullptr;
    }

    FTransform SpawnTransform = GetMesh()->GetSocketTransform(WeaponSocketName);
    
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.Instigator = this;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    CurrentWeapon = GetWorld()->SpawnActor<AWormWeapon>(
        AvailableWeapons[CurrentWeaponIndex],
        SpawnTransform,
        SpawnParams
    );

    if (CurrentWeapon)
    {
        // Attacher avec des règles strictes d'attachement
        CurrentWeapon->AttachToComponent(GetMesh(),
            FAttachmentTransformRules::SnapToTargetIncludingScale,
            WeaponSocketName);
            
        // Force update réseau et multicast
        ForceNetUpdate();
        
        FTimerHandle TimerHandle;
        GetWorld()->GetTimerManager().SetTimer(TimerHandle, [this]()
        {
            Multicast_WeaponChanged();
        }, 0.1f, false);
    }
}

void AWormCharacter::ApplyDamageToWorm(float DamageAmount, FVector ImpactDirection)
{
    // Skip if already dead
    if (IsDead())
    {
        UE_LOG(LogTemp, Warning, TEXT("Skipping damage on already dead character %s"), *GetName());
        return;
    }

    // Store previous health
    float PreviousHealth = Health;
    
    // Apply damage
    Health = FMath::Max(0.0f, Health - DamageAmount);
    
    // Log pour voir les changements de santé
    UE_LOG(LogTemp, Warning, TEXT("Character %s: Health changed from %.1f to %.1f (damage: %.1f)"), 
           *GetName(), PreviousHealth, Health, DamageAmount);
    
    // Apply impulse
    ApplyMovementImpulse(ImpactDirection.GetSafeNormal(), ImpactDirection.Size());
    
    // Record damage in game state
    AActor* DamageInstigator = GetInstigator();
    if (DamageInstigator)
    {
        // Get the game state to record damage
        AWormGameState* GameState = Cast<AWormGameState>(UGameplayStatics::GetGameState(this));
        if (GameState)
        {
            AWormCharacter* InstigatorChar = Cast<AWormCharacter>(DamageInstigator);
            FString InstigatorName;
            
            if (InstigatorChar && !InstigatorChar->InGameName.IsEmpty())
            {
                InstigatorName = InstigatorChar->InGameName;
            }
            else
            {
                InstigatorName = DamageInstigator->GetName();
            }
            
            GameState->AddDamageDealt(InstigatorName, DamageAmount);
            UE_LOG(LogTemp, Warning, TEXT("Recorded %.1f damage dealt by %s"), 
                DamageAmount, *InstigatorName);
                
            // NEW: Check if instigator's turn should end
            if (InstigatorChar && InstigatorChar->IsMyTurn())
            {
                // End instigator's turn after damage (water or projectile)
                AWormGameMode* GameMode = Cast<AWormGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
                if (GameMode && !GameMode->bDeferTurnEnding)
                {
                    // Set flag to prevent multiple ending calls
                    GameMode->bDeferTurnEnding = true;
                    
                    UE_LOG(LogTemp, Warning, TEXT("Ending turn for %s after causing damage to %s"), 
                        *InstigatorChar->GetName(), *GetName());
                    
                    // Use timer to prevent immediate call during damage application
                    FTimerHandle EndTurnTimerHandle;
                    GetWorld()->GetTimerManager().SetTimer(
                        EndTurnTimerHandle,
                        [GameMode]()
                        {
                            if (GameMode && IsValid(GameMode))
                            {
                                GameMode->bDeferTurnEnding = false;
                                GameMode->EndCurrentTurn();
                            }
                        },
                        0.5f,  // Delay to allow damage effects to complete
                        false
                    );
                }
            }
        }
    }
    
    // Handle death
    if (Health <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("Character %s died!"), *GetName());
        
        // Handle death state
        HandleCharacterDeath();
        
        UE_LOG(LogTemp, Warning, TEXT("Character %s died! (Team %d, Index %d)"), 
            *GetName(), TeamId, CharacterIndexInTeam);
            
        // Check for game over
        AWormGameState* GameState = Cast<AWormGameState>(UGameplayStatics::GetGameState(this));
        if (GameState)
        {
            GameState->CheckGameOverCondition();
        }
    }
    else
    {
        // Play hit animation if not dead
        PlayHitReaction();
    }
}
void AWormCharacter::ApplyMovementImpulse(FVector Direction, float Strength)
{
    if (HasAuthority())
    {
        // S'assurer que la direction est normalisée
        Direction = Direction.GetSafeNormal();
        
        // Log pour déboguer
        UE_LOG(LogTemp, Warning, TEXT("Applying impulse to %s: Dir=%s, Strength=%.1f"), 
            *GetName(), *Direction.ToString(), Strength);
        
        // Appliquer l'impulsion avec une force suffisante
        // Utiliser bVelocityChange=true pour contourner la masse et appliquer directement un changement de vélocité
        GetCharacterMovement()->AddImpulse(Direction * Strength, true);
        
        // Assurer que le personnage soit en état de "falling" pour réagir à l'impulsion
        GetCharacterMovement()->SetMovementMode(MOVE_Falling);
        
        // Multicast RPC pour la synchronisation visuelle
        Multicast_ApplyImpulse(Direction, Strength);
    }
}

void AWormCharacter::Multicast_ApplyImpulse_Implementation(FVector Direction, float Strength)
{
    // Cette fonction est appelée sur tous les clients et le serveur
    if (!HasAuthority())
    {
        // S'assurer que la direction est normalisée
        Direction = Direction.GetSafeNormal();
        
        // Appliquer uniquement l'effet visuel sur les clients
        GetCharacterMovement()->AddImpulse(Direction * Strength, true);
        
        // Assurer que le personnage soit en état de "falling" pour réagir à l'impulsion
        GetCharacterMovement()->SetMovementMode(MOVE_Falling);
    }
}


void AWormCharacter::SetIsMyTurn(bool bNewTurn)
{
    //DEBUG
    UE_LOG(LogTemp, Warning, TEXT("SetIsMyTurn called"));
    UE_LOG(LogTemp, Warning, TEXT("bIsMyTurn: %d"), bIsMyTurn);
    UE_LOG(LogTemp, Warning, TEXT("bNewTurn: %d"), bNewTurn);
    UE_LOG(LogTemp, Warning, TEXT("bAutoEndTurnTimerActive: %d"), bAutoEndTurnTimerActive);
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
            GetCharacterMovement()->MaxWalkSpeed = 0;

            // NE PAS désactiver complètement le mouvement
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
    if (!HasAuthority() && IsLocallyControlled())
    {
        if (!CurrentWeapon && AvailableWeapons.Num() > 0)
        {
            UE_LOG(LogTemp, Warning, TEXT("[CLIENT] DiagnoseWeapons: Arme manquante pour %s, tentative de récupération"), *GetName());
            OnRep_CurrentWeaponIndex();
        }
        else if (CurrentWeapon)
        {
            UE_LOG(LogTemp, Warning, TEXT("[CLIENT] DiagnoseWeapons: %s a déjà l'arme %s"), 
                *GetName(), *CurrentWeapon->GetName());
        }
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
    // Une validation minimale mais suffisante
    return true;
}
void AWormCharacter::Server_UpdateWeaponRotation_Implementation(FRotator NewRotation)
{
    if (!CurrentWeapon || !bIsMyTurn)
    {
        return;
    }

    // On server, apply safety constraints to prevent extreme values
    const FRotator ActorRotation = GetActorRotation();
    float DeltaYaw = FMath::FindDeltaAngleDegrees(ActorRotation.Yaw, NewRotation.Yaw);
    
    // Apply clamping for safety
    float ClampedPitch = FMath::ClampAngle(NewRotation.Pitch, -MaxPitchAngle, MaxPitchAngle);
    float ClampedYaw = FMath::ClampAngle(DeltaYaw, -MaxYawAngle, MaxYawAngle);
    
    // Create a safe rotation that respects limits
    FRotator SafeRotation(ClampedPitch, ActorRotation.Yaw + ClampedYaw, 0.0f);

    // Apply the rotation to the weapon
    if (CurrentWeapon)
    {
        // Get the socket transform for consistent positioning
        FTransform SocketTransform = GetMesh()->GetSocketTransform(WeaponSocketName);
        
        // Create a new transform with our calculated rotation but socket location
        FTransform WeaponTransform = FTransform(
            SafeRotation.Quaternion(),
            SocketTransform.GetLocation(),
            FVector(1.0f, 1.0f, 1.0f) // Maintain scale
        );
        
        // Apply the transform to the weapon
        CurrentWeapon->SetActorTransform(WeaponTransform);
        
        // Force network update to propagate
        CurrentWeapon->ForceNetUpdate();
        
        // Broadcast to all clients (except the one that sent the update)
        Multicast_UpdateWeaponRotation(SafeRotation);
    }
}

void AWormCharacter::UpdateWeaponRotation()
{
    // Only update for the locally controlled character when it's their turn and has a weapon
    if (!IsLocallyControlled() || !CurrentWeapon || !bIsMyTurn)
    {
        return;
    }

    // Calculate target rotation based on camera mode
    const FRotator ActorRotation = GetActorRotation();
    const FRotator ControlRotation = GetControlRotation();
    
    // Calculate the target rotation based on the current mode
    FRotator TargetRotation;
    if (bIsInFirstPersonMode)
    {
        // Calculate a limited rotation angle relative to character's own rotation
        float DeltaYaw = FMath::FindDeltaAngleDegrees(ActorRotation.Yaw, ControlRotation.Yaw);
        float ClampedYaw = FMath::ClampAngle(DeltaYaw, -MaxYawAngle, MaxYawAngle);
        float ClampedPitch = FMath::ClampAngle(ControlRotation.Pitch, -MaxPitchAngle, MaxPitchAngle);
        
        TargetRotation = FRotator(ClampedPitch, ActorRotation.Yaw + ClampedYaw, 0.0f);
    }
    else
    {
        // In TPS mode, simply align with character facing direction
        TargetRotation = FRotator(0.0f, ActorRotation.Yaw, 0.0f);
    }

    // First apply the rotation locally for immediate feedback
    if (CurrentWeapon)
    {
        // Get the socket transform for consistent positioning
        FTransform SocketTransform = GetMesh()->GetSocketTransform(WeaponSocketName);
        
        // Create a new transform with our calculated rotation but socket location
        FTransform WeaponTransform = FTransform(
            TargetRotation.Quaternion(),
            SocketTransform.GetLocation(),
            FVector(1.0f, 1.0f, 1.0f) // Maintain scale
        );
        
        // Apply the transform to the weapon
        CurrentWeapon->SetActorTransform(WeaponTransform);
    }
    
    // Throttle network sends to reduce bandwidth 
    static float LastSendTime = 0.0f;
    const float CurrentTime = GetWorld()->GetTimeSeconds();
    const float MinTimeBetweenUpdates = 0.1f; // Reduced frequency for better stability
    
    if (CurrentTime - LastSendTime >= MinTimeBetweenUpdates) 
    {
        // Send reliable update to server - critical for sync
        Server_UpdateWeaponRotation(TargetRotation);
        LastSendTime = CurrentTime;
    }
}


void AWormCharacter::Multicast_UpdateWeaponRotation_Implementation(FRotator NewRotation)
{
    // Skip applying on the locally controlled character (they already have local rotation)
    if (IsLocallyControlled() || !CurrentWeapon)
    {
        return;
    }

    // Apply rotation to the weapon on all other clients
    FTransform SocketTransform = GetMesh()->GetSocketTransform(WeaponSocketName);
    
    // Create a new transform with our received rotation but socket location
    FTransform WeaponTransform = FTransform(
        NewRotation.Quaternion(),
        SocketTransform.GetLocation(),
        FVector(1.0f, 1.0f, 1.0f) // Maintain scale
    );
    
    // Apply the transform to the weapon
    CurrentWeapon->SetActorTransform(WeaponTransform);
}
void AWormCharacter::SyncWeaponRotationAfterAttachment()
{
    if (!CurrentWeapon)
        return;
        
    // Calculate proper rotation based on current mode
    FRotator TargetRotation;
    if (bIsInFirstPersonMode)
    {
        // In FPS mode, use control rotation with limits
        FRotator ControlRot = GetControlRotation();
        FRotator ActorRot = GetActorRotation();
        
        float DeltaYaw = FMath::FindDeltaAngleDegrees(ActorRot.Yaw, ControlRot.Yaw);
        float ClampedYaw = FMath::ClampAngle(DeltaYaw, -MaxYawAngle, MaxYawAngle);
        float ClampedPitch = FMath::ClampAngle(ControlRot.Pitch, -MaxPitchAngle, MaxPitchAngle);
        
        TargetRotation = FRotator(ClampedPitch, ActorRot.Yaw + ClampedYaw, 0.0f);
    }
    else
    {
        // In TPS mode, align with character
        TargetRotation = FRotator(0.0f, GetActorRotation().Yaw, 0.0f);
    }
    
    // Apply rotation to the weapon
    FTransform SocketTransform = GetMesh()->GetSocketTransform(WeaponSocketName);
    FTransform WeaponTransform = CurrentWeapon->GetActorTransform();
    
    WeaponTransform.SetRotation(TargetRotation.Quaternion());
    WeaponTransform.SetLocation(SocketTransform.GetLocation());
    
    CurrentWeapon->SetActorTransform(WeaponTransform);
    
    // Force update network
    if (HasAuthority())
    {
        CurrentWeapon->ForceNetUpdate();
        Multicast_UpdateWeaponRotation(TargetRotation);
    }
    else
    {
        Server_UpdateWeaponRotation(TargetRotation);
    }
}
void AWormCharacter::AttachWeaponToSocket(AWormWeapon* Weapon)
{
    if (!Weapon || !IsValid(Weapon) || !GetMesh() || !IsValid(GetMesh()))
    {
        UE_LOG(LogTemp, Error, TEXT("AttachWeaponToSocket: Invalid weapon or mesh"));
        return;
    }

    // Detach first
    Weapon->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    
    // Check socket exists
    if (!GetMesh()->DoesSocketExist(WeaponSocketName)) {
        UE_LOG(LogTemp, Error, TEXT("Socket '%s' does not exist on character mesh"), *WeaponSocketName.ToString());
        return;
    }
    
    // Reattach with strict rules
    Weapon->AttachToComponent(GetMesh(),
        FAttachmentTransformRules::SnapToTargetIncludingScale,
        WeaponSocketName);
        
    // Ensure weapon is visible
    Weapon->EnsureWeaponVisibility();
    
    // Add explicit sync of rotation after attachment with slight delay
    FTimerHandle RotationSyncTimerHandle;
    TWeakObjectPtr<AWormCharacter> WeakThis(this);
    GetWorld()->GetTimerManager().SetTimer(
        RotationSyncTimerHandle,
        [WeakThis]() {
            if (WeakThis.IsValid()) {
                WeakThis->SyncWeaponRotationAfterAttachment();
            }
        },
        0.1f,
        false
    );
    
    UE_LOG(LogTemp, Log, TEXT("Weapon %s attached to socket %s"), 
        *Weapon->GetName(), *WeaponSocketName.ToString());
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
            
        }
        else
        {
            // Restaurer la position du CameraBoom
            CameraBoom->TargetArmLength = SavedCameraDistance;
            
            // Si on retourne en mode TPS, remettre l'arme dans la rotation par défaut
            
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
    // Ne pas exécuter cette logique sur le serveur, seulement sur les clients
    if (HasAuthority())
    {
        return;
    }
    
    UE_LOG(LogTemp, Warning, TEXT("[CLIENT] Multicast_WeaponChanged pour %s"), *GetName());
    
    // S'assurer que l'index est valide avant de continuer
    if (AvailableWeapons.Num() > 0 && AvailableWeapons.IsValidIndex(CurrentWeaponIndex))
    {
        // IMPORTANT: Ne pas créer une nouvelle arme dans le multicast, utiliser OnRep_CurrentWeaponIndex
        // Cela élimine une source de duplication
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
    // Don't do anything on the server
    if (HasAuthority())
    {
        return;
    }

    // Valid index check
    if (!AvailableWeapons.IsValidIndex(CurrentWeaponIndex))
    {
        UE_LOG(LogTemp, Warning, TEXT("Invalid weapon index: %d (AvailableWeapons: %d)"), 
               CurrentWeaponIndex, AvailableWeapons.Num());
        return;
    }

    // Check if we already have the right weapon
    if (CurrentWeapon && CurrentWeapon->IsA(AvailableWeapons[CurrentWeaponIndex]))
    {
        UE_LOG(LogTemp, Log, TEXT("Already have correct weapon type: %s"), *CurrentWeapon->GetName());
        // Just re-attach to ensure proper socket placement
        AttachWeaponToSocket(CurrentWeapon);
        return;
    }

    // Cleaner weapon destruction with added safety
    if (CurrentWeapon)
    {
        AWormWeapon* WeaponToDestroy = CurrentWeapon;
        CurrentWeapon = nullptr; // Clear reference first
        
        // Detach before destruction
        WeaponToDestroy->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
        
        // Destroy with delay to avoid race conditions
        FTimerHandle DestroyTimer;
        TWeakObjectPtr<AWormWeapon> WeakWeapon(WeaponToDestroy);
        GetWorld()->GetTimerManager().SetTimer(
            DestroyTimer,
            [WeakWeapon]() {
                if (WeakWeapon.IsValid()) {
                    WeakWeapon->Destroy();
                }
            },
            0.1f,
            false
        );
    }

    // More safety checks before weapon creation
    if (!IsValid(this) || !GetWorld() || !GetMesh()) {
        UE_LOG(LogTemp, Error, TEXT("Cannot create weapon - invalid character state"));
        return;
    }

    // Get the socket transform for spawning
    FTransform SpawnTransform = GetMesh()->GetSocketTransform(WeaponSocketName);
    
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    // Get the weapon class with safety check
    TSubclassOf<AWormWeapon> WeaponClass = AvailableWeapons[CurrentWeaponIndex];
    if (!WeaponClass) {
        UE_LOG(LogTemp, Error, TEXT("NULL weapon class at index %d"), CurrentWeaponIndex);
        return;
    }

    // Create the new weapon
    CurrentWeapon = GetWorld()->SpawnActor<AWormWeapon>(
        WeaponClass,
        SpawnTransform,
        SpawnParams
    );

    if (CurrentWeapon)
    {
        // Schedule multiple visibility and rotation sync checks
        for (float Delay : {0.2f, 0.5f, 1.0f}) {
            FTimerHandle SyncTimerHandle;
            TWeakObjectPtr<AWormCharacter> WeakThis(this);
            
            GetWorld()->GetTimerManager().SetTimer(
                SyncTimerHandle,
                [WeakThis]() {
                    if (WeakThis.IsValid()) {
                        WeakThis->SyncWeaponRotationAfterAttachment();
                    }
                },
                Delay,
                false
            );
        }
    }
}


void AWormCharacter::PlayHitReaction()
{
    // Déclencher l'animation de dégâts
    bIsHit = true;
    
    // Créer un timer pour réinitialiser l'état après un délai
    FTimerHandle HitResetTimerHandle;
    GetWorldTimerManager().SetTimer(
        HitResetTimerHandle,
        [this]()
        {
            bIsHit = false;
        },
        0.5f, // Durée pendant laquelle bIsHit reste true (ajustez selon la durée de votre animation)
        false
    );
    
    // Log pour déboguer
    UE_LOG(LogTemp, Warning, TEXT("%s: Animation de dégâts déclenchée"), *GetName());
}


void AWormCharacter::OnSwitchTeamMemberAction(const FInputActionValue& Value)
{
  
}

void AWormCharacter::InitializeNameWidget()
{
    // S'assurer que cette logique est exécutée seulement côté client
    if (true)
    {
        if (!NameWidgetComponent)
        {
            // Créer le composant s'il n'existe pas
            NameWidgetComponent = NewObject<UWidgetComponent>(this, TEXT("NameWidgetComponent"));
            NameWidgetComponent->SetupAttachment(GetRootComponent());
            NameWidgetComponent->RegisterComponent();
            
            // Configurer la position et l'orientation
            NameWidgetComponent->SetRelativeLocation(FVector(0, 0, 120.0f)); // Positionner au-dessus de la tête
            NameWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen); // Toujours face à la caméra
            
            UE_LOG(LogTemp, Warning, TEXT("Created NameWidgetComponent for %s"), *GetName());
        }
        
        // Configurer la classe de widget
        if (NameWidgetComponent && NameIndicatorWidgetClass)
        {
            NameWidgetComponent->SetWidgetClass(NameIndicatorWidgetClass);
            NameWidgetComponent->SetDrawSize(FVector2D(150.0f, 50.0f));
            
            // Mettre à jour le widget avec les informations du personnage
            UWNameIndicatorWidget* NameWidget = Cast<UWNameIndicatorWidget>(NameWidgetComponent->GetUserWidgetObject());
            if (!NameWidget)
            {
                // Créer l'instance de widget si elle n'existe pas
                NameWidgetComponent->InitWidget();
                NameWidget = Cast<UWNameIndicatorWidget>(NameWidgetComponent->GetUserWidgetObject());
            }
            
            if (NameWidget)
            {
                // Définir les informations à afficher
                NameWidget->SetNameInfo(TeamId, InGameName.IsEmpty() ? GetName() : InGameName);
                UE_LOG(LogTemp, Warning, TEXT("Updated name widget for %s with TeamId %d and Name %s"), 
                    *GetName(), TeamId, *InGameName);
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("Failed to create or cast to UWNameIndicatorWidget for %s"), *GetName());
            }
        }
        else
        {
            if (!NameWidgetComponent)
                UE_LOG(LogTemp, Error, TEXT("NameWidgetComponent is null for %s"), *GetName());
            
            if (!NameIndicatorWidgetClass)
                UE_LOG(LogTemp, Error, TEXT("NameIndicatorWidgetClass is not set for %s"), *GetName());
        }
    }
    if (HasAuthority())
    {
        NameWidgetComponent->SetIsReplicated(true);
    }

}


void AWormCharacter::HandleCharacterDeath()
{
    if (!HasAuthority())
    {
        return; // Only run on server
    }

    // Don't process if already handled
    if (IsDead() && GetCapsuleComponent()->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
    {
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("Character %s died - handling death state"), *GetName());

    // Multicast to ensure visual state is consistent on all clients
    Multicast_ProcessDeath();
}
void AWormCharacter::Multicast_ProcessDeath_Implementation()
{
    // Stop all movement and animations
    GetCharacterMovement()->DisableMovement();
    GetCharacterMovement()->StopMovementImmediately();
    
    // Disable collision to prevent further damage
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    
    // If not possessed, we can adjust visibility (keep visible for possessed characters)
    if (!IsPlayerControlled())
    {
        // Make semi-transparent to show it's dead
        GetMesh()->SetRenderCustomDepth(true);
        GetMesh()->SetCustomDepthStencilValue(2); // Use a value for "dead" state
        
        // You could also add a death animation/pose here
        if (DeathMontage)
        {
            PlayAnimMontage(DeathMontage);
        }
    }
    
    // Disable weapon if present
    if (CurrentWeapon)
    {
        CurrentWeapon->SetActorEnableCollision(false);
    }
    
    // End turn if this was the active character
    if (bIsMyTurn)
    {
        AWormGameMode* GameMode = Cast<AWormGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
        if (GameMode)
        {
            FTimerHandle EndTurnHandle;
            GetWorldTimerManager().SetTimer(EndTurnHandle, GameMode, &AWormGameMode::EndCurrentTurn, 1.0f, false);
        }
    }
}

// Weapon Wheel

void AWormCharacter::ToggleWeaponWheel()
{
    UE_LOG(LogTemp, Warning, TEXT("ToggleWeaponWheel called, IsMyTurn: %s, IsLocallyControlled: %s"), 
        bIsMyTurn ? TEXT("true") : TEXT("false"),
        IsLocallyControlled() ? TEXT("true") : TEXT("false"));
    
    // Check turn condition
    if (!bIsMyTurn || !IsLocallyControlled())
    {
        UE_LOG(LogTemp, Warning, TEXT("ToggleWeaponWheel early return - not my turn or not locally controlled"));
        return;
    }
    // Toggle the flag
    bWeaponWheelActive = !bWeaponWheelActive;
    
    // Create or remove the widget
    if (bWeaponWheelActive)
    {
        if (WeaponWheelWidgetClass && !WeaponWheelWidget)
        {
            APlayerController* PC = Cast<APlayerController>(GetController());
            if (PC)
            {
                // Create the widget
                WeaponWheelWidget = CreateWidget<UUserWidget>(PC, WeaponWheelWidgetClass);
                
                // Cast to the specific type to access its methods
                if (UWeaponWheelWidget* TypedWidget = Cast<UWeaponWheelWidget>(WeaponWheelWidget))
                {
                    // Add debug log for data table
                    UE_LOG(LogTemp, Warning, TEXT("Setting weapon data table: %s"), 
                        WeaponDataTable ? *WeaponDataTable->GetName() : TEXT("NULL"));
                    
                    // Set the data table
                    TypedWidget->SetWeaponDataTable(WeaponDataTable);
                    
                    // Add to viewport
                    WeaponWheelWidget->AddToViewport(100); // High Z-order to be on top
                    
                    // Set input mode to UI with game
                    FInputModeGameAndUI InputMode;
                    InputMode.SetWidgetToFocus(WeaponWheelWidget->TakeWidget());
                    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
                    PC->SetInputMode(InputMode);
                    PC->SetShowMouseCursor(true);
                }
            }
        }
    }
    else
    {
        // Remove the widget and restore game input
        if (WeaponWheelWidget)
        {
            WeaponWheelWidget->RemoveFromParent();
            WeaponWheelWidget = nullptr;
            
            // Restore game input
            APlayerController* PC = Cast<APlayerController>(GetController());
            if (PC)
            {
                FInputModeGameOnly InputMode;
                PC->SetInputMode(InputMode);
                PC->SetShowMouseCursor(false);
            }
        }
    }
}


void AWormCharacter::SelectWeaponFromWheel(int32 WeaponIndex)
{
    // Close the weapon wheel
    bWeaponWheelActive = false;
    
    if (WeaponWheelWidget)
    {
        WeaponWheelWidget->RemoveFromParent();
        WeaponWheelWidget = nullptr;
        
        // Restore game input
        APlayerController* PC = Cast<APlayerController>(GetController());
        if (PC)
        {
            FInputModeGameOnly InputMode;
            PC->SetInputMode(InputMode);
            PC->SetShowMouseCursor(false);
        }
    }
    
    // Switch to the selected weapon if valid
    if (WeaponIndex >= 0 && WeaponIndex < AvailableWeapons.Num())
    {
        SwitchWeapon(WeaponIndex);
    }
}

void AWormCharacter::OnToggleWeaponWheelAction(const FInputActionValue& Value)
{
    UE_LOG(LogTemp, Warning, TEXT("G key pressed - OnToggleWeaponWheelAction called"));
    ToggleWeaponWheel();
}

void AWormCharacter::OnAbortMissileAction(const FInputActionValue& Value)
{
    if (bIsGuidingMissile && CurrentWeapon)
    {
        // Check if current weapon is a guided missile weapon
        AGuidedMissileWeapon* MissileWeapon = Cast<AGuidedMissileWeapon>(CurrentWeapon);
        if (MissileWeapon)
        {
            MissileWeapon->AbortGuidance();
            bIsGuidingMissile = false;
        }
    }
}
