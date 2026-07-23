#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "EOSCoreSubsystem.generated.h"

class FEOSAuthManager;
class FEOSSessionManager;
class FEOSTravelManager;
class UNetDriver;
class UWorld;
struct FEOSCoreResult;
struct FEOSHostOutcome;
struct FEOSJoinOutcome;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEOSLoginComplete, bool, bWasSuccessful);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEOSLobbyCreated, bool, bWasSuccessful);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEOSLobbyJoined, bool, bWasSuccessful);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEOSSessionDestroyed, bool, bWasSuccessful);

/**
 * Blueprint-facing facade for EOS authentication, sessions and travel.
 *
 * The Online Subsystem implementation lives in private, non-UObject managers so this
 * subsystem only owns their lifecycle and forwards stable Blueprint events.
 */
UCLASS()
class EOSCORE_API UEOSCoreSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual ~UEOSCoreSubsystem() override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UPROPERTY(BlueprintAssignable, Category = "EOS Core|Events")
	FOnEOSLoginComplete OnLoginCompleteEvent;

	UPROPERTY(BlueprintAssignable, Category = "EOS Core|Events")
	FOnEOSLobbyCreated OnLobbyCreatedEvent;

	UPROPERTY(BlueprintAssignable, Category = "EOS Core|Events")
	FOnEOSLobbyJoined OnLobbyJoinedEvent;

	UPROPERTY(BlueprintAssignable, Category = "EOS Core|Events")
	FOnEOSSessionDestroyed OnSessionDestroyedEvent;

	UFUNCTION(BlueprintCallable, Category = "EOS Core|Auth")
	void LoginToEOS();

	UFUNCTION(BlueprintPure, Category = "EOS Core|Auth")
	bool IsLoggedIn() const;

	UFUNCTION(BlueprintCallable, Category = "EOS Core|Matchmaking")
	void HostEOSLobby(int32 MaxPlayers, const FString& MapPath, bool bIsLAN = false);

	UFUNCTION(BlueprintCallable, Category = "EOS Core|Matchmaking")
	void FindAndJoinEOSLobby(bool bIsLanQuery = false);

	UFUNCTION(BlueprintCallable, Category = "EOS Core|Matchmaking")
	void FindAndJoinEOSLobbyByCode(const FString& SessionCode, bool bIsLanQuery = false);

	UFUNCTION(BlueprintCallable, Category = "EOS Core|Matchmaking")
	void DestroyEOSSession();

	UPROPERTY(BlueprintReadOnly, Category = "EOS Core|Matchmaking")
	FString CurrentSessionCode;

private:
	void HandleLoginComplete(const FEOSCoreResult& Result);
	void HandleSessionCreated(const FEOSHostOutcome& Outcome);
	void HandleSessionJoined(const FEOSJoinOutcome& Outcome);
	void HandleSessionDestroyed(const FEOSCoreResult& Result);
	void HandleNetworkFailure(
		UWorld* FailureWorld,
		UNetDriver* NetDriver,
		ENetworkFailure::Type FailureType,
		const FString& ErrorString);
	void HandleTravelFailure(
		UWorld* FailureWorld,
		ETravelFailure::Type FailureType,
		const FString& ErrorString);
	void RequestSessionCleanup(const FString& Reason);
	bool IsFailureForThisGameInstance(UWorld* FailureWorld, const UNetDriver* NetDriver = nullptr) const;

	TSharedPtr<FEOSAuthManager> AuthManager;
	TSharedPtr<FEOSSessionManager> SessionManager;
	TSharedPtr<FEOSTravelManager> TravelManager;
	FDelegateHandle NetworkFailureHandle;
	FDelegateHandle TravelFailureHandle;
	bool bIsDeinitializing = false;
};
