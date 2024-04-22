/**
 * UnThreadingBase.h -- Contains all base level interfaces for threading
 * support in the Unreal engine.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#ifndef _UNTHREADING_H
#define _UNTHREADING_H

#ifndef INFINITE
#define INFINITE ((DWORD)-1)
#endif

// Forward declaration for a platform specific implementation
class FCriticalSection;
/** Simple base class for polymorphic cleanup */
struct FSynchronize
{
	/** Simple destructor */
	virtual ~FSynchronize(void)
	{
	}
};

/**
 * This class is the abstract representation of a waitable event. It is used
 * to wait for another thread to signal that it is ready for the waiting thread
 * to do some work. Very useful for telling groups of threads to exit.
 */
class FEvent : public FSynchronize
{
public:
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
	virtual UBOOL Create(UBOOL bIsManualReset = FALSE,const TCHAR* InName = NULL) = 0;

	/**
	 * Triggers the event so any waiting threads are activated
	 */
	virtual void Trigger(void) = 0;

	/**
	 * Resets the event to an untriggered (waitable) state
	 */
	virtual void Reset(void) = 0;

	/**
	 * Triggers the event and resets the triggered state (like auto reset)
	 */
	virtual void Pulse(void) = 0;

	/**
	 * Waits for the event to be triggered
	 *
	 * @param WaitTime Time in milliseconds to wait before abandoning the event
	 * (DWORD)-1 is treated as wait infinite
	 *
	 * @return TRUE if the event was signaled, FALSE if the wait timed out
	 */
	virtual UBOOL Wait(DWORD WaitTime = INFINITE) = 0;
};

/**
 * This is the factory interface for creating various synchronization objects.
 * It is overloaded on a per platform basis to hide how each platform creates
 * the various synchronization objects. NOTE: The malloc used by it must be 
 * thread safe
 */
class FSynchronizeFactory
{
public:
	/**
	 * Creates a new critical section
	 *
	 * @return The new critical section object or NULL otherwise
	 */
	virtual FCriticalSection* CreateCriticalSection(void) = 0;

	/**
	 * Creates a new event
	 *
	 * @param bIsManualReset Whether the event requires manual reseting or not
	 * @param InName Whether to use a commonly shared event or not. If so this
	 * is the name of the event to share.
	 *
	 * @return Returns the new event object if successful, NULL otherwise
	 */
	virtual FEvent* CreateSynchEvent(UBOOL bIsManualReset = FALSE,const TCHAR* InName = NULL) = 0;

	/**
	 * Cleans up the specified synchronization object using the correct heap
	 *
	 * @param InSynchObj The synchronization object to destroy
	 */
	virtual void Destroy(FSynchronize* InSynchObj) = 0;
};

/*
 *  Global factory for creating synchronization objects
 */
extern FSynchronizeFactory* GSynchronizeFactory;

/**
 * This is the base interface for "runnable" object. A runnable object is an
 * object that is "run" on an arbitrary thread. The call usage pattern is
 * Init(), Run(), Exit(). The thread that is going to "run" this object always
 * uses those calling semantics. It does this on the thread that is created so
 * that any thread specific uses (TLS, etc.) are available in the contexts of
 * those calls. A "runnable" does all initialization in Init(). If
 * initialization fails, the thread stops execution and returns an error code.
 * If it succeeds, Run() is called where the real threaded work is done. Upon
 * completion, Exit() is called to allow correct clean up.
 */
class FRunnable
{
public:
	/**
	 * Allows per runnable object initialization. NOTE: This is called in the
	 * context of the thread object that aggregates this, not the thread that
	 * passes this runnable to a new thread.
	 *
	 * @return True if initialization was successful, false otherwise
	 */
	virtual UBOOL Init(void) = 0;

	/**
	 * This is where all per object thread work is done. This is only called
	 * if the initialization was successful.
	 *
	 * @return The exit code of the runnable object
	 */
	virtual DWORD Run(void) = 0;

	/**
	 * This is called if a thread is requested to terminate early
	 */
	virtual void Stop(void) = 0;

	/**
	 * Called in the context of the aggregating thread to perform any cleanup.
	 */
	virtual void Exit(void) = 0;

	/**
	 * Destructor
	 */
	virtual ~FRunnable(){}
};

