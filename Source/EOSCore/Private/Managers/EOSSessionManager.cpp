#include "Managers/EOSSessionManager.h"

#include "Core/EOSCoreConstants.h"
#include "EOSCore.h"
#include "EOSCoreSettings.h"
#include "Engine/GameInstance.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "Session/EOSSessionCodeGenerator.h"

FEOSSessionManager::FEOSSessionManager(UGameInstance* InGameInstance)
	: GameInstance(InGameInstance)
{
}

void FEOSSessionManager::Initialize()
{
	RefreshSessionInterface();
}

void FEOSSessionManager::HostSession(const FEOSHostSessionRequest& Request)
{
	if (Operation == EOperation::ShuttingDown)
	{
		return;
	}

	if (!CanStartOperation())
	{
		SessionCreatedEvent.Broadcast({
			FEOSCoreResultFactory::Failure(
				EEOSCoreError::OperationInProgress,
				TEXT("Another session operation is already in progress.")),
			FString(),
			Request.bIsLAN});
		return;
	}

	if (!RefreshSessionInterface())
	{
		SessionCreatedEvent.Broadcast({
			FEOSCoreResultFactory::Failure(
				EEOSCoreError::NotInitialized,
				TEXT("The EOS session interface is unavailable.")),
			FString(),
			Request.bIsLAN});
		return;
	}

	if (SessionInterface->GetNamedSession(NAME_GameSession))
	{
		SessionCreatedEvent.Broadcast({
			FEOSCoreResultFactory::Failure(
				EEOSCoreError::SessionAlreadyExists,
				TEXT("A game session already exists. Destroy it before hosting another one.")),
			FString(),
			Request.bIsLAN});
		return;
	}

	FEOSHostSessionRequest NormalizedRequest = Request;
	NormalizedRequest.SessionCode = FEOSSessionCodeGenerator::Generate(UE::EOSCore::DefaultSessionCodeLength);

	FOnlineSessionSettings SessionSettings;
	FString BuildError;
	const UEOSCoreSettings* CoreSettings = GetDefault<UEOSCoreSettings>();
	if (!CoreSettings || !FEOSSessionBuilder::TryBuildHostSettings(
		NormalizedRequest,
		*CoreSettings,
		SessionSettings,
		PendingMapPackageName,
		BuildError))
	{
		PendingMapPackageName.Reset();
		SessionCreatedEvent.Broadcast({
			FEOSCoreResultFactory::Failure(
				EEOSCoreError::InvalidArgument,
				BuildError.IsEmpty() ? TEXT("The host request is invalid.") : MoveTemp(BuildError)),
			FString(),
			Request.bIsLAN});
		return;
	}

	PendingSessionCode = NormalizedRequest.SessionCode;
	bPendingHostIsLAN = Request.bIsLAN;
	Operation = EOperation::Creating;

	const TWeakPtr<FEOSSessionManager> WeakThis = AsShared();
	CreateCompleteHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(
		FOnCreateSessionCompleteDelegate::CreateLambda(
			[WeakThis](const FName SessionName, const bool bWasSuccessful)
			{
				if (const TSharedPtr<FEOSSessionManager> PinnedThis = WeakThis.Pin())
				{
					PinnedThis->HandleCreateSessionComplete(SessionName, bWasSuccessful);
				}
			}));

	if (!SessionInterface->CreateSession(
		UE::EOSCore::LocalUserNum,
		NAME_GameSession,
		SessionSettings)
		&& Operation == EOperation::Creating)
	{
		ClearCreateDelegate();
		const FEOSHostOutcome Outcome{
			FEOSCoreResultFactory::Failure(
				EEOSCoreError::RequestRejected,
				TEXT("The online service rejected the create-session request.")),
			MoveTemp(PendingMapPackageName),
			bPendingHostIsLAN};
		PendingSessionCode.Reset();
		bPendingHostIsLAN = false;
		Operation = EOperation::Idle;
		SessionCreatedEvent.Broadcast(Outcome);
	}
}

