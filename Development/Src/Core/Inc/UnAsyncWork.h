/*=============================================================================
	UnAsyncWork.h: Definition of queued work classes
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _UNASYNCWORK_H
#define _UNASYNCWORK_H


/**
	FAutoDeleteAsyncTask - template task for jobs that delete themselves when complete

	Sample code:

	class ExampleAutoDeleteAsyncTask : public FNonAbandonableTask
	{
		friend class FAutoDeleteAsyncTask<ExampleAutoDeleteAsyncTask>;

		INT ExampleData;

		ExampleAutoDeleteAsyncTask(INT InExampleData)
		 : ExampleData(InExampleData)
		{
		}

		void DoWork()
		{
			... do the work here
		}

		static const TCHAR *Name()
		{
			return TEXT("ExampleAutoDeleteAsyncTask");
		}
	};

	start an example job at low priority

	(new FAutoDeleteAsyncTask<ExampleAutoDeleteAsyncTask>(5)->StartBackgroundTask();

	start an example job at high priority

	(new FAutoDeleteAsyncTask<ExampleAutoDeleteAsyncTask>(5)->StartHiPriorityTask();

	do an example job now, on this thread

	(new FAutoDeleteAsyncTask<ExampleAutoDeleteAsyncTask>(5)->StartSynchronousTask();

**/
template<typename TTask>
class FAutoDeleteAsyncTask : private FQueuedWork
{
	/** User job embedded in this task */ 
	TTask Task;

	/* Generic start function, not called directly
		* @param bForceSynchronous if true, this job will be started synchronously, now, on this thread
		* @param ThreadPriority controls the choice of the thread pool. TPri_BelowNormal uses GThreadPool, otherwise GHiPriThreadPool is used
		* @param bRunLowIfNoNormalPool if true and if the priority is not TPri_BelowNormal and if GHiPriThreadPool is not available on this platform or not enabled, then use the low priority,
			if false and the other conditions are met, then it is executed synchronously
	**/
	void Start(
		UBOOL bForceSynchronous,
		EThreadPriority ThreadPriority, 
		UBOOL bRunLowIfNoNormalPool)
	{
		appMemoryBarrier();
		FQueuedThreadPool* QueuedPool = ThreadPriority == TPri_BelowNormal ? GThreadPool : GHiPriThreadPool;
		if (QueuedPool == GHiPriThreadPool && GHiPriThreadPoolForceOff)
		{
			QueuedPool = 0;
		}
		if (!QueuedPool && GThreadPool && bRunLowIfNoNormalPool)
		{
			QueuedPool = GThreadPool;
		}
		if (bForceSynchronous)
		{
			QueuedPool = 0;
		}
		if (QueuedPool)
		{
			QueuedPool->AddQueuedWork(this);
		}
		else 
		{
			// we aren't doing async stuff
			DoWork();
		}
	}

	/* 
	* Tells the user job to do the work, sometimes called synchronously, sometimes from the thread pool. Calls the event tracker.
	**/
	void DoWork()
	{
		appBeginNamedEvent(FColor(0),Task.Name());
		Task.DoWork();
		appEndNamedEvent();
		delete this;
	}

	/* 
	* Always called from the thread pool. Just passes off to DoWork
	**/
	virtual void DoThreadedWork()
	{
		DoWork();
	}

