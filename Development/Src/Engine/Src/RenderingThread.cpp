/*=============================================================================
	RenderingThread.cpp: Rendering thread implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "ScenePrivate.h"

//
// Definitions
//

// This definition controls whether each uniquely named render command will have it's time individually
// tracked in the stats system.  If STATS is 0, this value is ignored.
#if !defined(ENABLE_DETAILED_RENDERCOMMAND_TIME_STATS)
#define ENABLE_DETAILED_RENDERCOMMAND_TIME_STATS 0
#endif

/** The size of the rendering command buffer, in bytes. */
#define RENDERING_COMMAND_BUFFER_SIZE	(256*1024)

/** comment in this line to display the average amount of data per second processed by the command buffer */
//#define RENDERING_COMMAND_BUFFER_STATS	1

//
// Globals
//

DECLARE_STATS_GROUP(TEXT("RenderThread"),STATGROUP_RenderThreadProcessing);

FRingBuffer GRenderCommandBuffer(RENDERING_COMMAND_BUFFER_SIZE, Max<UINT>(16,sizeof(FSkipRenderCommand)));
UBOOL GIsThreadedRendering = FALSE;
UBOOL GUseThreadedRendering = FALSE;
FMemStack GRenderingThreadMemStack(65536, FALSE, TRUE);

/** If the rendering thread has been terminated by an unhandled exception, this contains the error message. */
FString GRenderingThreadError;

/**
 * Polled by the game thread to detect crashes in the rendering thread.
 * If the rendering thread crashes, it sets this variable to FALSE.
 */
volatile UBOOL GIsRenderingThreadHealthy = TRUE;

UBOOL GGameThreadWantsToSuspendRendering = FALSE;

/**
 * If the rendering thread is in its idle loop (which ticks rendering tickables
 */
UBOOL GIsRenderingThreadIdling = FALSE;

/** Whether the rendering thread is suspended (not even processing the tickables) */
volatile INT GIsRenderingThreadSuspended = 0;

/**
 * Maximum rate the rendering thread will tick tickables when idle (in Hz)
 */
FLOAT GRenderingThreadMaxIdleTickFrequency = 40.f;

/** Whether we should generate crash reports even if the debugger is attached. */
extern UBOOL GAlwaysReportCrash;

/** Accumulates how many cycles the gamethread has been idle. */
DWORD GGameThreadIdle = 0;
/** How many cycles the gamethread used (excluding idle time). It's set once per frame in FViewport::Draw. */
DWORD GGameThreadTime = 0;
/** How many cycles it took to swap buffers to present the frame. */
DWORD GSwapBufferTime = 0;

/**
 *	Constructor that flushes and suspends the renderthread
 *	@param SuspendThreadFlags	Specifies the way the thread should be suspended
 */
FSuspendRenderingThread::FSuspendRenderingThread( FSuspendThreadFlags InSuspendThreadFlags )
{
	SuspendThreadFlags = InSuspendThreadFlags;
	bUseRenderingThread = GUseThreadedRendering;
	bWasRenderingThreadRunning = GIsThreadedRendering;
	if ( SuspendThreadFlags == ST_RecreateThread )
	{
		GUseThreadedRendering = FALSE;
		StopRenderingThread();
		appInterlockedIncrement( &GIsRenderingThreadSuspended );
	}
	else
	{
		if ( GIsRenderingThreadSuspended == 0 )
		{
			// First tell the render thread to finish up all pending commands and then suspend its activities.
			ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER( ScopedSuspendRendering, FSuspendThreadFlags, SuspendThreadFlags, SuspendThreadFlags, { RHISuspendRendering(); if (SuspendThreadFlags == ST_ReleaseThreadOwnership) { RHIReleaseThreadOwnership(); } appInterlockedIncrement( &GIsRenderingThreadSuspended ); } );

			// Block until the flag is set on the render-thread.
			while ( !GIsRenderingThreadSuspended )
			{
				appSleep( 0.0f );
			}

			// Then immediately queue up a command to resume rendering. This command won't be processed until GIsRenderingThreadSuspended == FALSE.
			// This command must be the very next thing the rendering thread executes.
			ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER( ScopedResumeRendering, FSuspendThreadFlags, SuspendThreadFlags, SuspendThreadFlags, {  if (SuspendThreadFlags == ST_ReleaseThreadOwnership) { RHIAcquireThreadOwnership(); } RHIResumeRendering(); } );
		}
		else
		{
			// The render-thread is already suspended. Just bump the ref-count.
			appInterlockedIncrement( &GIsRenderingThreadSuspended );
		}
	}
}

