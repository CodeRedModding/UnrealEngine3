/**
 * UnThreadingWindows.h -- Contains all Windows platform specific definitions
 * of interfaces and concrete classes for multithreading support in the Unreal
 * engine.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#ifndef _UNTHREADING_WINDOWS_H
#define _UNTHREADING_WINDOWS_H

// Notify people of the windows dependency.
#if !defined(_WINBASE_) && !defined(_XTL_)
#error UnThreadingWindows.h relies on Windows.h/Xtl.h being included ahead of it
#endif

// Make sure version is high enough for API to be defined.
#if !defined(_XTL_) && (_WIN32_WINNT < 0x0403)
#error SetCriticalSectionSpinCount requires _WIN32_WINNT >= 0x0403
#endif

/**
 * Interlocked style functions for threadsafe atomic operations
 */

/**
 * Atomically increments the value pointed to and returns that to the caller
 */
FORCEINLINE INT appInterlockedIncrement(volatile INT* Value)
{
	return (INT)InterlockedIncrement((LPLONG)Value);
}
/**
 * Atomically decrements the value pointed to and returns that to the caller
 */
FORCEINLINE INT appInterlockedDecrement(volatile INT* Value)
{
	return (INT)InterlockedDecrement((LPLONG)Value);
}
/**
 * Atomically adds the amount to the value pointed to and returns the old
 * value to the caller
 */
FORCEINLINE INT appInterlockedAdd(volatile INT* Value,INT Amount)
{
	return (INT)InterlockedExchangeAdd((LPLONG)Value,(LONG)Amount);
}
/**
 * Atomically swaps two values returning the original value to the caller
 */
FORCEINLINE INT appInterlockedExchange(volatile INT* Value,INT Exchange)
{
	return (INT)InterlockedExchange((LPLONG)Value,(LONG)Exchange);
}
/**
 * Atomically compares the value to comperand and replaces with the exchange
 * value if they are equal and returns the original value
 */
FORCEINLINE INT appInterlockedCompareExchange(INT* Dest,INT Exchange,INT Comperand)
{
	return (INT)InterlockedCompareExchange((LPLONG)Dest,(LONG)Exchange,(LONG)Comperand);
}
/**
 * Atomically compares the pointer to comperand and replaces with the exchange
 * pointer if they are equal and returns the original value
 */
FORCEINLINE void* appInterlockedCompareExchangePointer(void** Dest,void* Exchange,void* Comperand)
{
	return InterlockedCompareExchangePointer(Dest,Exchange,Comperand);
}

/**
 * Returns a pseudo-handle to the currently executing thread.
 */
FORCEINLINE HANDLE appGetCurrentThread(void)
{
	return GetCurrentThread();
}

/**
 * Returns the currently executing thread's id
 */
FORCEINLINE DWORD appGetCurrentThreadId(void)
{
	return GetCurrentThreadId();
}

/**
 * Sets the preferred processor for a thread.
 *
 * @param	ThreadHandle		handle for the thread to set affinity for
 * @param	PreferredProcessor	zero-based index of the processor that this thread prefers
 *
 * @return	the number of the processor previously preferred by the thread, MAXIMUM_PROCESSORS
 *			if the thread didn't have a preferred processor, or (DWORD)-1 if the call failed.
 */
FORCEINLINE DWORD appSetThreadAffinity( HANDLE ThreadHandle, DWORD PreferredProcessor )
{
#ifndef XBOX
	return SetThreadIdealProcessor(ThreadHandle,PreferredProcessor);
#else
	return XSetThreadProcessor(ThreadHandle,PreferredProcessor);
#endif
}

/**
 * Allocates a thread local store slot
 */
FORCEINLINE DWORD appAllocTlsSlot(void)
{
	return TlsAlloc();
}

/**
 * Sets a value in the specified TLS slot
 *
 * @param SlotIndex the TLS index to store it in
 * @param Value the value to store in the slot
 */