	/**
	 * Always called from the thread pool. Called if the task is removed from queue before it has started which might happen at exit.
	 * If the user job can abandon, we do that, otherwise we force the work to be done now (doing nothing would not be safe).
	 */
	virtual void Abandon(void)
	{
		if (Task.CanAbandon())
		{
			Task.Abandon();
			delete this;
		}
		else
		{
			DoWork();
		}
	}

public:
	/** Default constructor. */
	FAutoDeleteAsyncTask( )
	{
	}
	/** Passthrough constructor. Generally speaking references will not pass through; use pointers */
	template<typename T>
	FAutoDeleteAsyncTask( T Arg )
		: Task(Arg)
	{
	}
	/** Passthrough constructor. Generally speaking references will not pass through; use pointers */
	template<typename T1,typename T2>
	FAutoDeleteAsyncTask( T1 Arg1, T2 Arg2 )
		: Task(Arg1,Arg2)
	{
	}
	/** Passthrough constructor. Generally speaking references will not pass through; use pointers */
	template<typename T1,typename T2, typename T3>
	FAutoDeleteAsyncTask( T1 Arg1, T2 Arg2, T3 Arg3 )
		: Task(Arg1,Arg2,Arg3)
	{
	}
	/** Passthrough constructor. Generally speaking references will not pass through; use pointers */
	template<typename T1,typename T2, typename T3, typename T4>
	FAutoDeleteAsyncTask( T1 Arg1, T2 Arg2, T3 Arg3, T4 Arg4 )
		: Task(Arg1,Arg2,Arg3,Arg4)
	{
	}
	/** Passthrough constructor. Generally speaking references will not pass through; use pointers */
	template<typename T1,typename T2, typename T3, typename T4, typename T5>
	FAutoDeleteAsyncTask( T1 Arg1, T2 Arg2, T3 Arg3, T4 Arg4, T5 Arg5 )
		: Task(Arg1,Arg2,Arg3,Arg4,Arg5)
	{
	}
	/** Passthrough constructor. Generally speaking references will not pass through; use pointers */
	template<typename T1,typename T2, typename T3, typename T4, typename T5, typename T6>
	FAutoDeleteAsyncTask( T1 Arg1, T2 Arg2, T3 Arg3, T4 Arg4, T5 Arg5, T6 Arg6 )
		: Task(Arg1,Arg2,Arg3,Arg4,Arg5,Arg6)
	{
	}
	/** Passthrough constructor. Generally speaking references will not pass through; use pointers */
	template<typename T1,typename T2, typename T3, typename T4, typename T5, typename T6, typename T7>
	FAutoDeleteAsyncTask( T1 Arg1, T2 Arg2, T3 Arg3, T4 Arg4, T5 Arg5, T6 Arg6, T7 Arg7 )
		: Task(Arg1,Arg2,Arg3,Arg4,Arg5,Arg6,Arg7)
	{
	}
	/** Passthrough constructor. Generally speaking references will not pass through; use pointers */
	template<typename T1,typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8>
	FAutoDeleteAsyncTask( T1 Arg1, T2 Arg2, T3 Arg3, T4 Arg4, T5 Arg5, T6 Arg6, T7 Arg7, T8 Arg8 )
		: Task(Arg1,Arg2,Arg3,Arg4,Arg5,Arg6,Arg7,Arg8)
	{
	}
	/** Passthrough constructor. Generally speaking references will not pass through; use pointers */
	template<typename T1,typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9>
	FAutoDeleteAsyncTask( T1 Arg1, T2 Arg2, T3 Arg3, T4 Arg4, T5 Arg5, T6 Arg6, T7 Arg7, T8 Arg8, T9 Arg9 )
		: Task(Arg1,Arg2,Arg3,Arg4,Arg5,Arg6,Arg7,Arg8,Arg9)
	{
	}


	/* 
	* Run this task on this thread, now. Will end up destroying myself, so it is not safe to use this object after this call.
	**/
	void StartSynchronousTask()
	{
		Start(TRUE,TPri_BelowNormal,FALSE);
	}

	/* 
	* Run this task on the high priority pool, if it exists. It is not safe to use this object after this call.
	* @param bRunLowIfNoNormalPool if TRUE and there is no hi priority pool on this platform, use the lo priority pool. If FALSE, then run synchronously if the hi priority pool does not exist
	**/
	void StartHiPriorityTask(UBOOL bRunLowIfNoNormalPool = FALSE)
	{
		Start(FALSE,TPri_Normal,bRunLowIfNoNormalPool);
	}

	/* 
	* Run this task on the lo priority thread pool. It is not safe to use this object after this call.
	**/
	void StartBackgroundTask()
	{
		Start(FALSE,TPri_BelowNormal,FALSE);
	}

};


