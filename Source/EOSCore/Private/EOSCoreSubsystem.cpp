#include "EOSCoreSubsystem.h"

#include "Core/EOSCoreResultFactory.h"
#include "EOSCore.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "Managers/EOSAuthManager.h"
#include "Managers/EOSSessionManager.h"
#include "Managers/EOSTravelManager.h"

namespace
{
	bool ShouldCleanupAfterNetworkFailure(
		const UNetDriver* NetDriver,
		const ENetworkFailure::Type FailureType)
	{
		if (!NetDriver
			|| (NetDriver->NetDriverName != NAME_GameNetDriver
				&& NetDriver->NetDriverName != NAME_PendingNetDriver))
		{
			return false;
		}

		switch (FailureType)
		{
		case ENetworkFailure::ConnectionLost:
		case ENetworkFailure::ConnectionTimeout:
		case ENetworkFailure::NetGuidMismatch:
		case ENetworkFailure::NetChecksumMismatch:
			// These failures can represent one remote client on a listen server.
			// Only the failed local client must leave its EOS session.
			return NetDriver->GetNetMode() == NM_Client;

		default:
			return true;
		}
	}
}

UEOSCoreSubsystem::~UEOSCoreSubsystem() = default;

void UEOSCoreSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	bIsDeinitializing = false;

	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		UE_LOG(LogEOSCore, Error, TEXT("EOS Core could not initialize without a GameInstance."));
		return;
	}

	AuthManager = MakeShared<FEOSAuthManager>(GameInstance);
	SessionManager = MakeShared<FEOSSessionManager>(GameInstance);
	TravelManager = MakeShared<FEOSTravelManager>(GameInstance);
	SessionManager->Initialize();

	AuthManager->OnLoginComplete().AddUObject(this, &UEOSCoreSubsystem::HandleLoginComplete);
	SessionManager->OnSessionCreated().AddUObject(this, &UEOSCoreSubsystem::HandleSessionCreated);
	SessionManager->OnSessionJoined().AddUObject(this, &UEOSCoreSubsystem::HandleSessionJoined);
	SessionManager->OnSessionDestroyed().AddUObject(this, &UEOSCoreSubsystem::HandleSessionDestroyed);

	if (GEngine)
	{
		NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(
			this,
			&ThisClass::HandleNetworkFailure);
		TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(
			this,
			&ThisClass::HandleTravelFailure);
	}

	UE_LOG(LogEOSCore, Log, TEXT("EOS Core subsystem initialized."));
}

void UEOSCoreSubsystem::Deinitialize()
{
	bIsDeinitializing = true;
	if (GEngine)
	{
		if (NetworkFailureHandle.IsValid())
		{
			GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
		}
		if (TravelFailureHandle.IsValid())
		{
			GEngine->OnTravelFailure().Remove(TravelFailureHandle);
		}
	}
	NetworkFailureHandle.Reset();
	TravelFailureHandle.Reset();

	if (SessionManager.IsValid())
	{
		SessionManager->Shutdown();
	}
	if (AuthManager.IsValid())
	{
		AuthManager->Shutdown();
	}
	if (TravelManager.IsValid())
	{
		TravelManager->Shutdown();
	}

	SessionManager.Reset();
	AuthManager.Reset();
	TravelManager.Reset();
	CurrentSessionCode.Reset();

	Super::Deinitialize();
}

void UEOSCoreSubsystem::LoginToEOS()
{
	if (!AuthManager.IsValid())
	{
		UE_LOG(LogEOSCore, Error, TEXT("Cannot log in: the authentication manager is unavailable."));
		OnLoginCompleteEvent.Broadcast(false);
		return;
	}

	AuthManager->Login();
}

bool UEOSCoreSubsystem::IsLoggedIn() const
{
	return AuthManager.IsValid() && AuthManager->IsLoggedIn();
}

