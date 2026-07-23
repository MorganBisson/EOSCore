#include "Session/EOSSessionBuilder.h"

#include "Core/EOSCoreConstants.h"
#include "EOSCoreSettings.h"
#include "Misc/PackageName.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSessionSettings.h"


bool FEOSSessionBuilder::TryBuildHostSettings(
	const FEOSHostSessionRequest& Request,
	const UEOSCoreSettings& CoreSettings,
	FOnlineSessionSettings& OutSettings,
	FString& OutMapPackageName,
	FString& OutError)
{
	if (!TryResolveMapPackageName(Request.MapPath, CoreSettings, OutMapPackageName, OutError))
	{
		return false;
	}

	if (Request.SessionCode.IsEmpty())
	{
		OutError = TEXT("The session code is empty.");
		return false;
	}
	if (CoreSettings.MatchingVersion.TrimStartAndEnd().IsEmpty())
	{
		OutError = TEXT("The matchmaking version is empty in EOS Core settings.");
		return false;
	}

	OutSettings = FOnlineSessionSettings();
	OutSettings.bIsLANMatch = Request.bIsLAN;
	OutSettings.bUsesPresence = CoreSettings.bUsePresence;
	OutSettings.bShouldAdvertise = CoreSettings.bShouldAdvertise;
	OutSettings.bUseLobbiesIfAvailable = !Request.bIsLAN && CoreSettings.bUseLobbies;
	OutSettings.bAllowJoinInProgress = CoreSettings.bAllowJoinInProgress;
	OutSettings.bAllowInvites = CoreSettings.bAllowInvites;
	OutSettings.bAllowJoinViaPresence = CoreSettings.bUsePresence;
	OutSettings.bUseLobbiesVoiceChatIfAvailable = !Request.bIsLAN && CoreSettings.bEnableLobbyVoiceChat;
	OutSettings.NumPublicConnections = FMath::Clamp(
		Request.MaxPlayers > 0 ? Request.MaxPlayers : CoreSettings.DefaultMaxPlayers,
		1,
		64);

	OutSettings.Set(
		SETTING_MAPNAME,
		FPackageName::GetShortName(OutMapPackageName),
		EOnlineDataAdvertisementType::ViaOnlineService);
	OutSettings.Set(
		UE::EOSCore::LobbyCodeKey,
		Request.SessionCode,
		EOnlineDataAdvertisementType::ViaOnlineService);
	OutSettings.Set(
		UE::EOSCore::MatchingVersionKey,
		CoreSettings.MatchingVersion,
		EOnlineDataAdvertisementType::ViaOnlineService);

	return true;
}

TSharedRef<FOnlineSessionSearch> FEOSSessionBuilder::BuildSearch(
	const FEOSFindSessionRequest& Request,
	const UEOSCoreSettings& CoreSettings)
{
	TSharedRef<FOnlineSessionSearch> Search = MakeShared<FOnlineSessionSearch>();
	Search->bIsLanQuery = Request.bIsLAN;
	Search->MaxSearchResults = FMath::Clamp(CoreSettings.MaxSearchResults, 1, 100);
	if (!Request.bIsLAN && CoreSettings.bUseLobbies)
	{
		Search->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
	}
	Search->QuerySettings.Set(
		UE::EOSCore::MatchingVersionKey,
		CoreSettings.MatchingVersion,
		EOnlineComparisonOp::Equals);

	if (Request.bFilterByCode)
	{
		Search->QuerySettings.Set(
			UE::EOSCore::LobbyCodeKey,
			Request.SessionCode,
			EOnlineComparisonOp::Equals);
	}

	return Search;
}

bool FEOSSessionBuilder::TryResolveMapPackageName(
	const FString& RequestedMapPath,
	const UEOSCoreSettings& CoreSettings,
	FString& OutMapPackageName,
	FString& OutError)
{
	FString Candidate = RequestedMapPath.TrimStartAndEnd();
	if (Candidate.IsEmpty())
	{
		Candidate = CoreSettings.DefaultGameplayMap.GetLongPackageName();
	}

	if (Candidate.Contains(TEXT(".")))
	{
		Candidate = FPackageName::ObjectPathToPackageName(Candidate);
	}

	if (!FPackageName::IsValidLongPackageName(Candidate, true))
	{
		OutError = FString::Printf(TEXT("'%s' is not a valid Unreal map package path."), *Candidate);
		return false;
	}

	OutMapPackageName = MoveTemp(Candidate);
	return true;
}
