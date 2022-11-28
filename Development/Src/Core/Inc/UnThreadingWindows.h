/**
 * UnThreadingWindows.h -- Contains all Windows platform specific definitions
 * of interfaces and concrete classes for multithreading support in the Unreal
 * engine.
 *
 * Copyright 2004 Epic Games, Inc. All Rights Reserved.
 *
 * Revision history:
 *		Created by Joe Graf.
 */

#ifndef _UNTHREADING_WINDOWS_H
#define _UNTHREADING_WINDOWS_H

// Notify people of the windows dependency
#if !defined(_WINDOWS_) && !defined(_XTL_)
#pragma message("UnThreadingWindows.h relies on Windows.h/Xtl.h being included ahead of it")
#endif

/**
 * This is the Windows version of a critical section. It uses an aggregate
 * CRITICAL_SECTION to implement its locking.
 */
class FCriticalSectionWin : public FCriticalSection
{
	/**
	 * The windows specific critical section
	 */
	CRITICAL_SECTION CriticalSection;

public:
	/**
	 * Constructor that initializes the aggregated critical section
	 */
	FCriticalSectionWin(void);

	/**
	 * Destructor cleaning up the critical section
	 */
	virtual ~FCriticalSectionWin(void);

	/**
	 * Locks the critical section
	 */
	virtual void Lock(void);

	/**
	 * Releases the lock on the critical seciton
	 */
	virtual void Unlock(void);
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
 * This is a Windows mutex. It should only be used as a sharable critical
 * section
 */
class FMutexWin : public FMutex
{
public:
	/**
	 * The windows handle to the mutex
	 */
	HANDLE Mutex;

public:
	/**
	 * Constructor that zeroes the handle
	 */
	FMutexWin(void);

	/**
	 * Cleans up the mutex handle if valid
	 */
	virtual ~FMutexWin(void);

	/**
	 * Locks the mutex
	 */
	virtual void Lock(void);

	/**
	 * Releases the lock on the mutex
	 */
	virtual void Unlock(void);

	/**
	 * Creates the mutex. If a name is supplied, that name is used to find
	 * a shared mutex instance.
	 *
	 * @param InName Whether to use a commonly shared mutex or not. If so this
	 * is the name of the mutex to share.
	 *
	 * @return Returns TRUE if the mutex was created, FALSE otherwise
	 */
	virtual UBOOL Create(const TCHAR* InName = NULL);
};

/**
 * This is a Windows semaphore object. It is meant for counted locking
 */
class FSemaphoreWin : public FSemaphore
{
	/**
	 * The windows handle to the semaphore
	 */
	HANDLE Semaphore;

public:
	/**
	 * Constructor that zeroes the handle
	 */
	FSemaphoreWin(void);

	/**
	 * Cleans up the semaphore handle if valid
	 */
	virtual ~FSemaphoreWin(void);

	/**
	 * Acquires a lock on the semaphore
	 */
	virtual void Lock(void);

	/**
	 * Releases one acquired lock back to the semaphore
	 */
	virtual void Unlock(void);

	/**
	 * Creates the semaphore with the specified number of locks. Named
	 * semaphores share instances the same way events and mutexes do.
	 *
	 * @param InNumLocks The number of locks this semaphore will support
	 * @param InName Whether to use a commonly shared semaphore or not. If so
	 * this is the name of the semaphore to share.
	 *
	 * @return Returns TRUE if the semaphore was created, FALSE otherwise
	 */
	virtual UBOOL Create(DWORD InNumLocks,const TCHAR* InName = NULL);
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
	 * Creates a new semaphore
	 *
	 * @param InNumLocks The number of locks this semaphore will support
	 * @param InName Whether to use a commonly shared semaphore or not. If so
	 * this is the name of the semaphore to share.
	 *
	 * @return The new semaphore object or NULL if creation fails
	 */
	virtual FSemaphore* CreateSemaphore(DWORD InNumLocks,const TCHAR* InName = NULL);

