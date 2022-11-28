/**
 * UnThreadingBase.h -- Contains all base level interfaces for threading
 * support in the Unreal engine.
 *
 * Copyright 2004 Epic Games, Inc. All Rights Reserved.
 *
 * Revision history:
 *		Created by Joe Graf.
 */

#ifndef _UNTHREADING_H
#define _UNTHREADING_H

/** Thread safe counter */
class FThreadSafeCounter
{
public:
	/** Default constructor, initializing Value to 0 */
	FThreadSafeCounter();
	/**
	 * Constructor, initializing counter to passed in value.
	 *
	 * @param InValue	Value to initialize counter to
	 */
	FThreadSafeCounter( LONG InValue );

	/**
	 * Increment and return new value.	
	 *
	 * @return the incremented value
	 */
	LONG Increment();

	/**
	 * Decrement and return new value.
	 *
	 * @return the decremented value
	 */
	LONG Decrement();

	/**
	 * Return the current value.
	 *
	 * @return the current value
	 */
	LONG GetValue();

private:
	// Hidden on purpose as usage wouldn't be thread safe.
	FThreadSafeCounter( const FThreadSafeCounter& Other ){}
	void operator=(const FThreadSafeCounter& Other){}

	/** Value of counter */
	LONG Value;
};


/**
 * This is the base interface all synchronization objects must inherit from.
 * It is an abstract base class that allows synchronizations objects to passed
 * by interface without having to know the implementation details or the type
 * of object involved. NOTE: This requires the ability for a thread to lock
 * a resource multiple times without deadlock; it also requires that unlocking
 * of a resource matches the number of locks or a deadlock with occur.
 */
class FSynchronize : public FSystemAllocatorNewDelete
{
public:
	/**
	 * Virtual destructor so that child implementations are guaranteed a chance
	 * to clean up any resources they allocated.
	 */
	virtual ~FSynchronize(void) {}

	/**
	 * Tells the synchronization object to acquire a lock.
	 */
	virtual void Lock(void) = 0;

	/**
	 * Tells the synchronization object to release the lock.
	 */
	virtual void Unlock(void) = 0;
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
	FSynchronize* SynchObject;

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
	FScopeLock(FSynchronize* InSynchObject) :
		SynchObject(InSynchObject)
	{
		check(SynchObject);
		SynchObject->Lock();
	}

	/**
	 * Destructor that performs a release on the synchronization object
	 */
	~FScopeLock(void)
	{
		check(SynchObject);
		SynchObject->Unlock();
	}
};

/**
 * This is the lightest weight syncronization object and is very useful for
 * multiple CPUs. It will only enter the kernel after attempting to spin lock
 * first. This should be used for most state syncronization. This interface is
 * just a named version of the FSynchronize interface and has no specialization.
 */
class FCriticalSection : public FSynchronize
{
public:
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
 * This is a mutual excelusive lock. It is a heavier weight verison of
 * synchronization than a critical section. It does require kernel mode
 * so should not be used for high access object synchronization. It can
 * be named which is useful for sharing synchronizaiton objects by name.
 */
class FMutex : public FSynchronize
{
public:
	/**
	 * Creates the mutex. If a name is supplied, that name is used to find
	 * a shared mutex instance.
	 *
	 * @param InName Whether to use a commonly shared mutex or not. If so this
	 * is the name of the mutex to share.
	 *
	 * @return Returns TRUE if the mutex was created, FALSE otherwise
	 */
	virtual UBOOL Create(const TCHAR* InName = NULL) = 0;
};

/**
 * This is a multiple lock synchronization object. It is useful for items that
 * can be locked multiple times before needing to wait. Resources that are
 * finite and cannot fail to create are great objects to synchronize access to
 * using this interface.
 */
class FSemaphore : public FSynchronize
{
public:
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
	virtual UBOOL Create(DWORD InNumLocks,const TCHAR* InName = NULL) = 0;
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
	 * Creates a new semaphore
	 *
	 * @param InNumLocks The number of locks this semaphore will support
	 * @param InName Whether to use a commonly shared semaphore or not. If so
	 * this is the name of the semaphore to share.
	 *
	 * @return The new semaphore object or NULL if creation fails
	 */
	virtual FSemaphore* CreateSemaphore(DWORD InNumLocks,const TCHAR* InName = NULL) = 0;

	/**
	 * Creates a new mutex
	 *
	 * @param InName Whether to use a commonly shared mutex or not. If so this
	 * is the name of the mutex to share.
	 *
	 * @return The new mutex object or NULL if creation fails
	 */
	virtual FMutex* CreateMutex(const TCHAR* InName = NULL) = 0;

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
};

/**
 * The list of enumerated thread priorties we support
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
	virtual void Suspend(UBOOL bShouldPause = 1) = 0;

	/**
	 * Tells the thread to exit. If the caller needs to know when the thread
	 * has exited, it should use the bShouldWait value and tell it how long
	 * to wait before deciding that it is deadlocked and needs to be destroyed.
	 * The implementation is responsible for calling Stop() on the runnable.
	 * NOTE: having a thread forcibly destroyed can cause leaks in TLS, etc.
	 *
	 * @param bShouldWait If true, the call will wait for the thread to exit
	 * @param MaxWaitTime The amount of time to wait before killing it.
	 * Defaults to inifinite.
	 *
	 * @return True if the thread exited gracefull, false otherwise
	 */
	virtual UBOOL Kill(UBOOL bShouldWait = 0,DWORD MaxWaitTime = 0) = 0;

