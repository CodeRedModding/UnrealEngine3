/**
 * MacThreading.cpp -- Contains all Mac platform-specific definitions
 * of interfaces and concrete classes for multithreading support in the Unreal
 * engine.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
#include "CorePrivate.h"

#ifdef PLATFORM_MACOSX

#include "MacThreading.h"
#include "MacObjCWrapper.h"

/** The global synchonization object factory.	*/
FSynchronizeFactory*	GSynchronizeFactory = NULL;
/** The global thread factory.					*/
FThreadFactory*			GThreadFactory		= NULL;
/** The global thread pool */
FQueuedThreadPool*		GThreadPool			= NULL;


/**
 * Constructor that zeroes the handle
 */
FEventMac::FEventMac(void)
{
	bInitialized = FALSE;
	bIsManualReset = FALSE;
	Triggered = TRIGGERED_NONE;
	WaitingThreads = 0;
}

/**
 * Cleans up the event handle if valid
 */
FEventMac::~FEventMac(void)
{
	// Safely destructing an Event is VERY delicate, so it can handle badly-designed
	//  calling code that might still be waiting on the event.
	if (bInitialized)
	{
		// try to flush out any waiting threads...
		LockEventMutex();
		bIsManualReset = TRUE;
		UnlockEventMutex();
		Trigger();  // any waiting threads should start to wake up now.

		LockEventMutex();
		bInitialized = FALSE;  // further incoming calls to this object will now crash in check().
		while (WaitingThreads)  // spin while waiting threads wake up.
		{
			UnlockEventMutex();  // cycle through waiting threads...
			LockEventMutex();
		}
		// No threads are currently waiting on Condition and we hold the Mutex. Kill it.
		pthread_cond_destroy(&Condition);
		// Unlock and kill the mutex, since nothing else can grab it now.
		UnlockEventMutex();
		pthread_mutex_destroy(&Mutex);
	}
}

void FEventMac::LockEventMutex()
{
	const int rc = pthread_mutex_lock(&Mutex);
	check(rc == 0);
}

void FEventMac::UnlockEventMutex()
{
	const int rc = pthread_mutex_unlock(&Mutex);
	check(rc == 0);
}

/**
 * Waits for the event to be signaled before returning
 */
void FEventMac::Lock(void)
{
    Wait((DWORD)-1);  // infinite wait.
}

/**
 * Triggers the event so any waiting threads are allowed access
 */
void FEventMac::Unlock(void)
{
	Pulse();  // This matches the Windows codepath.
}

/**
 * Creates the event. Manually reset events stay triggered until reset.
 * Named events share the same underlying event.
 *
 * @param bIsManualReset Whether the event requires manual reseting or not
 * @param InName Whether to use a commonly shared event or not. If so this
 * is the name of the event to share.
 *
 * @return Returns TRUE if the event was created, FALSE otherwise
 */
UBOOL FEventMac::Create(UBOOL _bIsManualReset,const TCHAR* InName)
{
	check(!bInitialized);
	UBOOL RetVal = FALSE;
	Triggered = TRIGGERED_NONE;
	bIsManualReset = _bIsManualReset;

	if (pthread_mutex_init(&Mutex, NULL) == 0)
	{
		if (pthread_cond_init(&Condition, NULL) == 0)
		{
			bInitialized = TRUE;
			RetVal = TRUE;
		}
		else
		{
			pthread_mutex_destroy(&Mutex);
		}
	}
	return RetVal;
}

/**
 * Triggers the event so any waiting threads are activated
 */
void FEventMac::Trigger(void)
{
	check(bInitialized);

	LockEventMutex();

	if (bIsManualReset)
	{
		// Release all waiting threads at once.
		Triggered = TRIGGERED_ALL;
		int rc = pthread_cond_broadcast(&Condition);
		check(rc == 0);
	}
	else
	{
		// Release one or more waiting threads (first one to get the mutex
		//  will reset Triggered, rest will go back to waiting again).
		Triggered = TRIGGERED_ONE;
		int rc = pthread_cond_signal(&Condition);  // may release multiple threads anyhow!
		check(rc == 0);
	}

	UnlockEventMutex();
}

/**
 * Resets the event to an untriggered (waitable) state
 */
