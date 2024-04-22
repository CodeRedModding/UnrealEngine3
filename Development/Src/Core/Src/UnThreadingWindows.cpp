/**
 * UnThreadingWindows.cpp -- Contains all Windows platform specific definitions
 * of interfaces and concrete classes for multithreading support in the Unreal
 * engine.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "CorePrivate.h"

#if _MSC_VER

#include "UnThreadingWindows.h"

/** The global synchonization object factory.	*/
FSynchronizeFactory*	GSynchronizeFactory				= NULL;
/** The global thread factory.					*/
FThreadFactory*			GThreadFactory					= NULL;
/** The global thread pool */
FQueuedThreadPool*		GThreadPool						= NULL;


/**
 * Code setting the thread name for use in the debugger.
 *
 * http://msdn.microsoft.com/en-us/library/xcb2z8hs.aspx
 */
#define MS_VC_EXCEPTION 0x406D1388

#pragma pack(push,8)
typedef struct tagTHREADNAME_INFO
{
	DWORD dwType;		// Must be 0x1000.
	LPCSTR szName;		// Pointer to name (in user addr space).
	DWORD dwThreadID;	// Thread ID (-1=caller thread).
	DWORD dwFlags;		// Reserved for future use, must be zero.
} THREADNAME_INFO;
#pragma pack(pop)

/**
 * Helper function to set thread names, visible by the debugger.
 * @param ThreadID		Thread ID for the thread who's name is going to be set
 * @param ThreadName	Name to set
 */
void SetThreadName( DWORD ThreadID, LPCSTR ThreadName )
{
	// on the xbox setting thread names messes up the XDK COM API that UnrealConsole uses so check to see if they have been
	// explicitly enabled
#if XBOX
	if(GSetThreadNames)
	{
#endif
		Sleep(10);
		THREADNAME_INFO ThreadNameInfo;
		ThreadNameInfo.dwType		= 0x1000;
		ThreadNameInfo.szName		= ThreadName;
		ThreadNameInfo.dwThreadID	= ThreadID;
		ThreadNameInfo.dwFlags		= 0;

		__try
		{
			RaiseException( MS_VC_EXCEPTION, 0, sizeof(ThreadNameInfo)/sizeof(ULONG_PTR), (ULONG_PTR*)&ThreadNameInfo );
		}
		__except(EXCEPTION_EXECUTE_HANDLER)
		{
		}
#if XBOX
	}
#endif
}


/**
 * Constructor that zeroes the handle
 */
FEventWin::FEventWin(void)
{
	Event = NULL;
}

/**
 * Cleans up the event handle if valid
 */
FEventWin::~FEventWin(void)
{
	if (Event != NULL)
	{
		CloseHandle(Event);
	}
}

/**
 * Waits for the event to be signaled before returning
 */
void FEventWin::Lock(void)
{
	WaitForSingleObject(Event,INFINITE);
}

/**
 * Triggers the event so any waiting threads are allowed access
 */