void UEOSCoreSubsystem::HostEOSLobby(
	const int32 MaxPlayers,
	const FString& MapPath,
	const bool bIsLAN)
{
	if (!SessionManager.IsValid())
	{
		UE_LOG(LogEOSCore, Error, TEXT("Cannot host: the session manager is unavailable."));
		OnLobbyCreatedEvent.Broadcast(false);
		return;
	}

	FEOSHostSessionRequest Request;
	Request.MaxPlayers = MaxPlayers;
	Request.MapPath = MapPath;
	Request.bIsLAN = bIsLAN;
	SessionManager->HostSession(Request);
}

void UEOSCoreSubsystem::FindAndJoinEOSLobby(const bool bIsLanQuery)
{
	if (!SessionManager.IsValid())
	{
		UE_LOG(LogEOSCore, Error, TEXT("Cannot search: the session manager is unavailable."));
		OnLobbyJoinedEvent.Broadcast(false);
		return;
	}

	FEOSFindSessionRequest Request;
	Request.bIsLAN = bIsLanQuery;
	SessionManager->FindAndJoinSession(Request);
}

void UEOSCoreSubsystem::FindAndJoinEOSLobbyByCode(
	const FString& SessionCode,
	const bool bIsLanQuery)
{
	if (!SessionManager.IsValid())
	{
		UE_LOG(LogEOSCore, Error, TEXT("Cannot search by code: the session manager is unavailable."));
		OnLobbyJoinedEvent.Broadcast(false);
		return;
	}

	FEOSFindSessionRequest Request;
	Request.SessionCode = SessionCode;
	Request.bIsLAN = bIsLanQuery;
	Request.bFilterByCode = true;
	SessionManager->FindAndJoinSession(Request);
}

void UEOSCoreSubsystem::DestroyEOSSession()
{
	if (!SessionManager.IsValid())
	{
		UE_LOG(LogEOSCore, Error, TEXT("Cannot destroy: the session manager is unavailable."));
		OnSessionDestroyedEvent.Broadcast(false);
		return;
	}

	SessionManager->DestroySession();
}

void UEOSCoreSubsystem::HandleLoginComplete(const FEOSCoreResult& Result)
{
	if (bIsDeinitializing)
	{
		return;
	}

	if (Result.bWasSuccessful)
	{
		UE_LOG(LogEOSCore, Log, TEXT("EOS authentication: %s"), *Result.Message);
	}
	else
	{
		UE_LOG(LogEOSCore, Error, TEXT("EOS authentication: %s"), *Result.Message);
	}
	OnLoginCompleteEvent.Broadcast(Result.bWasSuccessful);
}

void UEOSCoreSubsystem::HandleSessionCreated(const FEOSHostOutcome& Outcome)
{
	if (bIsDeinitializing)
	{
		return;
	}

	bool bWasSuccessful = Outcome.Result.bWasSuccessful;
	bool bNeedsSessionCleanup = false;
	CurrentSessionCode = SessionManager.IsValid()
		? SessionManager->GetCurrentSessionCode()
		: FString();

	if (bWasSuccessful)
	{
		const FEOSCoreResult TravelResult = TravelManager.IsValid()
			? TravelManager->StartListenTravel(Outcome.MapPackageName, Outcome.bIsLAN)
			: FEOSCoreResultFactory::Failure(
				EEOSCoreError::NotInitialized,
				TEXT("The travel manager is unavailable."));
		bWasSuccessful = TravelResult.bWasSuccessful;
		if (!TravelResult.bWasSuccessful)
		{
			UE_LOG(LogEOSCore, Error, TEXT("EOS host travel: %s"), *TravelResult.Message);
			bNeedsSessionCleanup = true;
		}
	}
	else
	{
		UE_LOG(LogEOSCore, Error, TEXT("EOS session creation: %s"), *Outcome.Result.Message);
	}
	OnLobbyCreatedEvent.Broadcast(bWasSuccessful);

	if (bNeedsSessionCleanup)
	{
		RequestSessionCleanup(TEXT("Host travel failed to start."));
	}
}