void FEOSSessionManager::FindAndJoinSession(const FEOSFindSessionRequest& Request)
{
	if (Operation == EOperation::ShuttingDown)
	{
		return;
	}

	if (!CanStartOperation())
	{
		SessionJoinedEvent.Broadcast({
			FEOSCoreResultFactory::Failure(
				EEOSCoreError::OperationInProgress,
				TEXT("Another session operation is already in progress.")),
			FString()});
		return;
	}

	if (!RefreshSessionInterface())
	{
		SessionJoinedEvent.Broadcast({
			FEOSCoreResultFactory::Failure(
				EEOSCoreError::NotInitialized,
				TEXT("The EOS session interface is unavailable.")),
			FString()});
		return;
	}

	if (SessionInterface->GetNamedSession(NAME_GameSession))
	{
		SessionJoinedEvent.Broadcast({
			FEOSCoreResultFactory::Failure(
				EEOSCoreError::SessionAlreadyExists,
				TEXT("A game session already exists. Destroy it before joining another one.")),
			FString()});
		return;
	}

	FEOSFindSessionRequest NormalizedRequest = Request;
	if (NormalizedRequest.bFilterByCode)
	{
		NormalizedRequest.SessionCode = FEOSSessionCodeGenerator::Normalize(Request.SessionCode);
		if (!FEOSSessionCodeGenerator::IsValid(
			NormalizedRequest.SessionCode,
			UE::EOSCore::DefaultSessionCodeLength))
		{
			SessionJoinedEvent.Broadcast({
				FEOSCoreResultFactory::Failure(
					EEOSCoreError::InvalidArgument,
					FString::Printf(
						TEXT("A session code must contain exactly %d letters or digits."),
						UE::EOSCore::DefaultSessionCodeLength)),
				FString()});
			return;
		}
	}

	const UEOSCoreSettings* CoreSettings = GetDefault<UEOSCoreSettings>();
	if (!CoreSettings || CoreSettings->MatchingVersion.TrimStartAndEnd().IsEmpty())
	{
		SessionJoinedEvent.Broadcast({
			FEOSCoreResultFactory::Failure(
				CoreSettings ? EEOSCoreError::InvalidArgument : EEOSCoreError::NotInitialized,
				CoreSettings
					? TEXT("The matchmaking version is empty in EOS Core settings.")
					: TEXT("EOS Core settings are unavailable.")),
			FString()});
		return;
	}

	SessionSearch = FEOSSessionBuilder::BuildSearch(NormalizedRequest, *CoreSettings);
	Operation = EOperation::Searching;
	const TWeakPtr<FEOSSessionManager> WeakThis = AsShared();
	FindCompleteHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(
		FOnFindSessionsCompleteDelegate::CreateLambda(
			[WeakThis](const bool bWasSuccessful)
			{
				if (const TSharedPtr<FEOSSessionManager> PinnedThis = WeakThis.Pin())
				{
					PinnedThis->HandleFindSessionsComplete(bWasSuccessful);
				}
			}));

	if (!SessionInterface->FindSessions(UE::EOSCore::LocalUserNum, SessionSearch.ToSharedRef())
		&& Operation == EOperation::Searching)
	{
		ClearFindDelegate();
		SessionSearch.Reset();
		Operation = EOperation::Idle;
		SessionJoinedEvent.Broadcast({
			FEOSCoreResultFactory::Failure(
				EEOSCoreError::RequestRejected,
				TEXT("The online service rejected the session-search request.")),
			FString()});
	}
}

