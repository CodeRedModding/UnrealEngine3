/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

// Includes

#include "UnIpDrv.h"
#include "OnlineAsyncTaskManager.h"

#if WITH_UE3_NETWORKING

// Static variable initialization

DWORD FOnlineAsyncItem::OnlineThreadId			= 0;
DWORD FOnlineAsyncItem::GameThreadId			= 0;

/** Set to zero so that Run() can prevent a second invocation */
INT FOnlineAsyncTaskManager::InvocationCount		= 0;


// Defines

/** The default value for the polling interval when not set by config */
#define POLLING_INTERVAL_MS 50

/** Whether or not to enable detailed logging of the triggering/return of callbacks */
#define VERBOSE_QUEUE_LOG 0


// FOnlineAsyncTaskManager implementation


/**
 * Base constructor
 */
FOnlineAsyncTaskManager::FOnlineAsyncTaskManager()
	: WorkEvent(NULL)
	, PollingInterval(POLLING_INTERVAL_MS)
	, bAllowAsyncBlocking(TRUE)
	, DebugTaskDelayInMs(0)
	, bRequestingExit(0)
	, bIsOnlineThreadHealthy(TRUE)
	, bOnlineExceptionAcknowledged(FALSE)
{
}

/**
 * Init the online async task manager
 *
 * @return	TRUE if initialization was successful, FALSE otherwise
 */
UBOOL FOnlineAsyncTaskManager::Init()
{
	WorkEvent = GSynchronizeFactory->CreateSynchEvent();
	INT PollingConfig = POLLING_INTERVAL_MS;

	// Read the polling interval to use from the INI file
	if (GConfig->GetInt(TEXT("OnlineSubsystem"), TEXT("PollingIntervalInMs"), PollingConfig, GEngineIni))
	{
		PollingInterval = (DWORD)PollingConfig;
	}

	UBOOL bInAllowAsyncBlocking = FALSE;

	if (GConfig->GetBool(TEXT("OnlineSubsystem"), TEXT("bAllowAsyncBlocking"), bInAllowAsyncBlocking, GEngineIni))
	{
		bAllowAsyncBlocking = bInAllowAsyncBlocking;
	}

	INT InDebugTaskDelayInMs = 0;

	// Read the polling interval to use from the INI file
	if (GConfig->GetInt(TEXT("OnlineSubsystem"), TEXT("DebugTaskDelayInMs"), InDebugTaskDelayInMs, GEngineIni))
	{
		DebugTaskDelayInMs = (DWORD)InDebugTaskDelayInMs;
	}

	appInterlockedExchange((volatile INT*)&FOnlineAsyncTaskBase::GameThreadId, GGameThreadId);

	return (WorkEvent != NULL);
}

/**
 * This is where all per object thread work is done. This is only called if the initialization was successful.
 *
 * @return	The exit code of the runnable object
 */
DWORD FOnlineAsyncTaskManager::Run()
{
	DWORD ReturnVal = 0;

	// Exception handling based upon render thread
#if _MSC_VER && !CONSOLE
	extern INT CreateMiniDump(LPEXCEPTION_POINTERS ExceptionInfo);

	if (!appIsDebuggerPresent())
	{
		__try
		{
			ReturnVal = OnlineMain();
		}
		__except(CreateMiniDump(GetExceptionInformation()))
		{
			OnlineThreadError = GErrorHist;

			// Use a memory barrier to ensure OnlineThreadError reaches game thread before bIsOnlineThreadHealthy
			appMemoryBarrier();

			bIsOnlineThreadHealthy = FALSE;


			// Now spin until the game thread acknowledges the exception (FOnlineAysncTaskManager is auto-deleted once this returns,
			//	so must give game thread time to act)
			do
			{
				appSleep(0.0f);
			}
			while (!bOnlineExceptionAcknowledged);
		}
	}
	else
#endif
	{
		ReturnVal = OnlineMain();
	}

	return ReturnVal;
}


/**
 * Main thread entry function for the online thread (split out of 'Run' to add exception handling)
 *
 * @return	The exit code of the runnable object
 */
DWORD FOnlineAsyncTaskManager::OnlineMain()
{
	check(InvocationCount == 0);

	InvocationCount++;

	// This should not be set yet
	check (FOnlineAsyncTaskBase::OnlineThreadId == 0);

	appInterlockedExchange((volatile INT*)&FOnlineAsyncTaskBase::OnlineThreadId, appGetCurrentThreadId());

	do
	{
		INT CurrentQueueSize = 0;
		FOnlineAsyncTaskBase* Task = NULL;


		// Wait for a trigger event to start work
		WorkEvent->Wait(PollingInterval);

		if (!bRequestingExit)
		{
			DOUBLE TimeElapsed = 0;

			// Chance for services to do work
			OnlineTick();

			do
			{
				Task = NULL;
				INT TaskIndex = 0;

				// Grab the current task from the queue
				{
					FScopeLock LockInQueue(&InQueueLock);

					CurrentQueueSize = InQueue.Num();

					if (CurrentQueueSize > 0)
					{
						Task = InQueue(0);
					}
				}


				PollTask:

				if (Task != NULL)
				{
					Task->Tick();

					if (Task->IsDone())
					{
						debugf(NAME_DevOnline, TEXT("Async task '%s' completed in %f seconds with %d"),
							*Task->ToString(), Task->GetElapsedTime(), Task->WasSuccessful());

						// Task is done, remove from the incomine queue and add to the outgoing queue
						PopFromInQueue(TaskIndex);
						AddToOutQueue(Task);

						// NOTE: It is not safe to access Task after AddToOutQueue, as the game thread could delete it immediately
					}
					else if (bAllowAsyncBlocking && Task->IsBlocking())
					{
						Task = NULL;
					}
					// If the task is not-blocking, proceed to the next task
					else
					{
						Task = NULL;
						TaskIndex++;

						{
							FScopeLock LockInQueue(&InQueueLock);

							CurrentQueueSize = InQueue.Num();

							if (CurrentQueueSize > TaskIndex)
							{
								Task = InQueue(TaskIndex);
							}
						}

						if (Task != NULL)
						{
							goto PollTask;
						}
					}
				}
			}
			while (Task != NULL);
		}
	}
	while (!bRequestingExit);

	return 0;
}