/**
	FAsyncTask - template task for jobs queued to thread pools

	Sample code:

	class ExampleAsyncTask : public FNonAbandonableTask
	{
		friend class FAsyncTask<ExampleAsyncTask>;

		INT ExampleData;

		ExampleAsyncTask(INT InExampleData)
		 : ExampleData(InExampleData)
		{
		}

		void DoWork()
		{
			... do the work here
		}

		static const TCHAR *Name()
		{
			return TEXT("ExampleAsyncTask");
		}
	};

	start an example job at low priority

	FAutoDeleteAsyncTask<ExampleAutoDeleteAsyncTask> MyTask(5);
	
	MyTask.StartBackgroundTask();

	-- or -- 

	MyTask.StartHiPriorityTask();

	to start it hi priority, or

	MyTask.StartSynchronousTask();

	to just do it now on this thread

	Check if the task is done:

	if (MyTask.IsDone())
	{
	}

	Spinning on IsDone is not acceptable (see EnsureCompletion), but it is ok to check once a frame.

	Ensure the task is done, doing the task on the current thread if it has not been started, waiting until completion in all cases.

	Task.EnsureCompletion();


**/
template<typename TTask>
class FAsyncTask : private FQueuedWork
{
	/** User job embedded in this task */ 
	TTask Task;
	/** Thread safe counter that indicates WORK completion, no necessarily finalization of the job */
	FThreadSafeCounter	WorkNotFinishedCounter;
	/** If we aren't doing the work synchrnously, this will hold the completion event */
	FEvent*				DoneEvent;
	/** Pool we are queued into, maintained by the calling thread */
	FQueuedThreadPool*	QueuedPool;

	/* Internal function to destroy the completion event
	**/
	void DestroyEvent()
	{
		if (DoneEvent)
		{
			GSynchronizeFactory->Destroy(DoneEvent);
			DoneEvent = 0;
		}
	}

	/* Generic start function, not called directly
		* @param bForceSynchronous if true, this job will be started synchronously, now, on this thread
		* @param ThreadPriority controls the choice of the thread pool. TPri_BelowNormal uses GThreadPool, otherwise GHiPriThreadPool is used
		* @param bRunLowIfNoNormalPool if true and if the priority is not TPri_BelowNormal and if GHiPriThreadPool is not available on this platform or not enabled, then use the low priority,
			if false and the other conditions are met, then it is executed synchronously
		* @param bDoNowIfSynchronous if TRUE and if this will end up running synchronously, otherwise do it at EnsureCompletion. 
	**/
	void Start(
		UBOOL bForceSynchronous,
		EThreadPriority ThreadPriority, 
		UBOOL bRunLowIfNoNormalPool,
		UBOOL bDoNowIfSynchronous)
	{
		appMemoryBarrier();
		CheckIdle();  // can't start a job twice without it being completed first
		WorkNotFinishedCounter.Increment();
		QueuedPool = ThreadPriority == TPri_BelowNormal ? GThreadPool : GHiPriThreadPool;
		if (QueuedPool == GHiPriThreadPool && GHiPriThreadPoolForceOff)
		{
			QueuedPool = 0;
		}
		if (!QueuedPool && GThreadPool && bRunLowIfNoNormalPool)
		{
			QueuedPool = GThreadPool;
		}
		if (bForceSynchronous)
		{
			QueuedPool = 0;
		}
		if (QueuedPool)
		{
			if (!DoneEvent)
			{
				DoneEvent = GSynchronizeFactory->CreateSynchEvent(TRUE);
			}
			DoneEvent->Reset();
//if (QueuedPool == GHiPriThreadPool)
//{
//	debugf(TEXT("Started %s on hi pri"),Task.Name());
//}
			QueuedPool->AddQueuedWork(this);
		}
		else 
		{
			// we aren't doing async stuff
			DestroyEvent();
			if (bDoNowIfSynchronous)
			{
				DoWork();
			}
		}
	}

	/* 
	* Tells the user job to do the work, sometimes called synchronously, sometimes from the thread pool. Calls the event tracker.
	**/
	void DoWork()
	{
		appBeginNamedEvent(FColor(0),Task.Name());
		Task.DoWork();
		appEndNamedEvent();
		check(WorkNotFinishedCounter.GetValue() == 1);
		WorkNotFinishedCounter.Decrement();
	}

