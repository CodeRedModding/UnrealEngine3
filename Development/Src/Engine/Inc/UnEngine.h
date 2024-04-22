/*=============================================================================
	UnEngine.h: Unreal engine helper definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/*-----------------------------------------------------------------------------
	UServerCommandlet.
-----------------------------------------------------------------------------*/

BEGIN_COMMANDLET(Server,Engine)
	void StaticInitialize()
	{
		IsClient = FALSE;
		IsEditor = FALSE;
		LogToConsole = TRUE;
	}
END_COMMANDLET


BEGIN_COMMANDLET(SmokeTest,Engine)
	void StaticInitialize()
	{
		IsClient = FALSE;
		IsEditor = FALSE;
		LogToConsole = TRUE;
	}
END_COMMANDLET

BEGIN_COMMANDLET(PatchScript,Engine)
	void StaticInitialize();
	class FScriptPatchWorker* Worker;
END_COMMANDLET


//
//	FLocalPlayerIterator - Iterates over local players in the game.
//

class FLocalPlayerIterator
{
protected:

	UEngine*	Engine;
	INT			Index;

public:

	// Constructor.

	FLocalPlayerIterator(UEngine* InEngine):
		Engine(InEngine),
		Index(-1)
	{
		++*this;
	}

	void operator++()
	{
		if ( Engine != NULL )
		{
			while (Engine->GamePlayers.IsValidIndex(++Index) && !Engine->GamePlayers(Index));
		}
	}
	ULocalPlayer* operator*()
	{
		return Engine && Engine->GamePlayers.IsValidIndex(Index) ? Engine->GamePlayers(Index) : NULL;
	}
	ULocalPlayer* operator->()
	{
		return Engine && Engine->GamePlayers.IsValidIndex(Index) ? Engine->GamePlayers(Index) : NULL;
	}
	operator UBOOL()
	{
		return Engine && Engine->GamePlayers.IsValidIndex(Index);
	}
	void RemoveCurrent()
	{
		checkSlow(Engine);
		check(Engine->GamePlayers.IsValidIndex(Index));
		Engine->GamePlayers.Remove(Index--);
	}
	INT GetIndex()
	{
		return Index;
	}
};

/*-----------------------------------------------------------------------------
	Tick/ update stats helper for profiling.
-----------------------------------------------------------------------------*/

/**
 * Helper structure encapsulating all information gathered.
 */
struct FTickStats
{
	/** Object associated with instances. We keep the name because the object might be gone.*/
	FString ObjectPathName;
	/** Result of GetDetailedInfo() on the above.  */
	FString ObjectDetailedInfo;
	/** Result of GetDetailedInfo() on the above.  */
	FName ObjectClassFName;
	/** Index of GC run when the validity of the UObject Pointer was last checked.  */
	INT GCIndex;
	/** Total accumulative time captured. */
	FLOAT TotalTime;
	/** Number of captures this frame. */
	INT Count;
	/** bForSummary is used for the logging code to know if this should be used for a summary or not **/
	UBOOL bForSummary;

	/** Compare helper for Sort<> */
	static inline INT Compare( const FTickStats& A, const FTickStats& B	)
	{
		return (B.TotalTime - A.TotalTime) > 0 ? 1 : -1;
	}
};

/**
 * Helper struct for gathering detailed per object tick stats.
 */
struct FDetailedTickStats : public FCallbackEventDevice
{
	/** Constructor, initializing all members. */
	FDetailedTickStats( INT InNumClassesToReport, FLOAT InTimeBetweenLogDumps, FLOAT InMinTimeBetweenLogDumps, FLOAT InTimesToReport, const TCHAR* InOperationPerformed );

	/** Destructor */
	virtual ~FDetailedTickStats();

	/**
	 * Starts tracking an object and returns whether it's a recursive call or not. If it is recursive
	 * the function will return FALSE and EndObject should not be called on the object.
	 *
	 * @param	Object		Object to track
	 * @return	FALSE if object is already tracked and EndObject should NOT be called, TRUE otherwise
	 */
	UBOOL BeginObject( UObject* Object );