void UEOSCoreSubsystem::HandleSessionJoined(const FEOSJoinOutcome& Outcome)
{
	if (bIsDeinitializing)
	{
		return;
	}

	bool bWasSuccessful = Outcome.Result.bWasSuccessful;
	bool bNeedsSessionCleanup = false;
	if (bWasSuccessful)
	{
		const FEOSCoreResult TravelResult = TravelManager.IsValid()
			? TravelManager->StartClientTravel(Outcome.ConnectString)
			: FEOSCoreResultFactory::Failure(
				EEOSCoreError::NotInitialized,
				TEXT("The travel manager is unavailable."));
		bWasSuccessful = TravelResult.bWasSuccessful;
		if (!TravelResult.bWasSuccessful)
		{
			UE_LOG(LogEOSCore, Error, TEXT("EOS client travel: %s"), *TravelResult.Message);
			bNeedsSessionCleanup = true;
		}
	}
	else
	{
		UE_LOG(LogEOSCore, Error, TEXT("EOS session join: %s"), *Outcome.Result.Message);
	}
	OnLobbyJoinedEvent.Broadcast(bWasSuccessful);

	if (bNeedsSessionCleanup)
	{
		RequestSessionCleanup(TEXT("Client travel failed to start."));
	}
}

void UEOSCoreSubsystem::HandleSessionDestroyed(const FEOSCoreResult& Result)
{
	if (bIsDeinitializing)
	{
		return;
	}

	CurrentSessionCode = SessionManager.IsValid()
		? SessionManager->GetCurrentSessionCode()
		: FString();
	if (!Result.bWasSuccessful)
	{
		UE_LOG(LogEOSCore, Error, TEXT("EOS session destruction: %s"), *Result.Message);
	}

	OnSessionDestroyedEvent.Broadcast(Result.bWasSuccessful);
}

void UEOSCoreSubsystem::HandleNetworkFailure(
	UWorld* FailureWorld,
	UNetDriver* NetDriver,
	const ENetworkFailure::Type FailureType,
	const FString& ErrorString)
{
	if (bIsDeinitializing
		|| !IsFailureForThisGameInstance(FailureWorld, NetDriver)
		|| !ShouldCleanupAfterNetworkFailure(NetDriver, FailureType))
	{
		return;
	}

	RequestSessionCleanup(FString::Printf(
		TEXT("Network failure '%s': %s"),
		ENetworkFailure::ToString(FailureType),
		ErrorString.IsEmpty() ? TEXT("No additional details.") : *ErrorString));
}

void UEOSCoreSubsystem::HandleTravelFailure(
	UWorld* FailureWorld,
	const ETravelFailure::Type FailureType,
	const FString& ErrorString)
{
	if (bIsDeinitializing || !IsFailureForThisGameInstance(FailureWorld))
	{
		return;
	}

	RequestSessionCleanup(FString::Printf(
		TEXT("Travel failure '%s': %s"),
		ETravelFailure::ToString(FailureType),
		ErrorString.IsEmpty() ? TEXT("No additional details.") : *ErrorString));
}

void UEOSCoreSubsystem::RequestSessionCleanup(const FString& Reason)
{
	if (bIsDeinitializing
		|| !SessionManager.IsValid()
		|| !SessionManager->HasActiveSession())
	{
		return;
	}

	UE_LOG(LogEOSCore, Warning, TEXT("%s Cleaning up the EOS session."), *Reason);
	SessionManager->DestroySession();
}

bool UEOSCoreSubsystem::IsFailureForThisGameInstance(
	UWorld* FailureWorld,
	const UNetDriver* NetDriver) const
{
	const UGameInstance* OwningGameInstance = GetGameInstance();
	if (!OwningGameInstance)
	{
		return false;
	}

	if (FailureWorld)
	{
		return FailureWorld->GetGameInstance() == OwningGameInstance;
	}

	if (NetDriver)
	{
		if (const UWorld* DriverWorld = NetDriver->GetWorld())
		{
			return DriverWorld->GetGameInstance() == OwningGameInstance;
		}

		const FWorldContext* WorldContext = GEngine
			? GEngine->GetWorldContextFromPendingNetGameNetDriver(NetDriver)
			: nullptr;
		return WorldContext && WorldContext->OwningGameInstance.Get() == OwningGameInstance;
	}

	return false;
}
