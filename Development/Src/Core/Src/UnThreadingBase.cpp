/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "CorePrivate.h"

/** The global hi priority thread pool */
FQueuedThreadPool*		GHiPriThreadPool				= NULL;
/** The number of threads in the hi priority thread pool 
*	zero on platforms that don't have the CPU resources to gain any benefits
*/
INT						GHiPriThreadPoolNumThreads		= 0;
/** Debug setting for ToggleHiPriThreadPool */
UBOOL					GHiPriThreadPoolForceOff		= FALSE;
/** Determines if high precision threading is enabled */
UBOOL				GIsHighPrecisionThreadingEnabled = SUPPORTS_HIGH_PRECISION_THREAD_TIMING;


/** the stats system increments this and when pool threads notice it has been 
*   incremented, they call GStatManager.AdvanceFrameForThread() to advance any
*   stats that have been collected on a pool thread.
*/
STAT(FThreadSafeCounter   GStatsFrameForPoolThreads);

#if STATS
/**
 * Constructor for FCheckForStatsUpdate, saves the current value of the frame counter
 */
FCheckForStatsUpdate::FCheckForStatsUpdate()
// must be sure not to advance until the stat system has advanced at least once
: StatsFrameLastUpdate(GStatsFrameForPoolThreads.GetValue()) 
{
}

/**
 * See if we are due for a stats advance call, if so do it
 */
void FCheckForStatsUpdate::ConditionalUpdate()
{
	DWORD Current = GStatsFrameForPoolThreads.GetValue();
	if (StatsFrameLastUpdate != Current)
	{
		GStatManager.AdvanceFrameForThread();
		StatsFrameLastUpdate = Current;
	}
}

#endif 

/**
 * Clean up the synch objects
 */
FQueuedThreadPoolBase::~FQueuedThreadPoolBase(void)
{
	if (SynchQueue != NULL)
	{
		GSynchronizeFactory->Destroy(SynchQueue);
	}
}

/**
 * Creates the synchronization object for locking the queues
 *
 * @return Whether the pool creation was successful or not
 */
UBOOL FQueuedThreadPoolBase::CreateSynchObjects(void)
{
	check(SynchQueue == NULL);
	// Create them with the factory
	SynchQueue = GSynchronizeFactory->CreateCriticalSection();
	// If it is valid then we succeeded
	return SynchQueue != NULL;
}

/**
 * Tells the pool to clean up all background threads
 */
void FQueuedThreadPoolBase::Destroy(void)
{
	FScopeLock Lock(SynchQueue);

	TimeToDie = TRUE;
	// Clean up all queued objects
	for (INT Index = 0; Index < QueuedWork.Num(); Index++)
	{
		QueuedWork(Index)->Abandon();
	}
	// Empty out the invalid pointers
	QueuedWork.Empty();

	// Now tell each thread to die and delete those
	for (INT Index = 0; Index < QueuedThreads.Num(); Index++)
	{
		// Wait for the thread to die and have it delete itself using
		// whatever malloc it should
		QueuedThreads(Index)->Kill(TRUE,TRUE);
	}
	// All the pointers are invalid so clean up
	QueuedThreads.Empty();
}

/**
 * Checks to see if there is a thread available to perform the task. If not,
 * it queues the work for later. Otherwise it is immediately dispatched.
 *
 * @param InQueuedWork The work that needs to be done asynchronously
 */
void FQueuedThreadPoolBase::AddQueuedWork(FQueuedWork* InQueuedWork)
{
	check(InQueuedWork != NULL);

#if FLASH
	// service immediately for Flash (no threads)
	InQueuedWork->DoThreadedWork();
	return;
#endif

	FQueuedThread* Thread = NULL;
	// Check to see if a thread is available. Make sure no other threads
	// can manipulate the thread pool while we do this.
	check(SynchQueue && "Did you forget to call Create()?");
	FScopeLock sl(SynchQueue);
	if (TimeToDie)
	{
		check(!QueuedThreads.Num() && !QueuedWork.Num());  // we better not have anything if we are dying
		InQueuedWork->Abandon();
		return;
	}
	if (QueuedThreads.Num() > 0)
	{
		// Figure out which thread is available
		INT Index = QueuedThreads.Num() - 1;
		// Grab that thread to use
		Thread = QueuedThreads(Index);
		// Remove it from the list so no one else grabs it
		QueuedThreads.Remove(Index);
	}
	// Was there a thread ready?
	if (Thread != NULL)
	{
		// We have a thread, so tell it to do the work
		Thread->DoWork(InQueuedWork);
	}
	else
	{
		// There were no threads available, queue the work to be done
		// as soon as one does become available
		QueuedWork.AddItem(InQueuedWork);
	}
}

/**
 * Attempts to retract a previously queued task.
 *
 * @param InQueuedWork The work to try to retract
 * @return TRUE if the work was retracted
 */
UBOOL FQueuedThreadPoolBase::RetractQueuedWork(FQueuedWork* InQueuedWork)
{
	check(InQueuedWork != NULL);
	check(SynchQueue && "Did you forget to call Create()?");
	FScopeLock sl(SynchQueue);
	if (TimeToDie)
	{
		return FALSE; // no special consideration for this, refuse the retraction and let shutdown proceed
	}
	return !!QueuedWork.RemoveSingleItem(InQueuedWork);
}

/**
 * Places a thread back into the available pool if there is no pending work
 * to be done. If there is, the thread is put right back to work without it
 * reaching the queue.
 *
 * @param InQueuedThread The thread that is ready to be pooled
 * @return next job to process or NULL if there are no jobs left to process
*/
FQueuedWork* FQueuedThreadPoolBase::ReturnToPoolOrGetNextJob(FQueuedThread* InQueuedThread)
{
	check(InQueuedThread != NULL);
	FQueuedWork* Work = NULL;
	// Check to see if there is any work to be done
	FScopeLock sl(SynchQueue);
	if (TimeToDie)
	{
		check(!QueuedThreads.Num() && !QueuedWork.Num());  // we better not have anything if we are dying
		return NULL;
	}
	if (QueuedWork.Num() > 0)
	{
		// Grab the oldest work in the queue. This is slower than
		// getting the most recent but prevents work from being
		// queued and never done
		Work = QueuedWork(0);
		// Remove it from the list so no one else grabs it
		QueuedWork.Remove(0);
	}
	if (!Work)
	{
		// There was no work to be done, so add the thread to the pool
		QueuedThreads.AddItem(InQueuedThread);
	}
	return Work;
}
