#include "AWormCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "WeaponWheelWidget.h"
#include "WormGameMode.h"
#include "WormWeapon.h"
#include "Blueprint/UserWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
//include for   AWormCharacter.cpp(1045): [C2039] 'IsNormalized': is not a member of 'UE::Math::TRotator<double>'
#include "Math/UnrealMathUtility.h"
// Ajouter les includes manquants pour les collisions Cannot resolve symbol 'SetCollisionEnabled'
#include "Components/PrimitiveComponent.h"
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
    // Ne configurer que pour les clients et limiter à 3 tentatives maximum
    if (!HasAuthority() && IsLocallyControlled())
    {
        UE_LOG(LogTemp, Warning, TEXT("[CLIENT] Configuration du diagnostic d'arme pour %s"), *GetName());
        
        static int32 DiagnosticCount = 0;
        FTimerHandle DiagnosticTimerHandle;
        
        GetWorld()->GetTimerManager().SetTimer(
            DiagnosticTimerHandle,
            [this]() {
                if (DiagnosticCount < 3) // Limiter à 3 diagnostics
                {
                    DiagnoseWeapons();
                    DiagnosticCount++;
                    
                    // Force la création d'arme uniquement après la 2ème tentative et seulement si nécessaire
                    if (DiagnosticCount >= 2 && !CurrentWeapon && AvailableWeapons.Num() > 0)
                    {
                        OnRep_CurrentWeaponIndex();
                    }
                }
            },
            2.0f,  // Premier diagnostic après 2 secondes
            false   // Ne pas répéter
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
    FVector Velocity = GetVelocity();
    AnimationSpeed = FVector2D(Velocity.X, Velocity.Y).Size();
    
    // Interface utilisateur de visée
    if (AimingWidget && CurrentWeapon)
    {
        UpdateAimingWidget();
    }
    
    // Limiter le mouvement quand ce n'est pas le tour du personnage
    LimitMovementWhenNotMyTurn();
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
    // Vérification d'autorité
    if (!HasAuthority())
    {
        return;
    }
    
    // Vérifier l'index valide avant tout
    if (!AvailableWeapons.IsValidIndex(CurrentWeaponIndex))
    {
        return;
    }

    // Nettoyage plus strict de l'arme existante
    if (CurrentWeapon)
    {
        AWormWeapon* WeaponToDestroy = CurrentWeapon;
        CurrentWeapon = nullptr;
        WeaponToDestroy->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
        GetWorld()->DestroyActor(WeaponToDestroy);
    }

    // Obtenir la transformation du socket pour le spawn
    FTransform SpawnTransform;
    if (USkeletalMeshComponent* Meshss = GetMesh())
    {
        SpawnTransform = Meshss->GetSocketTransform(WeaponSocketName);
    }
    else
    {
        return;
    }

    // Spawn avec vérification du propriétaire
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.Instigator = this;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    // Création de l'arme
    CurrentWeapon = GetWorld()->SpawnActor<AWormWeapon>(
        AvailableWeapons[CurrentWeaponIndex],
        SpawnTransform,
        SpawnParams
    );

    if (CurrentWeapon)
    {
        // Attachement direct avec une seule méthode
        CurrentWeapon->AttachToComponent(GetMesh(),
            FAttachmentTransformRules::SnapToTargetIncludingScale,
            WeaponSocketName);
            
        // Force une update réseau
        ForceNetUpdate();
        
        // Attendre un court instant avant de propager aux clients
        FTimerHandle TimerHandle;
        GetWorld()->GetTimerManager().SetTimer(TimerHandle, [this]()
        {
            Multicast_WeaponChanged();
        }, 0.1f, false);
    }
}
void AWormCharacter::ApplyDamageToWorm(float DamageAmount, FVector ImpactDirection)
{
    // Appliquer les dégâts
    Health = FMath::Max(0.0f, Health - DamageAmount);
        
    // L'impulsion est déjà incluse dans ImpactDirection, ne pas multiplier à nouveau
    // Juste normaliser pour être sûr
    ApplyMovementImpulse(ImpactDirection.GetSafeNormal(), ImpactDirection.Size());
        
    // Vérifier si le personnage est mort
    if (Health <= 0)
    {
        // Désactiver les collisions et le mouvement seulement si mort
        GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        GetCharacterMovement()->DisableMovement();
    }
    else
    {
        // Jouer l'animation de réaction aux dégâts si pas mort
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
            // Arrêter le mouvement horizontal à la fin du tour, mais laisser la gravité active
            FVector CurrentVelocity = GetCharacterMovement()->Velocity;
            GetCharacterMovement()->Velocity = FVector(0, 0, CurrentVelocity.Z);
            GetCharacterMovement()->MaxWalkSpeed = 0;
            
            // NE PAS désactiver complètement le mouvement
            // GetCharacterMovement()->DisableMovement(); // SUPPRIMER OU COMMENTER CETTE LIGNE
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

    // Calculer les angles relatifs au personnage
    const FRotator ActorRotation = GetActorRotation();
    float DeltaYaw = FMath::FindDeltaAngleDegrees(ActorRotation.Yaw, NewRotation.Yaw);
    
    // Clamper les angles avec une tolérance plus large
    const float Tolerance = 10.0f;
    float ClampedPitch = FMath::ClampAngle(NewRotation.Pitch, -MaxPitchAngle - Tolerance, MaxPitchAngle + Tolerance);
    float ClampedYaw = FMath::ClampAngle(DeltaYaw, -MaxYawAngle - Tolerance, MaxYawAngle + Tolerance);
    
    // Construire la rotation finale
    FRotator SafeRotation(ClampedPitch, ActorRotation.Yaw + ClampedYaw, 0.0f);

    // Appliquer la rotation
    if (CurrentWeapon)
    {
        CurrentWeapon->SetActorRotation(SafeRotation);
        Multicast_UpdateWeaponRotation(SafeRotation);
    }
}

void AWormCharacter::UpdateWeaponRotation()
{
    // Vérifications de base
    if (!IsLocallyControlled() || !CurrentWeapon || !bIsMyTurn)
    {
        return;
    }

    // Obtenir les rotations nécessaires
    const FRotator ActorRotation = GetActorRotation();
    const FRotator ControlRotation = GetControlRotation();
    
    // Calculer la rotation cible selon le mode
    FRotator TargetRotation;
    if (bIsInFirstPersonMode)
    {
        // Calculer les deltas d'angles
        float DeltaYaw = FMath::FindDeltaAngleDegrees(ActorRotation.Yaw, ControlRotation.Yaw);
        
        // Appliquer les limites
        float ClampedYaw = FMath::ClampAngle(DeltaYaw, -MaxYawAngle, MaxYawAngle);
        float ClampedPitch = FMath::ClampAngle(ControlRotation.Pitch, -MaxPitchAngle, MaxPitchAngle);
        
        TargetRotation = FRotator(ClampedPitch, ActorRotation.Yaw + ClampedYaw, 0.0f);
    }
    else
    {
        // En TPS, simplement aligner avec le personnage
        TargetRotation = FRotator(0.0f, ActorRotation.Yaw, 0.0f);
    }

    // Appliquer localement
    if (CurrentWeapon)
    {
        CurrentWeapon->SetActorRotation(TargetRotation);
    }
    
    // Throttling des envois réseau
    static float LastSendTime = 0.0f;
    const float CurrentTime = GetWorld()->GetTimeSeconds();
    const float MinTimeBetweenUpdates = 0.05f; // 20 updates par seconde
    
    if (CurrentTime - LastSendTime >= MinTimeBetweenUpdates) 
    {
        if (GetLocalRole() < ROLE_Authority)
        {
            Server_UpdateWeaponRotation(TargetRotation);
            LastSendTime = CurrentTime;
        }
    }
}

void AWormCharacter::Multicast_UpdateWeaponRotation_Implementation(FRotator NewRotation)
{
    // Ne pas appliquer sur le client qui a envoyé la rotation
    if (!IsLocallyControlled() && CurrentWeapon)
    {
        CurrentWeapon->SetActorRotation(NewRotation);
    }
}

void AWormCharacter::AttachWeaponToSocket(AWormWeapon* Weapon)
{
    if (!Weapon || !GetMesh())
    {
        return;
    }

    // Détacher d'abord
    Weapon->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    
    // Réattacher avec des règles strictes
    Weapon->AttachToComponent(GetMesh(),
        FAttachmentTransformRules::SnapToTargetIncludingScale,
        WeaponSocketName);
        
    // S'assurer que l'arme est visible
    Weapon->EnsureWeaponVisibility();
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
    // Ne rien faire sur le serveur
    if (HasAuthority())
    {
        return;
    }

    // Vérification index valide
    if (!AvailableWeapons.IsValidIndex(CurrentWeaponIndex))
    {
        return;
    }

    // Vérifier si on a déjà la bonne arme
    if (CurrentWeapon && CurrentWeapon->IsA(AvailableWeapons[CurrentWeaponIndex]))
    {
        AttachWeaponToSocket(CurrentWeapon);
        return;
    }

    // Destruction propre de l'arme existante
    if (CurrentWeapon)
    {
        AWormWeapon* WeaponToDestroy = CurrentWeapon;
        CurrentWeapon = nullptr;
        WeaponToDestroy->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
        WeaponToDestroy->Destroy();
    }

    // Création de la nouvelle arme avec la transformation du socket
    FTransform SpawnTransform = GetMesh()->GetSocketTransform(WeaponSocketName);
    
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    CurrentWeapon = GetWorld()->SpawnActor<AWormWeapon>(
        AvailableWeapons[CurrentWeaponIndex],
        SpawnTransform,
        SpawnParams
    );

    if (CurrentWeapon)
    {
        AttachWeaponToSocket(CurrentWeapon);
        CurrentWeapon->EnsureWeaponVisibility();
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

// void AWormCharacter::OnToggleWeaponWheelAction(const FInputActionValue& Value)
// {
//     // Visual on-screen message - visible during gameplay
//     GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, TEXT("G KEY PRESSED - Toggle Weapon Wheel Action Called"));
//     
//     // Console log message
//     UE_LOG(LogTemp, Warning, TEXT("G KEY PRESSED - Toggle Weapon Wheel Action Called"));
//     
//     // Call the original function
//     ToggleWeaponWheel();
// }

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

