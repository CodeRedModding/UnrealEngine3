/*=============================================================================
	UnAsyncLoading.h: Unreal async loading definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#if PERF_TRACK_DETAILED_ASYNC_STATS
/** Entry storing total time to process all objects of a specific class */
struct FMapTimeEntry
{
	/** Class of objects that we are timing */
	const UClass*	Class;
	/** How many objects of this class were processed */
	INT				ObjCount;
	/** Total time taken to process all objects of this class */
	DOUBLE			Time;

	FMapTimeEntry(const UClass* InClass, INT InObjCount, DOUBLE InTime) :
		Class(InClass),
		ObjCount(InObjCount),
		Time(InTime)
	{}
};

/** Utility that prints out a sorted list of entries in the supplied TMap; The return value is the sum total of
    time spent in all entries */
DOUBLE PrintSortedListFromMap(TMap<const UClass*,FMapTimeEntry>& Map);

#endif // PERF_TRACK_DETAILED_ASYNC_STATS


/**
 * Structure containing intermediate data required for async loading of all imports and exports of a
 * ULinkerLoad.
 */
struct FAsyncPackage : public FSerializableObject
{
	/**
	 * Constructor
	 */
	FAsyncPackage(const FString& InPackageName, const FGuid* InPackageGuid, FName InPackageType = NAME_None)
	:	PackageName					( InPackageName			)
	,	PackageGuid					( InPackageGuid != NULL ? *InPackageGuid : FGuid(0,0,0,0) )
	,	PackageType					( InPackageType			)
	,	Linker						( NULL					)
	,	ImportIndex					( 0						)
	,	ExportIndex					( 0						)
	,	PreLoadIndex				( 0						)
	,	PostLoadIndex				( 0						)
	,	TimeLimit					( FLT_MAX				)
	,	bUseTimeLimit				( FALSE					)
	,	bTimeLimitExceeded			( FALSE					)
	,	TickStartTime				( 0						)
	,	LastObjectWorkWasPerformedOn( NULL					)
	,	LastTypeOfWorkPerformed		( NULL					)
	,	LoadStartTime				( 0.0					)
	,	LoadPercentage				( 0						)
	,	bHasFinishedExportGuids		( FALSE					)
#if PERF_TRACK_DETAILED_ASYNC_STATS
	,	TickCount					( 0						)
	,	TickLoopCount				( 0						)
	,	CreateLinkerCount			( 0						)
	,	FinishLinkerCount			( 0						)
	,	CreateImportsCount			( 0						)
	,	CreateExportsCount			( 0						)
	,	PreLoadObjectsCount			( 0						)
	,	PostLoadObjectsCount		( 0						)
	,	FinishObjectsCount			( 0						)
	,	TickTime					( 0.0					)
	,	CreateLinkerTime			( 0.0					)
	,	FinishLinkerTime			( 0.0					)
	,	CreateImportsTime			( 0.0					)
	,	CreateExportsTime			( 0.0					)
	,	PreLoadObjectsTime			( 0.0					)
	,	FinishExportGuidsTime		( 0.0					)
	,	PostLoadObjectsTime			( 0.0					)
	,	FinishObjectsTime			( 0.0					)
#endif // PERF_TRACK_DETAILED_ASYNC_STATS
	{}

	/**
	 * Ticks the async loading code.
	 *
	 * @param	InbUseTimeLimit		Whether to use a time limit
	 * @param	InTimeLimit			Soft limit to time this function may take
	 *
	 * @return	TRUE if package has finished loading, FALSE otherwise
	 */
	UBOOL Tick( UBOOL bUseTimeLimit, FLOAT TimeLimit );

	/**
	 * @return Estimated load completion percentage.
	 */
	FLOAT GetLoadPercentage() const;

	/**
	 * @return Time load begun. This is NOT the time the load was requested in the case of other pending requests.
	 */
	DOUBLE GetLoadStartTime() const;

	/**
	 * Emulates UObject::ResetLoaders for the package's Linker objects, hence deleting it. 
	 */
	void ResetLoader();