	/* 
	* Triggers the work completion event, only called from a pool thread
	**/
	void FinishThreadedWork()
	{
		check(QueuedPool);
		if (DoneEvent)
		{
			DoneEvent->Trigger();
		}
	}

	/* 
	* Performs the work, this is only called from a pool thread.
	**/
	virtual void DoThreadedWork()
	{
		DoWork();
		FinishThreadedWork();
	}

	/**
	 * Always called from the thread pool. Called if the task is removed from queue before it has started which might happen at exit.
	 * If the user job can abandon, we do that, otherwise we force the work to be done now (doing nothing would not be safe).
	 */
	virtual void Abandon(void)
	{
		if (Task.CanAbandon())
		{
			Task.Abandon();
			check(WorkNotFinishedCounter.GetValue() == 1);
			WorkNotFinishedCounter.Decrement();
		}
		else
		{
			DoWork();
		}
		FinishThreadedWork();
	}

	/* 
	* Internal call to assert that we are idle
	**/
	void CheckIdle() const
	{
		check(WorkNotFinishedCounter.GetValue() == 0);
		check(!QueuedPool);
	}

	/* 
	* Internal call to synchronize completion between threads, never called from a pool thread
	**/
	void SyncCompletion()
	{
		appMemoryBarrier();
		if (QueuedPool)
		{
			check(DoneEvent); // if it is not done yet, we must have an event
			DoneEvent->Wait();
			QueuedPool = 0;
		}
		CheckIdle();
	}

	/* 
	* Internal call to intialize internal variables
	**/
	void Init()
	{
		DoneEvent = 0;
		QueuedPool = 0;
	}

public:
	/** Default constructor. */
	FAsyncTask( )
	{
		Init();
	}
	/** Passthrough constructor. Generally speaking references will not pass through; use pointers */
	template<typename T>
	FAsyncTask( T Arg )
		: Task(Arg)
	{
		Init();
	}
	/** Passthrough constructor. Generally speaking references will not pass through; use pointers */
	template<typename T1,typename T2>
	FAsyncTask( T1 Arg1, T2 Arg2 )
		: Task(Arg1,Arg2)
	{
		Init();
	}
	/** Passthrough constructor. Generally speaking references will not pass through; use pointers */
	template<typename T1,typename T2, typename T3>
	FAsyncTask( T1 Arg1, T2 Arg2, T3 Arg3 )
		: Task(Arg1,Arg2,Arg3)
	{
		Init();
	}
	/** Passthrough constructor. Generally speaking references will not pass through; use pointers */
	template<typename T1,typename T2, typename T3, typename T4>
	FAsyncTask( T1 Arg1, T2 Arg2, T3 Arg3, T4 Arg4 )
		: Task(Arg1,Arg2,Arg3,Arg4)
	{
		Init();
	}
	/** Passthrough constructor. Generally speaking references will not pass through; use pointers */
	template<typename T1,typename T2, typename T3, typename T4, typename T5>
	FAsyncTask( T1 Arg1, T2 Arg2, T3 Arg3, T4 Arg4, T5 Arg5 )
		: Task(Arg1,Arg2,Arg3,Arg4,Arg5)
	{
		Init();
	}
	/** Passthrough constructor. Generally speaking references will not pass through; use pointers */
	template<typename T1,typename T2, typename T3, typename T4, typename T5, typename T6>
	FAsyncTask( T1 Arg1, T2 Arg2, T3 Arg3, T4 Arg4, T5 Arg5, T6 Arg6 )
		: Task(Arg1,Arg2,Arg3,Arg4,Arg5,Arg6)
	{
		Init();
	}
	/** Passthrough constructor. Generally speaking references will not pass through; use pointers */
	template<typename T1,typename T2, typename T3, typename T4, typename T5, typename T6, typename T7>
	FAsyncTask( T1 Arg1, T2 Arg2, T3 Arg3, T4 Arg4, T5 Arg5, T6 Arg6, T7 Arg7 )
		: Task(Arg1,Arg2,Arg3,Arg4,Arg5,Arg6,Arg7)
	{
		Init();
	}
	/** Passthrough constructor. Generally speaking references will not pass through; use pointers */
	template<typename T1,typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8>
	FAsyncTask( T1 Arg1, T2 Arg2, T3 Arg3, T4 Arg4, T5 Arg5, T6 Arg6, T7 Arg7, T8 Arg8 )
		: Task(Arg1,Arg2,Arg3,Arg4,Arg5,Arg6,Arg7,Arg8)
	{
		Init();
	}
	/** Passthrough constructor. Generally speaking references will not pass through; use pointers */
	template<typename T1,typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9>
	FAsyncTask( T1 Arg1, T2 Arg2, T3 Arg3, T4 Arg4, T5 Arg5, T6 Arg6, T7 Arg7, T8 Arg8, T9 Arg9 )
		: Task(Arg1,Arg2,Arg3,Arg4,Arg5,Arg6,Arg7,Arg8,Arg9)
	{
		Init();
	}