void FEOSSessionManager::DestroySession()
{
	if (Operation == EOperation::ShuttingDown)
	{
		return;
	}

	// Network and travel failures can be reported during the same frame. The
	// first destroy operation remains their shared completion point.
	if (Operation == EOperation::Destroying)
	{
		return;
	}

	if (!CanStartOperation())
	{
		SessionDestroyedEvent.Broadcast(FEOSCoreResultFactory::Failure(
			EEOSCoreError::OperationInProgress,
			TEXT("Another session operation is already in progress.")));
		return;
	}

	if (!RefreshSessionInterface())
	{
		SessionDestroyedEvent.Broadcast(FEOSCoreResultFactory::Failure(
			EEOSCoreError::NotInitialized,
			TEXT("The EOS session interface is unavailable.")));
		return;
	}

	if (!SessionInterface->GetNamedSession(NAME_GameSession))
	{
		CurrentSessionCode.Reset();
		SessionDestroyedEvent.Broadcast(FEOSCoreResultFactory::Success(TEXT("No game session exists.")));
		return;
	}

	Operation = EOperation::Destroying;
	const TWeakPtr<FEOSSessionManager> WeakThis = AsShared();
	DestroyCompleteHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(
		FOnDestroySessionCompleteDelegate::CreateLambda(
			[WeakThis](const FName SessionName, const bool bWasSuccessful)
			{
				if (const TSharedPtr<FEOSSessionManager> PinnedThis = WeakThis.Pin())
				{
					PinnedThis->HandleDestroySessionComplete(SessionName, bWasSuccessful);
				}
			}));

	if (!SessionInterface->DestroySession(NAME_GameSession)
		&& Operation == EOperation::Destroying)
	{
		ClearDestroyDelegate();
		Operation = EOperation::Idle;
		SessionDestroyedEvent.Broadcast(FEOSCoreResultFactory::Failure(
			EEOSCoreError::RequestRejected,
			TEXT("The online service rejected the destroy-session request.")));
	}
}

bool FEOSSessionManager::HasActiveSession() const
{
	return SessionInterface.IsValid()
		&& SessionInterface->GetNamedSession(NAME_GameSession) != nullptr;
}

void FEOSSessionManager::Shutdown()
{
	if (Operation == EOperation::ShuttingDown)
	{
		return;
	}

	const bool bSearchWasInProgress = Operation == EOperation::Searching;
	Operation = EOperation::ShuttingDown;
	ClearAllDelegates();
	if (bSearchWasInProgress && SessionInterface.IsValid())
	{
		SessionInterface->CancelFindSessions();
	}

	SessionCreatedEvent.Clear();
	SessionJoinedEvent.Clear();
	SessionDestroyedEvent.Clear();
	SessionSearch.Reset();
	SessionInterface.Reset();
	PendingMapPackageName.Reset();
	PendingSessionCode.Reset();
	CurrentSessionCode.Reset();
	GameInstance.Reset();
}

bool FEOSSessionManager::RefreshSessionInterface()
{
	if (SessionInterface.IsValid())
	{
		BindDestroyRequestedDelegate();
		return true;
	}

	ClearDestroyRequestedDelegate();
	UGameInstance* GameInstancePtr = GameInstance.Get();
	const UWorld* World = GameInstancePtr ? GameInstancePtr->GetWorld() : nullptr;
	IOnlineSubsystem* EOSSubsystem = World
		? Online::GetSubsystem(World, UE::EOSCore::EOSSubsystemName)
		: nullptr;
	SessionInterface = EOSSubsystem ? EOSSubsystem->GetSessionInterface() : nullptr;
	BindDestroyRequestedDelegate();
	return SessionInterface.IsValid();
}

bool FEOSSessionManager::CanStartOperation() const
{
	return Operation == EOperation::Idle;
}