/** Destructor that starts the renderthread again */
FSuspendRenderingThread::~FSuspendRenderingThread()
{
	if ( SuspendThreadFlags == ST_RecreateThread )
	{
		GUseThreadedRendering = bUseRenderingThread;
		appInterlockedDecrement( &GIsRenderingThreadSuspended );
		if ( bUseRenderingThread && bWasRenderingThreadRunning )
		{
			StartRenderingThread();
		}
	}
	else
	{
		// Resume the render thread again. It should immediately process the ScopedResumeRendering command queued up from before.
		appInterlockedDecrement( &GIsRenderingThreadSuspended );
	}
}


/**
 * Tick all rendering thread tickable objects
 */

/** Static array of tickable objects that are ticked from rendering thread*/
TArrayNoInit<FTickableObjectRenderThread*> FTickableObjectRenderThread::RenderingThreadTickableObjects;

void TickRenderingTickables()
{
	static DOUBLE LastTickTime = appSeconds();

	// calc how long has passed since last tick
	DOUBLE CurTime = appSeconds();
	FLOAT DeltaSeconds = CurTime - LastTickTime;
	
	// don't let idle ticks happen too fast
	UBOOL bShouldDoTick = !GIsRenderingThreadIdling || DeltaSeconds > (1.f/GRenderingThreadMaxIdleTickFrequency);

	if (!bShouldDoTick || GIsRenderingThreadSuspended)
	{
		return;
	}

	UINT ObjectsThatResumedRendering = 0;

	// tick any rendering thread tickables
	for (INT ObjectIndex = 0; ObjectIndex < FTickableObjectRenderThread::RenderingThreadTickableObjects.Num(); ObjectIndex++)
	{
		FTickableObjectRenderThread* TickableObject = FTickableObjectRenderThread::RenderingThreadTickableObjects(ObjectIndex);
		// make sure it wants to be ticked and the rendering thread isn't suspended
		if (TickableObject->IsTickable())
		{
			if (GGameThreadWantsToSuspendRendering && TickableObject->NeedsRenderingResumedForRenderingThreadTick())
			{
				RHIResumeRendering();
				ObjectsThatResumedRendering++;
			}
			TickableObject->Tick(DeltaSeconds);
		}
	}
	// update the last time we ticked
	LastTickTime = CurTime;

	// if no ticked objects resumed rendering, make sure we're suspended if game thread wants us to be
	if (ObjectsThatResumedRendering == 0 && GGameThreadWantsToSuspendRendering)
	{
		RHISuspendRendering();
	}
}

/** Accumulates how many cycles the renderthread has been idle. It's defined in RenderingThread.cpp. */
DWORD GRenderThreadIdle = 0;
/** How many cycles the renderthread used (excluding idle time). It's set once per frame in FViewport::Draw. */
DWORD GRenderThreadTime = 0;