void FEventMac::Reset(void)
{
	check(bInitialized);
	LockEventMutex();
	Triggered = TRIGGERED_NONE;
	UnlockEventMutex();
}

/**
 * Triggers the event and resets the triggered state NOTE: This behaves
 * differently for auto-reset versus manual reset events. All threads
 * are released for manual reset events and only one is for auto reset
 */
void FEventMac::Pulse(void)
{
	check(bInitialized);
	if (!bIsManualReset)
	{
		Trigger();  // msdn suggests auto-reset Pulse and Trigger are the same.
	}
	else
	{
		LockEventMutex();
		while (WaitingThreads > 0)
		{
			Triggered = TRIGGERED_PULSE;
			UnlockEventMutex();
			// Presumably this puts us at the back of the mutex queue.
			LockEventMutex();
		}
		Triggered = TRIGGERED_NONE;
		UnlockEventMutex();
	}
}


static inline void SubtractTimevals(const struct timeval *FromThis,
                                    struct timeval *SubThis,
                                    struct timeval *Difference)
{
	if (FromThis->tv_usec < SubThis->tv_usec)
	{
		int nsec = (SubThis->tv_usec - FromThis->tv_usec) / 1000000 + 1;
		SubThis->tv_usec -= 1000000 * nsec;
		SubThis->tv_sec += nsec;
	}

	if (FromThis->tv_usec - SubThis->tv_usec > 1000000)
	{
		int nsec = (FromThis->tv_usec - SubThis->tv_usec) / 1000000;
		SubThis->tv_usec += 1000000 * nsec;
		SubThis->tv_sec -= nsec;
	}

	Difference->tv_sec = FromThis->tv_sec - SubThis->tv_sec;
	Difference->tv_usec = FromThis->tv_usec - SubThis->tv_usec;
}


/**
 * Waits for the event to be triggered
 *
 * @param WaitTime Time in milliseconds to wait before abandoning the event
 * (DWORD)-1 is treated as wait infinite
 *
 * @return TRUE if the event was signaled, FALSE if the wait timed out
 */
UBOOL FEventMac::Wait(DWORD WaitTime)
{
	check(bInitialized);

	struct timeval StartTime;

	// We need to know the start time if we're going to do a timed wait.
	if ( (WaitTime > 0) && (WaitTime != ((DWORD)-1)) )  // not polling and not infinite wait.
	{
		gettimeofday(&StartTime, NULL);
	}

	LockEventMutex();

	// Spin here while a manual reset Pulse is still ongoing...
	while (Triggered == TRIGGERED_PULSE)
	{
		UnlockEventMutex();
		// Presumably this puts us at the back of the mutex queue.
		LockEventMutex();
	}

	UBOOL bRetVal = FALSE;

	// loop in case we fall through the Condition signal but someone else claims the event.
	do
	{
		// See what state the event is in...we may not have to wait at all...

		// One thread should be released. We saw it first, so we'll take it.
		if (Triggered == TRIGGERED_ONE)
		{
			Triggered = TRIGGERED_NONE;  // dibs!
			bRetVal = TRUE;
		}

		// manual reset that is still signaled. Every thread goes.
		else if ((Triggered == TRIGGERED_ALL) || (Triggered == TRIGGERED_PULSE))
		{
			bRetVal = TRUE;
		}

		// No event signalled yet.
		else if (WaitTime != 0)  // not just polling, wait on the condition variable.
		{
			WaitingThreads++;
			if (WaitTime == ((DWORD)-1)) // infinite wait?
			{
				int rc = pthread_cond_wait(&Condition, &Mutex);  // unlocks Mutex while blocking...
				check(rc == 0);
			}
			else  // timed wait.
			{
				struct timespec TimeOut;
				const DWORD ms = (StartTime.tv_usec / 1000) + WaitTime;
				TimeOut.tv_sec = StartTime.tv_sec + (ms / 1000);
				TimeOut.tv_nsec = (ms % 1000) * 1000000;  // remainder of milliseconds converted to nanoseconds.
				int rc = pthread_cond_timedwait(&Condition, &Mutex, &TimeOut);    // unlocks Mutex while blocking...
				check((rc == 0) || (rc == ETIMEDOUT));

				// Update WaitTime and StartTime in case we have to go again...
				struct timeval Now, Difference;
				gettimeofday(&Now, NULL);
				SubtractTimevals(&Now, &StartTime, &Difference);
				const INT DifferenceMS = ((Difference.tv_sec * 1000) + (Difference.tv_usec / 1000));
				WaitTime = ((DifferenceMS >= WaitTime) ? 0 : (WaitTime - DifferenceMS));
				StartTime = Now;
			}
			WaitingThreads--;
			check(WaitingThreads >= 0);
		}

	} while ((!bRetVal) && (WaitTime != 0));

	UnlockEventMutex();
	return bRetVal;
}


