#include "Managers/EOSTravelManager.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

FEOSTravelManager::FEOSTravelManager(UGameInstance* InGameInstance)
	: GameInstance(InGameInstance)
{
}

FEOSCoreResult FEOSTravelManager::StartListenTravel(const FString& MapPackageName, const bool bIsLAN) const
{
	const UGameInstance* GameInstancePtr = GameInstance.Get();
	UWorld* World = GameInstancePtr ? GameInstancePtr->GetWorld() : nullptr;
	if (!World)
	{
		return FEOSCoreResultFactory::Failure(
			EEOSCoreError::TravelFailure,
			TEXT("Listen travel failed because the GameInstance has no active world."));
	}

	const FString TravelURL = bIsLAN
		? FString::Printf(TEXT("%s?listen?bIsLanMatch=1"), *MapPackageName)
		: FString::Printf(TEXT("%s?listen"), *MapPackageName);
	if (!World->ServerTravel(TravelURL, true))
	{
		return FEOSCoreResultFactory::Failure(
			EEOSCoreError::TravelFailure,
			FString::Printf(TEXT("ServerTravel rejected URL '%s'."), *TravelURL));
	}

	return FEOSCoreResultFactory::Success(TEXT("Listen travel started."));
}

FEOSCoreResult FEOSTravelManager::StartClientTravel(const FString& ConnectString) const
{
	if (ConnectString.IsEmpty())
	{
		return FEOSCoreResultFactory::Failure(
			EEOSCoreError::CouldNotResolveAddress,
			TEXT("Client travel failed because the resolved connection string is empty."));
	}

	UGameInstance* GameInstancePtr = GameInstance.Get();
	APlayerController* PlayerController = GameInstancePtr ? GameInstancePtr->GetFirstLocalPlayerController() : nullptr;
	if (!PlayerController)
	{
		return FEOSCoreResultFactory::Failure(
			EEOSCoreError::TravelFailure,
			TEXT("Client travel failed because no local PlayerController is available."));
	}

	// The configured GameNetDriver owns transport selection. The OSS-resolved URL must not be rewritten here.
	PlayerController->ClientTravel(ConnectString, TRAVEL_Absolute);
	return FEOSCoreResultFactory::Success(TEXT("Client travel started."));
}

void FEOSTravelManager::Shutdown()
{
	GameInstance.Reset();
}