/** The rendering thread main loop */
void RenderingThreadMain()
{
#if RENDERING_COMMAND_BUFFER_STATS
	static UINT		RenderingCommandByteCount = 0;
	static DOUBLE	Then = 0.0;
#endif

	void* ReadPointer = NULL;
	UINT NumReadBytes = 0;

	while(GIsThreadedRendering)
	{
		// Command processing loop.
		while ( GRenderCommandBuffer.BeginRead(ReadPointer,NumReadBytes) )
		{
			// Process one render command.
			{
				SCOPE_CYCLE_COUNTER(STAT_RenderingBusyTime);

				FRenderCommand* Command = (FRenderCommand*)ReadPointer;

#if ENABLE_DETAILED_RENDERCOMMAND_TIME_STATS && STATS
				const DWORD RenderCommandStatID = GStatManager.FindStatIDForString(Command->DescribeCommand(), STATGROUP_RenderThreadProcessing);
				SCOPE_CYCLE_COUNTER(RenderCommandStatID);
#endif

				UINT CommandSize = Command->Execute();
				Command->~FRenderCommand();
				GRenderCommandBuffer.FinishRead(CommandSize);

#if RENDERING_COMMAND_BUFFER_STATS
				RenderingCommandByteCount += CommandSize;
#endif
			}

			// Suspending loop.
			{
				while ( GIsRenderingThreadSuspended )
				{
					if (GHandleDirtyDiscError)
					{
						appHandleIOFailure( NULL );
					}

					// Just sleep a little bit.
					appSleep( 0.001f );
				}
			}
		}

		// Idle loop:
		{
			SCOPE_CYCLE_COUNTER(STAT_RenderingIdleTime);
			GIsRenderingThreadIdling = TRUE;
			DWORD IdleStart = appCycles();
			while ( GIsThreadedRendering && !GRenderCommandBuffer.BeginRead(ReadPointer,NumReadBytes) )
			{
				if (GHandleDirtyDiscError)
				{
					appHandleIOFailure( NULL );
				}
				// Suspend rendering thread while we're trace dumping to avoid running out of memory due to holding
				// an IRQ for too long.
				while( GShouldSuspendRenderingThread )
				{
					appSleep( 1 );
				}

				// Wait for up to 16ms for rendering commands.
				GRenderCommandBuffer.WaitForRead(16);

				GRenderThreadIdle += appCycles() - IdleStart;

				// tick tickable objects when there are no commands to process, so if there are
				// no commands for a long time, we don't want to starve the tickables
				TickRenderingTickables();
				IdleStart = appCycles();

#if RENDERING_COMMAND_BUFFER_STATS
				DOUBLE Now = appSeconds();
				if( Now - Then > 2.0 )
				{
					debugf( TEXT( "RenderCommands: %7d bytes per second" ), UINT( RenderingCommandByteCount / ( Now - Then ) ) );
					Then = Now;
					RenderingCommandByteCount = 0;
				}
#endif
			}
			GIsRenderingThreadIdling = FALSE;
			GRenderThreadIdle += appCycles() - IdleStart;
		}
	};

	// Advance and reset the rendering thread stats before returning from the thread.
	// This is necessary to prevent the stats for the last frame of the thread from persisting.
	RenderingThreadAdvanceFrame();
}

/**
 * Advances stats for the rendering thread.
 */
void RenderingThreadAdvanceFrame()
{
	STAT(GStatManager.AdvanceFrameForThread());
}

/** The rendering thread runnable object. */
class FRenderingThread : public FRunnable
{
public:

	// FRunnable interface.
	virtual UBOOL Init(void) 
	{ 
		FLightPrimitiveInteraction::InitializeMemoryPool();

		// Acquire rendering context ownership on the current thread
		RHIAcquireThreadOwnership();

#if IPHONE
		// Put the render thread into real-time mode so that thread scheduling becomes much more precise.
		IPhoneThreadSetRealTimeMode( pthread_self(), 20, 60);
#endif
		return TRUE; 
	}

	virtual void Exit(void) 
	{
		// Release rendering context ownership on the current thread
		RHIReleaseThreadOwnership();
	}

	virtual void Stop(void)
	{
	}

	virtual DWORD Run(void)
	{
#if _MSC_VER && !CONSOLE
		extern INT CreateMiniDump( LPEXCEPTION_POINTERS ExceptionInfo );
		if ( !appIsDebuggerPresent() || GAlwaysReportCrash )
		{
			__try
			{
				RenderingThreadMain();
			}
			__except( CreateMiniDump( GetExceptionInformation() ) )
			{
				GRenderingThreadError = GErrorHist;

				// Use a memory barrier to ensure that the game thread sees the write to GRenderingThreadError before
				// the write to GIsRenderingThreadHealthy.
				appMemoryBarrier();

				GIsRenderingThreadHealthy = FALSE;
			}
		}
		else
#endif
		{
			RenderingThreadMain();
		}

		return 0;
	}
};