	/** Destructor, not legal when a task is in process */
	~FAsyncTask()
	{
		// destroying an unfinished task is a bug
		CheckIdle();
		DestroyEvent();
	}

	/* Retrieve embedded user job, not legal to call while a job is in process
		* @return reference to embedded user job 
	**/
	TTask &GetTask()
	{
		CheckIdle();  // can't modify a job without it being completed first
		return Task;
	}

	/* Retrieve embedded user job, not legal to call while a job is in process
		* @return reference to embedded user job 
	**/
	const TTask &GetTask() const
	{
		CheckIdle();  // could be safe, but I won't allow it anyway because the data could be changed while it is being read
		return Task;
	}

	/* 
	* Run this task on this thread
	* @param bDoNow if TRUE then do the job now instead of at EnsureCompletion
	**/
	void StartSynchronousTask(UBOOL bDoNow = TRUE)
	{
		Start(TRUE,TPri_BelowNormal,FALSE,bDoNow);
		check(WorkNotFinishedCounter.GetValue() == 0 || !bDoNow); // if we are doing it now, it should be done
	}

	/* 
	* Run this task on the high priority pool, if it exists.
	* @param bRunLowIfNoNormalPool if TRUE and there is no hi priority pool on this platform, use the lo priority pool. If FALSE, then run synchronously if the hi priority pool does not exist
	* @param bDoNowIfSynchronous if TRUE and if we need to run this job synchronously, then do it now instead of at EnsureCompletion
	**/
	void StartHiPriorityTask(UBOOL bRunLowIfNoNormalPool = FALSE,UBOOL bDoNowIfSynchronous = FALSE)
	{
		Start(FALSE,TPri_Normal,bRunLowIfNoNormalPool,bDoNowIfSynchronous);
	}

	/* 
	* Queue this task for processing by the background thread pool
	**/
	void StartBackgroundTask()
	{
		Start(FALSE,TPri_BelowNormal,FALSE,TRUE);
	}

	/* 
	* Wait until the job is complete
	* @param bDoWorkOnThisThreadIfNotStarted if TRUE and the work has not been started, retract the async task and do it now on this thread
	**/
	void EnsureCompletion(UBOOL bDoWorkOnThisThreadIfNotStarted = TRUE)
	{
		UBOOL DoSyncCompletion = TRUE;
		if (bDoWorkOnThisThreadIfNotStarted)
		{
			if (QueuedPool)
			{
				if (QueuedPool->RetractQueuedWork(this))
				{
					// we got the job back, so do the work now and no need to synchronize
					DoSyncCompletion = FALSE;
					DoWork(); 
					FinishThreadedWork();
					QueuedPool = 0;
				}
			}
			else if (WorkNotFinishedCounter.GetValue())  // in the synchronous case, if we haven't done it yet, do it now
			{
				DoWork(); 
			}
		}
		if (DoSyncCompletion)
		{
			SyncCompletion();
		}
		CheckIdle(); // Must have had bDoWorkOnThisThreadIfNotStarted == FALSE and needed it to be TRUE for a synchronous job
	}

