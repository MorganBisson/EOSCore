#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UObject/SoftObjectPath.h"

#include "EOSCoreSettings.generated.h"

/** Project-wide defaults used when building EOS sessions and searches. */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "EOS Core Network"))
class EOSCORE_API UEOSCoreSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UEOSCoreSettings();

	/** Network protocol version used to keep incompatible builds out of the same lobby. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Matchmaking")
	FString MatchingVersion;

	/** Maximum number of lobbies returned by a session search. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Matchmaking", meta = (ClampMin = "1", ClampMax = "100"))
	int32 MaxSearchResults;

	/** Lobby capacity used when a Blueprint host request passes zero or less. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Matchmaking", meta = (ClampMin = "1", ClampMax = "64"))
	int32 DefaultMaxPlayers;

	/** Use Epic's Lobby API for non-LAN sessions. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Lobby Settings")
	bool bUseLobbies;

	/** Publish the session through the local player's online presence. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Lobby Settings")
	bool bUsePresence;

	/** Advertise the lobby in public session searches. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Lobby Settings")
	bool bShouldAdvertise;

	/** Create an EOS voice channel alongside a supported lobby. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Lobby Settings")
	bool bEnableLobbyVoiceChat;

	/** Allow new players to join after the session has started. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Lobby Settings")
	bool bAllowJoinInProgress;

	/** Allow session invites through supported platform overlays. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Lobby Settings")
	bool bAllowInvites;

	/** Main menu map used by higher-level return-to-menu flows. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Maps & Travel", meta = (AllowedClasses = "/Script/Engine.World"))
	FSoftObjectPath MainMenuMap;

	/** Gameplay map used when a host request does not provide one. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Maps & Travel", meta = (AllowedClasses = "/Script/Engine.World"))
	FSoftObjectPath DefaultGameplayMap;
};
