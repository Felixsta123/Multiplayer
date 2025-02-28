#include "AWormCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInput/Public/EnhancedInputComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Kismet/GameplayStatics.h"
#include "EnhancedInputComponent.h"
#include "WormGameMode.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputSubsystems.h"
#include "WormWeapon.h"
#include "Blueprint/UserWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"


// Conserve le constructeur existant sans modifications
AWormCharacter::AWormCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    
    // Configurer la réplication
    bReplicates = true;
    
    // Définir les valeurs par défaut
    Health = 100.0f;
    bIsMyTurn = false;
    CurrentWeaponIndex = 0;
    WeaponSocketName = "WeaponSocket";
    MaxMovementPoints = 100.0f;
    MovementPoints = MaxMovementPoints;
    WeaponCooldown = 0.5f;
    LastWeaponUseTime = 0.0f;
    CurrentCameraMode = ECameraMode::ThirdPerson;

    // Configurer le Character Movement Component
    GetCharacterMovement()->GravityScale = 1.5f;
    GetCharacterMovement()->AirControl = 0.8f;
    GetCharacterMovement()->JumpZVelocity = 600.0f;
    GetCharacterMovement()->BrakingDecelerationWalking = 2000.0f;
    bAutoEndTurnTimerActive = false;
    // Créer le Spring Arm Component (le bras qui contrôle la distance de la caméra)
    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(RootComponent);
    CameraBoom->TargetArmLength = 300.0f; // Distance initiale de la caméra
    CameraBoom->bUsePawnControlRotation = true; // La caméra suit la rotation du contrôleur
    
    // Créer la caméra principale (TPS)
    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    FollowCamera->bUsePawnControlRotation = false; // Le bras ajuste déjà la rotation
    
    // Créer la caméra FPS (pour le mode visée)
    FPSCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FPSCamera"));
    FPSCamera->SetupAttachment(GetMesh(), TEXT("head")); // Utiliser le socket de la tête
    FPSCamera->bUsePawnControlRotation = true;
    FPSCamera->SetActive(false); // Désactivée par défaut
    
    // Valeurs par défaut pour le zoom de caméra
    DefaultCameraDistance = 300.0f;
    MinCameraDistance = 100.0f;
    MaxCameraDistance = 800.0f;
    CameraZoomSpeed = 50.0f;
    CurrentCameraDistance = DefaultCameraDistance;
    bUseFirstPersonViewWhenAiming = true;
    TempCamera = nullptr;

}

// Modifie BeginPlay pour initialiser l'Enhanced Input
void AWormCharacter::BeginPlay()
{
    Super::BeginPlay();
    
    LastPosition = GetActorLocation();
    LastWeaponUseTime = UGameplayStatics::GetTimeSeconds(this) - (WeaponCooldown * 2);
    if (CameraBoom)
    {
        CurrentCameraDistance = DefaultCameraDistance;
        CameraBoom->TargetArmLength = CurrentCameraDistance;
    }

    ValidateCameraComponents();
    
    if (FollowCamera)
    {
        OriginalCameraLocation = FollowCamera->GetRelativeLocation();
        UE_LOG(LogTemp, Warning, TEXT("Saved original camera location: %s"), 
            *OriginalCameraLocation.ToString());
    }

    // Sauvegarder la position originale si ce n'est pas déjà fait
    if (FollowCamera && OriginalCameraLocation.IsZero())
    {
        OriginalCameraLocation = FollowCamera->GetRelativeLocation();
        UE_LOG(LogTemp, Warning, TEXT("Saved original camera location in BeginPlay: %s"), 
            *OriginalCameraLocation.ToString());
    }
    
    // Force le mode TPS au démarrage
    SetCameraMode(ECameraMode::ThirdPerson);
    // Configurer l'Enhanced Input pour le joueur local
    if (IsLocallyControlled())
    {
        if (APlayerController* PC = Cast<APlayerController>(GetController()))
        {
            SetupEnhancedInput(PC);
        }
        
        // Si c'est un client, on va programmer un diagnostic périodique
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
                        
                        // Si on n'a toujours pas d'arme après 3 tentatives, forcer la création
                        if (DiagnosticCount >= 3 && !CurrentWeapon && AvailableWeapons.Num() > 0)
                        {
                            UE_LOG(LogTemp, Warning, TEXT("Emergency weapon creation for client"));
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
}

// Nouvelle fonction pour gérer quand un controller possède ce personnage
void AWormCharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);
    
    // Configure l'Enhanced Input si c'est un joueur qui contrôle ce personnage
    if (IsLocallyControlled())
    {
        if (APlayerController* PC = Cast<APlayerController>(NewController))
        {
            SetupEnhancedInput(PC);
        }
    }
}

// Fonction utilitaire pour configurer l'Enhanced Input
void AWormCharacter::SetupEnhancedInput(APlayerController* PlayerController)
{
    if (!PlayerController || !InputMappingContext)
    {
        return;
    } 
    // Get the local player subsystem
    if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
    {
        // Clear existing mappings and add our mapping context
        Subsystem->ClearAllMappings();
        Subsystem->AddMappingContext(InputMappingContext, 0);
        UE_LOG(LogTemp, Log, TEXT("Enhanced Input setup for %s"), *GetName());
    }
}