	/** Returns TRUE if the work and TASK has completed, FALSE while it's still in progress. 
	 * prior to returning true, it synchronizes so the task can be destroyed or reused
	*/
	UBOOL IsDone()
	{
		if (!IsWorkDone())
		{
			return FALSE;
		}
		SyncCompletion();
		return TRUE;
	}

	/** Returns TRUE if the work has completed, FALSE while it's still in progress. 
	 * This does not block and if true, you can use the results.
	 * But you can't destroy or reuse the task without IsDone() being true or EnsureCompletion()
	*/
	UBOOL IsWorkDone() const
	{
		if (WorkNotFinishedCounter.GetValue())
		{
			return FALSE;
		}
		return TRUE;
	}

	/** Returns TRUE if the work has not been started or has been completed. NOT to be used for synchronization, but great for check()'s */
	UBOOL IsIdle() const
	{
		return WorkNotFinishedCounter.GetValue() == 0 && QueuedPool == 0;
	}

};

/**
 * Stub class to use a base class for tasks that cannot be abandoned
 */
class FNonAbandonableTask
{
public:
	UBOOL CanAbandon()
	{
		return FALSE;
	}
	void Abandon()
	{
	}
};

/**
 * Asynchronous decompression, used for decompressing chunks of memory in the background
 */
class FAsyncUncompress : public FNonAbandonableTask
{
	/** Buffer containing uncompressed data					*/
	void*	UncompressedBuffer;
	/** Size of uncompressed data in bytes					*/
	INT		UncompressedSize;
	/** Buffer compressed data is going to be written to	*/
	void*	CompressedBuffer;
	/** Size of CompressedBuffer data in bytes				*/
	INT		CompressedSize;
	/** Flags to control decompression						*/
	ECompressionFlags Flags;
	/** Whether the source memory is padded with a full cache line at the end */
	UBOOL	bIsSourceMemoryPadded;

public:
	/**
	 * Initializes the data and creates the event.
	 *
	 * @param	Flags				Flags to control what method to use for decompression
	 * @param	UncompressedBuffer	Buffer containing uncompressed data
	 * @param	UncompressedSize	Size of uncompressed data in bytes
	 * @param	CompressedBuffer	Buffer compressed data is going to be written to
	 * @param	CompressedSize		Size of CompressedBuffer data in bytes
	 * @param	bIsSourcePadded		Whether the source memory is padded with a full cache line at the end
	 */
	FAsyncUncompress( 
		ECompressionFlags InFlags, 
		void* InUncompressedBuffer, 
		INT InUncompressedSize, 
		void* InCompressedBuffer, 
		INT InCompressedSize, 
		UBOOL bIsSourcePadded = FALSE)
	:	UncompressedBuffer( InUncompressedBuffer )
		,	UncompressedSize( InUncompressedSize )
		,	CompressedBuffer( InCompressedBuffer )
		,	CompressedSize( InCompressedSize )
		,	Flags( InFlags )
		,	bIsSourceMemoryPadded( bIsSourcePadded )
	{

	}
	/**
	 * Performs the async decompression
	 */
	void DoWork()
	{
		// Uncompress from memory to memory.
		verify( appUncompressMemory( Flags, UncompressedBuffer, UncompressedSize, CompressedBuffer, CompressedSize, bIsSourceMemoryPadded ) );
	}
	/** Give the name for external event viewers
	* @return	the name to display in external event viewers
	*/
	static const TCHAR *Name()
	{
		return TEXT("FAsyncUncompress");
	}
};


#if _MSC_VER && !CONSOLE && !UE3_LEAN_AND_MEAN



/**
 * Asynchronous DXT compression, used for compressing textures in the background
 */