// !!! FIXME: how much of this cut-and-paste can go into a base class?

/**
 * Zeroes its members
 */
FSynchronizeFactoryMac::FSynchronizeFactoryMac(void)
{
}

/**
 * Creates a new critical section
 *
 * @return The new critical section object or NULL otherwise
 */
FCriticalSection* FSynchronizeFactoryMac::CreateCriticalSection(void)
{
	return new FCriticalSection();
}

/**
 * Creates a new event
 *
 * @param bIsManualReset Whether the event requires manual reseting or not
 * @param InName Whether to use a commonly shared event or not. If so this
 * is the name of the event to share.
 *
 * @return Returns the new event object if successful, NULL otherwise
 */
FEvent* FSynchronizeFactoryMac::CreateSynchEvent(UBOOL bIsManualReset,
	const TCHAR* InName)
{
	// Allocate the new object
	FEvent* Event = new FEventMac();
	// If the internal create fails, delete the instance and return NULL
	if (Event->Create(bIsManualReset,InName) == FALSE)
	{
		delete Event;
		Event = NULL;
	}
	return Event;
}

/**
 * Cleans up the specified synchronization object using the correct heap
 *
 * @param InSynchObj The synchronization object to destroy
 */
void FSynchronizeFactoryMac::Destroy(FSynchronize* InSynchObj)
{
	delete InSynchObj;
}

/**
 * Zeros any members
 */
FQueuedThreadMac::FQueuedThreadMac(void) :
	DoWorkEvent(NULL),
	ThreadCreated(FALSE),
	TimeToDie(FALSE),
	ThreadHasTerminated(FALSE),
	QueuedWork(NULL),
	QueuedWorkSynch(NULL),
	OwningThreadPool(NULL)
{
}

/**
 * Deletes any allocated resources. Kills the thread if it is running.
 */
FQueuedThreadMac::~FQueuedThreadMac(void)
{
	// If there is a background thread running, kill it
	if (ThreadCreated)
	{
		// Kill() will clean up the event
		Kill(TRUE);
	}
}

/**
 * The thread entry point. Simply forwards the call on to the right
 * thread main function
 */
void *FQueuedThreadMac::_ThreadProc(void *pThis)
{
	check(pThis);
	void *CocoaAutoreleasePool = MacCreateCocoaAutoreleasePool();
	((FQueuedThreadMac*)pThis)->Run();
	MacReleaseCocoaAutoreleasePool( CocoaAutoreleasePool );
	return NULL;
}

/**
 * The real thread entry point. It waits for work events to be queued. Once
 * an event is queued, it executes it and goes back to waiting.
 */
void FQueuedThreadMac::Run(void)
{
	while (!TimeToDie)
	{
		STAT(StatsUpdate.ConditionalUpdate());   // maybe advance the stats frame
		// Wait for some work to do
		DoWorkEvent->Wait();
		FQueuedWork* LocalQueuedWork = QueuedWork;
		QueuedWork = NULL;
		check(LocalQueuedWork || TimeToDie); // well you woke me up, where is the job or termination request?
		while (LocalQueuedWork)
		{
			STAT(StatsUpdate.ConditionalUpdate());   // maybe advance the stats frame
			// Tell the object to do the work
			LocalQueuedWork->DoThreadedWork();
			// Let the object cleanup before we remove our ref to it
			LocalQueuedWork = OwningThreadPool->ReturnToPoolOrGetNextJob(this);
		} 
	}
	ThreadHasTerminated = TRUE;
}


typedef void *(PthreadEntryPoint)(void *arg);