void FEOSSessionManager::HandleCreateSessionComplete(const FName SessionName, const bool bWasSuccessful)
{
	if (Operation != EOperation::Creating || SessionName != NAME_GameSession)
	{
		return;
	}

	ClearCreateDelegate();
	FEOSHostOutcome Outcome;
	Outcome.MapPackageName = MoveTemp(PendingMapPackageName);
	Outcome.bIsLAN = bPendingHostIsLAN;
	if (bWasSuccessful)
	{
		CurrentSessionCode = MoveTemp(PendingSessionCode);
		Outcome.Result = FEOSCoreResultFactory::Success(TEXT("The EOS session was created."));

		if (SessionInterface.IsValid())
		{
			const FNamedOnlineSession* NamedSession = SessionInterface->GetNamedSession(SessionName);
			if (NamedSession && NamedSession->SessionInfo.IsValid())
			{
				UE_LOG(LogEOSCore, Log, TEXT("Created EOS session %s."), *NamedSession->SessionInfo->GetSessionId().ToString());
			}
		}
	}
	else
	{
		CurrentSessionCode.Reset();
		PendingSessionCode.Reset();
		Outcome.Result = FEOSCoreResultFactory::Failure(
			EEOSCoreError::OnlineServiceFailure,
			TEXT("The online service failed to create the session."));
	}
	bPendingHostIsLAN = false;
	Operation = EOperation::Idle;
	SessionCreatedEvent.Broadcast(Outcome);
}

void FEOSSessionManager::HandleFindSessionsComplete(const bool bWasSuccessful)
{
	if (Operation != EOperation::Searching)
	{
		return;
	}

	ClearFindDelegate();
	if (!bWasSuccessful || !SessionSearch.IsValid())
	{
		SessionSearch.Reset();
		Operation = EOperation::Idle;
		SessionJoinedEvent.Broadcast({
			FEOSCoreResultFactory::Failure(
				EEOSCoreError::OnlineServiceFailure,
				TEXT("The online service failed to search for sessions.")),
			FString()});
		return;
	}

	const FOnlineSessionSearchResult* Match = nullptr;
	bool bFoundFullSession = false;
	for (const FOnlineSessionSearchResult& SearchResult : SessionSearch->SearchResults)
	{
		if (!SearchResult.IsSessionInfoValid())
		{
			continue;
		}

		if (SearchResult.Session.NumOpenPublicConnections > 0)
		{
			Match = &SearchResult;
			break;
		}

		bFoundFullSession = true;
	}

	if (!Match)
	{
		SessionSearch.Reset();
		Operation = EOperation::Idle;
		SessionJoinedEvent.Broadcast({
			FEOSCoreResultFactory::Failure(
				bFoundFullSession ? EEOSCoreError::SessionFull : EEOSCoreError::NoSessionFound,
				bFoundFullSession ? TEXT("Matching sessions are full.") : TEXT("No matching session was found.")),
			FString()});
		return;
	}

	Operation = EOperation::Joining;
	const TWeakPtr<FEOSSessionManager> WeakThis = AsShared();
	JoinCompleteHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(
		FOnJoinSessionCompleteDelegate::CreateLambda(
			[WeakThis](const FName SessionName, const EOnJoinSessionCompleteResult::Type Result)
			{
				if (const TSharedPtr<FEOSSessionManager> PinnedThis = WeakThis.Pin())
				{
					PinnedThis->HandleJoinSessionComplete(SessionName, Result);
				}
			}));

	if (!SessionInterface->JoinSession(UE::EOSCore::LocalUserNum, NAME_GameSession, *Match)
		&& Operation == EOperation::Joining)
	{
		ClearJoinDelegate();
		SessionSearch.Reset();
		Operation = EOperation::Idle;
		SessionJoinedEvent.Broadcast({
			FEOSCoreResultFactory::Failure(
				EEOSCoreError::RequestRejected,
				TEXT("The online service rejected the join-session request.")),
			FString()});
	}
}

void FEOSSessionManager::HandleJoinSessionComplete(
	const FName SessionName,
	const EOnJoinSessionCompleteResult::Type Result)
{
	if (Operation != EOperation::Joining || SessionName != NAME_GameSession)
	{
		return;
	}

	ClearJoinDelegate();
	FEOSJoinOutcome Outcome;
	Outcome.Result = FEOSCoreResultFactory::FromJoinResult(Result);
	const bool bJoinedSession = Outcome.Result.bWasSuccessful;
	if (Outcome.Result.bWasSuccessful
		&& (!SessionInterface.IsValid()
			|| !SessionInterface->GetResolvedConnectString(SessionName, Outcome.ConnectString)
			|| Outcome.ConnectString.IsEmpty()))
	{
		Outcome.Result = FEOSCoreResultFactory::Failure(
			EEOSCoreError::CouldNotResolveAddress,
			TEXT("The session was joined, but its connection address could not be resolved."));
	}
	const bool bShouldDestroyJoinedSession = bJoinedSession && !Outcome.Result.bWasSuccessful;

	SessionSearch.Reset();
	Operation = EOperation::Idle;
	SessionJoinedEvent.Broadcast(Outcome);
	if (bShouldDestroyJoinedSession)
	{
		DestroySession();
	}
}