/** Thread used for rendering */
FRunnableThread* GRenderingThread = NULL;
FRunnable* GRenderingThreadRunnable = NULL;

void StartRenderingThread()
{
	check(!GIsThreadedRendering && GUseThreadedRendering);

	// Turn on the threaded rendering flag.
	GIsThreadedRendering = TRUE;

	// Create the rendering thread.
	GRenderingThreadRunnable = new FRenderingThread();

	EThreadPriority RenderingThreadPrio = TPri_Normal;
#if PS3
	// below normal, so streaming doesn't get blocked
	RenderingThreadPrio = TPri_BelowNormal;
#endif

	// Release rendering context ownership on the current thread
	RHIReleaseThreadOwnership();

	GRenderingThread = GThreadFactory->CreateThread(GRenderingThreadRunnable, TEXT("RenderingThread"), 0, 0, 0, RenderingThreadPrio);
#if _XBOX
	check(GRenderingThread);
	// Assign the rendering thread to the specified hwthread
	GRenderingThread->SetProcessorAffinity(RENDERING_HWTHREAD);
#endif
}

void StopRenderingThread()
{
	// This function is not thread-safe. Ensure it is only called by the main game thread.
	check( IsInGameThread() );

	if( GIsThreadedRendering )
	{
		// Get the list of objects which need to be cleaned up when the rendering thread is done with them.
		FPendingCleanupObjects* PendingCleanupObjects = GetPendingCleanupObjects();

		// Make sure we're not in the middle of streaming textures.
		(*GFlushStreamingFunc)();

		// Wait for the rendering thread to finish executing all enqueued commands.
		FlushRenderingCommands();

		// The rendering thread may have already been stopped during the call to GFlushStreamingFunc or FlushRenderingCommands.
		if ( GIsThreadedRendering )
		{
			check( GRenderingThread );

			// Turn off the threaded rendering flag.
			GIsThreadedRendering = FALSE;

			// Wait for the rendering thread to return.
			GRenderingThread->WaitForCompletion();

			// Destroy the rendering thread objects.
			GThreadFactory->Destroy(GRenderingThread);
			GRenderingThread = NULL;
			delete GRenderingThreadRunnable;
			GRenderingThreadRunnable = NULL;

			// Acquire rendering context ownership on the current thread
			RHIAcquireThreadOwnership();
		}

		// Delete the pending cleanup objects which were in use by the rendering thread.
		delete PendingCleanupObjects;
	}
}

void CheckRenderingThreadHealth()
{
	if(!GIsRenderingThreadHealthy)
	{
#if !CONSOLE
		GErrorHist[0] = 0;
#endif
		GIsCriticalError = FALSE;
		GError->Logf(TEXT("Rendering thread exception:\r\n%s"),*GRenderingThreadError);
	}
	
	GLog->FlushThreadedLogs();

	// Process pending windows messages, which is necessary to the rendering thread in some rare cases where DX10
	// sends window messages (from IDXGISwapChain::Present) to the main thread owned viewport window.
	// Only process sent messages to minimize the opportunity for re-entry in the editor, since wx messages are not deferred.
#if _WINDOWS
	if ( GIsEditor )
	{
		appWinPumpSentMessages();
	}
	else
	{
		appWinPumpMessages();
	}
#endif
}

UBOOL IsInRenderingThread()
{
	return !GRenderingThread || (appGetCurrentThreadId() == GRenderingThread->GetThreadID());
}

UBOOL IsInGameThread()
{
	// if the game thread is uninitialized, then we are calling this VERY early before other threads will have started up, so it will be the game thread
	return !GIsGameThreadIdInitialized || appGetCurrentThreadId() == GGameThreadId;
}