static UBOOL SpinPthread(pthread_t *HandlePtr, PthreadEntryPoint Proc, DWORD ProcessorMask, DWORD InStackSize, void *Arg)
{
	UBOOL ThreadCreated = FALSE;
	pthread_attr_t *AttrPtr = NULL;
	pthread_attr_t StackAttr;

	if (InStackSize != 0)
	{
		// !!! FIXME: there are places where the requested stack is too small, which causes crashes from stack overflow on Mac OS X. Look into these later. For now, just use the default stack size everywhere.
		STUBBED("Non-default stack requested for new thread");
		InStackSize = 0;
	}

	if (InStackSize != 0)
	{
		if (pthread_attr_init(&StackAttr) == 0)
		{
			// we'll use this the attribute if this succeeds, otherwise, we'll wing it without it.
			const size_t StackSize = (size_t) InStackSize;
			if (pthread_attr_setstacksize(&StackAttr, StackSize) == 0)
			{
				AttrPtr = &StackAttr;
			}
		}

		if (AttrPtr == NULL)
		{
			debugf(TEXT("Failed to change pthread stack size to %d bytes"), (int) InStackSize);
		}
	}

	const int ThreadErrno = pthread_create(HandlePtr, AttrPtr, Proc, Arg);
	ThreadCreated = (ThreadErrno == 0);
	if (AttrPtr != NULL)
	{
		pthread_attr_destroy(AttrPtr);
	}

	// Move the thread to the specified processors if requested
	if (!ThreadCreated)
	{
		debugf(TEXT("Failed to create thread! (err=%d, %s)"), ThreadErrno, UTF8_TO_TCHAR(strerror(ThreadErrno)));
	}
	else
	{
		// just daemonize the thread, let it die on its own timetable and not
		//  leave a zombie that the system doesn't clean up.
		pthread_detach(*HandlePtr);

		if (ProcessorMask > 0)
		{
			// !!! FIXME
			// The Linux API for this is a little flakey right now...apparently
			//  the default scheduling is quite good, though!
			STUBBED("Processor affinity");
		}
	}

	return ThreadCreated;
}


/**
 * Creates the thread with the specified stack size and creates the various
 * events to be able to communicate with it.
 *
 * @param InPool The thread pool interface used to place this thread
 * back into the pool of available threads when its work is done
 * @param ProcessorMask Specifies which processors should be used by the pool
 * @param InStackSize The size of the stack to create. 0 means use the
 * current thread's stack size
 *
 * @return True if the thread and all of its initialization was successful, false otherwise
 */
UBOOL FQueuedThreadMac::Create(FQueuedThreadPool* InPool,DWORD ProcessorMask,
	DWORD InStackSize,EThreadPriority ThreadPriority)
{
	check(OwningThreadPool == NULL && ThreadCreated == FALSE);
	ThreadHasTerminated = TimeToDie = FALSE;
	// Copy the parameters for use in the thread
	OwningThreadPool = InPool;
	// Create the work event used to notify this thread of work
	DoWorkEvent = GSynchronizeFactory->CreateSynchEvent();
	QueuedWorkSynch = GSynchronizeFactory->CreateCriticalSection();
	if (DoWorkEvent != NULL && QueuedWorkSynch != NULL)
	{
		ThreadCreated = SpinPthread(&ThreadHandle, _ThreadProc, ProcessorMask, InStackSize, this);
	}
	// If it fails, clear all the vars
	if (!ThreadCreated)
	{
		OwningThreadPool = NULL;
		// Use the factory to clean up this event
		if (DoWorkEvent != NULL)
		{
			GSynchronizeFactory->Destroy(DoWorkEvent);
		}
		DoWorkEvent = NULL;
		if (QueuedWorkSynch != NULL)
		{
			// Clean up the work synch
			GSynchronizeFactory->Destroy(QueuedWorkSynch);
		}
		QueuedWorkSynch = NULL;
	}

	return ThreadCreated;
}

/**
 * Tells the thread to exit. If the caller needs to know when the thread
 * has exited, it should use the bShouldWait value and tell it how long
 * to wait before deciding that it is deadlocked and needs to be destroyed.
 * NOTE: having a thread forcibly destroyed can cause leaks in TLS, etc.
 *
 * @param bShouldWait If true, the call will wait for the thread to exit
 * @param bShouldDeleteSelf Whether to delete ourselves upon completion
 *
 * @return True if the thread exited gracefully, false otherwise
 */
