/*=============================================================================
	RenderingThread.h: Rendering thread definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/** The rendering command queue. */
extern FRingBuffer GRenderCommandBuffer;

/**
 * Whether the renderer is currently running in a separate thread.
 * If this is false, then all rendering commands will be executed immediately instead of being enqueued in the rendering command buffer.
 */
extern UBOOL GIsThreadedRendering;

/**
 * Whether the rendering thread should be created or not.
 * Currently set by command line parameter and by the ToggleRenderingThread console command.
 */
extern UBOOL GUseThreadedRendering;

/** Whether the rendering thread is suspended (not even processing the tickables) */
extern volatile INT GIsRenderingThreadSuspended;

/** A memory stack usable by the rendering thread. */
extern FMemStack GRenderingThreadMemStack;

/** Starts the rendering thread. */
extern void StartRenderingThread();

/** Stops the rendering thread. */
extern void StopRenderingThread();

/**
 * Tick all rendering thread tickable objects
 */
extern void TickRenderingTickables();

/**
 * Checks if the rendering thread is healthy and running.
 * If it has crashed, appErrorf is called with the exception information.
 */
extern void CheckRenderingThreadHealth();

/** @return True if called from the rendering thread. */
extern UBOOL IsInRenderingThread();

/** @return True if called from the game thread. */
extern UBOOL IsInGameThread();

/** Advances stats for the rendering thread. */
void RenderingThreadAdvanceFrame();

/**
 * Encapsulates stopping and starting the renderthread so that other threads can manipulate graphics resources.
 */
class FSuspendRenderingThread
{
public:
	enum FSuspendThreadFlags
	{
		// Suspends the thread
		ST_SuspendThread,
		// Completely destroys and recreates the thread
		ST_RecreateThread,
		// Suspends the thread and calls RHIReleaseThreadOwnership() and RHIAcquireThreadOwnership()
		ST_ReleaseThreadOwnership,
	};

	/**
	 *	Constructor that flushes and suspends the renderthread
	 *	@param SuspendThreadFlags	Specifies the way the thread should be suspended
	 */
	FSuspendRenderingThread( FSuspendThreadFlags SuspendThreadFlags );

	/** Destructor that starts the renderthread again */
	~FSuspendRenderingThread();

private:
	/** Whether we should use a rendering thread or not */
	UBOOL bUseRenderingThread;

	/** Whether the rendering thread was currently running or not */
	UBOOL bWasRenderingThreadRunning;

	/** Controls the way the thread should be suspended */
	FSuspendThreadFlags SuspendThreadFlags;
};

/** Helper macro for safely flushing and suspending the rendering thread while manipulating graphics resources */
#define SCOPED_SUSPEND_RENDERING_THREAD(bRecreateThread)	FSuspendRenderingThread SuspendRenderingThread(bRecreateThread)


/** The parent class of commands stored in the rendering command queue. */
class FRenderCommand
{
public:

	void* operator new(size_t Size,const FRingBuffer::AllocationContext& Allocation)
	{
		return Allocation.GetAllocation();
	}

	virtual ~FRenderCommand() {}

	virtual UINT Execute() = 0;
	virtual const TCHAR* DescribeCommand() = 0;
};

/** A rendering command that simply consumes space in the rendering command queue. */
class FSkipRenderCommand : public FRenderCommand
{
public:

	/** Initialization constructor. */
	FSkipRenderCommand(UINT InNumSkipBytes)
	:	NumSkipBytes(InNumSkipBytes)
	{}

	// FRenderCommand interface.
	virtual UINT Execute()
	{
		return NumSkipBytes;
	}
	virtual const TCHAR* DescribeCommand()
	{
		return TEXT("FSkipRenderCommand");
	}

private:
	UINT NumSkipBytes;
};

//
// Macros for using render commands.
//

/**
 * Enqueues a rendering command with the given type and parameters.
 */
#define ENQUEUE_RENDER_COMMAND(TypeName,Params) \
	{ \
		check(IsInGameThread()); \
		if(GIsThreadedRendering) \
		{ \
			FRingBuffer::AllocationContext AllocationContext(GRenderCommandBuffer,sizeof(TypeName)); \
			if(AllocationContext.GetAllocatedSize() < sizeof(TypeName)) \
			{ \
				check(AllocationContext.GetAllocatedSize() >= sizeof(FSkipRenderCommand)); \
				new(AllocationContext) FSkipRenderCommand(AllocationContext.GetAllocatedSize()); \
				AllocationContext.Commit(); \
				new(FRingBuffer::AllocationContext(GRenderCommandBuffer,sizeof(TypeName))) TypeName Params; \
			} \
			else \
			{ \
				new(AllocationContext) TypeName Params; \
			} \
		} \
		else \
		{ \
			TypeName TypeName##Command Params; \
			TypeName##Command.Execute(); \
		} \
	}

/**
 * Declares a rendering command type with 0 parameters.
 */