void FEOSSessionManager::HandleDestroySessionComplete(const FName SessionName, const bool bWasSuccessful)
{
	if (Operation != EOperation::Destroying || SessionName != NAME_GameSession)
	{
		return;
	}

	ClearDestroyDelegate();
	if (bWasSuccessful
		|| !SessionInterface.IsValid()
		|| !SessionInterface->GetNamedSession(NAME_GameSession))
	{
		CurrentSessionCode.Reset();
	}

	Operation = EOperation::Idle;
	SessionDestroyedEvent.Broadcast(bWasSuccessful
		? FEOSCoreResultFactory::Success(TEXT("The EOS session was destroyed."))
		: FEOSCoreResultFactory::Failure(
			EEOSCoreError::OnlineServiceFailure,
			TEXT("The online service failed to destroy the session.")));
}

void FEOSSessionManager::HandleDestroySessionRequested(
	const int32 LocalUserNum,
	const FName SessionName)
{
	if (LocalUserNum != UE::EOSCore::LocalUserNum || SessionName != NAME_GameSession)
	{
		return;
	}

	UE_LOG(LogEOSCore, Log, TEXT("EOS requested cleanup of the local game session."));
	DestroySession();
}

void FEOSSessionManager::BindDestroyRequestedDelegate()
{
	if (!SessionInterface.IsValid() || DestroyRequestedHandle.IsValid())
	{
		return;
	}

	const TWeakPtr<FEOSSessionManager> WeakThis = AsShared();
	DestroyRequestedHandle = SessionInterface->AddOnDestroySessionRequestedDelegate_Handle(
		FOnDestroySessionRequestedDelegate::CreateLambda(
			[WeakThis](const int32 LocalUserNum, const FName SessionName)
			{
				if (const TSharedPtr<FEOSSessionManager> PinnedThis = WeakThis.Pin())
				{
					PinnedThis->HandleDestroySessionRequested(LocalUserNum, SessionName);
				}
			}));
}

void FEOSSessionManager::ClearCreateDelegate()
{
	if (SessionInterface.IsValid() && CreateCompleteHandle.IsValid())
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateCompleteHandle);
	}
	CreateCompleteHandle.Reset();
}

void FEOSSessionManager::ClearFindDelegate()
{
	if (SessionInterface.IsValid() && FindCompleteHandle.IsValid())
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindCompleteHandle);
	}
	FindCompleteHandle.Reset();
}

void FEOSSessionManager::ClearJoinDelegate()
{
	if (SessionInterface.IsValid() && JoinCompleteHandle.IsValid())
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinCompleteHandle);
	}
	JoinCompleteHandle.Reset();
}

void FEOSSessionManager::ClearDestroyDelegate()
{
	if (SessionInterface.IsValid() && DestroyCompleteHandle.IsValid())
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroyCompleteHandle);
	}
	DestroyCompleteHandle.Reset();
}

void FEOSSessionManager::ClearDestroyRequestedDelegate()
{
	if (SessionInterface.IsValid() && DestroyRequestedHandle.IsValid())
	{
		SessionInterface->ClearOnDestroySessionRequestedDelegate_Handle(DestroyRequestedHandle);
	}
	DestroyRequestedHandle.Reset();
}

void FEOSSessionManager::ClearAllDelegates()
{
	ClearCreateDelegate();
	ClearFindDelegate();
	ClearJoinDelegate();
	ClearDestroyDelegate();
	ClearDestroyRequestedDelegate();
}