UBOOL FQueuedThreadMac::Kill(UBOOL bShouldWait, UBOOL bShouldDeleteSelf)
{
	UBOOL bDidExitOK = TRUE;
	// Tell the thread it needs to die
	TimeToDie = TRUE;
	// Trigger the thread so that it will come out of the wait state if
	// it isn't actively doing work
	DoWorkEvent->Trigger();
	// If waiting was specified, wait the amount of time. If that fails,
	// brute force kill that thread. Very bad as that might leak.
	if (bShouldWait)
	{
		while (!ThreadHasTerminated)
		{
			usleep(10000);
		}

	}

	// It's not really safe to kill a pthread. So don't do it.

	// Now clean up the thread handle so we don't leak
	ThreadCreated = FALSE;
	// Clean up the event
	GSynchronizeFactory->Destroy(DoWorkEvent);
	DoWorkEvent = NULL;
	// Clean up the work synch
	GSynchronizeFactory->Destroy(QueuedWorkSynch);
	QueuedWorkSynch = NULL;
	// Delete ourselves if requested
	if (bShouldDeleteSelf)
	{
		delete this;
	}
	return bDidExitOK;
}

/**
 * Tells the thread there is work to be done. Upon completion, the thread
 * is responsible for adding itself back into the available pool.
 *
 * @param InQueuedWork The queued work to perform
 */
void FQueuedThreadMac::DoWork(FQueuedWork* InQueuedWork)
{
	{
		FScopeLock sl(QueuedWorkSynch);
		check(QueuedWork == NULL && "Can't do more than one task at a time");
		// Tell the thread the work to be done
		QueuedWork = InQueuedWork;
	}
	// Tell the thread to wake up and do its job
	DoWorkEvent->Trigger();
}

/**
 * Cleans up any threads that were allocated in the pool
 */
FQueuedThreadPoolMac::~FQueuedThreadPoolMac(void)
{
	if (QueuedThreads.Num() > 0)
	{
		Destroy();
	}
}

/**
 * Creates the thread pool with the specified number of threads
 *
 * @param InNumQueuedThreads Specifies the number of threads to use in the pool
 * @param ProcessorMask Specifies which processors should be used by the pool
 * @param StackSize The size of stack the threads in the pool need (32K default)
 *
 * @return Whether the pool creation was successful or not
 */
UBOOL FQueuedThreadPoolMac::Create(DWORD InNumQueuedThreads,DWORD ProcessorMask,
	DWORD StackSize,EThreadPriority ThreadPriority)
{
	// Make sure we have synch objects
	UBOOL bWasSuccessful = CreateSynchObjects();
	if (bWasSuccessful == TRUE)
	{
		FScopeLock Lock(SynchQueue);
		// Presize the array so there is no extra memory allocated
		QueuedThreads.Empty(InNumQueuedThreads);
		// Now create each thread and add it to the array
		for (DWORD Count = 0; Count < InNumQueuedThreads && bWasSuccessful == TRUE;
			Count++)
		{
			// Create a new queued thread
			FQueuedThread* pThread = new FQueuedThreadMac();
			// Now create the thread and add it if ok
			if (pThread->Create(this,ProcessorMask,StackSize) == TRUE)
			{
				QueuedThreads.AddItem(pThread);
			}
			else
			{
				// Failed to fully create so clean up
				bWasSuccessful = FALSE;
				delete pThread;
			}
		}
	}
	// Destroy any created threads if the full set was not succesful
	if (bWasSuccessful == FALSE)
	{
		Destroy();
	}
	return bWasSuccessful;
}

/**
 * Zeroes members
 */
FRunnableThreadMac::FRunnableThreadMac(void)
{
	ThreadCreated			= FALSE;
	ThreadHasTerminated		= FALSE;
	Runnable				= NULL;
	bShouldDeleteSelf		= FALSE;
	bShouldDeleteRunnable	= FALSE;
}

/**
 * Cleans up any resources
 */
FRunnableThreadMac::~FRunnableThreadMac(void)
{
	// Clean up our thread if it is still active
	if (ThreadCreated)
	{
		Kill(TRUE);
	}
}

