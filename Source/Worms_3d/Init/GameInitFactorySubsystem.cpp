#include "GameInitFactorySubsystem.h"

#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

void UGameInitFactorySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    
    UE_LOG(LogTemp, Log, TEXT("GameInitFactorySubsystem initialized"));
}

void UGameInitFactorySubsystem::Deinitialize()
{
    UE_LOG(LogTemp, Log, TEXT("GameInitFactorySubsystem deinitialized"));
    
    Super::Deinitialize();
}

AGameInitManager* UGameInitFactorySubsystem::GetOrCreateGameInitManager(UObject* WorldContextObject)
{
    // Check if we have a valid world context
    UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
    if (!World)
    {
        UE_LOG(LogTemp, Error, TEXT("GetOrCreateGameInitManager: Invalid world context"));
        return nullptr;
    }
    
    // Check if we already have a valid instance
    if (GameInitManagerInstance.IsValid())
    {
        return GameInitManagerInstance.Get();
    }
    
    // Try to find an existing instance in the world
    for (TActorIterator<AGameInitManager> It(World); It; ++It)
    {
        AGameInitManager* ExistingManager = *It;
        if (ExistingManager)
        {
            UE_LOG(LogTemp, Log, TEXT("Found existing GameInitManager: %s"), *ExistingManager->GetName());
            GameInitManagerInstance = ExistingManager;
            return ExistingManager;
        }
    }
    
    // No existing instance found, create a new one
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    
    AGameInitManager* NewManager = World->SpawnActor<AGameInitManager>(
        AGameInitManager::StaticClass(),
        FVector::ZeroVector,
        FRotator::ZeroRotator,
        SpawnParams
    );
    
    if (NewManager)
    {
        UE_LOG(LogTemp, Log, TEXT("Created new GameInitManager: %s"), *NewManager->GetName());
        
        // Configure the new manager
        if (LoadingWidgetClass)
        {
            NewManager->LoadingWidgetClass = LoadingWidgetClass;
        }
        
        // Save the reference
        GameInitManagerInstance = NewManager;
        return NewManager;
    }
    
    UE_LOG(LogTemp, Error, TEXT("Failed to create GameInitManager"));
    return nullptr;
}

TSubclassOf<UGameLoadingWidget> UGameInitFactorySubsystem::GetLoadingWidgetClass() const
{
    return LoadingWidgetClass;
}

void UGameInitFactorySubsystem::SetLoadingWidgetClass(TSubclassOf<UGameLoadingWidget> NewWidgetClass)
{
    LoadingWidgetClass = NewWidgetClass;
}