FORCEINLINE void appSetTlsValue(DWORD SlotIndex,void* Value)
{
	TlsSetValue(SlotIndex,Value);
}

/**
 * Reads the value stored at the specified TLS slot
 *
 * @return the value stored in the slot
 */
FORCEINLINE void* appGetTlsValue(DWORD SlotIndex)
{
	return TlsGetValue(SlotIndex);
}

/**
 * Frees a previously allocated TLS slot
 *
 * @param SlotIndex the TLS index to store it in
 */
FORCEINLINE void appFreeTlsSlot(DWORD SlotIndex)
{
	TlsFree(SlotIndex);
}

/**
 * This is the Windows version of a critical section. It uses an aggregate
 * CRITICAL_SECTION to implement its locking.
 */
class FCriticalSection :
	public FSynchronize
{
	/**
	 * The windows specific critical section
	 */
	CRITICAL_SECTION CriticalSection;

public:
	/**
	 * Constructor that initializes the aggregated critical section
	 */
	FORCEINLINE FCriticalSection(void)
	{
		InitializeCriticalSection(&CriticalSection);
		SetCriticalSectionSpinCount(&CriticalSection,4000);
	}

	/**
	 * Destructor cleaning up the critical section
	 */
	FORCEINLINE ~FCriticalSection(void)
	{
		DeleteCriticalSection(&CriticalSection);
	}

	/**
	 * Locks the critical section
	 */
	FORCEINLINE void Lock(void)
	{
		// Spin first before entering critical section, causing ring-0 transition and context switch.
		if( TryEnterCriticalSection(&CriticalSection) == 0 )
		{
			EnterCriticalSection(&CriticalSection);
		}
	}

	/**
	 * Releases the lock on the critical seciton
	 */
	FORCEINLINE void Unlock(void)
	{
		LeaveCriticalSection(&CriticalSection);
	}
};

/**
 * This is the Windows version of an event
 */
class FEventWin : public FEvent
{
	/**
	 * The handle to the event
	 */
	HANDLE Event;

public:
	/**
	 * Constructor that zeroes the handle
	 */
	FEventWin(void);

	/**
	 * Cleans up the event handle if valid
	 */
	virtual ~FEventWin(void);

	/**
	 * Waits for the event to be signaled before returning
	 */
	virtual void Lock(void);

	/**
	 * Triggers the event so any waiting threads are allowed access
	 */
	virtual void Unlock(void);

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
	virtual UBOOL Create(UBOOL bIsManualReset = FALSE,const TCHAR* InName = NULL);

	/**
	 * Triggers the event so any waiting threads are activated
	 */
	virtual void Trigger(void);

	/**
	 * Resets the event to an untriggered (waitable) state
	 */
	virtual void Reset(void);

	/**
	 * Triggers the event and resets the triggered state NOTE: This behaves
	 * differently for auto-reset versus manual reset events. All threads
	 * are released for manual reset events and only one is for auto reset
	 */
	virtual void Pulse(void);

	/**
	 * Waits for the event to be triggered
	 *
	 * @param WaitTime Time in milliseconds to wait before abandoning the event
	 * (DWORD)-1 is treated as wait infinite
	 *
	 * @return TRUE if the event was signaled, FALSE if the wait timed out
	 */
	virtual UBOOL Wait(DWORD WaitTime = (DWORD)-1);
};

/**
 * This is the Windows factory for creating various synchronization objects.
 */
class FSynchronizeFactoryWin : public FSynchronizeFactory
{
public:
	/**
	 * Zeroes its members
	 */
	FSynchronizeFactoryWin(void);

	/**
	 * Creates a new critical section
	 *
	 * @return The new critical section object or NULL otherwise
	 */
	virtual FCriticalSection* CreateCriticalSection(void);