/**
 * The list of enumerated thread priorities we support
 */
enum EThreadPriority
{
	TPri_Normal,
	TPri_AboveNormal,
	TPri_BelowNormal
};

/**
 * This is the base interface for all runnable thread classes. It specifies the
 * methods used in managing its life cycle.
 */
class FRunnableThread
{
public:
	/**
	 * Destructor
	 */
	virtual ~FRunnableThread(){}

	/**
	 * Changes the thread priority of the currently running thread
	 *
	 * @param NewPriority The thread priority to change to
	 */
	virtual void SetThreadPriority(EThreadPriority NewPriority) = 0;

	/**
	 * Tells the OS the preferred CPU to run the thread on. NOTE: Don't use
	 * this function unless you are absolutely sure of what you are doing
	 * as it can cause the application to run poorly by preventing the
	 * scheduler from doing its job well.
	 *
	 * @param ProcessorNum The preferred processor for executing the thread on
	 */
	virtual void SetProcessorAffinity(DWORD ProcessorNum) = 0;

	/**
	 * Tells the thread to either pause execution or resume depending on the
	 * passed in value.
	 *
	 * @param bShouldPause Whether to pause the thread (true) or resume (false)
	 */
	virtual void Suspend(UBOOL bShouldPause = TRUE) = 0;

	/**
	 * Tells the thread to exit. If the caller needs to know when the thread
	 * has exited, it should use the bShouldWait value and tell it how long
	 * to wait before deciding that it is deadlocked and needs to be destroyed.
	 * The implementation is responsible for calling Stop() on the runnable.
	 * NOTE: having a thread forcibly destroyed can cause leaks in TLS, etc.
	 *
	 * @param bShouldWait If true, the call will wait for the thread to exit
	 *
	 * @return True if the thread exited graceful, false otherwise
	 */
	virtual UBOOL Kill(UBOOL bShouldWait = FALSE) = 0;

	/**
	 * Halts the caller until this thread is has completed its work.
	 */
	virtual void WaitForCompletion(void) = 0;

	/**
	 * Thread ID for this thread 
	 *
	 * @return ID that was set by CreateThread
	 */
	virtual DWORD GetThreadID(void) = 0;
};

/**
 * This is the factory interface for creating threads. Each platform must
 * implement this with all the correct platform semantics
 */
class FThreadFactory
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
		DWORD InStackSize = 0,EThreadPriority InThreadPri = TPri_Normal) = 0;

	/**
	 * Cleans up the specified thread object using the correct heap
	 *
	 * @param InThread The thread object to destroy
	 */
	virtual void Destroy(FRunnableThread* InThread) = 0;
};

/*
 *  Global factory for creating threads
 */
extern FThreadFactory* GThreadFactory;

/**
 * Some standard IDs for special queued work types
 * These are just setup for future use, these aren't supported yet
 */
enum EWorkID
{
	WORK_None,
	WORK_DMARequest,
	WORK_Skin,
	WORK_ShadowExtrusion,
	WORK_Decompress,
	WORK_Stream,
	WORK_GenerateParticles,

	WORK_MAX
};
/**
 * This interface is a type of runnable object that requires no per thread
 * initialization. It is meant to be used with pools of threads in an
 * abstract way that prevents the pool from needing to know any details
 * about the object being run. This allows queueing of disparate tasks and
 * servicing those tasks with a generic thread pool.
 */
class FQueuedWork
{
public:
	/**
	 * Virtual destructor so that child implementations are guaranteed a chance
	 * to clean up any resources they allocated.
	 */
	virtual ~FQueuedWork(void) {}

	/**
	 * This is where the real thread work is done. All work that is done for
	 * this queued object should be done from within the call to this function.
	 */
	virtual void DoThreadedWork(void) = 0;

	/**
	 * Tells the queued work that it is being abandoned so that it can do
	 * per object clean up as needed. This will only be called if it is being
	 * abandoned before completion. NOTE: This requires the object to delete
	 * itself using whatever heap it was allocated in.
	 */
	virtual void Abandon(void) = 0;


#if PLATFORM_NEEDS_SPECIAL_QUEUED_WORK
	/**
	 * Return a special (possibly platform specific name for this type of work 
	 * in case the  platform needs to handle it specially. Most likely this 
	 * function does not need to be overridden.
	 * @return One of the EWorkID enums or another platform specific integer
	 */
	virtual DWORD GetSpecialID(void) { return WORK_None; }

