#include "EOSCoreSettings.h"

UEOSCoreSettings::UEOSCoreSettings()
	: MatchingVersion(TEXT("1.0"))
	, MaxSearchResults(20)
	, DefaultMaxPlayers(4)
	, bUseLobbies(true)
	, bUsePresence(true)
	, bShouldAdvertise(true)
	, bEnableLobbyVoiceChat(false)
	, bAllowJoinInProgress(true)
	, bAllowInvites(true)
{
}