	/**
	 * Creates a new event
	 *
	 * @param bIsManualReset Whether the event requires manual reseting or not
	 * @param InName Whether to use a commonly shared event or not. If so this
	 * is the name of the event to share.
	 *
	 * @return Returns the new event object if successful, NULL otherwise
	 */
	virtual FEvent* CreateSynchEvent(UBOOL bIsManualReset = FALSE,const TCHAR* InName = NULL);

	/**
	 * Cleans up the specified synchronization object using the correct heap
	 *
	 * @param InSynchObj The synchronization object to destroy
	 */
	virtual void Destroy(FSynchronize* InSynchObj);
};

/**
 * This is the Windows class used for all poolable threads
 */
class FQueuedThreadWin : public FQueuedThread
{
	/**
	 * The event that tells the thread there is work to do
	 */
	FEvent* DoWorkEvent;

	/**
	 * The thread handle to clean up. Must be closed or this will leak resources
	 */
	HANDLE ThreadHandle;

	/**
	 * The thread ID for this thread
	 */
	DWORD ThreadID;

	/**
	 * If true, the thread should exit
	 */
	volatile UBOOL TimeToDie;

	/**
	 * The work this thread is doing
	 */
	 FQueuedWork* volatile QueuedWork;

	/**
	 * The pool this thread belongs to
	 */
	FQueuedThreadPool* OwningThreadPool;

	/**
	 * The thread entry point. Simply forwards the call on to the right
	 * thread main function
	 */
	static DWORD STDCALL _ThreadProc(LPVOID pThis);

	/**
	 * Helper to manage stat updates
	 */
	STAT(FCheckForStatsUpdate StatsUpdate);

	/**
	 * The real thread entry point. It waits for work events to be queued. Once
	 * an event is queued, it executes it and goes back to waiting.
	 */
	void Run(void);

public:
	/**
	 * Zeros any members
	 */
	FQueuedThreadWin(void);

	/**
	 * Deletes any allocated resources. Kills the thread if it is running.
	 */
	virtual ~FQueuedThreadWin(void);

	/**
	 * Creates the thread with the specified stack size and creates the various
	 * events to be able to communicate with it.
	 *
	 * @param InPool The thread pool interface used to place this thread
	 *		  back into the pool of available threads when its work is done
	 * @param ProcessorMask The processor set to run the thread on
	 * @param InStackSize The size of the stack to create. 0 means use the
	 *		  current thread's stack size
	 * @param ThreadPriority priority of new thread
	 *
	 * @return True if the thread and all of its initialization was successful, false otherwise
	 */
	virtual UBOOL Create(class FQueuedThreadPool* InPool,DWORD ProcessorMask,
		DWORD InStackSize = 0,EThreadPriority ThreadPriority=TPri_Normal);
	
	/**
	 * Tells the thread to exit. If the caller needs to know when the thread
	 * has exited, it should use the bShouldWait value and tell it how long
	 * to wait before deciding that it is deadlocked and needs to be destroyed.
	 * NOTE: having a thread forcibly destroyed can cause leaks in TLS, etc.
	 *
	 * @param bShouldWait If true, the call will wait for the thread to exit
	 * @param bShouldDeleteSelf Whether to delete ourselves upon completion
	 *
	 * @return True if the thread exited graceful, false otherwise
	 */
	virtual UBOOL Kill(UBOOL bShouldWait = FALSE, UBOOL bShouldDeleteSelf = FALSE);

	/**
	 * Tells the thread there is work to be done. Upon completion, the thread
	 * is responsible for adding itself back into the available pool.
	 *
	 * @param InQueuedWork The queued work to perform
	 */
	virtual void DoWork(FQueuedWork* InQueuedWork);
};

/**
 * This class fills in the platform specific features that the parent
 * class doesn't implement. The parent class handles all common, non-
 * platform specific code, while this class provides all of the Windows
 * specific methods. It handles the creation of the threads used in the
 * thread pool.
 */
