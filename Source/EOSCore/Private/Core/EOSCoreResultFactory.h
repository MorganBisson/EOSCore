#pragma once

#include "CoreMinimal.h"
#include "Interfaces/OnlineSessionInterface.h"

enum class EEOSCoreError : uint8
{
	None,
	NotInitialized,
	OperationInProgress,
	InvalidArgument,
	RequestRejected,
	AuthenticationFailed,
	SessionAlreadyExists,
	NoSessionFound,
	SessionFull,
	CouldNotResolveAddress,
	AlreadyInSession,
	OnlineServiceFailure,
	TravelFailure
};

struct FEOSCoreResult
{
	bool bWasSuccessful = false;
	EEOSCoreError Error = EEOSCoreError::OnlineServiceFailure;
	FString Message;
};

class FEOSCoreResultFactory final
{
public:
	static FEOSCoreResult Success(FString Message = FString());
	static FEOSCoreResult Failure(EEOSCoreError Error, FString Message);
	static FEOSCoreResult FromJoinResult(EOnJoinSessionCompleteResult::Type Result);
};
