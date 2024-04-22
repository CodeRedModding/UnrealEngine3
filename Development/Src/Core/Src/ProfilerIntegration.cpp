/*=============================================================================
	VTuneIntegration.cpp: Provides integrated VTune timing support.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "CorePrivate.h"
#include "ProfilerIntegration.h"


/** 
 * Defines use to override which profiler library to try loading. Necessary as e.g. the AQtime DLL could be 
 * in the path, but you want to use VTune instead.
 */
#define USE_VTUNE	1
#define USE_AQTIME	1


/** Pointer to profiler wrapper. Type depends on defines in UnBuild.h. */
FProfilerBase* GExternalProfiler = NULL;


/*-----------------------------------------------------------------------------
	VTune implementation of FProfiler
-----------------------------------------------------------------------------*/

#if WITH_VTUNE

class FProfilerVTune : public FProfilerBase
{
public:
	/** Pauses profiling. */
	virtual void ProfilerPauseFunction()
	{
		VTPause();
	}

	/** Resumes profiling. */
	virtual void ProfilerResumeFunction()
	{
		VTResume();
	}

	/**
	 * Initializes profiler hooks. It is not valid to call pause/ resume on an uninitialized
	 * profiler and the profiler implementation is free to assert or have other undefined
	 * behavior.
	 *
	 * @return TRUE if successful, FALSE otherwise.
	 */
	virtual UBOOL Initialize()
	{
		check( DLLHandle == NULL );

		// Try to load the VTune DLL
		DLLHandle = appGetDllHandle( TEXT( "VtuneApi.dll" ) );
		if( DLLHandle != NULL )
		{
			// Get API function pointers of interest
			{
				// "VTPause"
				VTPause = (VTPauseFunctionPtr) appGetDllExport( DLLHandle, TEXT( "VTPause" ) );
				if( VTPause == NULL )
				{
					// Try decorated version of this function
					VTPause = (VTPauseFunctionPtr) appGetDllExport( DLLHandle, TEXT( "_VTPause@0" ) );
				}

				// "VTResume"
				VTResume = (VTResumeFunctionPtr) appGetDllExport( DLLHandle, TEXT( "VTResume" ) );
				if( VTResume == NULL )
				{
					// Try decorated version of this function
					VTResume = ( VTResumeFunctionPtr ) appGetDllExport( DLLHandle, TEXT( "_VTResume@0" ) );
				}
			}

			if( VTPause == NULL || VTResume == NULL )
			{
				// Couldn't find the functions we need.  VTune support will not be active.
				appFreeDllHandle( DLLHandle );
				DLLHandle = NULL;
				VTPause = NULL;
				VTResume = NULL;
			}
		}

		// Don't silently fail but rather warn immediately as no data will be collected.
		if( DLLHandle == NULL )
		{
			appMsgf( AMT_OK, TEXT("Failed to initialize VTuneApi.dll") );
		}

		return DLLHandle != NULL;
	}

private:
	/** Constructor, hidden as we need to use CreateSingleton to create instance. */
	FProfilerVTune()
	:	DLLHandle(NULL)
	,	VTPause(NULL)
	,	VTResume(NULL)
	{
	}

	/** Function pointer type for VTPause() */
	typedef void ( *VTPauseFunctionPtr )( void );

	/** Function pointer type for VTResume() */
	typedef void ( *VTResumeFunctionPtr )( void );

	/** DLL handle for VTuneApi.DLL */
	void* DLLHandle;
	/** Pointer to VTPause function. */
	VTPauseFunctionPtr VTPause;
	/** Pointer to VTResume function. */
	VTResumeFunctionPtr VTResume;

	/** Friend to access constructor. */
	friend class FProfilerBase;
};

#endif


/*-----------------------------------------------------------------------------
	AQtime implementation of FProfiler
-----------------------------------------------------------------------------*/

#if WITH_AQTIME

class FProfilerAQtime : public FProfilerBase
{
public:
	/** Pauses profiling. */
	virtual void ProfilerPauseFunction()
	{
		EnableProfiling((short)0);
	}

	/** Resumes profiling. */
	virtual void ProfilerResumeFunction()
	{
		EnableProfiling((short)-1);
	}

	/**
	 * Initializes profiler hooks. It is not valid to call pause/ resume on an uninitialized
	 * profiler and the profiler implementation is free to assert or have other undefined
	 * behavior.
	 *
	 * @return TRUE if successful, FALSE otherwise.
	 */
	virtual UBOOL Initialize()
	{
		check( DLLHandle == NULL );

		// Try to load the VTune DLL
		DLLHandle = appGetDllHandle( TEXT( "aqProf.dll" ) );
		if( DLLHandle != NULL )
		{
			// Get API function pointers of interest
			// "EnableProfiling"
			EnableProfiling = (EnableProfilingFunctionPtr) appGetDllExport( DLLHandle, TEXT( "EnableProfiling" ) );

			if( EnableProfiling == NULL )
			{
				// Couldn't find the function we need.  AQtime support will not be active.
				appFreeDllHandle( DLLHandle );
				DLLHandle = NULL;
			}
		}

		// Don't silently fail but rather warn immediately as no data will be collected.
		if( DLLHandle == NULL )
		{
			appMsgf( AMT_OK, TEXT("Failed to initialize aqProf.dll") );
		}

		return DLLHandle != NULL;
	}

private:
	/** Constructor, hidden as we need to use CreateSingleton to create instance. */
	FProfilerAQtime()
	:	DLLHandle(NULL)
	,	EnableProfiling(NULL)
	{
	}