	/**
	 * Return the size of the data that needs to be specially handled (copied 
	 * elsewhere, etc)
	 * NOTE: On the PS3, the size will need to be known before a Work object
	 * is created, so this function is just used to validate.
	 * @return Size of data payload that will need to be copied in GetData
	 */
	virtual DWORD GetDataSize(void) { return 0; }

	/**
	 * Fill out a buffer with the special data payload needed for this work
	 * @param Data A pointer to the data that will received the work's data
	 * @param True if successful
	 */
	virtual UBOOL GetData(void* Data) { appErrorf(TEXT("This function should be overridden if GetSpecialID doesn't return WORK_None")); return FALSE; }
#endif
};

/**
 * This is the interface used for all poolable threads. The usage pattern for
 * a poolable thread is different from a regular thread and this interface
 * reflects that. Queued threads spend most of their life cycle idle, waiting
 * for work to do. When signaled they perform a job and then return themselves
 * to their owning pool via a callback and go back to an idle state.
 */
class FQueuedThread
{
public:
	/**
	 * Virtual destructor so that child implementations are guaranteed a chance
	 * to clean up any resources they allocated.
	 */
	virtual ~FQueuedThread(void) {}

	/**
	 * Creates the thread with the specified stack size and creates the various
	 * events to be able to communicate with it.
	 *
	 * @param InPool The thread pool interface used to place this thread
	 * back into the pool of available threads when its work is done
	 * @param ProcessorMask The processor set to run the thread on
	 * @param InStackSize The size of the stack to create. 0 means use the
	 * current thread's stack size
	 * @param ThreadPriority priority of new thread
	 *
	 * @return True if the thread and all of its initialization was successful, false otherwise
	 */
	virtual UBOOL Create(class FQueuedThreadPool* InPool,DWORD ProcessorMask,
		DWORD InStackSize = 0,EThreadPriority ThreadPriority=TPri_Normal) = 0;
	
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
	virtual UBOOL Kill(UBOOL bShouldWait = FALSE, UBOOL bShouldDeleteSelf = FALSE) = 0;

	/**
	 * Tells the thread there is work to be done. Upon completion, the thread
	 * is responsible for adding itself back into the available pool.
	 *
	 * @param InQueuedWork The queued work to perform
	 */
	virtual void DoWork(FQueuedWork* InQueuedWork) = 0;

};

/**
 * This interface is used by all queued thread pools. It used as a callback by
 * FQueuedThreads and is used to queue asynchronous work for callers.
 */
class FQueuedThreadPool
{
public:
	/**
	 * Virtual destructor so that child implementations are guaranteed a chance
	 * to clean up any resources they allocated.
	 */
	virtual ~FQueuedThreadPool(void) {}

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
		DWORD StackSize = (32 * 1024),EThreadPriority ThreadPriority=TPri_Normal) = 0;

	/**
	 * Tells the pool to clean up all background threads
	 */
	virtual void Destroy(void) = 0;

	/**
	 * Checks to see if there is a thread available to perform the task. If not,
	 * it queues the work for later. Otherwise it is immediately dispatched.
	 *
	 * @param InQueuedWork The work that needs to be done asynchronously
	 */
	virtual void AddQueuedWork(FQueuedWork* InQueuedWork) = 0;

	/**
	 * Attempts to retract a previously queued task.
	 *
	 * @param InQueuedWork The work to try to retract
	 * @return TRUE if the work was retracted
	 */
	virtual UBOOL RetractQueuedWork(FQueuedWork* InQueuedWork) = 0;

	/**
	 * Places a thread back into the available pool
	 *
	 * @param InQueuedThread The thread that is ready to be pooled
	 * @return next job or null if there is no job available now
	 */
	virtual FQueuedWork* ReturnToPoolOrGetNextJob(FQueuedThread* InQueuedThread) = 0;

};

/*
 *  Global thread pool for shared async operations
 */
extern FQueuedThreadPool* GThreadPool;