void FRenderCommandFence::BeginFence()
{
	appInterlockedIncrement((INT*)&NumPendingFences);

	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		FenceCommand,
		FRenderCommandFence*,Fence,this,
		{
			// Use a memory barrier to ensure that memory writes made by the rendering thread are visible to the game thread before the
			// NumPendingFences decrement.
			appMemoryBarrier();

			appInterlockedDecrement((INT*)&Fence->NumPendingFences);
		});
}

UINT FRenderCommandFence::GetNumPendingFences() const
{
	CheckRenderingThreadHealth();
	return NumPendingFences;
}

/**
 * Waits for pending fence commands to retire.
 * @param NumFencesLeft	- Maximum number of fence commands to leave in queue
 */
void FRenderCommandFence::Wait( UINT NumFencesLeft/*=0*/ ) const
{
	check(IsInGameThread());

	SCOPE_CYCLE_COUNTER(STAT_GameIdleTime);
	DWORD IdleStart = appCycles();
	while(NumPendingFences > NumFencesLeft)
	{
		// Check that the rendering thread is still running.
		CheckRenderingThreadHealth();

		// Yield CPU time while waiting.
		appSleep(0);
	};
	GGameThreadIdle += appCycles() - IdleStart;
}

/**
 * Waits for all deferred deleted objects to be cleaned up. Should only be used from the game thread.
 */
void FlushDeferredDeletion()
{
#if XBOX && USE_XeD3D_RHI
	// make sure all textures are freed up after GCing
	FlushRenderingCommands();
	extern void DeleteUnusedXeResources();
	ENQUEUE_UNIQUE_RENDER_COMMAND( BlockUntilGPUIdle, { RHIBlockUntilGPUIdle(); } );
	for( INT i=0; i<2; i++ )
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND( DeleteResources, { DeleteUnusedXeResources(); } );
	}
	FlushRenderingCommands();
#elif PS3 && USE_PS3_RHI
	// make sure all textures are freed up after GCing
	FlushRenderingCommands();
	extern void DeleteUnusedPS3Resources();
	ENQUEUE_UNIQUE_RENDER_COMMAND( BlockUntilGPUIdle, { RHIBlockUntilGPUIdle(); } );
	for( INT i=0; i<2; i++ )
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND( DeleteResources, { DeleteUnusedPS3Resources(); } );
	}
	FlushRenderingCommands();
#endif
}

/**
 * Waits for the rendering thread to finish executing all pending rendering commands.  Should only be used from the game thread.
 */
void FlushRenderingCommands()
{
	// Find the objects which may be cleaned up once the rendering thread command queue has been flushed.
	FPendingCleanupObjects* PendingCleanupObjects = GetPendingCleanupObjects();

	// Issue a fence command to the rendering thread and wait for it to complete.
	FRenderCommandFence Fence;
	Fence.BeginFence();
	Fence.Wait();

	// Delete the objects which were enqueued for deferred cleanup before the command queue flush.
	delete PendingCleanupObjects;
}

/** The set of deferred cleanup objects which are pending cleanup. */
FPendingCleanupObjects* GPendingCleanupObjects = NULL;

FPendingCleanupObjects::~FPendingCleanupObjects()
{
	for(INT ObjectIndex = 0;ObjectIndex < Num();ObjectIndex++)
	{
		(*this)(ObjectIndex)->FinishCleanup();
	}
}

void BeginCleanup(FDeferredCleanupInterface* CleanupObject)
{
	if(GIsThreadedRendering)
	{
		// If no pending cleanup set exists, create a new one.
		if(!GPendingCleanupObjects)
		{
			GPendingCleanupObjects = new FPendingCleanupObjects();
		}

		// Add the specified object to the pending cleanup set.
		GPendingCleanupObjects->AddItem(CleanupObject);
	}
	else
	{
		CleanupObject->FinishCleanup();
	}
}

FPendingCleanupObjects* GetPendingCleanupObjects()
{
	FPendingCleanupObjects* OldPendingCleanupObjects = GPendingCleanupObjects;
	GPendingCleanupObjects = NULL;

	return OldPendingCleanupObjects;
}
