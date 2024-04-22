/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "UnIpDrv.h"

#if WITH_UE3_NETWORKING

#ifndef INCLUDED_ONLINESUBSYSTEMPC_H
#define INCLUDED_ONLINESUBSYSTEMPC_H 1

/**
 * Base class that holds a delegate to fire when a given async task is complete
 */
class FOnlineAsyncTaskPC :
	public FOnlineAsyncTask
{
protected:
	/**
	 * Holds the state of the async task. ERROR_IO_PENDING when in progress and
	 * any other value indicates completion
	 */
	DWORD CompletionStatus;

public:
	/**
	 * Initializes members
	 *
	 * @param InScriptDelegates the delegate list to fire off when complete
	 * @param InAsyncTaskName pointer to static data identifying the type of task
	 */
	FOnlineAsyncTaskPC(TArray<FScriptDelegate>* InScriptDelegates,const TCHAR* InAsyncTaskName) :
		FOnlineAsyncTask(InScriptDelegates,InAsyncTaskName),
		CompletionStatus(ERROR_IO_PENDING)
	{
	}

	/**
	 * Checks the completion status of the task
	 *
	 * @return TRUE if done, FALSE otherwise
	 */
	FORCEINLINE UBOOL HasTaskCompleted(void) const
	{
		return CompletionStatus == ERROR_IO_PENDING;
	}

	/**
	 * Returns the status code of the completed task. Assumes the task
	 * has finished.
	 *
	 * @return ERROR_SUCCESS if ok, an HRESULT error code otherwise
	 */
	virtual DWORD GetCompletionCode(void)
	{
		return CompletionStatus;
	}

	/**
	 * Used to route the final processing of the data to the correct subsystem
	 * function. Basically, this is a function pointer for doing final work
	 *
	 * @param Subsystem the object to make the final call on
	 *
	 * @return TRUE if this task should be cleaned up, FALSE to keep it
	 */
	virtual UBOOL ProcessAsyncResults(class UOnlineSubsystemPC* Subsystem)
	{
		return TRUE;
	}
};

#endif

#include "OnlineSubsystemPCClasses.h"

#endif	//#if WITH_UE3_NETWORKING
