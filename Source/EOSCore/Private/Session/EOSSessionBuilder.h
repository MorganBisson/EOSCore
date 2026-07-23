#pragma once

#include "CoreMinimal.h"

class FOnlineSessionSearch;
class FOnlineSessionSettings;
class UEOSCoreSettings;

struct FEOSHostSessionRequest
{
	int32 MaxPlayers = 0;
	FString MapPath;
	FString SessionCode;
	bool bIsLAN = false;
};

struct FEOSFindSessionRequest
{
	FString SessionCode;
	bool bIsLAN = false;
	bool bFilterByCode = false;
};

class FEOSSessionBuilder final
{
public:
	static bool TryBuildHostSettings(
		const FEOSHostSessionRequest& Request,
		const UEOSCoreSettings& CoreSettings,
		FOnlineSessionSettings& OutSettings,
		FString& OutMapPackageName,
		FString& OutError);

	static TSharedRef<FOnlineSessionSearch> BuildSearch(
		const FEOSFindSessionRequest& Request,
		const UEOSCoreSettings& CoreSettings);

private:
	static bool TryResolveMapPackageName(
		const FString& RequestedMapPath,
		const UEOSCoreSettings& CoreSettings,
		FString& OutMapPackageName,
		FString& OutError);
};