void FEventWin::Unlock(void)
{
	PulseEvent(Event);
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
UBOOL FEventWin::Create(UBOOL bIsManualReset,const TCHAR* InName)
{
	// Create the event and default it to non-signaled
#if XBOX
	Event = CreateEventA(NULL,bIsManualReset,0,TCHAR_TO_ANSI(InName));
#else
	Event = CreateEvent(NULL,bIsManualReset,0,InName);
#endif
	return Event != NULL;
}

/**
 * Triggers the event so any waiting threads are activated
 */
void FEventWin::Trigger(void)
{
	check(Event);
	SetEvent(Event);
}

/**
 * Resets the event to an untriggered (waitable) state
 */
void FEventWin::Reset(void)
{
	check(Event);
	ResetEvent(Event);
}

/**
 * Triggers the event and resets the triggered state NOTE: This behaves
 * differently for auto-reset versus manual reset events. All threads
 * are released for manual reset events and only one is for auto reset
 */
void FEventWin::Pulse(void)
{
	check(Event);
	PulseEvent(Event);
}

/**
 * Waits for the event to be triggered
 *
 * @param WaitTime Time in milliseconds to wait before abandoning the event
 * (DWORD)-1 is treated as wait infinite
 *
 * @return TRUE if the event was signaled, FALSE if the wait timed out
 */
UBOOL FEventWin::Wait(DWORD WaitTime)
{
	check(Event);
	return WaitForSingleObject(Event,WaitTime) == WAIT_OBJECT_0;
}

/**
 * Zeroes its members
 */
FSynchronizeFactoryWin::FSynchronizeFactoryWin(void)
{
}

/**
 * Creates a new critical section
 *
 * @return The new critical section object or NULL otherwise
 */
FCriticalSection* FSynchronizeFactoryWin::CreateCriticalSection(void)
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
FEvent* FSynchronizeFactoryWin::CreateSynchEvent(UBOOL bIsManualReset,
	const TCHAR* InName)
{
	// Allocate the new object
	FEvent* Event = new FEventWin();
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
void FSynchronizeFactoryWin::Destroy(FSynchronize* InSynchObj)
{
	delete InSynchObj;
}

/**
 * Zeros any members
 */
FQueuedThreadWin::FQueuedThreadWin(void) :
	DoWorkEvent(NULL),
	ThreadHandle(NULL),
	ThreadID(0),
	TimeToDie(FALSE),
	QueuedWork(NULL),
	OwningThreadPool(NULL)
{
}

/**
 * Deletes any allocated resources. Kills the thread if it is running.
 */
FQueuedThreadWin::~FQueuedThreadWin(void)
{
	// If there is a background thread running, kill it
	if (ThreadHandle != NULL)
	{
		// Kill() will clean up the event
		Kill(TRUE);
	}
}

/**
 * The thread entry point. Simply forwards the call on to the right
 * thread main function
 */
DWORD STDCALL FQueuedThreadWin::_ThreadProc(LPVOID pThis)
{
	check(pThis);
	((FQueuedThreadWin*)pThis)->Run();
	return 0;
}

/**
 * The real thread entry point. It waits for work events to be queued. Once
 * an event is queued, it executes it and goes back to waiting.
 */
void FQueuedThreadWin::Run(void)
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
 * @param ThreadPriority priority of new thread
 *
 * @return True if the thread and all of its initialization was successful, false otherwise
 */
UBOOL FQueuedThreadWin::Create(FQueuedThreadPool* InPool,DWORD ProcessorMask,
	DWORD InStackSize,EThreadPriority ThreadPriority)
{
	check(OwningThreadPool == NULL && ThreadHandle == NULL);
	// Copy the parameters for use in the thread
	OwningThreadPool = InPool;
	// Create the work event used to notify this thread of work
	DoWorkEvent = GSynchronizeFactory->CreateSynchEvent();
	if (DoWorkEvent != NULL)
	{
		// Create the new thread
		ThreadHandle = CreateThread(NULL,InStackSize,_ThreadProc,this,0,&ThreadID);
		// Move the thread to the specified processors if requested
		if (ThreadHandle != NULL && ProcessorMask > 0)
		{
#if PLATFORM_WINDOWS
			// The mask specifies a set of processors to run on
			SetThreadAffinityMask(ThreadHandle,(DWORD_PTR)ProcessorMask);
#elif XBOX
			DWORD ProcNum = 0;
			DWORD ProcMask = 1;
			// Search for the first matching processor
			while ((ProcMask & ProcessorMask) == 0 && ProcMask < 64)
			{
				ProcNum++;
				ProcMask <<= 1;
			}
			check(ProcNum < 6);
			// Use the converted mask value to determine the hwthread #
			XSetThreadProcessor(ThreadHandle,ProcNum);
#endif
		}
	}
	// If it fails, clear all the vars
	if (ThreadHandle == NULL)
	{
		OwningThreadPool = NULL;
		// Use the factory to clean up this event
		if (DoWorkEvent != NULL)
		{
			GSynchronizeFactory->Destroy(DoWorkEvent);
		}
		DoWorkEvent = NULL;
	}
	else
	{
		// Let the thread start up, then set the name for debug purposes.
		Sleep(1);
		if (ThreadPriority != TPri_Normal)
		{
			check(ThreadPriority == TPri_AboveNormal || ThreadPriority == TPri_BelowNormal);
			SetThreadName( ThreadID,ThreadPriority == TPri_AboveNormal ? "HiPriPoolThread" : "LoPriPoolThread" );
			SetThreadPriority( ThreadHandle, ThreadPriority == TPri_AboveNormal ? THREAD_PRIORITY_ABOVE_NORMAL : THREAD_PRIORITY_BELOW_NORMAL );
		}
		else
		{
			SetThreadName( ThreadID, "PoolThread" );
		}
	}
	return ThreadHandle != NULL;
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
UBOOL FQueuedThreadWin::Kill(UBOOL bShouldWait,UBOOL bShouldDeleteSelf)
{
	UBOOL bDidExitOK = TRUE;
	// Tell the thread it needs to die
	InterlockedExchange((LPLONG)&TimeToDie,TRUE);
	// Trigger the thread so that it will come out of the wait state if
	// it isn't actively doing work
	DoWorkEvent->Trigger();
	// If waiting was specified, wait the amount of time. If that fails,
	// brute force kill that thread. Very bad as that might leak.
	if (bShouldWait == TRUE)
	{
		// Wait indefinitely for the thread to finish.  IMPORTANT:  It's not safe to just go and
		// kill the thread with TerminateThread() as it could have a mutex lock that's shared
		// with a thread that's continuing to run, which would cause that other thread to
		// dead-lock.  (This can manifest itself in code as simple as the synchronization
		// object that is used by our logging output classes.  Trust us, we've seen it!)
		WaitForSingleObject(ThreadHandle, INFINITE);
	}
	// Now clean up the thread handle so we don't leak
	CloseHandle(ThreadHandle);
	ThreadHandle = NULL;
	// Clean up the event
	GSynchronizeFactory->Destroy(DoWorkEvent);
	DoWorkEvent = NULL;
	// Clean up the work synch
	TimeToDie = FALSE;
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
void FQueuedThreadWin::DoWork(FQueuedWork* InQueuedWork)
{
	check(QueuedWork == NULL && "Can't do more than one task at a time");
	// Tell the thread the work to be done
	QueuedWork = InQueuedWork;
	// Tell the thread to wake up and do its job
	DoWorkEvent->Trigger();
}

/**
 * Cleans up any threads that were allocated in the pool
 */
FQueuedThreadPoolWin::~FQueuedThreadPoolWin(void)
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
 * @param ThreadPriority priority of new pool thread
 *
 * @return Whether the pool creation was successful or not
 */
UBOOL FQueuedThreadPoolWin::Create(DWORD InNumQueuedThreads,DWORD ProcessorMask,
	DWORD StackSize,EThreadPriority ThreadPriority)
{
	// Make sure we have synch objects
	UBOOL bWasSuccessful = CreateSynchObjects();
	if (bWasSuccessful == TRUE)
	{
		FScopeLock Lock(SynchQueue);
		// Presize the array so there is no extra memory allocated
		QueuedThreads.Empty(InNumQueuedThreads);
#if XBOX
		// Used to set which processor for each pooled thread
		DWORD NextMask = 1;
#endif
		// Now create each thread and add it to the array
		for (DWORD Count = 0; Count < InNumQueuedThreads && bWasSuccessful == TRUE;
			Count++)
		{
			// Create a new queued thread
			FQueuedThread* pThread = new FQueuedThreadWin();
#if XBOX
			// Skip if we aren't specifying where
			if (ProcessorMask > 0)
			{
				// Figure out where we want this one to run
				while ((NextMask & ProcessorMask) == 0)
				{
					NextMask <<= 1;
					// Wrap back around if we've passed the end
					if (NextMask > ProcessorMask)
					{
						NextMask = 1;
					}
				}
			}
			else
			{
				NextMask = 0;
			}
			DWORD CurrentMask = NextMask;
			NextMask <<= 1;
			// Now create the thread and add it if ok
			if (pThread->Create(this,CurrentMask,StackSize,ThreadPriority) == TRUE)
#else
			// Now create the thread and add it if ok
			if (pThread->Create(this,ProcessorMask,StackSize,ThreadPriority) == TRUE)
#endif
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
	// Destroy any created threads if the full set was not successful
	if (bWasSuccessful == FALSE)
	{
		Destroy();
	}
	return bWasSuccessful;
}

/**
 * Zeroes members
 */
FRunnableThreadWin::FRunnableThreadWin(void) :
	Thread(NULL),
	Runnable(NULL),				
	ThreadInitSyncEvent(NULL),	
	bShouldDeleteSelf(FALSE),	
	bShouldDeleteRunnable(FALSE),
	ThreadID(NULL)				
{}

/**
 * Cleans up any resources
 */
FRunnableThreadWin::~FRunnableThreadWin(void)
{
	// Clean up our thread if it is still active
	if (Thread != NULL)
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
UBOOL FRunnableThreadWin::Create(FRunnable* InRunnable, const TCHAR* ThreadName,
	UBOOL bAutoDeleteSelf,UBOOL bAutoDeleteRunnable,DWORD InStackSize,
	EThreadPriority InThreadPri)
{
	check(InRunnable);
	Runnable = InRunnable;
	ThreadPriority = InThreadPri;
	bShouldDeleteSelf = bAutoDeleteSelf;
	bShouldDeleteRunnable = bAutoDeleteRunnable;

	// Create a sync event to guarantee the Init() function is called first
	ThreadInitSyncEvent	= GSynchronizeFactory->CreateSynchEvent(TRUE);
	// Create the new thread
	Thread = CreateThread(NULL,InStackSize,_ThreadProc,this,0,&ThreadID);
	// If it fails, clear all the vars
	if (Thread == NULL)
	{
		if (bAutoDeleteRunnable == TRUE)
		{
			delete InRunnable;
		}
		Runnable = NULL;
	}
	else
	{
		// Let the thread start up, then set the name for debug purposes.
		ThreadInitSyncEvent->Wait(INFINITE);
		SetThreadName( ThreadID, ThreadName ? TCHAR_TO_ANSI( ThreadName ) : "Unnamed UE3" );
	}

	// Cleanup the sync event
	GSynchronizeFactory->Destroy(ThreadInitSyncEvent);
	ThreadInitSyncEvent = NULL;
	return Thread != NULL;
}

/**
 * Tells the thread to either pause execution or resume depending on the
 * passed in value.
 *
 * @param bShouldPause Whether to pause the thread (true) or resume (false)
 */
void FRunnableThreadWin::Suspend(UBOOL bShouldPause)
{
	check(Thread);
	if (bShouldPause == TRUE)
	{
		SuspendThread(Thread);
	}
	else
	{
		ResumeThread(Thread);
	}
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
UBOOL FRunnableThreadWin::Kill(UBOOL bShouldWait)
{
	check(Thread && "Did you forget to call Create()?");
	UBOOL bDidExitOK = TRUE;
	// Let the runnable have a chance to stop without brute force killing
	if (Runnable)
	{
		Runnable->Stop();
	}
	// If waiting was specified, wait the amount of time. If that fails,
	// brute force kill that thread. Very bad as that might leak.
	if (bShouldWait == TRUE)
	{
		// Wait indefinitely for the thread to finish.  IMPORTANT:  It's not safe to just go and
		// kill the thread with TerminateThread() as it could have a mutex lock that's shared
		// with a thread that's continuing to run, which would cause that other thread to
		// dead-lock.  (This can manifest itself in code as simple as the synchronization
		// object that is used by our logging output classes.  Trust us, we've seen it!)
		WaitForSingleObject(Thread,INFINITE);
	}
	// Now clean up the thread handle so we don't leak
	CloseHandle(Thread);
	Thread = NULL;
	// delete the runnable if requested and we didn't shut down gracefully already.
	if (Runnable && bShouldDeleteRunnable == TRUE)
	{
		delete Runnable;
		Runnable = NULL;
	}
	// Delete ourselves if requested and we didn't shut down gracefully already.
	// This check prevents a double-delete of self when we shut down gracefully.
	if (!bDidExitOK && bShouldDeleteSelf == TRUE)
	{
		GThreadFactory->Destroy(this);
	}
	return bDidExitOK;
}

/**
 * The thread entry point. Simply forwards the call on to the right
 * thread main function
 */
DWORD STDCALL FRunnableThreadWin::_ThreadProc(LPVOID pThis)
{
	check(pThis);
	return ((FRunnableThreadWin*)pThis)->Run();
}

/**
 * The real thread entry point. It calls the Init/Run/Exit methods on
 * the runnable object
 *
 * @return The exit code of the thread
 */
DWORD FRunnableThreadWin::Run(void)
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
	if (Runnable->Init() == TRUE)
	{
		// Initialization has completed, release the sync event
		ThreadInitSyncEvent->Trigger();
		// Now run the task that needs to be done
		ExitCode = Runnable->Run();
		// Allow any allocated resources to be cleaned up
		Runnable->Exit();
	}
	else
	{
		// Initialization has failed, release the sync event
		ThreadInitSyncEvent->Trigger();
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
		// Now clean up the thread handle so we don't leak
		CloseHandle(Thread);
		Thread = NULL;
		GThreadFactory->Destroy(this);
	}
	return ExitCode;
}

/**
 * Changes the thread priority of the currently running thread
 *
 * @param NewPriority The thread priority to change to
 */
void FRunnableThreadWin::SetThreadPriority(EThreadPriority NewPriority)
{
	// Don't bother calling the OS if there is no need
	if (NewPriority != ThreadPriority)
	{
		ThreadPriority = NewPriority;
		// Change the priority on the thread
		::SetThreadPriority(Thread,
			ThreadPriority == TPri_AboveNormal ? THREAD_PRIORITY_ABOVE_NORMAL :
			ThreadPriority == TPri_BelowNormal ? THREAD_PRIORITY_BELOW_NORMAL :
			THREAD_PRIORITY_NORMAL);
	}
}

/**
 * Tells the OS the preferred CPU to run the thread on.
 *
 * @param ProcessorNum The preferred processor for executing the thread on
 */
void FRunnableThreadWin::SetProcessorAffinity(DWORD ProcessorNum)
{
	appSetThreadAffinity(Thread,ProcessorNum);
}

/**
 * Halts the caller until this thread is has completed its work.
 */
void FRunnableThreadWin::WaitForCompletion(void)
{
	// Block until this thread exits
	WaitForSingleObject(Thread,INFINITE);
}

/**
* Thread ID for this thread 
*
* @return ID that was set by CreateThread
*/
DWORD FRunnableThreadWin::GetThreadID(void)
{
	return ThreadID;
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
FRunnableThread* FThreadFactoryWin::CreateThread(FRunnable* InRunnable, const TCHAR* ThreadName,
	UBOOL bAutoDeleteSelf,UBOOL bAutoDeleteRunnable,DWORD InStackSize,
	EThreadPriority InThreadPri)
{
	check(InRunnable);
	// Create a new thread object
	FRunnableThreadWin* NewThread = new FRunnableThreadWin();
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
void FThreadFactoryWin::Destroy(FRunnableThread* InThread)
{
	delete InThread;
}

#endif

