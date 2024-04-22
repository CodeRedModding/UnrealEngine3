/**
 * MacThreading.h -- Contains all Mac platform-specific definitions
 * of interfaces and concrete classes for multithreading support in the Unreal
 * engine.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#ifndef _MAC_THREADING_H
#define _MAC_THREADING_H

#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <libkern/OSAtomic.h>


////////////////////
// @todo Mac: If we merge the unix code in to main branch, we could pretty easily share this
// this code with Unix and iPhone platforms
///////////////////



/**
 * Interlocked style functions for threadsafe atomic operations
 */

/**
 * Atomically increments the value pointed to and returns that to the caller
 */
FORCEINLINE INT appInterlockedIncrement(volatile INT* Value)
{
	return (INT) OSAtomicIncrement32Barrier((int32_t *) Value);
}
/**
 * Atomically decrements the value pointed to and returns that to the caller
 */
FORCEINLINE INT appInterlockedDecrement(volatile INT* Value)
{
	return (INT) OSAtomicDecrement32Barrier((int32_t *) Value);
}
/**
 * Atomically adds the amount to the value pointed to and returns the old
 * value to the caller
 */
FORCEINLINE INT appInterlockedAdd(volatile INT* Value,INT Amount)
{
	return (INT) OSAtomicAdd32Barrier((int32_t) Amount, (int32_t *) Value) - Amount;
}
/**
 * Atomically swaps two values returning the original value to the caller
 */
FORCEINLINE INT appInterlockedExchange(volatile INT* Value,INT Exchange)
{
	INT RetVal;
	do
	{
		RetVal = *Value;
	} while (!OSAtomicCompareAndSwap32Barrier(RetVal, Exchange, (int32_t *) Value));
	return RetVal;
}
/**
 * Atomically compares the value to comperand and replaces with the exchange
 * value if they are equal and returns the original value
 */
FORCEINLINE INT appInterlockedCompareExchange(volatile INT* Dest,INT Exchange,INT Comperand)
{
	INT RetVal;
	do
	{
		if (OSAtomicCompareAndSwap32Barrier(Comperand, Exchange, (int32_t *) Dest))
		{
			return Comperand;
		}
		RetVal = *Dest;
	} while (RetVal == Comperand);
	return RetVal;
}
/**
 * Atomically compares the pointer to comperand and replaces with the exchange
 * pointer if they are equal and returns the original value
 */
FORCEINLINE void* appInterlockedCompareExchangePointer(void** Dest,void* Exchange,void* Comperand)
{
	void *RetVal;
	do
	{
		if (OSAtomicCompareAndSwapPtrBarrier(Comperand, Exchange, Dest))
		{
			return Comperand;
		}
		RetVal = *Dest;
	} while (RetVal == Comperand);
	return RetVal;
}

/**
 * Returns a pseudo-handle to the currently executing thread.
 */
FORCEINLINE pthread_t appGetCurrentThread(void)
{
	return pthread_self();
}

/**
 * Returns the currently executing thread's id
 */