class FAsyncDXTCompress : public FNonAbandonableTask 
{
public:
	/**
	 * Initializes the data and creates the event.
	 *
	 * @param	SourceData		Source texture data to DXT compress, in BGRA 8bit per channel unsigned format.
	 * @param	InPixelFormat	Texture format
	 * @param	InSizeX			Number of texels along the X-axis
	 * @param	InSizeY			Number of texels along the Y-axis
	 * @param	SRGB			Whether the texture is in SRGB space
	 * @param	bIsNormalMap	Whether the texture is a normal map
	 * @param	bSupportDXT1a	Whether to use DXT1a or DXT1 format (if PixelFormat is DXT1)
	 * @param	QualityLevel	A value from nvtt::Quality that represents the quality of the lightmap encoding
	 */
	FAsyncDXTCompress(void* SourceData, enum EPixelFormat InPixelFormat, INT InSizeX, INT InSizeY, UBOOL SRGB, UBOOL bIsNormalMap, UBOOL bSupportDXT1a, INT QualityLevel);

	/** Destructor. Frees the memory used for the DXT-compressed resulting data.  */
	~FAsyncDXTCompress();
	
	/**
	 * Compresses the texture
	 */
	void DoWork();


	/**
	 * Returns the texture format.
	 * @return	Texture format
	 */
	enum EPixelFormat	GetPixelFormat() const;

	/**
	 * Returns the size of the image.
	 * @return	Number of texels along the X-axis
	 */
	INT					GetSizeX() const;

	/**
	 * Returns the size of the image.
	 * @return	Number of texels along the Y-axis
	 */
	INT					GetSizeY() const;

	/**
	 * Returns the compressed data, once the work is done. This buffer will be deleted when the work is deleted.
	 * @return	Start address of the compressed data
	 */
	const void*			GetResultData() const;

	/**
	 * Returns the size of compressed data in number of bytes, once the work is done.
	 * @return	Size of compressed data, in number of bytes
	 */
	INT					GetResultSize() const;

	/** Give the name for external event viewers
	* @return	the name to display in external event viewers
	*/
	static const TCHAR *Name()
	{
		return TEXT("FAsyncDXTCompress");
	}

private:
	/** Compression result */
	struct FNVCompression* Compression;

	/** Texture format */
	enum EPixelFormat	PixelFormat;

	/** Number of texels along the X-axis */
	INT					SizeX;

	/** Number of texels along the Y-axis */
	INT					SizeY;
};

/**
 * Asynchronous PVRTC compression, used for compressing textures in the background
 */
class FAsyncPVRTCCompressWork : public FNonAbandonableTask
{
public:
	/**
	 * Initializes the data and creates the event.
	 *
	 * @param	SourceData		Source texture data to PVRTC compress, in BGRA 8bit per channel unsigned format.
	 * @param	InSizeX			Number of texels along the X-axis
	 * @param	InSizeY			Number of texels along the Y-axis
	 * @param	InTexture		The texture to compress
	 * @param	UseFastPVRTC	If true, a fast,low quality compression will be used.
	 */
	FAsyncPVRTCCompressWork(void* SourceData, INT InSizeX, INT InSizeY, class UTexture2D* InTexture, UBOOL InUseFastPVRTC);

	/**
	 * Compresses the texture
	 */
	void DoWork();

	/** Give the name for external event viewers
	* @return	the name to display in external event viewers
	*/
	static const TCHAR *Name()
	{
		return TEXT("FAsyncPVRTCCompressWork");
	}

	/**
	 * Returns the size of the image.
	 * @return	Number of texels along the X-axis
	 */
	INT					GetSizeX() const
	{
		return SizeX;
	}

	/**
	 * Returns the size of the image.
	 * @return	Number of texels along the Y-axis
	 */
	INT					GetSizeY() const
	{
		return SizeY;
	}

protected:
	/** Raw data in BGRA8 */
	TArray<FColor> RawData;

	/** Lightmap texture we are compressing */
	class UTexture2D* Texture;

	/** Number of texels along the X-axis */
	INT					SizeX;

	/** Number of texels along the Y-axis */
	INT					SizeY;

	/** True if the texture should be compressed using fast compression */
	UBOOL				bUseFastPVRTC;
};


/** A small class to help with multithreaded PVR compression */
class FAsyncPVRTCCompressor
{
public:
	/** 
	 * Adds a texture for compression.  
	 * Textures do not necessarily compress in the order they were added.
	 * NOTE that you should check if your texture actually needs to be compressed before using this function.
	 * Calling this function will clear out cached pvrtc mip data for the passed in texture.
	 *
	 * @param TextureToCompress		The texture needing compression
	 * @param RawData				Optional RawData to compress if the texture does not contain it.
	 */
	void AddTexture( UTexture2D* TextureToCompress, UBOOL bUseFastPVRTC = FALSE, void* RawData = NULL );