// Remplace la fonction SetupPlayerInputComponent pour utiliser l'Enhanced Input
void AWormCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    
    // Cast to Enhanced Input Component
    if (UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(PlayerInputComponent))
    {
        UE_LOG(LogTemp, Log, TEXT("Setting up Enhanced Input Component for %s"), *GetName());
        // Binding for movement
        if (MoveAction)
        {
            EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AWormCharacter::OnMoveAction);
        }
        if (TestCameraAction)
        {
            EnhancedInputComponent->BindAction(TestCameraAction, ETriggerEvent::Started, this, &AWormCharacter::OnTestCameraAction);
        }

        
        // Binding for jumping
        if (JumpAction)
        {
            EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Triggered, this, &AWormCharacter::OnJumpAction);
            EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &AWormCharacter::OnJumpActionReleased);
        }
        if (ZoomAction)
        {
            EnhancedInputComponent->BindAction(ZoomAction, ETriggerEvent::Triggered, this, &AWormCharacter::OnZoomAction);
        }
        // Binding for firing
        if (FireAction)
        {
            EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Triggered, this, &AWormCharacter::OnFireAction);
        }
        // Dans la méthode SetupPlayerInputComponent
        if (PowerUpAction)
        {
            EnhancedInputComponent->BindAction(PowerUpAction, ETriggerEvent::Triggered, this, &AWormCharacter::OnPowerUpAction);
        }

        if (PowerDownAction)
        {
            EnhancedInputComponent->BindAction(PowerDownAction, ETriggerEvent::Triggered, this, &AWormCharacter::OnPowerDownAction);
        }
        // Binding for weapon switching
        if (NextWeaponAction)
        {
            EnhancedInputComponent->BindAction(NextWeaponAction, ETriggerEvent::Triggered, this, &AWormCharacter::OnNextWeaponAction);
        }
        
        if (PrevWeaponAction)
        {
            EnhancedInputComponent->BindAction(PrevWeaponAction, ETriggerEvent::Triggered, this, &AWormCharacter::OnPrevWeaponAction);
        }
        // Binding for looking
        if (LookAction)
        {
            EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AWormCharacter::OnLookAction);
        }
        if (EndTurnAction)
        {
            EnhancedInputComponent->BindAction(EndTurnAction, ETriggerEvent::Triggered, this, &AWormCharacter::OnEndTurnAction);
        }
        if (AimAction)
        {
            EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Started, this, &AWormCharacter::OnAimActionStarted);
            EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Completed, this, &AWormCharacter::OnAimActionEnded);
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to setup Enhanced Input Component for %s. Make sure to enable the Enhanced Input plugin!"), *GetName());
        
        // Fallback to legacy input system if Enhanced Input is not available
        PlayerInputComponent->BindAxis("MoveForward", this, &AWormCharacter::MoveForward);
        PlayerInputComponent->BindAxis("MoveRight", this, &AWormCharacter::MoveRight);
        
        PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
        PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);
        
        PlayerInputComponent->BindAction("Fire", IE_Pressed, this, &AWormCharacter::FireWeapon);
        
        PlayerInputComponent->BindAction("NextWeapon", IE_Pressed, this, &AWormCharacter::NextWeapon);
        PlayerInputComponent->BindAction("PrevWeapon", IE_Pressed, this, &AWormCharacter::PrevWeapon);

        PlayerInputComponent->BindAction("EndTurn", IE_Pressed, this, &AWormCharacter::EndTurn);

    }
}

// Nouvelles fonctions de callback pour l'Enhanced Input

