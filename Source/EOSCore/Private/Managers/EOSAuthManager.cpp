#include "Managers/EOSAuthManager.h"

#include "Core/EOSCoreConstants.h"
#include "EOSCore.h"
#include "Engine/GameInstance.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"

FEOSAuthManager::FEOSAuthManager(UGameInstance* InGameInstance)
	: GameInstance(InGameInstance)
{
	RefreshEOSIdentity();
}

void FEOSAuthManager::Login()
{
	if (State == EState::ShuttingDown)
	{
		return;
	}

	if (State != EState::Idle)
	{
		const FEOSCoreResult BusyResult = FEOSCoreResultFactory::Failure(
			EEOSCoreError::OperationInProgress,
			TEXT("An authentication operation is already in progress."));
		LoginCompleteEvent.Broadcast(BusyResult);
		return;
	}

	if (!RefreshEOSIdentity())
	{
		Complete(FEOSCoreResultFactory::Failure(
			EEOSCoreError::NotInitialized,
			TEXT("The EOS identity interface is unavailable.")));
		return;
	}

	if (IsLoggedIn())
	{
		Complete(FEOSCoreResultFactory::Success(TEXT("The local player is already logged in.")));
		return;
	}

	FString CommandLineAuthType;
	if (FParse::Value(FCommandLine::Get(), UE::EOSCore::CommandLineAuthType, CommandLineAuthType))
	{
		UE_LOG(LogEOSCore, Log, TEXT("Using EOS command-line authentication."));
		LoginWithCommandLine();
		return;
	}

	UGameInstance* GameInstancePtr = GameInstance.Get();
	const UWorld* World = GameInstancePtr ? GameInstancePtr->GetWorld() : nullptr;
	IOnlineSubsystem* SteamSubsystem = World
		? Online::GetSubsystem(World, UE::EOSCore::SteamSubsystemName)
		: nullptr;
	const IOnlineIdentityPtr SteamIdentity = SteamSubsystem ? SteamSubsystem->GetIdentityInterface() : nullptr;

	if (SteamIdentity.IsValid()
		&& SteamIdentity->GetLoginStatus(UE::EOSCore::LocalUserNum) == ELoginStatus::LoggedIn)
	{
		UE_LOG(LogEOSCore, Log, TEXT("Using the signed-in Steam account for EOS authentication."));
		LoginWithSteam(SteamIdentity);
		return;
	}

	UE_LOG(LogEOSCore, Log, TEXT("Using the Epic Account Portal for EOS authentication."));
	LoginWithAccountPortal();
}

bool FEOSAuthManager::IsLoggedIn() const
{
	return EOSIdentity.IsValid()
		&& EOSIdentity->GetLoginStatus(UE::EOSCore::LocalUserNum) == ELoginStatus::LoggedIn;
}

void FEOSAuthManager::Shutdown()
{
	if (State == EState::ShuttingDown)
	{
		return;
	}

	State = EState::ShuttingDown;
	ClearLoginDelegate();
	LoginCompleteEvent.Clear();
	EOSIdentity.Reset();
	GameInstance.Reset();
}

bool FEOSAuthManager::RefreshEOSIdentity()
{
	if (EOSIdentity.IsValid())
	{
		return true;
	}

	UGameInstance* GameInstancePtr = GameInstance.Get();
	const UWorld* World = GameInstancePtr ? GameInstancePtr->GetWorld() : nullptr;
	IOnlineSubsystem* EOSSubsystem = World
		? Online::GetSubsystem(World, UE::EOSCore::EOSSubsystemName)
		: nullptr;
	EOSIdentity = EOSSubsystem ? EOSSubsystem->GetIdentityInterface() : nullptr;
	return EOSIdentity.IsValid();
}

void FEOSAuthManager::LoginWithCommandLine()
{
	FOnlineAccountCredentials Credentials;
	FParse::Value(FCommandLine::Get(), UE::EOSCore::CommandLineAuthType, Credentials.Type);
	FParse::Value(FCommandLine::Get(), UE::EOSCore::CommandLineAuthLogin, Credentials.Id);
	FParse::Value(FCommandLine::Get(), UE::EOSCore::CommandLineAuthPassword, Credentials.Token, false);

	Credentials.Type.ReplaceInline(TEXT("\""), TEXT(""));
	Credentials.Id.ReplaceInline(TEXT("\""), TEXT(""));
	Credentials.Token.ReplaceInline(TEXT("\""), TEXT(""));
	Credentials.Type.TrimStartAndEndInline();
	Credentials.Id.TrimStartAndEndInline();
	Credentials.Token.TrimStartAndEndInline();

	if (Credentials.Type.IsEmpty() || Credentials.Id.IsEmpty() || Credentials.Token.IsEmpty())
	{
		Complete(FEOSCoreResultFactory::Failure(
			EEOSCoreError::InvalidArgument,
			TEXT("AUTH_TYPE, AUTH_LOGIN and AUTH_PASSWORD are required for command-line authentication.")));
		return;
	}

	BeginEOSLogin(MoveTemp(Credentials));
}

