/*=============================================================================
	ProfilerIntegration.h: Provides external profiler timing support.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __ProfilerIntegration_h__
#define __ProfilerIntegration_h__

/**
 * FProfilerBase
 *
 * Interface to various profiler API functions, dynamically linked
 */
class FProfilerBase
{
public:
	/**
	 * Returns true if profiler functionality is available, e.g. the appropriate dll was successfully 
	 * loaded and the hooks were found. If we haven't yet attempted to load the dll file, calling 
	 * IsActive() will load and initialize on demand.
	 *
	 * @return TRUE is profiler is available, FALSE otherwise
	 */
	UBOOL IsActive();

	/** Pauses profiling. */
	void PauseProfiler();

	/** Resumes profiling. */
	void ResumeProfiler();

	/**
	 * Creates instance of the profiler integration singleton. Which type depends on defines. 
	 *
	 * @return Singleton instance of profiler wrapper for specific profiler.
	 */
	static FProfilerBase* CreateSingleton();

protected:
	/**
	 * Profiler interface.
	 */

	/** Pauses profiling. */
	virtual void ProfilerPauseFunction()
	{}

	/** Resumes profiling. */
	virtual void ProfilerResumeFunction()
	{}

	/**
	 * Initializes profiler hooks. It is not valid to call pause/ resume on an uninitialized
	 * profiler and the profiler implementation is free to assert or have other undefined
	 * behavior.
	 *
	 * @return TRUE if successful, FALSE otherwise.
	 */
	virtual UBOOL Initialize()
	{
		return TRUE;
	}

	/** Hidden constructor. Use CreateSingleton to instance. */
	FProfilerBase();
	
	/** Empty virtual destructor. */
	virtual ~FProfilerBase()
	{}

private:
	/** TRUE if initialized. FALSE if not initialized yet or if initialization failed. */
	UBOOL bIsInitialized;

	/** TRUE if code tried to initialize but failed. */
	UBOOL bWasUnableToInitialize;

	/** Number of timers currently running. Timers are always 'global inclusive'. */
	INT TimerCount;

	/** Whether or not profiling is currently paused (as far as we know.) */
	UBOOL bIsPaused;

	/** Friend class so we can access the private members directly. */
	friend class FScopedProfilerBase;
};

/** Pointer to profiler wrapper. Type depends on defines in UnBuild.h. */
extern FProfilerBase* GExternalProfiler;


/**
 * Base class for FScoped*Timer and FScoped*Excluder
 */
class FScopedProfilerBase
{
protected:
	/**
	 * Pauses or resumes profiler and keeps track of the prior state so it can be restored later.
	 *
	 * @param	bWantPause	TRUE if this timer should 'include' code, or FALSE to 'exclude' code
	 *
	 **/
	void StartScopedTimer( const UBOOL bWantPause );

	/** Stops the scoped timer and restores profiler to its prior state */
	void StopScopedTimer();

private:
	/** Stores the previous 'Paused' state of VTune before this scope started */
	UBOOL bWasPaused;

};

/**
 * FScopedProfilerIncluder
 *
 * Use this to include a body of code in profiler's captured session using 'Resume' and 'Pause' cues.  It
 * can safely be embedded within another 'timer' or 'excluder' scope.
 */
class FScopedProfilerIncluder : public FScopedProfilerBase
{
public:
	/** Constructor */
	FScopedProfilerIncluder()
	{
		// 'Timer' scopes will always 'resume' VTune
		const UBOOL bWantPause = FALSE;
		StartScopedTimer( bWantPause );
	}

	/** Destructor */
	~FScopedProfilerIncluder()
	{
		StopScopedTimer();
	}
};

/**
 * FScopedProfilerExcluder
 *
 * Use this to EXCLUDE a body of code from profiler's captured session.  It can safely be embedded
 * within another 'timer' or 'excluder' scope.
 */
class FScopedProfilerExcluder : public FScopedProfilerBase
{
public:
	/** Constructor */
	FScopedProfilerExcluder()
	{
		// 'Excluder' scopes will always 'pause' VTune
		const UBOOL bWantPause = TRUE;
		StartScopedTimer( bWantPause );
	}

	/** Destructor */
	~FScopedProfilerExcluder()
	{
		StopScopedTimer();
	}

};

#define SCOPE_PROFILER_INCLUDER(X) FScopedProfilerIncluder ProfilerIncluder_##X;
#define SCOPE_PROFILER_EXCLUDER(X) FScopedProfilerExcluder ProfilerExcluder_##X;

#endif // __ProfilerIntegration_h__