void AWormCharacter::OnMoveAction(const FInputActionValue& Value)
{
    // Ne rien faire si ce n'est pas notre tour ou si on n'a plus de points de mouvement
    UE_LOG(LogTemp, Log, TEXT("OnMoveAction"));
    if (!bIsMyTurn || MovementPoints <= 0 || !Controller)
    {
        UE_LOG(LogTemp, Log, TEXT("Not my turn or no movement points : %f, %d"), MovementPoints, bIsMyTurn);
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
    if (CameraBoom)
    {
        // Calculer la nouvelle distance en fonction de l'entrée
        CurrentCameraDistance = FMath::Clamp(
            CurrentCameraDistance - (Amount * CameraZoomSpeed),
            MinCameraDistance,
            MaxCameraDistance
        );
        
        // Appliquer la nouvelle distance au spring arm
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
    if (!bIsMyTurn || MovementPoints <= 0)
    {
        return;
    }
    
    Jump();
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

void AWormCharacter::Tick(float DeltaTime)
{

    Super::Tick(DeltaTime);

    // Mise à jour de l'arme seulement si on est contrôlé localement et qu'on a une arme
    if (IsLocallyControlled() && WeaponPivotComponent && CurrentWeapon)
    {
        FRotator ControlRotation = GetControlRotation();
        WeaponPivotComponent->SetWorldRotation(FRotator(0.0f, ControlRotation.Yaw, 0.0f));
    
        // Envoyer au serveur si c'est un client et que c'est notre tour
        if (GetLocalRole() < ROLE_Authority && bIsMyTurn)
        {
            // Envoyer moins souvent pour réduire le trafic réseau (toutes les 5 frames)
            static int32 FrameCounter = 0;
            if (++FrameCounter >= 5)
            {
                Server_UpdateWeaponRotation(ControlRotation);
                FrameCounter = 0;
            }
        }
    }
        
    // Si c'est mon tour, consommer des points de mouvement basés sur la distance parcourue
    if (bIsMyTurn && HasAuthority())
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
            if (MovementPoints <= 0)
            {
                GetCharacterMovement()->MaxWalkSpeed = 0;
                
                // Vérifier si on vient juste d'épuiser les points de mouvement
                if (PreviousMovementPoints > 0 && !bAutoEndTurnTimerActive)
                {
                    // Démarrer le timer de fin automatique de tour (3 secondes)
                    GetWorldTimerManager().SetTimer(AutoEndTurnTimerHandle, this, &AWormCharacter::OnAutoEndTurnTimerExpired, 3.0f, false);
                    bAutoEndTurnTimerActive = true;
                    
                    UE_LOG(LogTemp, Log, TEXT("Movement points depleted for %s. Auto-end turn in 3 seconds."), *GetName());
                }
            }
        }
        
        LastPosition = CurrentPosition;
        if (AimingWidget && CurrentWeapon)
        {
            UpdateAimingWidget();
        }
    }
    
    // Limiter le mouvement quand ce n'est pas le tour du personnage
    LimitMovementWhenNotMyTurn();
}

void AWormCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    
    // Répliquer les variables
    DOREPLIFETIME(AWormCharacter, bIsMyTurn);
    DOREPLIFETIME(AWormCharacter, Health);
    DOREPLIFETIME(AWormCharacter, CurrentWeaponIndex);
    DOREPLIFETIME(AWormCharacter, CurrentWeapon);
    DOREPLIFETIME(AWormCharacter, MovementPoints);
    DOREPLIFETIME(AWormCharacter, AvailableWeapons);
    DOREPLIFETIME(AWormCharacter, WeaponPivotComponent);
}
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
    if (bIsMyTurn && CurrentWeapon && (CurrentTime - LastWeaponUseTime >= WeaponCooldown))
    {
        // Cacher la trajectoire AVANT de tirer
        if (CurrentWeapon)
        {
            CurrentWeapon->ShowTrajectory(false);
        }
        
        // Mettre à jour le timestamp
        LastWeaponUseTime = CurrentTime;
        UE_LOG(LogTemp, Warning, TEXT("FireWeapon called on %s (IsLocallyControlled: %s)"), 
    *GetName(), IsLocallyControlled() ? TEXT("Yes") : TEXT("No"));
        UE_LOG(LogTemp, Warning, TEXT("  - bIsMyTurn: %s"), bIsMyTurn ? TEXT("Yes") : TEXT("No"));
        UE_LOG(LogTemp, Warning, TEXT("  - CurrentWeapon: %s"), CurrentWeapon ? *CurrentWeapon->GetName() : TEXT("NULL"));
        UE_LOG(LogTemp, Warning, TEXT("  - AvailableWeapons.Num(): %d"), AvailableWeapons.Num());
        UE_LOG(LogTemp, Warning, TEXT("  - CurrentWeaponIndex: %d"), CurrentWeaponIndex);
        // Appeler le RPC serveur
        if (GetLocalRole() < ROLE_Authority)
        {
            Server_FireWeapon();
            UE_LOG(LogTemp, Log, TEXT("Firing weapon for %s on RPC"), *GetName());
        }
        else
        {
            // Sur le serveur, appeler directement
            CurrentWeapon->Fire();
            UE_LOG(LogTemp, Log, TEXT("Firing weapon for %s"), *GetName());
            UGameplayStatics::SpawnEmitterAtLocation(
                  GetWorld(),
                  FireEffect,
                  GetActorLocation() + FVector(0, 0, 100),  // Au-dessus du personnage
                  FRotator::ZeroRotator,
                  FVector(3, 3, 3)  // Grande taille pour être visible
              );
            // Jouer l'animation de tir
            if (FireMontage)
            {
                PlayAnimMontage(FireMontage);
            }
        }
    } else {
        UE_LOG(LogTemp, Log, TEXT("Cannot fire weapon for %s"), *GetName());
        //more logs
        UE_LOG(LogTemp, Log, TEXT("bIsMyTurn: %d"), bIsMyTurn);
        UE_LOG(LogTemp, Log, TEXT("CurrentWeapon: %s"), CurrentWeapon ? *CurrentWeapon->GetName() : TEXT("None"));
        UE_LOG(LogTemp, Log, TEXT("CurrentTime: %f"), CurrentTime);
        UE_LOG(LogTemp, Log, TEXT("LastWeaponUseTime: %f"), LastWeaponUseTime);
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
    UE_LOG(LogTemp, Warning, TEXT("SpawnCurrentWeapon called for %s (HasAuthority: %s)"), 
        *GetName(), HasAuthority() ? TEXT("Yes") : TEXT("No"));
    
    if (!HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("SpawnCurrentWeapon called on client - should not happen"));
        return;
    }
    
    if (!AvailableWeapons.IsValidIndex(CurrentWeaponIndex))
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid weapon index %d (max: %d)"), 
            CurrentWeaponIndex, AvailableWeapons.Num() - 1);
        return;
    }
    
    // Détruire l'ancienne arme si elle existe
    if (CurrentWeapon)
    {
        CurrentWeapon->Destroy();
        CurrentWeapon = nullptr;
    }
    
    // Spawner la nouvelle arme
    FTransform SpawnTransform;
    if (GetMesh()->DoesSocketExist(WeaponSocketName))
    {
        // Obtenir la transformation du socket
        SpawnTransform = GetMesh()->GetSocketTransform(WeaponSocketName);
        
        // Corriger la rotation pour qu'elle pointe toujours devant le personnage
        FRotator ControlRotation = GetControlRotation();
        // Garder seulement le Yaw (rotation horizontale) du controller
        FRotator NewRotation(0.0f, ControlRotation.Yaw, 0.0f);
        
        // Optionnel: Ajouter un léger offset de pitch pour pointer légèrement vers le haut
        NewRotation.Pitch = -10.0f; // Valeur négative pour pointer vers le haut
        
        // Appliquer la nouvelle rotation à la transformation de spawn
        SpawnTransform.SetRotation(NewRotation.Quaternion());
    }
    else
    {
        SpawnTransform = GetActorTransform();
        SpawnTransform.AddToTranslation(FVector(50.0f, 0.0f, 0.0f));
    }
    
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.Instigator = GetInstigator();
    
    CurrentWeapon = GetWorld()->SpawnActor<AWormWeapon>(
        AvailableWeapons[CurrentWeaponIndex], 
        SpawnTransform, 
        SpawnParams
    );
    
    if (CurrentWeapon)
    {
        // Créer d'abord le composant pivot pour la rotation de l'arme
        if (WeaponPivotComponent)
        {
            WeaponPivotComponent->DestroyComponent();
        }
        
        WeaponPivotComponent = NewObject<USceneComponent>(this, TEXT("WeaponPivot"));
        WeaponPivotComponent->RegisterComponent();
        
        if (GetMesh()->DoesSocketExist(WeaponSocketName))
        {
            WeaponPivotComponent->AttachToComponent(GetMesh(), 
                FAttachmentTransformRules::SnapToTargetNotIncludingScale, 
                WeaponSocketName);
                
            // Appliquer la rotation initiale au pivot
            FRotator ControlRotation = GetControlRotation();
            WeaponPivotComponent->SetWorldRotation(FRotator(0.0f, ControlRotation.Yaw, 0.0f));
        }
        else
        {
            WeaponPivotComponent->AttachToComponent(GetRootComponent(), 
                FAttachmentTransformRules::SnapToTargetNotIncludingScale);
        }
        
        // Attacher l'arme au pivot
        CurrentWeapon->AttachToComponent(WeaponPivotComponent, 
            FAttachmentTransformRules::SnapToTargetNotIncludingScale);
        
        CurrentWeapon->SetOwner(this);
        UE_LOG(LogTemp, Warning, TEXT("Server created weapon: %s, attached to pivot: %p"),
            *CurrentWeapon->GetName(), WeaponPivotComponent);
            
        // Forcer une mise à jour réseau
        ForceNetUpdate();
        
        // Notifier les clients du changement d'arme
        Multicast_WeaponChanged();
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to create weapon!"));
    }
}
void AWormCharacter::Multicast_WeaponChanged_Implementation()
{
    // Cette fonction est appelée sur tous les clients, pas besoin de vérifier l'autorité
    
    // Si nous ne sommes pas le serveur et que nous avons des armes
    if (!HasAuthority() && AvailableWeapons.Num() > 0)
    {
        // S'assurer que l'index est valide
        if (AvailableWeapons.IsValidIndex(CurrentWeaponIndex))
        {
            UE_LOG(LogTemp, Warning, TEXT("Client received weapon change notification"));
            
            // Détruire l'arme actuelle si elle existe
            if (CurrentWeapon)
            {
                CurrentWeapon->Destroy();
                CurrentWeapon = nullptr;
            }
            
            // On réutilise SpawnCurrentWeapon, mais ça ne devrait pas créer d'armes sur le client
            // C'est juste pour la visualisation
            // Si cela cause des problèmes, il faudrait dupliquer la logique ici
            SpawnCurrentWeapon();
        }
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
    UE_LOG(LogTemp, Warning, TEXT("OnRep_CurrentWeaponIndex called on %s (IsLocallyControlled: %s)"), 
        *GetName(), IsLocallyControlled() ? TEXT("Yes") : TEXT("No"));
    
    // Détruire l'arme actuelle si elle existe
    if (CurrentWeapon)
    {
        CurrentWeapon->Destroy();
        CurrentWeapon = nullptr;
    }
    
    // Vérifier si nous avons des armes disponibles
    if (AvailableWeapons.Num() <= 0)
    {
        UE_LOG(LogTemp, Error, TEXT("No available weapons for client %s"), *GetName());
        return;
    }
    
    // Vérifier que l'index est valide
    if (!AvailableWeapons.IsValidIndex(CurrentWeaponIndex))
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid weapon index %d (max: %d) for client %s"), 
            CurrentWeaponIndex, AvailableWeapons.Num() - 1, *GetName());
        return;
    }
    
    // Créer d'abord le composant pivot pour la rotation
    if (WeaponPivotComponent)
    {
        WeaponPivotComponent->DestroyComponent();
    }
    
    WeaponPivotComponent = NewObject<USceneComponent>(this, TEXT("WeaponPivot"));
    WeaponPivotComponent->RegisterComponent();
    
    if (GetMesh()->DoesSocketExist(WeaponSocketName))
    {
        WeaponPivotComponent->AttachToComponent(GetMesh(), 
            FAttachmentTransformRules::SnapToTargetNotIncludingScale, 
            WeaponSocketName);
            
        // Appliquer la rotation initiale au pivot
        FRotator ControlRotation = GetControlRotation();
        WeaponPivotComponent->SetWorldRotation(FRotator(0.0f, ControlRotation.Yaw, 0.0f));
    }
    else
    {
        WeaponPivotComponent->AttachToComponent(GetRootComponent(), 
            FAttachmentTransformRules::SnapToTargetNotIncludingScale);
    }
    
    // Créer l'arme
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.Instigator = GetInstigator();
    
    // Spawner au niveau du pivot
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
        
        UE_LOG(LogTemp, Warning, TEXT("Client successfully created weapon: %s"), *CurrentWeapon->GetName());
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Client failed to create weapon!"));
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
        // Log pour déboguer (en utilisant bNewTurn, pas bIsMyTurn qui n'est pas encore mis à jour)
        UE_LOG(LogTemp, Log, TEXT("Setting turn for %s to %s"), 
            *GetName(), bNewTurn ? TEXT("true") : TEXT("false"));
        
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
        if (bNewTurn && IsLocallyControlled())
        {
            UE_LOG(LogTemp, Warning, TEXT("My turn started - validating camera components"));
            ValidateCameraComponents();
        }
        // Appeler l'événement BlueprintNativeEvent seulement si l'état a changé
        if (bOldTurn != bIsMyTurn)
        {
            OnTurnChanged(bIsMyTurn);
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Attempted to set turn state on client. This should only be called on server."));
    }
}

