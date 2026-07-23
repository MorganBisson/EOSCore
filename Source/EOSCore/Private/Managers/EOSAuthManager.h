#pragma once

#include "CoreMinimal.h"
#include "Interfaces/OnlineIdentityInterface.h"

#include "Core/EOSCoreResultFactory.h"

class UGameInstance;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnEOSAuthCompleteNative, const FEOSCoreResult&);

class FEOSAuthManager final : public TSharedFromThis<FEOSAuthManager>
{
public:
	explicit FEOSAuthManager(UGameInstance* InGameInstance);

	void Login();
	bool IsLoggedIn() const;
	void Shutdown();

	FOnEOSAuthCompleteNative& OnLoginComplete()
	{
		return LoginCompleteEvent;
	}

private:
	enum class EState : uint8
	{
		Idle,
		WaitingForExternalToken,
		LoggingIn,
		ShuttingDown
	};

	bool RefreshEOSIdentity();
	void LoginWithCommandLine();
	void LoginWithSteam(const IOnlineIdentityPtr& SteamIdentity);
	void LoginWithAccountPortal();
	void BeginEOSLogin(FOnlineAccountCredentials Credentials);
	void HandleSteamAuthToken(int32 LocalUserNum, bool bWasSuccessful, const FExternalAuthToken& AuthToken);
	void HandleEOSLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error);
	void Complete(FEOSCoreResult Result);
	void ClearLoginDelegate();

	TWeakObjectPtr<UGameInstance> GameInstance;
	IOnlineIdentityPtr EOSIdentity;
	FDelegateHandle LoginCompleteHandle;
	FOnEOSAuthCompleteNative LoginCompleteEvent;
	EState State = EState::Idle;
};