class FQueuedThreadPoolWin : public FQueuedThreadPoolBase
{
public:
	/**
	 * Cleans up any threads that were allocated in the pool
	 */
	virtual ~FQueuedThreadPoolWin(void);

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
	virtual UBOOL Create(DWORD InNumQueuedThreads,DWORD ProcessorMask = 0,
		DWORD StackSize = (32 * 1024),EThreadPriority ThreadPriority=TPri_Normal);
};

/**
 * This is the base interface for all runnable thread classes. It specifies the
 * methods used in managing its life cycle.
 */
class FRunnableThreadWin : public FRunnableThread
{
	/**
	 * The thread handle for the thread
	 */
	HANDLE Thread;

	/**
	 * The runnable object to execute on this thread
	 */
	FRunnable* Runnable;

	/** 
	 * Sync event to make sure that Init() has been completed before allowing the main thread to continue
	 */
	FEvent* ThreadInitSyncEvent;

	/**
	 * Whether we should delete ourselves on thread exit
	 */
	UBOOL bShouldDeleteSelf;

	/**
	 * Whether we should delete the runnable on thread exit
	 */
	UBOOL bShouldDeleteRunnable;

	/**
	 * The priority to run the thread at
	 */
	EThreadPriority ThreadPriority;

	/**
	* ID set during thread creation
	*/
	DWORD ThreadID;

	/**
	 * The thread entry point. Simply forwards the call on to the right
	 * thread main function
	 */
	static DWORD STDCALL _ThreadProc(LPVOID pThis);

	/**
	 * The real thread entry point. It calls the Init/Run/Exit methods on
	 * the runnable object
	 */
	DWORD Run(void);

public:
	/**
	 * Zeroes members
	 */
	FRunnableThreadWin(void);

	/**
	 * Cleans up any resources
	 */
	~FRunnableThreadWin(void);

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
	UBOOL Create(FRunnable* InRunnable, const TCHAR* ThreadName,
		UBOOL bAutoDeleteSelf = 0,UBOOL bAutoDeleteRunnable = 0,DWORD InStackSize = 0,
		EThreadPriority InThreadPri = TPri_Normal);
	
	/**
	 * Changes the thread priority of the currently running thread
	 *
	 * @param NewPriority The thread priority to change to
	 */
	virtual void SetThreadPriority(EThreadPriority NewPriority);

	/**
	 * Tells the OS the preferred CPU to run the thread on. NOTE: Don't use
	 * this function unless you are absolutely sure of what you are doing
	 * as it can cause the application to run poorly by preventing the
	 * scheduler from doing its job well.
	 *
	 * @param ProcessorNum The preferred processor for executing the thread on
	 */
	virtual void SetProcessorAffinity(DWORD ProcessorNum);

	/**
	 * Tells the thread to either pause execution or resume depending on the
	 * passed in value.
	 *
	 * @param bShouldPause Whether to pause the thread (true) or resume (false)
	 */
	virtual void Suspend(UBOOL bShouldPause = TRUE);

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
	virtual UBOOL Kill(UBOOL bShouldWait = FALSE);

	/**
	 * Halts the caller until this thread is has completed its work.
	 */
	virtual void WaitForCompletion(void);

	/**
	* Thread ID for this thread 
	*
	* @return ID that was set by CreateThread
	*/
	virtual DWORD GetThreadID(void);
};

/**
 * This is the factory interface for creating threads on Windows
 */
class FThreadFactoryWin : public FThreadFactory
{
public:
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
	virtual FRunnableThread* CreateThread(FRunnable* InRunnable, const TCHAR* ThreadName,
		UBOOL bAutoDeleteSelf = 0,UBOOL bAutoDeleteRunnable = 0,
		DWORD InStackSize = 0,EThreadPriority InThreadPri = TPri_Normal);

	/**
	 * Cleans up the specified thread object using the correct heap
	 *
	 * @param InThread The thread object to destroy
	 */
	virtual void Destroy(FRunnableThread* InThread);
};

#endif