void AWormCharacter::OnTurnChanged_Implementation(bool bIsTurn)
{
    // Cette fonction peut être surchargée en Blueprint
    // Si vous voulez ajouter du comportement par défaut en C++, faites-le ici
    
    // Par exemple: Activer/désactiver une indication visuelle du tour actif
    UE_LOG(LogTemp, Log, TEXT("Turn changed for %s: %s"), 
        *GetName(), bIsTurn ? TEXT("It's my turn") : TEXT("Turn ended"));
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
        UE_LOG(LogTemp, Log, TEXT("Auto-ending turn for %s after movement points depletion"), *GetName());
        
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
void AWormCharacter::OnEndTurnAction(const FInputActionValue& Value)
{
    EndTurn();
}

void AWormCharacter::EndTurn()
{
    // Vérifier si c'est notre tour
    if (bIsMyTurn)
    {
        UE_LOG(LogTemp, Log, TEXT("Player %s manually ending turn"), *GetName());
        
        // Appeler le RPC serveur si on est sur un client
        if (GetLocalRole() < ROLE_Authority)
        {
            Server_EndTurn();
        }
        else
        {
            // Sur le serveur, terminer le tour directement
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
    // Journaliser pour déboguer
    UE_LOG(LogTemp, Warning, TEXT("[%s] SetAvailableWeapons called, HasAuthority: %s, Received %d weapon types"), 
        *GetName(), HasAuthority() ? TEXT("Yes") : TEXT("No"), WeaponTypes.Num());
    
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
                    UE_LOG(LogTemp, Warning, TEXT("Client triggering weapon creation manually"));
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
    UE_LOG(LogTemp, Warning, TEXT("===== WEAPON DIAGNOSTIC for %s ====="), *GetName());
    UE_LOG(LogTemp, Warning, TEXT("HasAuthority: %s"), HasAuthority() ? TEXT("Yes") : TEXT("No"));
    UE_LOG(LogTemp, Warning, TEXT("IsLocallyControlled: %s"), IsLocallyControlled() ? TEXT("Yes") : TEXT("No"));
    UE_LOG(LogTemp, Warning, TEXT("AvailableWeapons.Num(): %d"), AvailableWeapons.Num());
    UE_LOG(LogTemp, Warning, TEXT("CurrentWeaponIndex: %d"), CurrentWeaponIndex);
    UE_LOG(LogTemp, Warning, TEXT("CurrentWeapon: %s"), CurrentWeapon ? *CurrentWeapon->GetName() : TEXT("NULL"));
    if (!HasAuthority() && IsLocallyControlled())
    {
        UE_LOG(LogTemp, Warning, TEXT("Client-specific weapon diagnosis:"));
        UE_LOG(LogTemp, Warning, TEXT("  - Control Rotation: %s"), *GetControlRotation().ToString());
        if (WeaponPivotComponent)
        {
            UE_LOG(LogTemp, Warning, TEXT("  - Weapon Pivot Rotation: %s"), *WeaponPivotComponent->GetComponentRotation().ToString());
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("  - Weapon Pivot Component is NULL"));
        }
    }
    if (GetMesh())
    {
        UE_LOG(LogTemp, Warning, TEXT("Socket '%s' exists: %s"), 
            *WeaponSocketName.ToString(), 
            GetMesh()->DoesSocketExist(WeaponSocketName) ? TEXT("Yes") : TEXT("No"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Character has no mesh!"));
    }
    
    // Essayer de forcer la création de l'arme
    if (!CurrentWeapon && AvailableWeapons.Num() > 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("Trying to force weapon creation..."));
        if (HasAuthority())
        {
            SpawnCurrentWeapon();
        }
        else
        {
            OnRep_CurrentWeaponIndex();
        }
    }
    UE_LOG(LogTemp, Warning, TEXT("====================================="));
}


void AWormCharacter::SetAiming(bool bIsAiming)
{
    // Si on a une arme, activer/désactiver la prévisualisation
    if (CurrentWeapon)
    {
        // Appeler la méthode pour montrer/cacher la trajectoire
        CurrentWeapon->ShowTrajectory(bIsAiming);
    }
    
    // Basculer le mode de caméra si activé
    if (bUseFirstPersonViewWhenAiming)
    {
        if (bIsAiming)
        {
            SwitchToFirstPersonView();
        }
        else
        {
            SwitchToThirdPersonView();
        }
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

void AWormCharacter::ToggleCameraMode(bool bFirstPerson)
{
    // Vérifier que les deux caméras existent avant de continuer
    if (!FollowCamera || !FPSCamera)
    {
        UE_LOG(LogTemp, Error, TEXT("Camera components are null! FollowCamera: %p, FPSCamera: %p"), 
            FollowCamera, FPSCamera);
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("ToggleCameraMode: Switching to %s view"), 
        bFirstPerson ? TEXT("first-person") : TEXT("third-person"));
        
    // Activer/désactiver les caméras
    FollowCamera->SetActive(!bFirstPerson);
    FPSCamera->SetActive(bFirstPerson);
    
    // Vérifier l'état après activation
    UE_LOG(LogTemp, Warning, TEXT("Camera activation: FollowCamera: %s, FPSCamera: %s"),
        FollowCamera->IsActive() ? TEXT("Active") : TEXT("Inactive"),
        FPSCamera->IsActive() ? TEXT("Active") : TEXT("Inactive"));
    
    // Contrôle de la visibilité du mesh en FPS pour éviter les clips/clipping
    if (bFirstPerson)
    {
        // Option 2: Ajuster la distance de rendu (meilleur)
        FollowCamera->SetRelativeLocation(FVector(0, 0, -50));
    }
    else
    {
        // Restaurer la visibilité normale
        FollowCamera->SetRelativeLocation(OriginalCameraLocation);
        UE_LOG(LogTemp, Warning, TEXT("Restored camera to position: %s"), 
            *OriginalCameraLocation.ToString());
    }
}
void AWormCharacter::OnAimActionStarted(const FInputActionValue& Value)
{
    // Valider les caméras avant d'activer le mode visée
    if (IsLocallyControlled())
    {
        ValidateCameraComponents();
    }
    
    SetAiming(true);
}

void AWormCharacter::OnAimActionEnded(const FInputActionValue& Value)
{
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
        WeaponPivotComponent->SetWorldRotation(FRotator(0.0f, NewRotation.Yaw, 0.0f));
        
        // Propager aux clients via Multicast si nécessaire
        Multicast_UpdateWeaponRotation(NewRotation);
    }
}


void AWormCharacter::Multicast_UpdateWeaponRotation_Implementation(FRotator NewRotation)
{
    // Ne pas exécuter sur le client qui a envoyé la rotation (pour éviter les doublons)
    if (!IsLocallyControlled() && WeaponPivotComponent)
    {
        WeaponPivotComponent->SetWorldRotation(FRotator(0.0f, NewRotation.Yaw, 0.0f));
    }
}

void AWormCharacter::ValidateCameraComponents()
{
    UE_LOG(LogTemp, Warning, TEXT("Validating camera components for %s"), *GetName());
    
    bool bNeedsRecreation = false;
    
    // Vérifier si CameraBoom existe
    if (!CameraBoom)
    {
        UE_LOG(LogTemp, Warning, TEXT("CameraBoom is missing, recreating..."));
        CameraBoom = NewObject<USpringArmComponent>(this, TEXT("RecreatedCameraBoom"));
        CameraBoom->SetupAttachment(RootComponent);
        CameraBoom->TargetArmLength = DefaultCameraDistance;
        CameraBoom->bUsePawnControlRotation = true;
        CameraBoom->RegisterComponent();
        bNeedsRecreation = true;
    }
    
    // Vérifier FollowCamera
    if (!FollowCamera)
    {
        UE_LOG(LogTemp, Warning, TEXT("FollowCamera is missing, recreating..."));
        FollowCamera = NewObject<UCameraComponent>(this, TEXT("RecreatedFollowCamera"));
        FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
        FollowCamera->bUsePawnControlRotation = false;
        FollowCamera->RegisterComponent();
        FollowCamera->SetActive(true);
        bNeedsRecreation = true;
    }
    
    // Vérifier FPSCamera
    if (!FPSCamera)
    {
        UE_LOG(LogTemp, Warning, TEXT("FPSCamera is missing, recreating..."));
        FPSCamera = NewObject<UCameraComponent>(this, TEXT("RecreatedFPSCamera"));
        FPSCamera->SetupAttachment(GetMesh(), TEXT("head"));
        FPSCamera->bUsePawnControlRotation = true;
        FPSCamera->SetActive(false);
        FPSCamera->RegisterComponent();
        bNeedsRecreation = true;
    }
    
    // Si on a recréé des composants, réinitialiser les paramètres
    if (bNeedsRecreation)
    {
        // Sauvegarder la position originale
        if (FollowCamera)
        {
            OriginalCameraLocation = FollowCamera->GetRelativeLocation();
        }
        
        // Définir la distance de caméra
        if (CameraBoom)
        {
            CurrentCameraDistance = DefaultCameraDistance;
            CameraBoom->TargetArmLength = CurrentCameraDistance;
        }
    }
    
    // Vérifier l'état des caméras
    UE_LOG(LogTemp, Warning, TEXT("Camera components: CameraBoom: %p, FollowCamera: %p, FPSCamera: %p"),
        CameraBoom, FollowCamera, FPSCamera);
}

void AWormCharacter::SetCameraMode(ECameraMode NewMode)
{
    // Si le mode est déjà celui demandé, ne rien faire
    if (CurrentCameraMode == NewMode)
    {
        UE_LOG(LogTemp, Warning, TEXT("Camera is already in %s mode"), 
            NewMode == ECameraMode::FirstPerson ? TEXT("first-person") : TEXT("third-person"));
        return;
    }
    
    // Enregistrer l'ancien mode pour le log
    ECameraMode OldMode = CurrentCameraMode;
    
    // Définir le nouveau mode
    CurrentCameraMode = NewMode;
    
    UE_LOG(LogTemp, Warning, TEXT("SetCameraMode: Switching from %s to %s"),
        OldMode == ECameraMode::FirstPerson ? TEXT("first-person") : TEXT("third-person"),
        NewMode == ECameraMode::FirstPerson ? TEXT("first-person") : TEXT("third-person"));
    
    // Vérifier que les composants existent
    if (!FollowCamera || !FPSCamera || !CameraBoom)
    {
        UE_LOG(LogTemp, Error, TEXT("Camera components missing in SetCameraMode: CameraBoom: %p, FollowCamera: %p, FPSCamera: %p"),
            CameraBoom, FollowCamera, FPSCamera);
        
        // Tenter de récupérer les composants
        if (!CameraBoom) CameraBoom = FindComponentByClass<USpringArmComponent>();
        if (!FollowCamera) FollowCamera = FindComponentByClass<UCameraComponent>();
        if (!FPSCamera) 
        {
            // Recherche plus spécifique pour FPSCamera
            TArray<UCameraComponent*> Cameras;
            GetComponents<UCameraComponent>(Cameras);
            for (UCameraComponent* Camera : Cameras)
            {
                if (Camera != FollowCamera)
                {
                    FPSCamera = Camera;
                    break;
                }
            }
        }
        
        // Si toujours pas de caméras, sortir
        if (!FollowCamera || !FPSCamera || !CameraBoom)
        {
            UE_LOG(LogTemp, Error, TEXT("Still missing camera components after recovery attempt"));
            return;
        }
    }
    
    APlayerController* PC = Cast<APlayerController>(GetController());
    
    // Configurer les caméras selon le mode
    if (NewMode == ECameraMode::FirstPerson)
    {
        // Désactiver la caméra TPS
        if (FollowCamera)
        {
            FollowCamera->SetActive(false);
            UE_LOG(LogTemp, Verbose, TEXT("Deactivated FollowCamera"));
        }
        
        // Activer la caméra FPS
        if (FPSCamera)
        {
            FPSCamera->SetActive(true);
            UE_LOG(LogTemp, Verbose, TEXT("Activated FPSCamera"));
            
            // S'assurer que la caméra FPS est bien attachée à la tête
            if (!FPSCamera->IsAttachedTo(GetMesh()) && GetMesh())
            {
                FPSCamera->AttachToComponent(GetMesh(), 
                    FAttachmentTransformRules::SnapToTargetNotIncludingScale, 
                    TEXT("head"));
                UE_LOG(LogTemp, Verbose, TEXT("Attached FPSCamera to head socket"));
            }
        }
        
        // Cacher le mesh du personnage en FPS
        if (GetMesh())
        {
            GetMesh()->SetOwnerNoSee(true);
            UE_LOG(LogTemp, Verbose, TEXT("Set mesh to not visible to owner"));
        }
    }
    else // ThirdPerson
    {
        // Désactiver la caméra FPS
        if (FPSCamera)
        {
            FPSCamera->SetActive(false);
            UE_LOG(LogTemp, Verbose, TEXT("Deactivated FPSCamera"));
        }
        
        // Activer la caméra TPS
        if (FollowCamera)
        {
            FollowCamera->SetActive(true);
            UE_LOG(LogTemp, Verbose, TEXT("Activated FollowCamera"));
            
            // S'assurer que la caméra TPS est bien attachée au bras
            if (!FollowCamera->IsAttachedTo(CameraBoom) && CameraBoom)
            {
                FollowCamera->AttachToComponent(CameraBoom, 
                    FAttachmentTransformRules::SnapToTargetNotIncludingScale,
                    USpringArmComponent::SocketName);
                UE_LOG(LogTemp, Verbose, TEXT("Attached FollowCamera to CameraBoom"));
            }
            
            // Restaurer la position originale si nécessaire
            if (!OriginalCameraLocation.IsZero())
            {
                FollowCamera->SetRelativeLocation(OriginalCameraLocation);
                UE_LOG(LogTemp, Verbose, TEXT("Reset camera location to: %s"), 
                    *OriginalCameraLocation.ToString());
            }
        }
        
        // Rendre le mesh visible à nouveau
        if (GetMesh())
        {
            GetMesh()->SetOwnerNoSee(false);
            UE_LOG(LogTemp, Verbose, TEXT("Set mesh to visible to owner"));
        }
    }
    
    // Forcer la mise à jour du viewport
    if (PC && IsLocallyControlled())
    {
        // Utiliser SetViewTarget pour être sûr que la vue est mise à jour
        PC->SetViewTargetWithBlend(this, 0.1f);
        UE_LOG(LogTemp, Warning, TEXT("Updated viewtarget for player controller"));
    }
    
    // Vérifier l'état final des caméras
    UE_LOG(LogTemp, Warning, TEXT("Camera final state after SetCameraMode: FollowCamera: %s, FPSCamera: %s"),
        FollowCamera && FollowCamera->IsActive() ? TEXT("Active") : TEXT("Inactive"),
        FPSCamera && FPSCamera->IsActive() ? TEXT("Active") : TEXT("Inactive"));
}
bool AWormCharacter::IsViewingFromFirstPerson() const
{
    return (FPSCamera && FPSCamera->IsActive());
}

void AWormCharacter::ForceToggleCamera()
{
    if (IsViewingFromFirstPerson())
    {
        SwitchToThirdPersonView();
    }
    else
    {
        SwitchToFirstPersonView();
    }
}

void AWormCharacter::SwitchToFirstPersonView()
{
    UE_LOG(LogTemp, Warning, TEXT("SwitchToFirstPersonView called"));
    
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC || !IsLocallyControlled())
    {
        return;
    }
    
    // APPROCHE RADICALE : CRÉER UNE NOUVELLE CAMÉRA TEMPORAIRE
    if (TempCamera)
    {
        TempCamera->DestroyComponent();
    }
    
    TempCamera = NewObject<UCameraComponent>(this, TEXT("TempFPSCamera"));
    TempCamera->RegisterComponent();
    
    // La placer au même endroit que la caméra FPS
    if (GetMesh() && GetMesh()->DoesSocketExist(TEXT("head")))
    {
        TempCamera->AttachToComponent(GetMesh(), 
            FAttachmentTransformRules::SnapToTargetNotIncludingScale, 
            TEXT("head"));
        
        // Copier les paramètres de FPSCamera si disponible
        if (FPSCamera)
        {
            TempCamera->FieldOfView = FPSCamera->FieldOfView;
        }
    }
    
    // Désactiver les autres caméras, activer celle-ci
    if (FollowCamera) FollowCamera->SetActive(false);
    if (FPSCamera) FPSCamera->SetActive(true); // On garde quand même celle-ci active
    TempCamera->SetActive(true);
    
    // Manipuler directement le CameraManager
    PC->SetViewTargetWithBlend(this, 0.0f);
    
    // Force la mise à jour immédiate
    FMinimalViewInfo NewPOV;
    TempCamera->GetCameraView(0.0f, NewPOV);
    
    // Appliquer cette vue directement
    PC->PlayerCameraManager->SetViewTarget(this);    
    // Mise à jour forcée du viewport
    FViewTargetTransitionParams TransitionParams;
    TransitionParams.BlendTime = 0.0f;
    PC->PlayerCameraManager->SetViewTarget(this, TransitionParams);
    
    // On peut aussi essayer de forcer plus directement
    PC->PlayerCameraManager->PCOwner->SetControlRotation(TempCamera->GetComponentRotation());
    
    // Cacher le mesh pour éviter le clipping
    if (GetMesh())
    {
        GetMesh()->SetOwnerNoSee(true);
    }
    
    UE_LOG(LogTemp, Warning, TEXT("FORCE FIRST PERSON: TempCamera=%p, Location=%s, Rotation=%s"), 
        TempCamera, 
        *TempCamera->GetComponentLocation().ToString(),
        *TempCamera->GetComponentRotation().ToString());
        
    // Forcer une mise à jour du jeu
    if (GEngine && IsLocallyControlled())
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("SWITCHED TO FIRST PERSON"));
    }
}

void AWormCharacter::SwitchToThirdPersonView()
{
    UE_LOG(LogTemp, Warning, TEXT("SwitchToThirdPersonView called"));
    
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC || !IsLocallyControlled())
    {
        return;
    }
    
    // APPROCHE RADICALE : CRÉER UNE NOUVELLE CAMÉRA TEMPORAIRE
    if (TempCamera)
    {
        TempCamera->DestroyComponent();
    }
    
    TempCamera = NewObject<UCameraComponent>(this, TEXT("TempTPSCamera"));
    TempCamera->RegisterComponent();
    
    // La placer au même endroit que la caméra TPS
    if (CameraBoom)
    {
        TempCamera->AttachToComponent(CameraBoom, 
            FAttachmentTransformRules::SnapToTargetNotIncludingScale,
            USpringArmComponent::SocketName);
        
        // Copier les paramètres de FollowCamera si disponible
        if (FollowCamera)
        {
            TempCamera->FieldOfView = FollowCamera->FieldOfView;
            TempCamera->SetRelativeLocation(FollowCamera->GetRelativeLocation());
            TempCamera->SetRelativeRotation(FollowCamera->GetRelativeRotation());
        }
    }
    
    // Désactiver les autres caméras, activer celle-ci
    if (FPSCamera) FPSCamera->SetActive(false);
    if (FollowCamera) FollowCamera->SetActive(true); // On garde quand même celle-ci active
    TempCamera->SetActive(true);
    
    // Manipuler directement le CameraManager
    PC->SetViewTargetWithBlend(this, 0.0f);
    
    // Force la mise à jour immédiate
    FMinimalViewInfo NewPOV;
    TempCamera->GetCameraView(0.0f, NewPOV);
    
    // Appliquer cette vue directement
    PC->PlayerCameraManager->SetViewTarget(this);    
    // Mise à jour forcée du viewport
    FViewTargetTransitionParams TransitionParams;
    TransitionParams.BlendTime = 0.0f;
    PC->PlayerCameraManager->SetViewTarget(this, TransitionParams);
    
    // On peut aussi essayer de forcer plus directement
    PC->PlayerCameraManager->PCOwner->SetControlRotation(TempCamera->GetComponentRotation());
    
    // Rendre le mesh visible
    if (GetMesh())
    {
        GetMesh()->SetOwnerNoSee(false);
    }
    
    UE_LOG(LogTemp, Warning, TEXT("FORCE THIRD PERSON: TempCamera=%p, Location=%s, Rotation=%s"), 
        TempCamera, 
        *TempCamera->GetComponentLocation().ToString(),
        *TempCamera->GetComponentRotation().ToString());
        
    // Forcer une mise à jour du jeu
    if (GEngine && IsLocallyControlled())
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, TEXT("SWITCHED TO THIRD PERSON"));
    }
}

// Dans OnTestCameraAction et SetAiming, utilisez directement ces fonctions:
void AWormCharacter::OnTestCameraAction(const FInputActionValue& Value)
{
    if (IsViewingFromFirstPerson())
    {
        SwitchToThirdPersonView();
    }
    else
    {
        SwitchToFirstPersonView();
    }
}