#pragma once

#include "CoreMinimal.h"

namespace UE::EOSCore
{
	extern const FName EOSSubsystemName;
	extern const FName SteamSubsystemName;
	extern const FName LobbyCodeKey;
	extern const FName MatchingVersionKey;

	inline constexpr int32 LocalUserNum = 0;
	inline constexpr int32 DefaultSessionCodeLength = 6;
	inline constexpr TCHAR CommandLineAuthType[] = TEXT("AUTH_TYPE=");
	inline constexpr TCHAR CommandLineAuthLogin[] = TEXT("AUTH_LOGIN=");
	inline constexpr TCHAR CommandLineAuthPassword[] = TEXT("AUTH_PASSWORD=");
	inline constexpr TCHAR SteamEOSWebApiTokenType[] = TEXT("WebAPI:epiconlineservices");
	inline constexpr TCHAR SteamEOSCredentialType[] = TEXT("externalauth:SteamSessionTicket");
	inline constexpr TCHAR EpicAccountPortalCredentialType[] = TEXT("accountportal");
}