/** The global hi priority thread pool */
extern FQueuedThreadPool*	GHiPriThreadPool;
/** The number of threads in the hi priority thread pool 
*	zero on platforms that don't have the CPU resources to gain any benefits
*/
extern INT					GHiPriThreadPoolNumThreads;
/** Debug setting for ToggleHiPriThreadPool */
extern UBOOL				GHiPriThreadPoolForceOff;
/** Determines if high precision threading is enabled */
extern UBOOL				GIsHighPrecisionThreadingEnabled;

/**
 * A base implementation of a queued thread pool. It provides the common
 * methods & members needed to implement a pool.
 */
class FQueuedThreadPoolBase : public FQueuedThreadPool
{
protected:
	/**
	 * The work queue to pull from
	 */
	TArray<FQueuedWork*> QueuedWork;
	
	/**
	 * The thread pool to dole work out to
	 */
	TArray<FQueuedThread*> QueuedThreads;

	/**
	 * The synchronization object used to protect access to the queued work
	 */
	FCriticalSection* SynchQueue;

	/**
	 * If true, indicates the destruction process has taken place
	 */
	UBOOL TimeToDie;

	/**
	 * Constructor that creates the zeroes the critical sections
	 */
	FQueuedThreadPoolBase(void)
		: SynchQueue(NULL),
		TimeToDie(FALSE)
	{
	}

public:
	/**
	 * Clean up the synch objects
	 */
	virtual ~FQueuedThreadPoolBase(void);

	/**
	 * Creates the synchronization object for locking the queues
	 *
	 * @return Whether the pool creation was successful or not
	 */
	UBOOL CreateSynchObjects(void);

	/**
	 * Tells the pool to clean up all background threads
	 */
	virtual void Destroy(void);

	/**
	 * Checks to see if there is a thread available to perform the task. If not,
	 * it queues the work for later. Otherwise it is immediately dispatched.
	 *
	 * @param InQueuedWork The work that needs to be done asynchronously
	 */
	void AddQueuedWork(FQueuedWork* InQueuedWork);

	/**
	 * Attempts to retract a previously queued task.
	 *
	 * @param InQueuedWork The work to try to retract
	 * @return TRUE if the work was retracted
	 */
	virtual UBOOL RetractQueuedWork(FQueuedWork* InQueuedWork);

	/**
	 * Places a thread back into the available pool if there is no pending work
	 * to be done. If there is, the thread is put right back to work without it
	 * reaching the queue.
	 *
	 * @param InQueuedThread The thread that is ready to be pooled
	 * @return next job to process or NULL if there are no jobs left to process
	 */
	FQueuedWork* ReturnToPoolOrGetNextJob(FQueuedThread* InQueuedThread);
};


#if STATS
/**
 * Helper class that implementations of FQueuedThread can use to manage stat updates
 * the basic idea is to call this before and after each DoWork call
 */
class FCheckForStatsUpdate
{
public:
	DWORD StatsFrameLastUpdate;
/**
 * Constructor for FCheckForStatsUpdate, saves the current value of the frame counter
 */
	FCheckForStatsUpdate();
	/**
	 * See if we are due for a stats advance call, if so do it
	 */
	void ConditionalUpdate();
};
#endif

// Include the platform specific versions
#if PS3
	#include "PS3CoreThreading.h"
#elif NGP
	#include "NGPCoreThreading.h"
#elif IPHONE
	#include "IPhoneThreading.h"
#elif PLATFORM_MACOSX
	#include "MacThreading.h"
#elif ANDROID
	#include "AndroidThreading.h"
#elif FLASH
 	#include "ThreadingFlash.h"
#elif WIIU
	#include "ThreadingWiiU.h"
#else
	#include "UnThreadingWindows.h"
#endif


/** Thread safe counter */
class FThreadSafeCounter
{
public:
	/** Default constructor, initializing Value to 0 */
	FThreadSafeCounter()
	{
		Counter = 0;
	}
	/**
	 * Copy Constructor, 
	 *
	 * @param Other the other thread safe counter to copy
	 * if other is changing from other threads, there are no guarantees as to which values you will get
	 * up to the caller to not care, synchronize or other way to make those guarantees.
	 */
	FThreadSafeCounter( const FThreadSafeCounter& Other )
	{
		Counter = Other.GetValue();
	}
	/**
	 * Constructor, initializing counter to passed in value.
	 *
	 * @param Value	Value to initialize counter to
	 */
	FThreadSafeCounter( INT Value )
	{
		Counter = Value;
	}