/**
 * Checks that the online thread is still functioning correctly
 */
void FOnlineAsyncTaskManager::CheckOnlineThreadHealth()
{
	// Closely matches the render thread functionality
	if (!bIsOnlineThreadHealthy)
	{
#if !CONSOLE
		GErrorHist[0] = 0;
#endif

		// Copy the error message before letting the online thread begin shutdown
		FString ErrorCopy = OnlineThreadError;

		// Let the online thread know it's safe to shutdown now
		bOnlineExceptionAcknowledged = TRUE;

		GIsCriticalError = FALSE;
		GError->Logf(TEXT("Online thread exception:\r\n%s"), *ErrorCopy);
	}
}


/**
 * This is called if a thread is requested to terminate early
 */
void FOnlineAsyncTaskManager::Stop()
{
	appInterlockedExchange(&bRequestingExit, 1);
	WorkEvent->Trigger();
}

/**
 * Called in the context of the aggregating thread to perform any cleanup.
 */
void FOnlineAsyncTaskManager::Exit()
{
	FOnlineAsyncTaskBase::GameThreadId = 0;
	FOnlineAsyncTaskBase::OnlineThreadId = 0;
	InvocationCount = 0;
}

/**
 * Add online async tasks that need processing onto the incoming queue
 *
 * @param NewTask		Some request of the online services
 */
void FOnlineAsyncTaskManager::AddToInQueue(FOnlineAsyncTaskBase* NewTask)
{
	check(appGetCurrentThreadId() == FOnlineAsyncTaskBase::GameThreadId);

	// Quick check to see online thread is still going
	CheckOnlineThreadHealth();

	FScopeLock Lock(&InQueueLock);

	InQueue.AddItem(NewTask);
	WorkEvent->Trigger();
}

/**
 * Remove the current async task from the queue
 *
 * @param ItemIndex	The index of the current queue item (not always the first item, since queue items can be non-blocking)
 */
void FOnlineAsyncTaskManager::PopFromInQueue(INT ItemIndex)
{
	check(appGetCurrentThreadId() == FOnlineAsyncTaskBase::OnlineThreadId);

	FScopeLock Lock(&InQueueLock);

	InQueue.Remove(ItemIndex, 1);
}

/**
 * Add completed online async tasks that need processing onto the queue
 *
 * @param CompletedItem		Some finished request of the online services
 */
void FOnlineAsyncTaskManager::AddToOutQueue(FOnlineAsyncItem* CompletedItem)
{
	check(appGetCurrentThreadId() == FOnlineAsyncTaskBase::OnlineThreadId);

#if VERBOSE_QUEUE_LOG
	debugf(NAME_DevOnline, TEXT("%s"), *CompletedItem->ToString());
#endif

	FScopeLock Lock(&OutQueueLock);

	OutQueue.AddItem(CompletedItem);

	CompletedItem->CompleteTime = appSeconds();
}

/**
 * Give the completed async tasks a chance to marshal their data back onto the game thread, calling delegates where appropriate
 * NOTE: CALL ONLY FROM GAME THREAD
 */
void FOnlineAsyncTaskManager::GameTick()
{
	check(appGetCurrentThreadId() == FOnlineAsyncTaskBase::GameThreadId);

	// Check online thread is still going
	CheckOnlineThreadHealth();

	FOnlineAsyncItem* Item = NULL;
	INT CurrentQueueSize = 0;

	do
	{
		Item = NULL;

		// Grab a completed task from the queue
		{
			FScopeLock LockOutQueue(&OutQueueLock);

			CurrentQueueSize = OutQueue.Num();

			if (CurrentQueueSize > 0)
			{
				Item = OutQueue(0);

				// Don't proceed with this item until it exceeds DebugTaskDelayInMs
				if (DebugTaskDelayInMs <= 0 || ((DWORD)(appSeconds() - Item->CompleteTime) * 1000) >= DebugTaskDelayInMs)
				{
					OutQueue.Remove(0, 1);
				}
				else
				{
					Item = NULL;
				}
			}

			if (Item != NULL)
			{
				// Finish work and trigger delegates
				if (Item->CanExecute())
				{
					Item->Finalize();
					Item->TriggerDelegates();
				}

				if (Item->CanDelete())
				{
					delete Item;
				}
			}
		}
	}
	while (Item != NULL);
}


#endif // WITH_UE3_NETWORKING




