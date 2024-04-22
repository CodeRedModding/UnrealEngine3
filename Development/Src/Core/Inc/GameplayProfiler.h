/*=============================================================================
	GameplayProfiler.h: gameplay profiling support.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef GAMEPLAY_PROFILER_H
#define GAMEPLAY_PROFILER_H

#if !USE_GAMEPLAY_PROFILER

#define GAMEPLAY_PROFILER_TRACK_FUNCTION( Object )
#define GAMEPLAY_PROFILER_TRACK_ACTOR_WITH_ASSET( Object, AssetObject )
#define GAMEPLAY_PROFILER_SET_ACTOR_TICKING( Object, bTicked )
#define GAMEPLAY_PROFILER_TRACK_COMPONENT_WITH_ASSET( Object, AssetObject )
#define GAMEPLAY_PROFILER( x )

#else

#define GAMEPLAY_PROFILER_TRACK_FUNCTION( Object )								FScopedGameplayStats ScopedGameplay_##Object(Object,NULL,GPPToken_Function);
#define GAMEPLAY_PROFILER_TRACK_ACTOR_WITH_ASSET( Object, AssetObject )			FScopedGameplayStats ScopedGameplay_##Object(Object,AssetObject,GPPToken_Actor);
#define GAMEPLAY_PROFILER_SET_ACTOR_TICKING( Object, bTicked )					{ if( !(bTicked) ) { ScopedGameplay_##Object.SkipInDetailedView(); } }
#define GAMEPLAY_PROFILER_TRACK_COMPONENT_WITH_ASSET( Object, AssetObject )		FScopedGameplayStats ScopedGameplay_##Object(Object,AssetObject,GPPToken_Component);
#define GAMEPLAY_PROFILER( x )													x

/**
 * Enum of token types.
 */
enum EGPPTokenType
{
	GPPToken_Function	= 0,
	GPPToken_Actor		= 1,
	GPPToken_Component	= 2,
	GPPToken_EndOfScope = 3,
	GPPToken_Frame		= 4,
	GPPToken_EOS		= 5,
	GPPToken_CycleStat	= 6,
};


/**
 * Gameplay profiler. Uses token emission to data stream a la memory profiler to write
 * data to disk that is then analyzed/ visualized via a standalone tool.
 */
class FGameplayProfiler
{
public:
	/**
	 * Constructor.
	 */
	FGameplayProfiler();

	/**
	 * Destructur, finishing up serialization of data.
	 */
	virtual ~FGameplayProfiler();
	
	/**
	 * Exec handler. Parses command and returns TRUE if handled.
	 *
	 * @param	Cmd		Command to parse
	 * @param	Ar		Output device to use for logging
	 * @return	TRUE if handled, FALSE otherwise
	 */
	static UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar );

	/**
	 * Called every game thread tick.
	 */
	static void Tick();

	/**
	 * Returns true if currently active
	 */
	static UBOOL IsActive();

	/** @return Name of file being written to. */
	const FString GetFileName() const
	{
		return FileName;
	}

#if STATS
	/**
	 * Start tracking a cycle stat. Code relies on matching nested begin/ end.  Called by FCycleCounter.
	 *
	 * @param	Name	Name of stat to track
	 */
	void BeginTrackCycleStat( const struct FCycleStat& CycleStat );
#endif

	/**
	 * End tracking object.
	 *
	 * @param	DeltaCycles				Cycles elapsed since tracking started.
	 * @param	bShouldSkipInDetailedView	Whether this scope should be skipped in detailed view
	 */
	void EndTrackObject( DWORD DeltaCycles, UBOOL bShouldSkipInDetailedView );

private:
	// Keep those hidden to ensure we only ever have scoped tracking for proper nesting.
	
	/**
	 * Start tracking an object. Code relies on matching nested begin/ end.
	 *
	 * @param	Object			Object to track
	 * @param	AssetObject		Associated asset object (can be null)
	 * @param	Type			Type of object to track
	 */
	void BeginTrackObject( UObject* Object, UObject* AssetObject, EGPPTokenType Type );
	
	/**
	 * Emits an end of file marker into the data stream.
	 */
	void EmitEndMarker();

	/**
	 * Either teturns the index into the string table if found or adds and returns
	 * newly created index.
	 *
	 * @param	Object		Object to find full name index for
	 * @return	Index into string table
	 */
	INT GetStringTableIndex( UObject* Object );

	/**
	 * Flushes the memory writer to HDD.
	 */
	void FlushMemoryWriter();

	/** Friend class used for scoping */
	friend struct FScopedGameplayStats;

	/** Whether to toggle capturing the next tick. */
	static UBOOL bShouldToggleCaptureNextTick;

	/** Last frame time. */
	static DOUBLE LastFrameTime;

	/** If > 0 indicates the time at which to stop the capture. */
	static DOUBLE TimeToStopCapture;

	/** File writer used for serialization to HDD. */
	FArchive* FileWriter;

	/** Transient memory writer used to avoid blocking on I/O during capture. */
	FBufferArchive MemoryWriter;

	/** Name of file written to. */
	FString FileName;
};


/** Global gameplay profiler object. */
extern FGameplayProfiler* GGameplayProfiler;

/**
 * Scoped gameplay stats tracking helper. Useful for functions with multiple return paths and guarantees
 * proper nesting.
 */
struct FScopedGameplayStats
{
	/** 
	 * Constructor, starting the tracking if enabled.
	 *
	 * @param	Object	Object to track
	 */
	FScopedGameplayStats( UObject* InObject, UObject* InAssetObject, EGPPTokenType Type )
	:	Object(InObject)
	,	AssetObject(InAssetObject)
	,	StartCycles(0)
	,	bShouldSkipInDetailedView(FALSE)
	{
		if( GGameplayProfiler && Object )
		{
			StartCycles = appCycles();
			Object		= InObject;
			AssetObject = InAssetObject;
			GGameplayProfiler->BeginTrackObject( Object, AssetObject, Type );
		}
	}

	/**
	 * Let system know that we want to skip this object in detailed view.
	 */
	void SkipInDetailedView()
	{
		bShouldSkipInDetailedView = TRUE;
	}

	/**
	 * Destructor, ending tracking if enabled.
	 */
	~FScopedGameplayStats()
	{
		if( GGameplayProfiler && Object )
		{
			DWORD Cycles = appCycles() - StartCycles;
			GGameplayProfiler->EndTrackObject( Cycles, bShouldSkipInDetailedView );
		}
	}

private:
	/** Object to track. */
	UObject*	Object;
	/** Asset object associated with tracked object (can be NULL) */
	UObject*	AssetObject;
	/** Cycle count before function was being called. */
	DWORD		StartCycles;
	/** Whether to skip this object in detailed view. */
	UBOOL		bShouldSkipInDetailedView;
};


#endif // USE_GAMEPLAY_PROFILER

#endif	//#ifndef GAMEPLAY_PROFILER_H


