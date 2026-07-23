#pragma once

#include "CoreMinimal.h"
#include "Interfaces/OnlineSessionInterface.h"

#include "Core/EOSCoreResultFactory.h"
#include "Session/EOSSessionBuilder.h"

class UGameInstance;

struct FEOSHostOutcome
{
	FEOSCoreResult Result;
	FString MapPackageName;
	bool bIsLAN = false;
};

struct FEOSJoinOutcome
{
	FEOSCoreResult Result;
	FString ConnectString;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnEOSSessionCreatedNative, const FEOSHostOutcome&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnEOSSessionJoinedNative, const FEOSJoinOutcome&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnEOSSessionDestroyedNative, const FEOSCoreResult&);

class FEOSSessionManager final : public TSharedFromThis<FEOSSessionManager>
{
public:
	explicit FEOSSessionManager(UGameInstance* InGameInstance);

	void Initialize();
	void HostSession(const FEOSHostSessionRequest& Request);
	void FindAndJoinSession(const FEOSFindSessionRequest& Request);
	void DestroySession();
	void Shutdown();
	bool HasActiveSession() const;

	const FString& GetCurrentSessionCode() const
	{
		return CurrentSessionCode;
	}

	FOnEOSSessionCreatedNative& OnSessionCreated()
	{
		return SessionCreatedEvent;
	}

	FOnEOSSessionJoinedNative& OnSessionJoined()
	{
		return SessionJoinedEvent;
	}

	FOnEOSSessionDestroyedNative& OnSessionDestroyed()
	{
		return SessionDestroyedEvent;
	}

private:
	enum class EOperation : uint8
	{
		Idle,
		Creating,
		Searching,
		Joining,
		Destroying,
		ShuttingDown
	};

	bool RefreshSessionInterface();
	bool CanStartOperation() const;
	void HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void HandleFindSessionsComplete(bool bWasSuccessful);
	void HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful);
	void HandleDestroySessionRequested(int32 LocalUserNum, FName SessionName);
	void BindDestroyRequestedDelegate();
	void ClearCreateDelegate();
	void ClearFindDelegate();
	void ClearJoinDelegate();
	void ClearDestroyDelegate();
	void ClearDestroyRequestedDelegate();
	void ClearAllDelegates();

	TWeakObjectPtr<UGameInstance> GameInstance;
	IOnlineSessionPtr SessionInterface;
	TSharedPtr<FOnlineSessionSearch> SessionSearch;
	FDelegateHandle CreateCompleteHandle;
	FDelegateHandle FindCompleteHandle;
	FDelegateHandle JoinCompleteHandle;
	FDelegateHandle DestroyCompleteHandle;
	FDelegateHandle DestroyRequestedHandle;

	FString PendingMapPackageName;
	FString PendingSessionCode;
	FString CurrentSessionCode;
	bool bPendingHostIsLAN = false;
	EOperation Operation = EOperation::Idle;

	FOnEOSSessionCreatedNative SessionCreatedEvent;
	FOnEOSSessionJoinedNative SessionJoinedEvent;
	FOnEOSSessionDestroyedNative SessionDestroyedEvent;
};