	/**
	 * Compresses all queued textures.  This function blocks until all texture have been compressed
	 */
	void CompressTextures();
private:
	/** Waits for all outstanding tasks to finish */
	void FinishTasks();

	/** Array of compression work that is pending completion */
	TArray<FAsyncTask<FAsyncPVRTCCompressWork> > PendingWork;
};

#endif

/**
 * Asynchronous vorbis decompression
 */
class FAsyncVorbisDecompressWorker : public FNonAbandonableTask
{
protected:
	class USoundNodeWave*		Wave;

public:
	/**
	 * Async decompression of vorbis data
	 *
	 * @param	InWave		Wave data to decompress
	 */
	FAsyncVorbisDecompressWorker( USoundNodeWave* InWave)
		: Wave(InWave)
	{
	}

	/**
	 * Performs the async vorbis decompression
	 */
	void DoWork();

	/** Give the name for external event viewers
	* @return	the name to display in external event viewers
	*/
	static const TCHAR *Name()
	{
		return TEXT("FAsyncVorbisDecompress");
	}
};

typedef FAsyncTask<FAsyncVorbisDecompressWorker> FAsyncVorbisDecompress;


/**
 * Asynchronous SHA verification
 */
class FAsyncSHAVerify
{
protected:
	/** Buffer to run the has on. This class can take ownership of the buffer is bShouldDeleteBuffer is TRUE */
	void* Buffer;

	/** Size of Buffer */
	INT BufferSize;

	/** Hash to compare against */
	BYTE Hash[20];

	/** Filename to lookup hash value (can be empty if hash was passed to constructor) */
	FString Pathname;

	/** If this is TRUE, and looking up the hash by filename fails, this will abort execution */
	UBOOL bIsUnfoundHashAnError;

	/** Should this class delete the buffer memory when verification is complete? */
	UBOOL bShouldDeleteBuffer;

public:

	/**
	 * Constructor. 
	 * 
	 * @param	InBuffer				Buffer of data to calculate has on. MUST be valid until this task completes (use Counter or pass ownership via bInShouldDeleteBuffer)
	 * @param	InBufferSize			Size of InBuffer
	 * @param	bInShouldDeleteBuffer	TRUE if this task should appFree InBuffer on completion of the verification. Useful for a fire & forget verification
	 *									NOTE: If you pass ownership to the task MAKE SURE you are done using the buffer as it could go away at ANY TIME
	 * @param	Pathname				Pathname to use to have the platform lookup the hash value
	 * @param	bInIsUnfoundHashAnError TRUE if failing to lookup the hash value results in a fail (only for Shipping PC)
	 */
	FAsyncSHAVerify(
		void* InBuffer, 
		INT InBufferSize, 
		UBOOL bInShouldDeleteBuffer, 
		const TCHAR* InPathname, 
		UBOOL bInIsUnfoundHashAnError)
		:	Buffer(InBuffer)
		,	BufferSize(InBufferSize)
		,	Pathname(InPathname)
		,	bIsUnfoundHashAnError(bInIsUnfoundHashAnError)
		,	bShouldDeleteBuffer(bInShouldDeleteBuffer)
	{
	}

	/**
	 * Performs the async hash verification
	 */
	void DoWork();

	/**
	 * Task API, return true to indicate that we can abandon
	 */
	UBOOL CanAbandon()
	{
		return TRUE;
	}

	/**
	 * Abandon task, deletes the buffer if that is what was requested
	 */
	void Abandon()
	{
		if( bShouldDeleteBuffer )
		{
			appFree( Buffer );
			Buffer = 0;
		}
	}
	/** Give the name for external event viewers
	* @return	the name to display in external event viewers
	*/
	static const TCHAR *Name()
	{
		return TEXT("FAsyncSHAVerify");
	}
};

#endif