/**
 * Creates the thread with the specified stack size and thread priority.
 *
 * @param InRunnable The runnable object to execute
 * @param ThreadName Name of the thread
 * @param bAutoDeleteSelf Whether to delete this object on exit
 * @param bAutoDeleteRunnable Whether to delete the runnable object on exit
 * @param InStackSize The size of the stack to create. 0 means use the
 * current thread's stack size
 * @param InThreadPri Tells the thread whether it needs to adjust its
 * priority or not. Defaults to normal priority
 *
 * @return True if the thread and all of its initialization was successful, false otherwise
 */
UBOOL FRunnableThreadMac::Create(FRunnable* InRunnable, const TCHAR* ThreadName,
	UBOOL bAutoDeleteSelf,UBOOL bAutoDeleteRunnable,DWORD InStackSize,
	EThreadPriority InThreadPri)
{
	check(InRunnable);
	Runnable = InRunnable;
	ThreadPriority = InThreadPri;
	bShouldDeleteSelf = bAutoDeleteSelf;
	bShouldDeleteRunnable = bAutoDeleteRunnable;
	ThreadHasTerminated = FALSE;
	bInitCalled = FALSE;

	// Create the new thread
	ThreadCreated = SpinPthread(&ThreadHandle, _ThreadProc, 0, InStackSize, this);
	if (ThreadCreated)
	{
		pthread_detach(ThreadHandle);  // we can't join on these, since we can't determine when they'll die.

		while (!bInitCalled)
		{
			usleep(10000);  // spin until thread initializes or dies.
		}
	}
	else // If it fails, clear all the vars
	{
		if (bAutoDeleteRunnable == TRUE)
		{
			delete InRunnable;
		}
		Runnable = NULL;
	}
	return ThreadCreated;
}

/**
 * Tells the thread to either pause execution or resume depending on the
 * passed in value.
 *
 * @param bShouldPause Whether to pause the thread (true) or resume (false)
 */
void FRunnableThreadMac::Suspend(UBOOL bShouldPause)
{
	// !!! FIXME: you can't do this in pthreads! And you SHOULDN'T do this anywhere!!
	STUBBED("Thread suspension");
#if 0
	check(Thread);
	if (bShouldPause == TRUE)
	{
		SuspendThread(Thread);
	}
	else
	{
		ResumeThread(Thread);
	}
#endif
}

/**
 * Tells the thread to exit. If the caller needs to know when the thread
 * has exited, it should use the bShouldWait value and tell it how long
 * to wait before deciding that it is deadlocked and needs to be destroyed.
 * NOTE: having a thread forcibly destroyed can cause leaks in TLS, etc.
 *
 * @param bShouldWait If true, the call will wait for the thread to exit
 *
 * @return True if the thread exited graceful, false otherwise
 */
UBOOL FRunnableThreadMac::Kill(UBOOL bShouldWait)
{
	check(ThreadCreated && Runnable && "Did you forget to call Create()?");
	UBOOL bMakeDaemon = FALSE;
	UBOOL bDidExitOK = TRUE;
	// Let the runnable have a chance to stop without brute force killing
	Runnable->Stop();

	// If waiting was specified, wait the amount of time. If that fails,
	// brute force kill that thread. Very bad as that might leak.
	if (bShouldWait)
	{
		while (!ThreadHasTerminated)
		{
			usleep(10000);
		}

	}

	// It's not really safe to kill a pthread. So don't do it.


	// Now clean up the thread handle so we don't leak
	ThreadCreated = FALSE;

	// Should we delete the runnable?
	if (bShouldDeleteRunnable == TRUE)
	{
		delete Runnable;
		Runnable = NULL;
	}
	// Delete ourselves if requested
	if (bShouldDeleteSelf == TRUE)
	{
		GThreadFactory->Destroy(this);
	}
	return bDidExitOK;
}

/**
 * The thread entry point. Simply forwards the call on to the right
 * thread main function
 */
void *FRunnableThreadMac::_ThreadProc(void *pThis)
{
	check(pThis);
	// !!! FIXME: we ignore the return value.
	//return ((FRunnableThreadMac*)pThis)->Run();
	void *CocoaAutoreleasePool = MacCreateCocoaAutoreleasePool();
	((FRunnableThreadMac*)pThis)->Run();
	MacReleaseCocoaAutoreleasePool( CocoaAutoreleasePool );
	return NULL;
} 

/**
 * The real thread entry point. It calls the Init/Run/Exit methods on
 * the runnable object
 *
 * @return The exit code of the thread
 */