#define ENQUEUE_UNIQUE_RENDER_COMMAND(TypeName,Code) \
	class TypeName : public FRenderCommand \
	{ \
	public: \
		virtual UINT Execute() \
		{ \
			Code; \
			return sizeof(*this); \
		} \
		virtual const TCHAR* DescribeCommand() \
		{ \
			return TEXT( #TypeName ); \
		} \
	}; \
	ENQUEUE_RENDER_COMMAND(TypeName,);

/**
 * Declares a rendering command type with 1 parameters.
 */
#define ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(TypeName,ParamType1,ParamName1,ParamValue1,Code) \
	class TypeName : public FRenderCommand \
	{ \
	public: \
		typedef ParamType1 _ParamType1; \
		TypeName(const _ParamType1& In##ParamName1): \
		  ParamName1(In##ParamName1) \
		{} \
		virtual UINT Execute() \
		{ \
			Code; \
			return sizeof(*this); \
		} \
		virtual const TCHAR* DescribeCommand() \
		{ \
			return TEXT( #TypeName ); \
		} \
	private: \
		ParamType1 ParamName1; \
	}; \
	ENQUEUE_RENDER_COMMAND(TypeName,(ParamValue1));

/**
 * Declares a rendering command type with 2 parameters.
 */
#define ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(TypeName,ParamType1,ParamName1,ParamValue1,ParamType2,ParamName2,ParamValue2,Code) \
	class TypeName : public FRenderCommand \
	{ \
	public: \
		typedef ParamType1 _ParamType1; \
		typedef ParamType2 _ParamType2; \
		TypeName(const _ParamType1& In##ParamName1,const _ParamType2& In##ParamName2): \
		  ParamName1(In##ParamName1), \
		  ParamName2(In##ParamName2) \
		{} \
		virtual UINT Execute() \
		{ \
			Code; \
			return sizeof(*this); \
		} \
		virtual const TCHAR* DescribeCommand() \
		{ \
			return TEXT( #TypeName ); \
		} \
	private: \
		ParamType1 ParamName1; \
		ParamType2 ParamName2; \
	}; \
	ENQUEUE_RENDER_COMMAND(TypeName,(ParamValue1,ParamValue2));


/**
 * Declares a rendering command type with 3 parameters.
 */
#define ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(TypeName,ParamType1,ParamName1,ParamValue1,ParamType2,ParamName2,ParamValue2,ParamType3,ParamName3,ParamValue3,Code) \
	class TypeName : public FRenderCommand \
	{ \
	public: \
		typedef ParamType1 _ParamType1; \
		typedef ParamType2 _ParamType2; \
		typedef ParamType3 _ParamType3; \
		TypeName(const _ParamType1& In##ParamName1,const _ParamType2& In##ParamName2,const _ParamType3& In##ParamName3): \
		  ParamName1(In##ParamName1), \
		  ParamName2(In##ParamName2), \
		  ParamName3(In##ParamName3) \
		{} \
		virtual UINT Execute() \
		{ \
			Code; \
			return sizeof(*this); \
		} \
		virtual const TCHAR* DescribeCommand() \
		{ \
			return TEXT( #TypeName ); \
		} \
	private: \
		ParamType1 ParamName1; \
		ParamType2 ParamName2; \
		ParamType3 ParamName3; \
	}; \
	ENQUEUE_RENDER_COMMAND(TypeName,(ParamValue1,ParamValue2,ParamValue3));

 
 /**
 * Used to track pending rendering commands from the game thread.
 */
class FRenderCommandFence
{
public:

	/**
	 * Minimal initialization constructor.
	 */
	FRenderCommandFence():
		NumPendingFences(0)
	{}

	/**
	 * Adds a fence command to the rendering command queue.
	 * The pending fence count is incremented to reflect the pending fence command.
	 * Once the rendering thread has executed the fence command, it decrements the pending fence count.
	 */
	void BeginFence();

	/**
	 * Waits for pending fence commands to retire.
	 * @param NumFencesLeft	- Maximum number of fence commands to leave in queue
	 */
	void Wait( UINT NumFencesLeft=0 ) const;

	// Accessors.
	UINT GetNumPendingFences() const;

private:
	volatile UINT NumPendingFences;
};

/**
 * Waits for all deferred deleted objects to be cleaned up. Should  only be used from the game thread.
 */
extern void FlushDeferredDeletion();

/**
 * Waits for the rendering thread to finish executing all pending rendering commands.  Should only be used from the game thread.
 */
extern void FlushRenderingCommands();

/**
 * The base class of objects that need to defer deletion until the render command queue has been flushed.
 */
class FDeferredCleanupInterface
{
public:
	virtual void FinishCleanup() = 0;
	virtual ~FDeferredCleanupInterface() {}
};

/**
 * A set of cleanup objects which are pending deletion.
 */
class FPendingCleanupObjects : public TArray<FDeferredCleanupInterface*>
{
public:
	~FPendingCleanupObjects();
};

/**
 * Adds the specified deferred cleanup object to the current set of pending cleanup objects.
 */
extern void BeginCleanup(FDeferredCleanupInterface* CleanupObject);

/**
 * Transfers ownership of the current set of pending cleanup objects to the caller.  A new set is created for subsequent BeginCleanup calls.
 * @return A pointer to the set of pending cleanup objects.  The called is responsible for deletion.
 */
extern FPendingCleanupObjects* GetPendingCleanupObjects();