	/**
	 * Returns the name of the package to load.
	 */
	const FString& GetPackageName() const
	{
		return PackageName;
	}

	/**
	 * Returns the type name associated with this package
	 */
	const FName &GetPackageType() const
	{
		return PackageType;
	}

	void AddCompletionCallback(const FAsyncCompletionCallbackInfo& Callback)
	{
		CompletionCallbacks.AddUniqueItem(Callback);
	}

	/**
	 * Serialize the linke for garbage collection.
	 * 
	 * @param Ar The archive to serialize with
	 */
	virtual void Serialize( FArchive& Ar )
	{
		Ar << Linker;
	}

private:
	/** Name of the package to load.																	*/
	FString						PackageName;
	/** GUID of the package to load, or the zeroed invalid GUID for "don't care" */
	FGuid						PackageGuid;
	/** An abstract type name associated with this package, for tagging use								*/
	FName						PackageType;
	/** Linker which is going to have its exports and imports loaded									*/
	ULinkerLoad*				Linker;
	/** Call backs called when we finished loading this package											*/
	TArray<FAsyncCompletionCallbackInfo>	CompletionCallbacks;
	/** Current index into linkers import table used to spread creation over several frames				*/
	INT							ImportIndex;
	/** Current index into linkers export table used to spread creation over several frames				*/
	INT							ExportIndex;
	/** Current index into GObjLoaded array used to spread routing PreLoad over several frames			*/
	INT							PreLoadIndex;
	/** Current index into GObjLoaded array used to spread routing PostLoad over several frames			*/
	INT							PostLoadIndex;
	/** Currently used time limit for this tick.														*/
	FLOAT						TimeLimit;
	/** Whether we are using a time limit for this tick.												*/
	UBOOL						bUseTimeLimit;
	/** Whether we already exceed the time limit this tick.												*/
	UBOOL						bTimeLimitExceeded;
	/** The time taken when we started the tick.														*/
	DOUBLE						TickStartTime;
	/** Last object work was performed on. Used for debugging/ logging purposes.						*/
	UObject*					LastObjectWorkWasPerformedOn;
	/** Last type of work performed on object.															*/
	const TCHAR*				LastTypeOfWorkPerformed;
	/** Time load begun. This is NOT the time the load was requested in the case of pending requests.	*/
	DOUBLE						LoadStartTime;
	/** Estimated load percentage.																		*/
	FLOAT						LoadPercentage;
	/** Whether the ExportGuids array has finished being filled out										*/
	/** @todo: Should this be spread across frames, it shouldn't be slow, but it needs testing */
	UBOOL						bHasFinishedExportGuids;

public:
#if PERF_TRACK_DETAILED_ASYNC_STATS
	/** Number of times Tick function has been called.													*/
	INT							TickCount;
	/** Number of iterations in loop inside Tick.														*/
	INT							TickLoopCount;

	/** Number of iterations for CreateLinker.															*/
	INT							CreateLinkerCount;
	/** Number of iterations for FinishLinker.															*/
	INT							FinishLinkerCount;
	/** Number of iterations for CreateImports.															*/
	INT							CreateImportsCount;
	/** Number of iterations for CreateExports.															*/
	INT							CreateExportsCount;
	/** Number of iterations for PreLoadObjects.														*/
	INT							PreLoadObjectsCount;
	/** Number of iterations for PostLoadObjects.														*/
	INT							PostLoadObjectsCount;
	/** Number of iterations for FinishObjects.															*/
	INT							FinishObjectsCount;