DWORD FRunnableThreadMac::Run(void)
{
	// Assume we'll fail init
	DWORD ExitCode = 1;
	check(Runnable);
	// Twiddle the thread priority
	if (ThreadPriority != TPri_Normal)
	{
		SetThreadPriority(ThreadPriority);
	}
	// Initialize the runnable object
	const UBOOL bInitSucceeded = (Runnable->Init() == TRUE);

	bInitCalled = TRUE;  // let parent thread know it's safe to continue.

	if (bInitSucceeded)
	{
		// Now run the task that needs to be done
		ExitCode = Runnable->Run();
		// Allow any allocated resources to be cleaned up
		Runnable->Exit();
	}
	// Should we delete the runnable?
	if (bShouldDeleteRunnable == TRUE)
	{
		delete Runnable;
		Runnable = NULL;
	}
	// Clean ourselves up without waiting
	if (bShouldDeleteSelf == TRUE)
	{
		ThreadCreated = FALSE;
		GThreadFactory->Destroy(this);
	}

	ThreadHasTerminated = TRUE;
	return ExitCode;
}

/**
 * Changes the thread priority of the currently running thread
 *
 * @param NewPriority The thread priority to change to
 */
void FRunnableThreadMac::SetThreadPriority(EThreadPriority NewPriority)
{
	// Don't bother calling the OS if there is no need
	if (NewPriority != ThreadPriority)
	{
		ThreadPriority = NewPriority;
		STUBBED("Thread priority");
		// Change the priority on the thread
#if 0
		::SetThreadPriority(Thread,
			ThreadPriority == TPri_AboveNormal ? THREAD_PRIORITY_ABOVE_NORMAL :
			ThreadPriority == TPri_BelowNormal ? THREAD_PRIORITY_BELOW_NORMAL :
			THREAD_PRIORITY_NORMAL);
#endif
	}
}

/**
 * Tells the OS the preferred CPU to run the thread on.
 *
 * @param ProcessorNum The preferred processor for executing the thread on
 */
void FRunnableThreadMac::SetProcessorAffinity(DWORD ProcessorNum)
{
	appSetThreadAffinity(ThreadHandle,ProcessorNum);
}

/**
 * Halts the caller until this thread is has completed its work.
 */
void FRunnableThreadMac::WaitForCompletion(void)
{
	// Block until this thread exits
	while (!ThreadHasTerminated)
	{
		usleep(10000);
    }
}

/**
* Thread ID for this thread 
*
* @return ID that was set by CreateThread
*/
DWORD FRunnableThreadMac::GetThreadID(void)
{
	return ((ThreadCreated) ? ((DWORD) pthread_mach_thread_np(ThreadHandle)) : 0);
}

/**
 * Creates the thread with the specified stack size and thread priority.
 *
 * @param InRunnable The runnable object to execute
 * @param ThreadName Name of the thread
 * @param bAutoDeleteSelf Whether to delete this object on exit
 * @param bAutoDeleteRunnable Whether to delete the runnable object on exit
 * @param InStackSize The size of the stack to create. 0 means use the
 * current thread's stack size
 * @param InThreadPri Tells the thread whether it needs to adjust its
 * priority or not. Defaults to normal priority
 *
 * @return The newly created thread or NULL if it failed
 */
FRunnableThread* FThreadFactoryMac::CreateThread(FRunnable* InRunnable, const TCHAR* ThreadName,
	UBOOL bAutoDeleteSelf,UBOOL bAutoDeleteRunnable,DWORD InStackSize,
	EThreadPriority InThreadPri)
{
	check(InRunnable);
	// Create a new thread object
	FRunnableThreadMac* NewThread = new FRunnableThreadMac();
	if (NewThread)
	{
		// Call the thread's create method
		if (NewThread->Create(InRunnable,ThreadName,bAutoDeleteSelf,bAutoDeleteRunnable,
			InStackSize,InThreadPri) == FALSE)
		{
			// We failed to start the thread correctly so clean up
			Destroy(NewThread);
			NewThread = NULL;
		}
	}
	return NewThread;
}

/**
 * Cleans up the specified thread object using the correct heap
 *
 * @param InThread The thread object to destroy
 */
void FThreadFactoryMac::Destroy(FRunnableThread* InThread)
{
	delete InThread;
}

#endif  // PLATFORM_MACOSX 