	/** Function pointer type for EnableProfiling */
	typedef void (__stdcall *EnableProfilingFunctionPtr )( short Enable );

	/** DLL handle for VTuneApi.DLL */
	void* DLLHandle;
	/** Pointer to VTPause function. */
	EnableProfilingFunctionPtr EnableProfiling;

	/** Friend to access constructor. */
	friend class FProfilerBase;
};

#endif

/*-----------------------------------------------------------------------------
	FProfilerBase
-----------------------------------------------------------------------------*/

/** Hidden constructor. Use CreateSingleton to instance. */
FProfilerBase::FProfilerBase()
:	bIsInitialized(FALSE)
,	bWasUnableToInitialize(FALSE)
,	TimerCount(0)
// No way to tell whether we're paused or not so we assume paused as it makes the most sense
,	bIsPaused(TRUE)
{
}

/**
 * Creates instance of the profiler integration singleton. Which type depends on defines. 
 *
 * @return Singleton instance of profiler wrapper for specific profiler.
 */
FProfilerBase* FProfilerBase::CreateSingleton()
{
	static FProfilerBase* WrapperSingleton = NULL;

#if WITH_AQTIME && USE_AQTIME
	if( WrapperSingleton == NULL )
	{
		WrapperSingleton = new FProfilerAQtime();
		if( !WrapperSingleton->IsActive() )
		{
			delete WrapperSingleton;
			WrapperSingleton = NULL;
		}
	}
#endif

#if WITH_VTUNE && USE_VTUNE
	if( WrapperSingleton == NULL )
	{
		WrapperSingleton = new FProfilerVTune();
		if( WrapperSingleton->IsActive() )
		{
			delete WrapperSingleton;
			WrapperSingleton = NULL;
		}

	}
#endif

	// Fallback implementation.
	if( WrapperSingleton == NULL )
	{
		WrapperSingleton = new FProfilerBase();
	}

	return WrapperSingleton;
}

/**
 * Returns true if profiler functionality is available, e.g. the appropriate dll was successfully 
 * loaded and the hooks were found. If we haven't yet attempted to load the dll file, calling 
 * IsActive() will load and initialize on demand.
 *
 * @return TRUE is profiler is available, FALSE otherwise
 */
UBOOL FProfilerBase::IsActive()
{
	// Only try to initialize on demand once.
	if( !bIsInitialized && !bWasUnableToInitialize )
	{
		if( Initialize() )
		{
			bIsInitialized = TRUE;
		}
		else
		{
			bWasUnableToInitialize = TRUE;
		}
	}
	return bIsInitialized;
}

/** Pauses profiling. */
void FProfilerBase::PauseProfiler()
{
	if( IsActive() )
	{
		ProfilerPauseFunction();
		bIsPaused = TRUE;
	}
}

/** Resumes profiling. */
void FProfilerBase::ResumeProfiler()
{
	if( IsActive() )
	{
		ProfilerResumeFunction();
		bIsPaused = FALSE;
	}
}

/*-----------------------------------------------------------------------------
	FScopedProfilerBase
-----------------------------------------------------------------------------*/

/**
 * Pauses or resumes profiler and keeps track of the prior state so it can be restored later.
 *
 * @param	bWantPause	TRUE if this timer should 'include' code, or FALSE to 'exclude' code
 *
 */
void FScopedProfilerBase::StartScopedTimer( const UBOOL bWantPause )
{
	// Create profiler on demand.
	if( !GExternalProfiler )
	{
		GExternalProfiler = FProfilerBase::CreateSingleton();
	}

	check( GExternalProfiler );
	// Store the current state of profiler
	bWasPaused = GExternalProfiler->bIsPaused;

	// If the current profiler state isn't set to what we need, or if the global profiler sampler isn't currently
	// running, then start it now
	if( GExternalProfiler->TimerCount == 0 || bWantPause != GExternalProfiler->bIsPaused )
	{
		if( bWantPause )
		{
			GExternalProfiler->PauseProfiler();
		}
		else
		{
			GExternalProfiler->ResumeProfiler();
		}
	}

	// Increment number of overlapping timers
	GExternalProfiler->TimerCount++;
}

/** Stops the scoped timer and restores profiler to its prior state */
void FScopedProfilerBase::StopScopedTimer()
{
	check( GExternalProfiler );
	// Make sure a timer was already started
	check( GExternalProfiler->TimerCount > 0 );

	// Decrement timer count
	GExternalProfiler->TimerCount--;

	// Restore the previous state of VTune
	if( bWasPaused != GExternalProfiler->bIsPaused )
	{
		if( bWasPaused )
		{
			GExternalProfiler->PauseProfiler();
		}
		else
		{
			GExternalProfiler->ResumeProfiler();
		}
	}
}