FORCEINLINE DWORD appGetCurrentThreadId(void)
{
	return (DWORD) pthread_mach_thread_np(pthread_self());
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
FORCEINLINE DWORD appSetThreadAffinity( pthread_t ThreadHandle, DWORD PreferredProcessor )
{
	// @TODO
	return (DWORD)-1;
}


/**
 * Allocates a thread local store slot
 */
FORCEINLINE DWORD appAllocTlsSlot(void)
{
	// allocate a per-thread mem slot
    pthread_key_t SlotKey = 0;
    if (pthread_key_create(&SlotKey, NULL) != 0)
	{
        SlotKey = 0xFFFFFFFF;  // matches the Windows TlsAlloc() retval.
	}
	return SlotKey;
}

/**
 * Sets a value in the specified TLS slot
 *
 * @param SlotIndex the TLS index to store it in
 * @param Value the value to store in the slot
 */
FORCEINLINE void appSetTlsValue(DWORD SlotIndex, void* Value)
{
    pthread_setspecific((pthread_key_t)SlotIndex, Value);
}

/**
 * Reads the value stored at the specified TLS slot
 *
 * @return the value stored in the slot
 */
FORCEINLINE void* appGetTlsValue(DWORD SlotIndex)
{
	return pthread_getspecific((pthread_key_t)SlotIndex);
}

/**
 * Frees a previously allocated TLS slot
 *
 * @param SlotIndex the TLS index to store it in
 */
FORCEINLINE void appFreeTlsSlot(DWORD SlotIndex)
{
    pthread_key_delete((pthread_key_t)SlotIndex);
}


/**
 * This is the Mac version of a critical section.
 */
class FCriticalSection :
	public FSynchronize
{
	/**
	 * The pthread-specific critical section
	 */
	pthread_mutex_t Mutex;

public:
	/**
	 * Constructor that initializes the aggregated critical section
	 */
	FORCEINLINE FCriticalSection(void)
	{
		// make a recursive mutex
		pthread_mutexattr_t MutexAttributes;
		pthread_mutexattr_init(&MutexAttributes);
		pthread_mutexattr_settype(&MutexAttributes, PTHREAD_MUTEX_RECURSIVE);
		pthread_mutex_init(&Mutex, &MutexAttributes);
	}

	/**
	 * Destructor cleaning up the critical section
	 */
	FORCEINLINE ~FCriticalSection(void)
	{
		pthread_mutex_destroy(&Mutex);
	}

	/**
	 * Locks the critical section
	 */
	FORCEINLINE void Lock(void)
	{
        pthread_mutex_lock(&Mutex);
	}

	/**
	 * Releases the lock on the critical seciton
	 */
	FORCEINLINE void Unlock(void)
	{
		pthread_mutex_unlock(&Mutex);
	}
};

/**
 * This is the Mac version of an event
 */
class FEventMac : public FEvent
{
	// This is a little complicated, in an attempt to match Win32 Event semantics...
    typedef enum
    {
        TRIGGERED_NONE,
        TRIGGERED_ONE,
        TRIGGERED_ALL,
        TRIGGERED_PULSE,
    } TriggerType;

    inline void LockEventMutex();
    inline void UnlockEventMutex();
	UBOOL bInitialized;
	UBOOL bIsManualReset;
	volatile TriggerType Triggered;
	volatile INT WaitingThreads;
    pthread_mutex_t Mutex;
    pthread_cond_t Condition;

public:
	/**
	 * Constructor that zeroes the handle
	 */
	FEventMac(void);

	/**
	 * Cleans up the event handle if valid
	 */
	virtual ~FEventMac(void);

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
 * This is the Mac factory for creating various synchronization objects.
 */
class FSynchronizeFactoryMac : public FSynchronizeFactory
{
public:
	/**
	 * Zeroes its members
	 */
	FSynchronizeFactoryMac(void);

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
 * This is the Mac class used for all poolable threads
 */
class FQueuedThreadMac : public FQueuedThread
{
	/**
	 * The event that tells the thread there is work to do
	 */
	FEvent* DoWorkEvent;

	/**
	 * The thread handle to clean up. Must be closed or this will leak resources
	 */
	pthread_t ThreadHandle;

	/**
	 * If true, thread was created.
	 */
	UBOOL ThreadCreated;

	/**
	 * If true, the thread should exit
	 */
	volatile UBOOL TimeToDie;

	/**
	 * If true, the thread is ready to be joined.
	 */
	volatile UBOOL ThreadHasTerminated;

	/**
	 * The work this thread is doing
	 */
	FQueuedWork* QueuedWork;

	/**
	 * The synchronization object for the work member
	 */
	FCriticalSection* QueuedWorkSynch;

	/**
	 * The pool this thread belongs to
	 */
	FQueuedThreadPool* OwningThreadPool;

	/**
	 * Helper to manage stat updates
	 */
	STAT(FCheckForStatsUpdate StatsUpdate);

	/**
	 * The real thread entry point. It waits for work events to be queued. Once
	 * an event is queued, it executes it and goes back to waiting.
	 */
	void Run(void);

	/**
	 * Bridge between Pthread entry point and Unreal's.
	 */
	static void *_ThreadProc(void *pThis);

public:
	/**
	 * Zeros any members
	 */
	FQueuedThreadMac(void);

	/**
	 * Deletes any allocated resources. Kills the thread if it is running.
	 */
	virtual ~FQueuedThreadMac(void);

	/**
	 * Creates the thread with the specified stack size and creates the various
	 * events to be able to communicate with it.
	 *
	 * @param InPool The thread pool interface used to place this thread
	 *		  back into the pool of available threads when its work is done
	 * @param ProcessorMask The processor set to run the thread on
	 * @param InStackSize The size of the stack to create. 0 means use the
	 *		  current thread's stack size
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
	 * @return True if the thread exited gracefull, false otherwise
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
 * platform specific code, while this class provides all of the Mac
 * specific methods. It handles the creation of the threads used in the
 * thread pool.
 */
class FQueuedThreadPoolMac : public FQueuedThreadPoolBase
{
public:
	/**
	 * Cleans up any threads that were allocated in the pool
	 */
	virtual ~FQueuedThreadPoolMac(void);

	/**
	 * Creates the thread pool with the specified number of threads
	 *
	 * @param InNumQueuedThreads Specifies the number of threads to use in the pool
	 * @param ProcessorMask Specifies which processors should be used by the pool
	 * @param StackSize The size of stack the threads in the pool need (32K default)
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
class FRunnableThreadMac : public FRunnableThread
{
	/**
	 * The thread handle to clean up. Must be closed or this will leak resources
	 */
	pthread_t ThreadHandle;

	/**
	 * If true, thread was created.
	 */
	UBOOL ThreadCreated;

	/**
	 * The runnable object to execute on this thread
	 */
	FRunnable* Runnable;

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
	 * If true, the thread is ready to be joined.
	 */
	volatile UBOOL ThreadHasTerminated;

	/**
	 * If true, the thread has finished Init(). Parent thread can assume thread is ready to work.
	 */
	volatile UBOOL bInitCalled;

	/**
	 * The real thread entry point. It calls the Init/Run/Exit methods on
	 * the runnable object
	 */
	DWORD Run(void);

	/**
	 * Bridge between Pthread entry point and Unreal's.
	 */
	static void *_ThreadProc(void *pThis);

public:
	/**
	 * Zeroes members
	 */
	FRunnableThreadMac(void);

	/**
	 * Cleans up any resources
	 */
	~FRunnableThreadMac(void);

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
	 * @return True if the thread exited gracefull, false otherwise
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
 * This is the factory interface for creating threads on Mac
 */
class FThreadFactoryMac : public FThreadFactory
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

#endif  // define _MAC_THREADING_H