	/**
	 * Creates a new mutex
	 *
	 * @param InName Whether to use a commonly shared mutex or not. If so this
	 * is the name of the mutex to share.
	 *
	 * @return The new mutex object or NULL if creation fails
	 */
	virtual FMutex* CreateMutex(const TCHAR* InName = NULL);

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
	UBOOL TimeToDie;

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
	 * The thread entry point. Simply forwards the call on to the right
	 * thread main function
	 */
	static DWORD STDCALL _ThreadProc(LPVOID pThis);

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
	 * back into the pool of available threads when its work is done
	 * @param InStackSize The size of the stack to create. 0 means use the
	 * current thread's stack size
	 *
	 * @return True if the thread and all of its initialization was successful, false otherwise
	 */
	virtual UBOOL Create(FQueuedThreadPool* InPool,DWORD InStackSize = 0);
	
	/**
	 * Tells the thread to exit. If the caller needs to know when the thread
	 * has exited, it should use the bShouldWait value and tell it how long
	 * to wait before deciding that it is deadlocked and needs to be destroyed.
	 * NOTE: having a thread forcibly destroyed can cause leaks in TLS, etc.
	 *
	 * @param bShouldWait If true, the call will wait for the thread to exit
	 * @param MaxWaitTime The amount of time to wait before killing it. It
	 * defaults to inifinite.
	 * @param bShouldDeleteSelf Whether to delete ourselves upon completion
	 *
	 * @return True if the thread exited gracefull, false otherwise
	 */
	virtual UBOOL Kill(UBOOL bShouldWait = FALSE,DWORD MaxWaitTime = INFINITE,UBOOL bShouldDeleteSelf = FALSE);

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
	 * @param StackSize The size of stack the threads in the pool need (32K default)
	 *
	 * @return Whether the pool creation was successful or not
	 */
	virtual UBOOL Create(DWORD InNumQueuedThreads,DWORD StackSize = (32 * 1024));
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
	 * @param bAutoDeleteSelf Whether to delete this object on exit
	 * @param bAutoDeleteRunnable Whether to delete the runnable object on exit
	 * @param InStackSize The size of the stack to create. 0 means use the
	 * current thread's stack size
	 * @param InThreadPri Tells the thread whether it needs to adjust its
	 * priority or not. Defaults to normal priority
	 *
	 * @return True if the thread and all of its initialization was successful, false otherwise
	 */
	UBOOL Create(FRunnable* InRunnable,UBOOL bAutoDeleteSelf = 0,
		UBOOL bAutoDeleteRunnable = 0,DWORD InStackSize = 0,
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
	virtual void Suspend(UBOOL bShouldPause = 1);

	/**
	 * Tells the thread to exit. If the caller needs to know when the thread
	 * has exited, it should use the bShouldWait value and tell it how long
	 * to wait before deciding that it is deadlocked and needs to be destroyed.
	 * NOTE: having a thread forcibly destroyed can cause leaks in TLS, etc.
	 *
	 * @param bShouldWait If true, the call will wait for the thread to exit
	 * @param MaxWaitTime The amount of time to wait before killing it.
	 * Defaults to inifinite.
	 *
	 * @return True if the thread exited gracefull, false otherwise
	 */
	virtual UBOOL Kill(UBOOL bShouldWait = 0,DWORD MaxWaitTime = 0);

	/**
	 * Halts the caller until this thread is has completed its work.
	 */
	virtual void WaitForCompletion(void);
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
	 * @param bAutoDeleteSelf Whether to delete this object on exit
	 * @param bAutoDeleteRunnable Whether to delete the runnable object on exit
	 * @param InStackSize The size of the stack to create. 0 means use the
	 * current thread's stack size
	 * @param InThreadPri Tells the thread whether it needs to adjust its
	 * priority or not. Defaults to normal priority
	 *
	 * @return The newly created thread or NULL if it failed
	 */
	virtual FRunnableThread* CreateThread(FRunnable* InRunnable,
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
