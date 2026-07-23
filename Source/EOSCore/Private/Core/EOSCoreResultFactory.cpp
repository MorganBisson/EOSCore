#include "Core/EOSCoreResultFactory.h"

FEOSCoreResult FEOSCoreResultFactory::Success(FString Message)
{
	return {true, EEOSCoreError::None, MoveTemp(Message)};
}

FEOSCoreResult FEOSCoreResultFactory::Failure(const EEOSCoreError Error, FString Message)
{
	return {false, Error, MoveTemp(Message)};
}

FEOSCoreResult FEOSCoreResultFactory::FromJoinResult(const EOnJoinSessionCompleteResult::Type Result)
{
	switch (Result)
	{
	case EOnJoinSessionCompleteResult::Success:
		return Success(TEXT("Session joined."));
	case EOnJoinSessionCompleteResult::SessionIsFull:
		return Failure(EEOSCoreError::SessionFull, TEXT("The session is full."));
	case EOnJoinSessionCompleteResult::SessionDoesNotExist:
		return Failure(EEOSCoreError::NoSessionFound, TEXT("The session no longer exists."));
	case EOnJoinSessionCompleteResult::CouldNotRetrieveAddress:
		return Failure(EEOSCoreError::CouldNotResolveAddress, TEXT("The session address could not be resolved."));
	case EOnJoinSessionCompleteResult::AlreadyInSession:
		return Failure(EEOSCoreError::AlreadyInSession, TEXT("The local player is already in this session."));
	case EOnJoinSessionCompleteResult::UnknownError:
	default:
		return Failure(EEOSCoreError::OnlineServiceFailure, TEXT("The online service rejected the join request."));
	}
}
