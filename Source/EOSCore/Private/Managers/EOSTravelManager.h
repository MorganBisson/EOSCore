#pragma once

#include "CoreMinimal.h"
#include "Core/EOSCoreResultFactory.h"

class UGameInstance;

class FEOSTravelManager final
{
public:
	explicit FEOSTravelManager(UGameInstance* InGameInstance);

	FEOSCoreResult StartListenTravel(const FString& MapPackageName, bool bIsLAN) const;
	FEOSCoreResult StartClientTravel(const FString& ConnectString) const;
	void Shutdown();

private:
	TWeakObjectPtr<UGameInstance> GameInstance;
};