	/**
	 * Finishes tracking the object and updates the time spent.
	 *
	 * @param	Object		Object to track
	 * @param	DeltaTime	Time we've been tracking it
	 * @param   bForSummary Object should be used for high level summary
	 */
	void EndObject( UObject* Object, FLOAT DeltaTime, UBOOL bForSummary );

	/**
	 * Reset stats to clean slate.
	 */
	void Reset();

	/**
	 * Dump gathered stats informatoin to the log.
	 */
	void DumpStats();

	/**
	 * Responds to CALLBACK_PreGarbageCollection; used indirectly to avoid GC issues
	 * @param InType Callback type (should only be CALLBACK_PreGarbageCollection)
	 */
	virtual void Send( ECallbackEventType InType)
	{
		GCIndex++;
		check(ObjectsInFlight.Num() == 0); // probably not required, but we shouldn't have anything in flight when we GC
	}

private:
	/** This is the collection of stats; some refer to objects that are long gone. */
	TArray<FTickStats> AllStats;
	/** Mapping from class to an index into the AllStats array. */
	TMap<const UObject*,INT> ObjectToStatsMap;
	/** Set of objects currently being tracked. Needed to correctly handle recursion. */
	TSet<const UObject*> ObjectsInFlight;

	/** Index of GC run. This is used to invalidate UObject pointers to make the whole system GC safe.  */
	INT GCIndex;
	/** The GC callback cannot usually be registered at construction because this comes from a static data structure  */
	UBOOL GCCallBackRegistered;
	/** Number of objects to report. Top X */
	INT	NumObjectsToReport;
	/** Time between dumping to the log in seconds. */
	FLOAT TimeBetweenLogDumps;
	/** Minimum time between log dumps, used for e.g. slow frames dumping. */
	FLOAT MinTimeBetweenLogDumps;
	/** Last time stats where dumped to the log. */
	DOUBLE LastTimeOfLogDump;
	/** Tick time in ms to report if above. */
	FLOAT TimesToReport;
	/** Name of operation performed that is being tracked. */
	FString OperationPerformed;
};

/** Scoped helper structure for capturing tick time. */
struct FScopedDetailTickStats
{
	/**
	 * Constructor, keeping track of object and start time.
	 */
	FScopedDetailTickStats( FDetailedTickStats& InDetailedTickStats, UObject* ObjectToTrack );

	/**
	 * Destructor, calculating delta time and updating global helper.
	 */
	~FScopedDetailTickStats();


private:
	/** Object to track. 
		Not GC safe, but we won't have anything in-flight during GC so that should be moot
	*/
	UObject* Object;
	/** Tick start time. */
	DWORD StartCycles;
	/** Detailed tick stats to update. */
	FDetailedTickStats& DetailedTickStats;
	/** Whether object should be tracked. FALSE e.g. when recursion is involved. */
	UBOOL bShouldTrackObject;
	/** Whether object class should be tracked. FALSE e.g. when recursion is involved. */
	UBOOL bShouldTrackObjectClass;
};

/**
 * Empty base class for post-ship patching support, allowing per-game
 * code-based level patching, etc.
 */
class FGamePatchHelper
{
public:
	/**
	 * Fix up the world in code however necessary
	 *
	 * @param World UWorld object for the world to fix
	 */
	virtual void FixupWorld(UWorld* World)
	{
	}
};

/** Global GamePatchHelper object that a game can override */
extern FGamePatchHelper* GGamePatchHelper;


// Calculate the average frame time by using the stats system.
inline void CalculateFPSTimings()
{
	extern FLOAT GAverageFPS;
	extern FLOAT GAverageMS;
#if STATS
	// Calculate the average frame time by using the stats system.
	GAverageMS	= (FLOAT)GFPSCounter.GetAverage() * 1000.0;
#else
	// Calculate the average frame time via continued averaging.
	static DOUBLE LastTime	= 0;
	DOUBLE CurrentTime		= appSeconds();
	FLOAT FrameTime			= (CurrentTime - LastTime) * 1000;
	// A 3/4, 1/4 split gets close to a simple 10 frame moving average
	GAverageMS				= GAverageMS * 0.75f + FrameTime * 0.25f;
	LastTime				= CurrentTime;
#endif
	// Calculate average framerate.
	GAverageFPS = 1000.f / GAverageMS;
}