	/** Total time spent in Tick.																		*/
	DOUBLE						TickTime;
	/** Total time spent in	CreateLinker.																*/
	DOUBLE						CreateLinkerTime;
	/** Total time spent in FinishLinker.																*/
	DOUBLE						FinishLinkerTime;
	/** Total time spent in CreateImports.																*/
	DOUBLE						CreateImportsTime;
	/** Total time spent in CreateExports.																*/
	DOUBLE						CreateExportsTime;
	/** Total time spent in PreLoadObjects.																*/
	DOUBLE						PreLoadObjectsTime;
	/** Total time spent in FinishExportGuids.															*/
	DOUBLE						FinishExportGuidsTime;
	/** Total time spent in PostLoadObjects.															*/
	DOUBLE						PostLoadObjectsTime;
	/** Total time spent in FinishObjects.																*/
	DOUBLE						FinishObjectsTime;

	/** Map of each class of object loaded to the total time spent calling CreateExport on those objects */
	TMap<const UClass*,FMapTimeEntry>		CreateExportTimePerClass;
	/** Map of each class of object loaded to the total time spent calling PostLoad on those objects */
	TMap<const UClass*,FMapTimeEntry>		PostLoadTimePerClass;

#endif

private:
	/**
	 * Gives up time slice if time limit is enabled.
	 *
	 * @return TRUE if time slice can be given up, FALSE otherwise
	 */
	UBOOL GiveUpTimeSlice();
	/**
	 * Returns whether time limit has been exceeded.
	 *
	 * @return TRUE if time limit has been exceeded (and is used), FALSE otherwise
	 */
	UBOOL IsTimeLimitExceeded();
	/**
	 * Begin async loading process. Simulates parts of UObject::BeginLoad.
	 *
	 * Objects created during BeginAsyncLoad and EndAsyncLoad will have RF_AsyncLoading set
	 */
	void BeginAsyncLoad();
	/**
	 * End async loading process. Simulates parts of UObject::EndLoad(). FinishObjects 
	 * simulates some further parts once we're fully done loading the package.
	 */
	void EndAsyncLoad();
	/**
	 * Create linker async. Linker is not finalized at this point.
	 *
	 * @return TRUE
	 */
	UBOOL CreateLinker();
	/**
	 * Finalizes linker creation till time limit is exceeded.
	 *
	 * @return TRUE if linker is finished being created, FALSE otherwise
	 */
	UBOOL FinishLinker();
	/** 
	 * Create imports till time limit is exceeded.
	 *
	 * @return TRUE if we finished creating all imports, FALSE otherwise
	 */
	UBOOL CreateImports();
	/**
	 * Checks if all async texture allocations for this package have been completed.
	 *
	 * @return TRUE if all texture allocations have been completed, FALSE otherwise
	 */
	UBOOL FinishTextureAllocations();
	/**
	 * Create exports till time limit is exceeded.
	 *
	 * @return TRUE if we finished creating and preloading all exports, FALSE otherwise.
	 */
	UBOOL CreateExports();
	/**
	 * Preloads aka serializes all loaded objects.
	 *
	 * @return TRUE if we finished serializing all loaded objects, FALSE otherwise.
	 */
	UBOOL PreLoadObjects();
	/**
	 * Hookup any cross level exports that hadn't been moved from Linker's ExportGuidsAwaitingLookup to the Package's ExportGuids array
	 *
	 * @return TRUE if ExportGuids is finished being filled out
	 */
	UBOOL FinishExportGuids();
	/**
	 * Route PostLoad to all loaded objects. This might load further objects!
	 *
	 * @return TRUE if we finished calling PostLoad on all loaded objects and no new ones were created, FALSE otherwise
	 */
	UBOOL PostLoadObjects();
	/**
	 * Finish up objects and state, which means clearing the RF_AsyncLoading flag on newly created ones
	 *
	 * @return TRUE
	 */
	UBOOL FinishObjects();

#if PERF_TRACK_DETAILED_ASYNC_STATS
	/** Add this time taken for object of class Class to have CreateExport called, to the stats we track. */
	void TrackCreateExportTimeForClass(const UClass* Class, DOUBLE Time);

	/** Add this time taken for object of class Class to have PostLoad called, to the stats we track. */
	void TrackPostLoadTimeForClass(const UClass* Class, DOUBLE Time);
#endif // PERF_TRACK_DETAILED_ASYNC_STATS
};