void FEOSAuthManager::LoginWithSteam(const IOnlineIdentityPtr& SteamIdentity)
{
	State = EState::WaitingForExternalToken;
	const TWeakPtr<FEOSAuthManager> WeakThis = AsShared();
	SteamIdentity->GetLinkedAccountAuthToken(
		UE::EOSCore::LocalUserNum,
		UE::EOSCore::SteamEOSWebApiTokenType,
		IOnlineIdentity::FOnGetLinkedAccountAuthTokenCompleteDelegate::CreateLambda(
			[WeakThis](const int32 LocalUserNum, const bool bWasSuccessful, const FExternalAuthToken& AuthToken)
			{
				if (const TSharedPtr<FEOSAuthManager> PinnedThis = WeakThis.Pin())
				{
					PinnedThis->HandleSteamAuthToken(LocalUserNum, bWasSuccessful, AuthToken);
				}
			}));
}

void FEOSAuthManager::LoginWithAccountPortal()
{
	FOnlineAccountCredentials Credentials;
	Credentials.Type = UE::EOSCore::EpicAccountPortalCredentialType;
	BeginEOSLogin(MoveTemp(Credentials));
}

void FEOSAuthManager::BeginEOSLogin(FOnlineAccountCredentials Credentials)
{
	if (!EOSIdentity.IsValid())
	{
		Complete(FEOSCoreResultFactory::Failure(
			EEOSCoreError::NotInitialized,
			TEXT("The EOS identity interface became unavailable.")));
		return;
	}

	ClearLoginDelegate();
	State = EState::LoggingIn;
	const TWeakPtr<FEOSAuthManager> WeakThis = AsShared();
	LoginCompleteHandle = EOSIdentity->AddOnLoginCompleteDelegate_Handle(
		UE::EOSCore::LocalUserNum,
		FOnLoginCompleteDelegate::CreateLambda(
			[WeakThis](const int32 LocalUserNum, const bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error)
			{
				if (const TSharedPtr<FEOSAuthManager> PinnedThis = WeakThis.Pin())
				{
					PinnedThis->HandleEOSLoginComplete(LocalUserNum, bWasSuccessful, UserId, Error);
				}
			}));

	if (!EOSIdentity->Login(UE::EOSCore::LocalUserNum, Credentials) && State == EState::LoggingIn)
	{
		ClearLoginDelegate();
		Complete(FEOSCoreResultFactory::Failure(
			EEOSCoreError::RequestRejected,
			TEXT("The EOS identity interface rejected the login request.")));
	}
}

void FEOSAuthManager::HandleSteamAuthToken(
	const int32 LocalUserNum,
	const bool bWasSuccessful,
	const FExternalAuthToken& AuthToken)
{
	if (State != EState::WaitingForExternalToken || LocalUserNum != UE::EOSCore::LocalUserNum)
	{
		return;
	}

	if (!bWasSuccessful || AuthToken.TokenString.IsEmpty())
	{
		Complete(FEOSCoreResultFactory::Failure(
			EEOSCoreError::AuthenticationFailed,
			TEXT("Steam did not provide a valid EOS Web API authentication ticket.")));
		return;
	}

	FOnlineAccountCredentials Credentials;
	Credentials.Type = UE::EOSCore::SteamEOSCredentialType;
	Credentials.Token = AuthToken.TokenString;
	BeginEOSLogin(MoveTemp(Credentials));
}

void FEOSAuthManager::HandleEOSLoginComplete(
	const int32 LocalUserNum,
	const bool bWasSuccessful,
	const FUniqueNetId& UserId,
	const FString& Error)
{
	if (State != EState::LoggingIn || LocalUserNum != UE::EOSCore::LocalUserNum)
	{
		return;
	}

	ClearLoginDelegate();
	if (bWasSuccessful)
	{
		Complete(FEOSCoreResultFactory::Success(
			FString::Printf(TEXT("EOS login succeeded for user %s."), *UserId.ToString())));
	}
	else
	{
		Complete(FEOSCoreResultFactory::Failure(
			EEOSCoreError::AuthenticationFailed,
			Error.IsEmpty() ? TEXT("EOS login failed.") : Error));
	}
}

void FEOSAuthManager::Complete(FEOSCoreResult Result)
{
	if (State == EState::ShuttingDown)
	{
		return;
	}

	State = EState::Idle;
	LoginCompleteEvent.Broadcast(Result);
}

void FEOSAuthManager::ClearLoginDelegate()
{
	if (EOSIdentity.IsValid() && LoginCompleteHandle.IsValid())
	{
		EOSIdentity->ClearOnLoginCompleteDelegate_Handle(UE::EOSCore::LocalUserNum, LoginCompleteHandle);
	}
	LoginCompleteHandle.Reset();
}