	/**
	 * Increment and return new value.	
	 *
	 * @return the new, incremented value
	 */
	INT Increment()
	{
		return appInterlockedIncrement(&Counter);
	}

	/**
	 * Adds an amount and returns the old value.	
	 *
	 * @param Amount Amount to increase the counter by
	 * @return the old value
	 */
	INT Add( INT Amount )
	{
		return appInterlockedAdd(&Counter, Amount);
	}

	/**
	 * Decrement and return new value.
	 *
	 * @return the new, decremented value
	 */
	INT Decrement()
	{
		return appInterlockedDecrement(&Counter);
	}

	/**
	 * Subtracts an amount and returns the old value.	
	 *
	 * @param Amount Amount to decrease the counter by
	 * @return the old value
	 */
	INT Subtract( INT Amount )
	{
		return appInterlockedAdd(&Counter, -Amount);
	}

	/**
	 * Sets the counter to a specific value and returns the old value.
	 *
	 * @param Value	Value to set the counter to
	 * @return The old value
	 */
	INT Set( INT Value )
	{
		return appInterlockedExchange(&Counter, Value);
	}

	/**
	 * Resets the counter's value to zero.
	 *
	 * @return the old value.
	 */
	INT Reset()
	{
		return appInterlockedExchange(&Counter, 0);
	}

	/**
	 * Return the current value.
	 *
	 * @return the current value
	 */
	INT GetValue() const
	{
		return Counter;
	}

private:
	// Hidden on purpose as usage wouldn't be thread safe.
	void operator=(const FThreadSafeCounter& Other){}

	/** Thread-safe counter */
	volatile INT Counter;
};


/**
 * This is a utility class that handles scope level locking. It's very useful
 * to keep from causing deadlocks due to exceptions being caught and knowing
 * about the number of locks a given thread has on a resource. Example:
 *
 * <code>
 *	{
 *		// Syncronize thread access to the following data
 *		FScopeLock ScopeLock(SynchObject);
 *		// Access data that is shared among multiple threads
 *		...
 *		// When ScopeLock goes out of scope, other threads can access data
 *	}
 * </code>
 */
class FScopeLock
{
	/**
	 * The synchronization object to aggregate and scope manage
	 */
	FCriticalSection* SynchObject;

	/**
	 * Default constructor hidden on purpose
	 */
	FScopeLock(void);

	/**
	 * Copy constructor hidden on purpose
	 *
	 * @param InScopeLock ignored
	 */
	FScopeLock(FScopeLock* InScopeLock);

	/**
	 * Assignment operator hidden on purpose
	 *
	 * @param InScopeLock ignored
	 */
	FScopeLock& operator=(FScopeLock& InScopeLock) { return *this; }

public:
	/**
	 * Constructor that performs a lock on the synchronization object
	 *
	 * @param InSynchObject The synchronization object to manage
	 */
	FScopeLock(FCriticalSection* InSynchObject) :
		SynchObject(InSynchObject)
	{
// @todo flash - make dummy synchobjects maybe instead of handling NULL synchobjects?
#if !FLASH
		check(SynchObject);
#endif
		SynchObject->Lock();
	}

	/**
	 * Destructor that performs a release on the synchronization object
	 */
	~FScopeLock(void)
	{
// @todo flash - make dummy synchobjects maybe instead of handling NULL synchobjects?
#if !FLASH
		check(SynchObject);
#endif
		SynchObject->Unlock();
	}
};

#define SCOPE_LOCK_REF(X) FScopeLock ScopeLock(&X);
#define SCOPE_LOCK_PTR(X) FScopeLock ScopeLock(X);


/** @return True if called from the rendering thread. */
extern UBOOL IsInRenderingThread();

/** @return True if called from the game thread. */
extern UBOOL IsInGameThread();

/** the stats system increments this and when pool threads notice it has been 
*   incremented, they call GStatManager.AdvanceFrameForThread() to advance any
*   stats that have been collected on a pool thread.
*/
STAT(extern FThreadSafeCounter GStatsFrameForPoolThreads);

#endif
