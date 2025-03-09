#include "NetworkLoadingManager.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"

UNetworkLoadingManager::UNetworkLoadingManager()
{
    // For performance, only tick on clients where UI updates matter
    PrimaryComponentTick.bCanEverTick = false;
    
    // This component should replicate
    SetIsReplicatedByDefault(true);
    
    // Initialize loading state
    bIsLoadingActive = false;
    LoadingProgress = 0.0f;
    LoadingStatusText = TEXT("Initializing...");
}

void UNetworkLoadingManager::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    
    // Replicate loading state to all clients
    DOREPLIFETIME(UNetworkLoadingManager, bIsLoadingActive);
    DOREPLIFETIME(UNetworkLoadingManager, LoadingProgress);
    DOREPLIFETIME(UNetworkLoadingManager, LoadingStatusText);
}

void UNetworkLoadingManager::BeginPlay()
{
    Super::BeginPlay();
    
    // Log component initialization
    UE_LOG(LogTemp, Log, TEXT("NetworkLoadingManager initialized on %s"),
        GetOwner()->HasAuthority() ? TEXT("server") : TEXT("client"));
}

void UNetworkLoadingManager::ShowLoadingScreen(float Duration)
{
    // Only the server can control the loading screen
    if (!GetOwner()->HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("ShowLoadingScreen called on client - ignoring"));
        return;
    }
    
    // Set loading state
    bIsLoadingActive = true;
    LoadingProgress = 0.0f;
    
    // Reset status text
    LoadingStatusText = TEXT("Loading game...");
    
    // Broadcast to all clients
    Multicast_ShowLoadingScreen(Duration);
    
    UE_LOG(LogTemp, Log, TEXT("Server: Showing loading screen with duration %.1f"), Duration);
}

void UNetworkLoadingManager::UpdateLoadingProgress(float Progress, const FString& StatusText)
{
    // Only the server can update loading progress
    if (!GetOwner()->HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("UpdateLoadingProgress called on client - ignoring"));
        return;
    }
    
    // Update loading state
    LoadingProgress = FMath::Clamp(Progress, 0.0f, 1.0f);
    LoadingStatusText = StatusText;
    
    // Broadcast to all clients
    Multicast_UpdateLoadingProgress(LoadingProgress, LoadingStatusText);
    
    UE_LOG(LogTemp, Log, TEXT("Server: Updating loading progress: %.2f - %s"), 
        LoadingProgress, *LoadingStatusText);
}

void UNetworkLoadingManager::DismissLoadingScreen()
{
    // Only the server can dismiss the loading screen
    if (!GetOwner()->HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("DismissLoadingScreen called on client - ignoring"));
        return;
    }
    
    // Set loading state to inactive
    bIsLoadingActive = false;
    
    // Broadcast to all clients
    Multicast_DismissLoadingScreen();
    
    UE_LOG(LogTemp, Log, TEXT("Server: Dismissing loading screen"));
}

void UNetworkLoadingManager::Multicast_ShowLoadingScreen_Implementation(float Duration)
{
    // Skip on server, we only need to create UI on clients
    if (GetOwner()->HasAuthority() && !IsRunningDedicatedServer())
    {
        // For listen servers, create loading UI for the hosting player
        EnsureLoadingWidgetExists();
        
        if (LoadingWidget)
        {
            LoadingWidget->ShowLoadingScreen(Duration);
        }
        
        return;
    }
    
    // Create the loading widget if it doesn't exist
    EnsureLoadingWidgetExists();
    
    // Show the loading screen
    if (LoadingWidget)
    {
        LoadingWidget->ShowLoadingScreen(Duration);
        UE_LOG(LogTemp, Log, TEXT("Client: Showing loading screen with duration %.1f"), Duration);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Client: Failed to create loading widget!"));
    }
}

void UNetworkLoadingManager::Multicast_UpdateLoadingProgress_Implementation(float Progress, const FString& StatusText)
{
    // Skip on server unless it's a listen server
    if (GetOwner()->HasAuthority() && !IsRunningDedicatedServer())
    {
        if (LoadingWidget)
        {
            LoadingWidget->SetLoadingProgress(Progress, StatusText);
        }
        return;
    }
    
    // Update the loading widget
    if (LoadingWidget)
    {
        LoadingWidget->SetLoadingProgress(Progress, StatusText);
        UE_LOG(LogTemp, Verbose, TEXT("Client: Updating loading progress: %.2f - %s"), 
            Progress, *StatusText);
    }
}

void UNetworkLoadingManager::Multicast_DismissLoadingScreen_Implementation()
{
    // Skip on server unless it's a listen server
    if (GetOwner()->HasAuthority() && !IsRunningDedicatedServer())
    {
        if (LoadingWidget)
        {
            LoadingWidget->DismissLoadingScreen();
        }
        return;
    }
    
    // Dismiss the loading screen
    if (LoadingWidget)
    {
        LoadingWidget->DismissLoadingScreen();
        UE_LOG(LogTemp, Log, TEXT("Client: Dismissing loading screen"));
    }
}

void UNetworkLoadingManager::EnsureLoadingWidgetExists()
{
    // If the widget already exists, we're done
    if (LoadingWidget)
    {
        return;
    }
    
    // Make sure we have a valid class
    if (!LoadingWidgetClass)
    {
        UE_LOG(LogTemp, Error, TEXT("LoadingWidgetClass is not set!"));
        return;
    }
    
    // Get a PlayerController to create the widget
    APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    if (!PC)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to get PlayerController!"));
        return;
    }
    
    // Create the widget
    LoadingWidget = CreateWidget<UGameLoadingWidget>(PC, LoadingWidgetClass);
    if (LoadingWidget)
    {
        // Initialize loading widget with current progress
        LoadingWidget->SetLoadingProgress(LoadingProgress, LoadingStatusText);
        
        // Add it to the viewport with high Z-order to ensure it's on top
        LoadingWidget->AddToViewport(9999);
        UE_LOG(LogTemp, Log, TEXT("Loading widget created successfully"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to create loading widget!"));
    }
}