	/**
	 * Halts the caller until this thread is has completed its work.
	 */
	virtual void WaitForCompletion(void) = 0;
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
	virtual void DoWork(void) = 0;

	/**
	 * Tells the queued work that it is being abandoned so that it can do
	 * per object clean up as needed. This will only be called if it is being
	 * abandoned before completion. NOTE: This requires the object to delete
	 * itself using whatever heap it was allocated in.
	 */
	virtual void Abandon(void) = 0;

	/**
	 * This method is also used to tell the object to cleanup but not before
	 * the object has finished it's work.
	 */ 
	virtual void Dispose(void) = 0;
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
	 * @param InStackSize The size of the stack to create. 0 means use the
	 * current thread's stack size
	 *
	 * @return True if the thread and all of its initialization was successful, false otherwise
	 */
	virtual UBOOL Create(class FQueuedThreadPool* InPool,DWORD InStackSize = 0) = 0;
	
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
	virtual UBOOL Kill(UBOOL bShouldWait = FALSE,DWORD MaxWaitTime = INFINITE,UBOOL bShouldDeleteSelf = FALSE) = 0;

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
	 * @param StackSize The size of stack the threads in the pool need (32K default)
	 */
	virtual UBOOL Create(DWORD InNumQueuedThreads,DWORD StackSize = (32 * 1024)) = 0;

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
	 * Places a thread back into the available pool
	 *
	 * @param InQueuedThread The thread that is ready to be pooled
	 */
	virtual void ReturnToPool(FQueuedThread* InQueuedThread) = 0;
};

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
	FSynchronize* SynchWorkQueue;

	/**
	 * The synchronization object used to protect access to the thread pool
	 */
	FSynchronize* SynchThreadQueue;

	/**
	 * Constructor that creates the zeroes the critical sections
	 */
	FQueuedThreadPoolBase(void)
	{
		SynchWorkQueue = SynchThreadQueue = NULL;
	}

public:
	/**
	 * Tells the pool to clean up all background threads
	 */
	virtual void Destroy(void)
	{
		FScopeLock LockThreads(SynchThreadQueue);
		FScopeLock LockWork(SynchWorkQueue);
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
			QueuedThreads(Index)->Kill(TRUE,INFINITE,TRUE);
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
	void AddQueuedWork(FQueuedWork* InQueuedWork)
	{
		check(InQueuedWork != NULL);
		FQueuedThread* Thread = NULL;
		// Check to see if a thread is available. Make sure no other threads
		// can manipulate the thread pool while we do this. NOTE: This is done
		// in a two step approach to keep each array locked as little time as
		// possible
		{
			check(SynchThreadQueue && "Did you forget to call Create()?");
			FScopeLock sl(SynchThreadQueue);
			if (QueuedThreads.Num() > 0)
			{
				// Figure out which thread is available
				INT Index = QueuedThreads.Num();
				// Grab that thread to use
				Thread = QueuedThreads(Index);
				// Remove it from the list so no one else grabs it
				QueuedThreads.Remove(Index);
			}
		}
		// Was there a thread ready?
		if (Thread != NULL)
		{
			// We have a thread, so tell it to do the work
			Thread->DoWork(InQueuedWork);
		}
		else
		{
			check(SynchWorkQueue && "Did you forget to call Create()?");
			// There were no threads available, queue the work to be done
			// as soon as one does become available
			FScopeLock sl(SynchWorkQueue);
			QueuedWork.AddItem(InQueuedWork);
		}
	}

	/**
	 * Places a thread back into the available pool if there is no pending work
	 * to be done. If there is, the thread is put right back to work without it
	 * reaching the queue.
	 *
	 * @param InQueuedThread The thread that is ready to be pooled
	 */
	void ReturnToPool(FQueuedThread* InQueuedThread)
	{
		check(InQueuedThread != NULL);
		FQueuedWork* Work = NULL;
		// Check to see if there is any work to be done. NOTE: This is done
		// in a two step approach to keep each array locked as little time as
		// possible
		{
			FScopeLock sl(SynchWorkQueue);
			if (QueuedWork.Num() > 0)
			{
				// Grab the oldest work in the queue. This is slower than
				// getting the most recent but prevents work from being
				// queued and never done
				Work = QueuedWork(0);
				// Remove it from the list so no one else grabs it
				QueuedWork.Remove(0);
			}
		}
		// Was there any work to be done?
		if (Work != NULL)
		{
			// Rather than returning this thread to the pool, tell it to do
			// the work instead
			InQueuedThread->DoWork(Work);
		}
		else
		{
			// There was no work to be done, so add the thread to the pool
			FScopeLock sl(SynchThreadQueue);
			QueuedThreads.AddItem(InQueuedThread);
		}
	}
};
#endif