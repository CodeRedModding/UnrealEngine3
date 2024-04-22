/*=============================================================================
	UnAsyncLoading.cpp: Unreal async loading code.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "CorePrivate.h"
#include "UnScriptPatcher.h"
/*-----------------------------------------------------------------------------
	Async loading stats.
-----------------------------------------------------------------------------*/

DECLARE_STATS_GROUP(TEXT("AsyncIO"),STATGROUP_AsyncIO);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Fulfilled read count"),STAT_AsyncIO_FulfilledReadCount,STATGROUP_AsyncIO);
DECLARE_MEMORY_STAT(TEXT("Fulfilled read size"),STAT_AsyncIO_FulfilledReadSize,STATGROUP_AsyncIO);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Canceled read count"),STAT_AsyncIO_CanceledReadCount,STATGROUP_AsyncIO);
DECLARE_MEMORY_STAT(TEXT("Canceled read size"),STAT_AsyncIO_CanceledReadSize,STATGROUP_AsyncIO);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Outstanding read count"),STAT_AsyncIO_OutstandingReadCount,STATGROUP_AsyncIO);
DECLARE_MEMORY_STAT(TEXT("Outstanding read size"),STAT_AsyncIO_OutstandingReadSize,STATGROUP_AsyncIO);
DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("Uncompressor wait time"),STAT_AsyncIO_UncompressorWaitTime,STATGROUP_AsyncIO);
DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("Uncompressor total time"),STAT_UncompressorTime,STATGROUP_AsyncIO);
DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("Main thread block time"),STAT_AsyncIO_MainThreadBlockTime,STATGROUP_AsyncIO);
DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("Async package precache wait time"),STAT_AsyncIO_AsyncPackagePrecacheWaitTime,STATGROUP_AsyncIO);
DECLARE_FLOAT_COUNTER_STAT(TEXT("Bandwidth (MByte/ sec)"),STAT_AsyncIO_Bandwidth,STATGROUP_AsyncIO);


/*-----------------------------------------------------------------------------
	FAsyncPackage implementation.
-----------------------------------------------------------------------------*/

/**
 * Returns an estimated load completion percentage.
 */
FLOAT FAsyncPackage::GetLoadPercentage() const
{
	return LoadPercentage;
}

/**
 * @return Time load begun. This is NOT the time the load was requested in the case of other pending requests.
 */
DOUBLE FAsyncPackage::GetLoadStartTime() const
{
	return LoadStartTime;
}

/**
 * Emulates UObject::ResetLoaders for the package's Linker objects, hence deleting it. 
 */
void FAsyncPackage::ResetLoader()
{
	// Reset loader.
	if( Linker )
	{
		Linker->Detach( FALSE );
		Linker = NULL;
	}
}

/**
 * Returns whether time limit has been exceeded.
 *
 * @return TRUE if time limit has been exceeded (and is used), FALSE otherwise
 */
UBOOL FAsyncPackage::IsTimeLimitExceeded()
{
	if( !bTimeLimitExceeded && bUseTimeLimit )
	{
		DOUBLE CurrentTime = appSeconds();
		bTimeLimitExceeded = CurrentTime - TickStartTime > TimeLimit;
		if( GUseSeekFreeLoading )
		{
			// Log single operations that take longer than timelimit.
			if( (CurrentTime - TickStartTime) > (2.5 * TimeLimit) )
			{
 				debugfSuppressed(NAME_DevStreaming,TEXT("FAsyncPackage: %s %s took (less than) %5.2f ms"), 
 					LastTypeOfWorkPerformed, 
 					LastObjectWorkWasPerformedOn ? *LastObjectWorkWasPerformedOn->GetFullName() : TEXT(""), 
 					(CurrentTime - TickStartTime) * 1000);
			}
		}
	}
	return bTimeLimitExceeded;
}

/**
 * Gives up time slice if time limit is enabled.
 *
 * @return TRUE if time slice can be given up, FALSE otherwise
 */
UBOOL FAsyncPackage::GiveUpTimeSlice()
{
	bTimeLimitExceeded = bUseTimeLimit;
	return bTimeLimitExceeded;
}

/**
 * Begin async loading process. Simulates parts of UObject::BeginLoad.
 *
 * Objects created during BeginAsyncLoad and EndAsyncLoad will have RF_AsyncLoading set
 */
void FAsyncPackage::BeginAsyncLoad()
{
	// Manually increase the GObjBeginLoadCount as done in BeginLoad();
	UObject::GObjBeginLoadCount++;
	// All objects created from now on should be flagged as RF_AsyncLoading so StaticFindObject doesn't return them.
	GIsAsyncLoading = TRUE;
}

/**
 * End async loading process. Simulates parts of UObject::EndLoad(). FinishObjects 
 * simulates some further parts once we're fully done loading the package.
 */
void FAsyncPackage::EndAsyncLoad()
{
	// We're no longer async loading.
	GIsAsyncLoading = FALSE;
	// Simulate EndLoad();
	UObject::GObjBeginLoadCount--;
}

#if PERF_TRACK_DETAILED_ASYNC_STATS && STATS
#define DETAILED_ASYNC_STATS_TIMER(x) FScopeSecondsCounter ScopeSecondsCounter##x(x)
#define DETAILED_ASYNC_STATS(x) x
#else
#define DETAILED_ASYNC_STATS(x)
#define DETAILED_ASYNC_STATS_TIMER(x)
#endif

/**
 * Ticks the async loading code.
 *
 * @param	InbUseTimeLimit		Whether to use a time limit
 * @param	InTimeLimit			Soft limit to time this function may take
 *
 * @return	TRUE if package has finished loading, FALSE otherwise
 */
UBOOL FAsyncPackage::Tick( UBOOL InbUseTimeLimit, FLOAT InTimeLimit )
{
	DETAILED_ASYNC_STATS_TIMER( TickTime );
	DETAILED_ASYNC_STATS( TickCount++ );

 	check(LastObjectWorkWasPerformedOn==NULL);
	check(LastTypeOfWorkPerformed==NULL);

	// Set up tick relevant variables.
	bUseTimeLimit			= InbUseTimeLimit;
	bTimeLimitExceeded		= FALSE;
	TimeLimit				= InTimeLimit;
	TickStartTime			= appSeconds();

	// Keep track of time when we start loading.
	if( LoadStartTime == 0.0 )
	{
		LoadStartTime = TickStartTime;
	}

	// Whether we should execute the next step.
	UBOOL bExecuteNextStep	= TRUE;

	// Make sure we finish our work if there's no time limit. The loop is required as PostLoad
	// might cause more objects to be loaded in which case we need to Preload them again.
	do
	{
 		// force shut down from main thread if a shutdown request happened
		SHUTDOWN_IF_EXIT_REQUESTED;

		DETAILED_ASYNC_STATS( TickLoopCount++ );

		// Reset value to TRUE at beginning of loop.
		bExecuteNextStep	= TRUE;

		// Begin async loading, simulates BeginLoad and sets GIsAsyncLoading to TRUE.
		BeginAsyncLoad();

		// Create raw linker. Needs to be async created via ticking before it can be used.
		if( bExecuteNextStep )
		{
			DETAILED_ASYNC_STATS_TIMER( CreateLinkerTime );
			bExecuteNextStep = CreateLinker();
		}

		// Async create linker.
		if( bExecuteNextStep )
		{
			DETAILED_ASYNC_STATS_TIMER( FinishLinkerTime );
			bExecuteNextStep = FinishLinker();
		}

		// Create imports from linker import table.
		if( bExecuteNextStep )
		{
			DETAILED_ASYNC_STATS_TIMER( CreateImportsTime );
			bExecuteNextStep = CreateImports();
		}

		// Finish all async texture allocations.
		if( bExecuteNextStep )
		{
			bExecuteNextStep = FinishTextureAllocations();
		}

		// Create exports from linker export table and also preload them.
		if( bExecuteNextStep )
		{
			DETAILED_ASYNC_STATS_TIMER( CreateExportsTime );
			bExecuteNextStep = CreateExports();
		}
		
		// Call Preload on the linker for all loaded objects which causes actual serialization.
		if( bExecuteNextStep )
		{
			DETAILED_ASYNC_STATS_TIMER( PreLoadObjectsTime );
			bExecuteNextStep = PreLoadObjects();
		}

		// match up the Guids to their objects for cross level references pointing into this package
		if( bExecuteNextStep )
		{
			DETAILED_ASYNC_STATS_TIMER( FinishExportGuidsTime );
			bExecuteNextStep = FinishExportGuids();
		}

		// Call PostLoad on objects, this could cause new objects to be loaded that require
		// another iteration of the PreLoad loop.
		if( bExecuteNextStep )
		{
			DETAILED_ASYNC_STATS_TIMER( PostLoadObjectsTime );
			bExecuteNextStep = PostLoadObjects();
		}

		// End async loading, simulates EndLoad and sets GIsAsyncLoading to FALSE.
		EndAsyncLoad();

		// Finish objects (removing RF_AsyncLoading, dissociate imports and forced exports, 
		// call completion callback, ...
		if( bExecuteNextStep )
		{
			DETAILED_ASYNC_STATS_TIMER( FinishObjectsTime );
			bExecuteNextStep = FinishObjects();
		}
	// Only exit loop if we're either done or the time limit has been exceeded.
	} while( !IsTimeLimitExceeded() && !bExecuteNextStep );	

	check( bUseTimeLimit || bExecuteNextStep );

	// We can't have a reference to a UObject.
	LastObjectWorkWasPerformedOn	= NULL;
	// Reset type of work performed.
	LastTypeOfWorkPerformed			= NULL;

	// TRUE means that we're done loading this package.
	return bExecuteNextStep;
}

/**
 * Create linker async. Linker is not finalized at this point.
 *
 * @return TRUE
 */
UBOOL FAsyncPackage::CreateLinker()
{
	if( Linker == NULL )
	{
		DETAILED_ASYNC_STATS(CreateLinkerCount++);

		LastObjectWorkWasPerformedOn	= NULL;
		LastTypeOfWorkPerformed			= TEXT("creating Linker");

		// Try to find existing package or create it if not already present.
		UPackage* Package = UObject::CreatePackage( NULL, *PackageName );
		
		// if the linker already exists, we don't need to lookup the file (it may have been pre-created with
		// a different filename)
		Linker = ULinkerLoad::FindExistingLinkerForPackage(Package);

		if (!Linker)
		{
			// Retrieve filename on disk for package name. Errors are fatal here.
			FString PackageFileName;

			//If the linker does not exist for the basepackagename, we also need to check the packagename map to see if the packagename should be remapped
			
			FString PackageNameToLoad = PackageName;

			const FName *TempPackageName = UObject::GetPackageNameToFileMapping()->Find(FName::FName(*PackageName));
			if (TempPackageName != NULL)
			{
				PackageNameToLoad = TempPackageName->ToString();
			}

			if( !GPackageFileCache->FindPackageFile( *PackageNameToLoad, PackageGuid.IsValid() ? &PackageGuid : NULL, PackageFileName ) )
			{
				appErrorf(TEXT("Couldn't find file for package %s requested by async loading code."),*PackageName);
			}
		
			// Create raw async linker, requiring to be ticked till finished creating.
			Linker = ULinkerLoad::CreateLinkerAsync( Package, *PackageFileName, (GIsGame && !GIsEditor) ? (LOAD_SeekFree | LOAD_NoVerify) : LOAD_None  );
		}
	}
	return TRUE;
}

/**
 * Finalizes linker creation till time limit is exceeded.
 *
 * @return TRUE if linker is finished being created, FALSE otherwise
 */
UBOOL FAsyncPackage::FinishLinker()
{
	if( !Linker->HasFinishedInitializtion() )
	{
		DETAILED_ASYNC_STATS(FinishLinkerCount++);
		LastObjectWorkWasPerformedOn	= Linker->LinkerRoot;
		LastTypeOfWorkPerformed			= TEXT("ticking linker");
	
		// Operation still pending if Tick returns FALSE
		if( !Linker->Tick( TimeLimit, bUseTimeLimit ) )
		{
			// Give up remainder of timeslice if there is one to give up.
			GiveUpTimeSlice();
			return FALSE;
		}
	}
	return TRUE;
}

/** 
 * Create imports till time limit is exceeded.
 *
 * @return TRUE if we finished creating all imports, FALSE otherwise
 */
UBOOL FAsyncPackage::CreateImports()
{
	DETAILED_ASYNC_STATS( if( ImportIndex < Linker->ImportMap.Num() ) CreateImportsCount++; );

	// Create imports.
	while( ImportIndex < Linker->ImportMap.Num() && !IsTimeLimitExceeded() )
	{
 		UObject* Object	= Linker->CreateImport( ImportIndex++ );
		LastObjectWorkWasPerformedOn	= Object;
		LastTypeOfWorkPerformed			= TEXT("creating imports for");
	}

	return ImportIndex == Linker->ImportMap.Num();
}

/**
 * Checks if all async texture allocations for this package have been completed.
 *
 * @return TRUE if all texture allocations have been completed, FALSE otherwise
 */
UBOOL FAsyncPackage::FinishTextureAllocations()
{
	//@TODO: Cancel allocations if they take too long.

	UBOOL bHasCompleted = Linker->Summary.TextureAllocations.HasCompleted();
	if ( !bHasCompleted )
	{
		if ( bUseTimeLimit )
		{
			// Try again next Tick instead.
			GiveUpTimeSlice();
		}
		else
		{
			// Need to finish right now. Cancel async allocations that haven't finished yet.
			// Those will be allocated immediately by UTexture2D during serialization instead.
			Linker->Summary.TextureAllocations.CancelRemainingAllocations( FALSE );
			bHasCompleted = TRUE;
		}
	}
	return bHasCompleted;
}

/**
 * Create exports till time limit is exceeded.
 *
 * @return TRUE if we finished creating and preloading all exports, FALSE otherwise.
 */
UBOOL FAsyncPackage::CreateExports()
{
	DETAILED_ASYNC_STATS( if( ExportIndex < Linker->ExportMap.Num() ) CreateExportsCount++; );

	// Create exports.
	while( ExportIndex < Linker->ExportMap.Num() && !IsTimeLimitExceeded() )
	{
		const FObjectExport& Export = Linker->ExportMap(ExportIndex);
		
		// Precache data and see whether it's already finished.

		// We have sufficient data in the cache so we can load.
		//@script patcher (EF_ScriptPatcherExport): if the export was added by the script patcher, its SerialSize and SerialOffset are into
		// the memory reader, so don't check the file reader using these values
		if( Export.HasAnyFlags(EF_ScriptPatcherExport) || Linker->Precache( Export.SerialOffset, Export.SerialSize ) )
		{
#if PERF_TRACK_DETAILED_ASYNC_STATS
			DOUBLE StartTime = appSeconds();
#endif
			// Create the object...
			UObject* Object	= Linker->CreateExport( ExportIndex++ );
			// ... and preload it.
			if( Object )
			{
				// This will cause the object to be serialized. We do this here for all objects and
				// not just UClass and template objects, for which this is required in order to ensure
				// seek free loading, to be able introduce async file I/O.
				Linker->Preload( Object );

#if PERF_TRACK_DETAILED_ASYNC_STATS
				TrackCreateExportTimeForClass(Object->GetClass(), appSeconds() - StartTime);
#endif	
			}

			LastObjectWorkWasPerformedOn	= Object;
			LastTypeOfWorkPerformed			= TEXT("creating exports for");
			// This assumes that only CreateExports is taking a significant amount of time.
			LoadPercentage = 100.f * ExportIndex / Linker->ExportMap.Num();
		}
		// Data isn't ready yet. Give up remainder of time slice if we're not using a time limit.
		else if( GiveUpTimeSlice() )
		{
			INC_FLOAT_STAT_BY(STAT_AsyncIO_AsyncPackagePrecacheWaitTime,(FLOAT)GDeltaTime);
			return FALSE;
		}
	}
	
	return ExportIndex == Linker->ExportMap.Num();
}

/**
 * Preloads aka serializes all loaded objects.
 *
 * @return TRUE if we finished serializing all loaded objects, FALSE otherwise.
 */
UBOOL FAsyncPackage::PreLoadObjects()
{
	DETAILED_ASYNC_STATS( if( PreLoadIndex < UObject::GObjLoaded.Num() ) PreLoadObjectsCount++; );

	// Preload (aka serialize) the objects.
	while( PreLoadIndex < UObject::GObjLoaded.Num() && !IsTimeLimitExceeded() )
	{
		//@todo async: make this part async as well.
		UObject* Object = UObject::GObjLoaded( PreLoadIndex++ );
		check( Object && Object->GetLinker() );
		Object->GetLinker()->Preload( Object );
		LastObjectWorkWasPerformedOn	= Object;
		LastTypeOfWorkPerformed			= TEXT("preloading");
	}

	return PreLoadIndex == UObject::GObjLoaded.Num();
}

/**
 * Hookup any cross level exports that hadn't been moved from Linker's ExportGuidsAwaitingLookup to the Package's ExportGuids array
 *
 * @return TRUE if ExportGuids is finished being filled out
 */
UBOOL FAsyncPackage::FinishExportGuids()
{
	// @todo: spread this across multiple frames? doesn't seem worth it
	// if we do, we will want to count with something like the other functions count iterations:
	// DETAILED_ASYNC_STATS( if (!bHasFinishedExportGuids) FinishExportGuidsCount++; );
	// and make sure to print out the iterations in the PERF_TRACK_DETAILED_ASYNC_STATS below

	if (!bHasFinishedExportGuids)
	{
		Linker->LinkerRoot->LookupAllOutstandingCrossLevelExports(Linker);
		bHasFinishedExportGuids = TRUE;
	}

	return TRUE;
}

/**
 * Route PostLoad to all loaded objects. This might load further objects!
 *
 * @return TRUE if we finished calling PostLoad on all loaded objects and no new ones were created, FALSE otherwise
 */
UBOOL FAsyncPackage::PostLoadObjects()
{
	DETAILED_ASYNC_STATS( if( PostLoadIndex < UObject::GObjLoaded.Num() ) PostLoadObjectsCount++; );

	// PostLoad objects.
	while( PostLoadIndex < UObject::GObjLoaded.Num() && !IsTimeLimitExceeded() )
	{
		UObject* Object	= UObject::GObjLoaded( PostLoadIndex++ );
		check(Object);

#if PERF_TRACK_DETAILED_ASYNC_STATS
		DOUBLE StartTime = appSeconds();
#endif

		Object->ConditionalPostLoad();

#if PERF_TRACK_DETAILED_ASYNC_STATS
		TrackPostLoadTimeForClass(Object->GetClass(), appSeconds() - StartTime);
#endif

		LastObjectWorkWasPerformedOn	= Object;
		LastTypeOfWorkPerformed			= TEXT("postloading");
	}

	// New objects might have been loaded during PostLoad.
	return (PreLoadIndex == UObject::GObjLoaded.Num()) && (PostLoadIndex == UObject::GObjLoaded.Num());
}

/**
 * Finish up objects and state, which means clearing the RF_AsyncLoading flag on newly created ones
 *
 * @return TRUE
 */
UBOOL FAsyncPackage::FinishObjects()
{
	DETAILED_ASYNC_STATS(FinishObjectsCount++);
	LastObjectWorkWasPerformedOn	= NULL;
	LastTypeOfWorkPerformed			= TEXT("finishing all objects");

	// All loaded objects are now finished loading so we can clear the RF_AsyncLoading flag so
	// StaticFindObject returns them by default.
	for( INT ObjectIndex=0; ObjectIndex<UObject::GObjConstructedDuringAsyncLoading.Num(); ObjectIndex++ )
	{
		UObject* Object = UObject::GObjConstructedDuringAsyncLoading(ObjectIndex);
		Object->ClearFlags( RF_AsyncLoading );
	}
	UObject::GObjConstructedDuringAsyncLoading.Empty();
			
	// Simulate what UObject::EndLoad does.
	UObject::GObjLoaded.Empty();
	UObject::DissociateImportsAndForcedExports(); //@todo: this should be avoidable

	// Mark package as having been fully loaded and update load time.
	if( Linker->LinkerRoot )
	{
		Linker->LinkerRoot->MarkAsFullyLoaded();
		Linker->LinkerRoot->SetLoadTime( appSeconds() - LoadStartTime );
	}

	// Call any completion callbacks specified.
	for (INT i = 0; i < CompletionCallbacks.Num(); i++)
	{
		(*CompletionCallbacks(i).Callback)(Linker->LinkerRoot, CompletionCallbacks(i).UserData);
	}

	// give a hint to the IO system that we are done with this file for now
	FIOSystem* AsyncIO = GIOManager->GetIOSystem( IOSYSTEM_GenericAsync );
	if (AsyncIO)
	{
		AsyncIO->HintDoneWithFile(Linker->Filename);
	}

#if SUPPORTS_SCRIPTPATCH_LOADING
	// okay, now we're done with the patch data, we can toss it
	FScriptPatcher* Patcher = Linker->GetExistingScriptPatcher();
	if (Patcher)
	{
		Patcher->FreeLinkerPatch(Linker->LinkerRoot->GetFName());
	}
#endif

	// Cancel all texture allocations that haven't been claimed yet.
	Linker->Summary.TextureAllocations.CancelRemainingAllocations( TRUE );

	return TRUE;
}

#if PERF_TRACK_DETAILED_ASYNC_STATS
/** Add this time taken for object of class Class to have CreateExport called, to the stats we track. */
void FAsyncPackage::TrackCreateExportTimeForClass(const UClass* Class, DOUBLE Time)
{
	FMapTimeEntry* CurrentEntry = CreateExportTimePerClass.Find(Class);
	if(CurrentEntry)
	{
		CurrentEntry->Time += Time;
		CurrentEntry->ObjCount += 1;
	}
	else
	{
		CreateExportTimePerClass.Set(Class, FMapTimeEntry(Class, 1, Time));
	}
}

/** Add this time taken for object of class Class to have PostLoad called, to the stats we track. */
void FAsyncPackage::TrackPostLoadTimeForClass(const UClass* Class, DOUBLE Time)
{
	FMapTimeEntry* CurrentEntry = PostLoadTimePerClass.Find(Class);
	if(CurrentEntry)
	{
		CurrentEntry->Time += Time;
		CurrentEntry->ObjCount += 1;
	}
	else
	{
		PostLoadTimePerClass.Set(Class, FMapTimeEntry(Class, 1, Time));
	}
}
#endif // PERF_TRACK_DETAILED_ASYNC_STATS

/*-----------------------------------------------------------------------------
	UObject async (pre)loading.
-----------------------------------------------------------------------------*/

/**
 * Asynchronously load a package and all contained objects that match context flags. Non- blocking.
 *
 * @param	InPackageName		Name of package to load
 * @param	CompletionCallback	Callback called on completion of loading
 * @param	CallbackUserData	User data passed to callback
 * @param	PackageGuid			GUID of the package to load, or NULL for "don't care"
 * @param	PackageType			A type name associated with this package for later use
 */
void UObject::LoadPackageAsync( const FString& InPackageName, FAsyncCompletionCallback CompletionCallback, void* CallbackUserData, const FGuid* PackageGuid, FName PackageType )
{
	// The comments clearly state that it should be a package name but we also handle it being a filename as this function is not perf critical
	// and LoadPackage handles having a filename being passed in as well.
	FString PackageName = FFilename(InPackageName).GetBaseFilename();


	// Check whether the file is already in the queue.
	for( INT PackageIndex=0; PackageIndex<GObjAsyncPackages.Num(); PackageIndex++ )
	{
		FAsyncPackage& PendingPackage = GObjAsyncPackages(PackageIndex);
		if( PendingPackage.GetPackageName() == PackageName )
		{
			// make sure package has this completion callback
			if (CompletionCallback != NULL)
			{
				PendingPackage.AddCompletionCallback(FAsyncCompletionCallbackInfo(CompletionCallback, CallbackUserData));
			}
			// Early out as package is already being preloaded.
			return;
		}
	}
	// Add to (FIFO) queue.
	FAsyncPackage *Package = new(GObjAsyncPackages) FAsyncPackage(PackageName, PackageGuid, PackageType);
	if (CompletionCallback != NULL)
	{
		Package->AddCompletionCallback(FAsyncCompletionCallbackInfo(CompletionCallback, CallbackUserData));
	}
}

/**
 * Returns the async load percentage for a package in flight with the passed in name or -1 if there isn't one.
 *
 * @param	PackageName			Name of package to query load percentage for
 * @return	Async load percentage if package is currently being loaded, -1 otherwise
 */
FLOAT UObject::GetAsyncLoadPercentage( const FString& PackageName )
{
	FLOAT LoadPercentage = -1.f;
	for( INT PackageIndex=0; PackageIndex<GObjAsyncPackages.Num(); PackageIndex++ )
	{
		const FAsyncPackage& PendingPackage = GObjAsyncPackages(PackageIndex);
		if( PendingPackage.GetPackageName() == PackageName )
		{
			LoadPercentage = PendingPackage.GetLoadPercentage();
			break;
		}
	}
	return LoadPercentage;
}


/**
 * Blocks till all pending package/ linker requests are fulfilled.
 *
 * @param	ExcludeType					Do not flush packages associated with this specific type name
 */
void UObject::FlushAsyncLoading(FName ExcludeType/*=NAME_None*/)
{
	if( GObjAsyncPackages.Num() )
	{
		// Disallow low priority requests like texture streaming while we are flushing streaming
		// in order to avoid excessive seeking.
		FIOSystem* AsyncIO = GIOManager->GetIOSystem( IOSYSTEM_GenericAsync );
		if (AsyncIO)
		{
			AsyncIO->SetMinPriority( AIOP_Normal );
		}

		// Flush async loaders by not using a time limit. Needed for e.g. garbage collection.
#if XBOX
		XeControlHDDCaching( FALSE );
#endif
		debugf( NAME_Log, TEXT("Flushing async loaders.") );
		ProcessAsyncLoading( FALSE, 0, ExcludeType );
		debugf( NAME_Log, TEXT("Flushed async loaders.") );
#if XBOX
		XeControlHDDCaching( TRUE );
#endif

		if (ExcludeType == NAME_None)
		{
			// It's fine to have pending loads if we excluded some from the check
			check( !IsAsyncLoading() );
		}

		// Reset min priority again.
		AsyncIO->SetMinPriority( AIOP_MIN );
	}
}

/**
 * Returns whether we are currently async loading a package.
 *
 * @return TRUE if we are async loading a package, FALSE otherwise
 */
UBOOL UObject::IsAsyncLoading()
{
	return GObjAsyncPackages.Num() > 0 ? TRUE : FALSE;
}

DECLARE_CYCLE_STAT(TEXT("Async Loading Time"),STAT_AsyncLoadingTime,STATGROUP_StreamingDetails);


#if PERF_TRACK_DETAILED_ASYNC_STATS



IMPLEMENT_COMPARE_CONSTREF( FMapTimeEntry, UnAsyncLoading, { return A.Time < B.Time ? 1 : -1; } );

/** Utility that prints out a sorted list of entries in the supplied TMap;  The return value is the sum total of
    time spent in all entries */
DOUBLE PrintSortedListFromMap(TMap<const UClass*,FMapTimeEntry>& Map)
{
	// Array used to sort entries
	TArray<FMapTimeEntry> SortedEntries;

	// First we must convert TMap into array, so it can be sorted
	for (TMap<const UClass*, FMapTimeEntry>::TIterator It(Map); It; ++It)
	{
		SortedEntries.AddItem( It.Value() );
	}

	// if we actually have something..
	DOUBLE TotalElapsed = 0.0;
	if(SortedEntries.Num() > 0)
	{
		// ..sort the array
		Sort<USE_COMPARE_CONSTREF(FMapTimeEntry,UnAsyncLoading)>( &SortedEntries(0), SortedEntries.Num() );

		// ..and print it out
		for(INT EntryIdx=0; EntryIdx < SortedEntries.Num(); EntryIdx++)
		{
			TotalElapsed += SortedEntries(EntryIdx).Time;
			debugfSuppressed( NAME_DevStreaming, TEXT("   %6.3f ms : %4d : %s"), SortedEntries(EntryIdx).Time * 1000, SortedEntries(EntryIdx).ObjCount, *SortedEntries(EntryIdx).Class->GetName() );
		}
	}

	return TotalElapsed;
}
#endif // PERF_TRACK_DETAILED_ASYNC_STATS

/**
 * Serializes a bit of data each frame with a soft time limit. The function is designed to be able
 * to fully load a package in a single pass given sufficient time.
 *
 * @param	bUseTimeLimit	Whether to use a time limit
 * @param	TimeLimit		Soft limit of time this function is allowed to consume
 * @param	ExcludeType		Do not process packages associated with this specific type name
 */
void UObject::ProcessAsyncLoading( UBOOL bUseTimeLimit, FLOAT TimeLimit, FName ExcludeType )
{
	SCOPE_CYCLE_COUNTER(STAT_AsyncLoadingTime);
	// Whether to continue execution.
	UBOOL bExecuteNextStep = TRUE;

	// We need to loop as the function has to handle finish loading everything given no time limit
	// like e.g. when called from FlushAsyncLoading.
	for (INT i = 0; bExecuteNextStep && i < GObjAsyncPackages.Num(); i++)
	{
		// Package to be loaded.
		FAsyncPackage& Package = GObjAsyncPackages(i);

		if (ExcludeType != NAME_None && ExcludeType == Package.GetPackageType())
		{
			// We should skip packages of this type
			continue;
		}

		// Package tick returns TRUE on completion.
		bExecuteNextStep = Package.Tick( bUseTimeLimit, TimeLimit );
		if( bExecuteNextStep )
		{
#if PERF_TRACK_DETAILED_ASYNC_STATS
			DOUBLE LoadTime = appSeconds() - Package.GetLoadStartTime();
			debugfSuppressed( NAME_DevStreaming, TEXT("Detailed async load stats for package '%s', finished loading in %5.2f seconds"), *Package.GetPackageName(), LoadTime );
			debugfSuppressed( NAME_DevStreaming, TEXT("Tick             : %6.2f ms [%3i, %3i iterations]"), Package.TickTime * 1000, Package.TickCount, Package.TickLoopCount );
			debugfSuppressed( NAME_DevStreaming, TEXT("CreateLinker     : %6.2f ms [%3i iterations]"), Package.CreateLinkerTime    * 1000, Package.CreateLinkerCount    );
			debugfSuppressed( NAME_DevStreaming, TEXT("FinishLinker     : %6.2f ms [%3i iterations]"), Package.FinishLinkerTime    * 1000, Package.FinishLinkerCount    );
			debugfSuppressed( NAME_DevStreaming, TEXT("CreateImports    : %6.2f ms [%3i iterations]"), Package.CreateImportsTime	 * 1000, Package.CreateImportsCount   );

			debugfSuppressed( NAME_DevStreaming, TEXT("CreateExports    : %6.2f ms [%3i iterations]"), Package.CreateExportsTime   * 1000, Package.CreateExportsCount   );
			DOUBLE AccountedForExportTime = PrintSortedListFromMap(Package.CreateExportTimePerClass);
			debugfSuppressed( NAME_DevStreaming, TEXT("   %6.3f ms :      : %s"), (Package.CreateExportsTime - AccountedForExportTime) * 1000, TEXT("Waiting for Precache") );

			debugfSuppressed( NAME_DevStreaming, TEXT("PreLoadObjects   : %6.2f ms [%3i iterations]"), Package.PreLoadObjectsTime  * 1000, Package.PreLoadObjectsCount  );
			debugfSuppressed( NAME_DevStreaming, TEXT("FinishExportGuids: %6.2f ms [%3i iterations]"), Package.FinishExportGuidsTime  * 1000, 1  );

			debugfSuppressed( NAME_DevStreaming, TEXT("PostLoadObjects  : %6.2f ms [%3i iterations]"), Package.PostLoadObjectsTime * 1000, Package.PostLoadObjectsCount );
			PrintSortedListFromMap(Package.PostLoadTimePerClass);

			debugfSuppressed( NAME_DevStreaming, TEXT("FinishObjects    : %6.2f ms [%3i iterations]"), Package.FinishObjectsTime   * 1000, Package.FinishObjectsCount   );
#endif
	
			if( GUseSeekFreeLoading )
			{
				// Emulates UObject::ResetLoaders on the package linker's linkerroot.
				Package.ResetLoader();
			}

			// We're done so we can remove the package now. @warning invalidates local Package variable!.
			GObjAsyncPackages.Remove( i );

			// Need to process this index again as we just removed an item
			i--;
		}

		// We cannot access Package anymore!
	}
}


/*-----------------------------------------------------------------------------
	FAsyncIOSystemBase implementation.
-----------------------------------------------------------------------------*/

#if FLASH
#define BLOCK_ON_ASYNCIO 1
#else
#define BLOCK_ON_ASYNCIO 0
#endif

UBOOL GbLogAsyncLoading = FALSE;

/**
 * Packs IO request into a FAsyncIORequest and queues it in OutstandingRequests array
 *
 * @param	FileName			File name associated with request
 * @param	Offset				Offset in bytes from beginning of file to start reading from
 * @param	Size				Number of bytes to read
 * @param	UncompressedSize	Uncompressed size in bytes of original data, 0 if data is not compressed on disc	
 * @param	Dest				Pointer holding to be read data
 * @param	CompressionFlags	Flags controlling data decompression
 * @param	Counter				Thread safe counter associated with this request; will be decremented when fulfilled
 * @param	Priority			Priority of request
 * 
 * @return	unique ID for request
 */
QWORD FAsyncIOSystemBase::QueueIORequest( 
	const FString& FileName, 
	INT Offset, 
	INT Size, 
	INT UncompressedSize, 
	void* Dest, 
	ECompressionFlags CompressionFlags, 
	FThreadSafeCounter* Counter,
	EAsyncIOPriority Priority )
{
	FScopeLock ScopeLock( CriticalSection );
	check( Offset != INDEX_NONE );

	// Create an IO request containing passed in information.
	FAsyncIORequest IORequest;
	IORequest.RequestIndex				= RequestIndex++;
	IORequest.FileSortKey				= INDEX_NONE;
	IORequest.FileName					= FileName;
	IORequest.Offset					= Offset;
	IORequest.Size						= Size;
	IORequest.UncompressedSize			= UncompressedSize;
	IORequest.Dest						= Dest;
	IORequest.CompressionFlags			= CompressionFlags;
	IORequest.Counter					= Counter;
	IORequest.Priority					= Priority;

	if (GbLogAsyncLoading == TRUE)
	{
		LogIORequest(TEXT("QueueIORequest"), IORequest);
	}

	INC_DWORD_STAT( STAT_AsyncIO_OutstandingReadCount );
	INC_DWORD_STAT_BY( STAT_AsyncIO_OutstandingReadSize, IORequest.Size );

	// Add to end of queue.
	OutstandingRequests.AddItem( IORequest );

#if FLASH
	// No threads, we service immediately for now.
	ServiceRequestsSynchronously();
#else
	// Trigger event telling IO thread to wake up to perform work.
	OutstandingRequestsEvent->Trigger();
#endif

	// Return unique ID associated with request which can be used to cancel it.
	return IORequest.RequestIndex;
}

/**
 * Adds a destroy handle request top the OutstandingRequests array
 * 
 * @param	FileName			File name associated with request
 *
 * @return	unique ID for request
 */
QWORD FAsyncIOSystemBase::QueueDestroyHandleRequest(const FString& FileName)
{
	FScopeLock ScopeLock( CriticalSection );
	FAsyncIORequest IORequest;
	IORequest.RequestIndex				= RequestIndex++;
	IORequest.FileName					= FileName;
	IORequest.Priority					= AIOP_MAX;
	IORequest.bIsDestroyHandleRequest	= TRUE;

	if (GbLogAsyncLoading == TRUE)
	{
		LogIORequest(TEXT("QueueDestroyHandleRequest"), IORequest);
	}

	// Add to end of queue.
	OutstandingRequests.AddItem( IORequest );

	// Trigger event telling IO thread to wake up to perform work.
	OutstandingRequestsEvent->Trigger();

	// Return unique ID associated with request which can be used to cancel it.
	return IORequest.RequestIndex;
}

/** 
 *	Logs out the given file IO information w/ the given message.
 *	
 *	@param	Message		The message to prepend
 *	@param	IORequest	The IORequest to log
 */
void FAsyncIOSystemBase::LogIORequest(const FString& Message, const FAsyncIORequest& IORequest)
{
	FString OutputStr = FString::Printf(TEXT("ASYNC: %32s: %s\n"), *Message, *(IORequest.ToString()));
	appOutputDebugString(*OutputStr);
}

/** 
 * Implements shared stats handling and passes read to PlatformReadDoNotCallDirectly
 *
 * @param	FileHandle	Platform specific file handle
 * @param	Offset		Offset in bytes from start, INDEX_NONE if file pointer shouldn't be changed
 * @param	Size		Size in bytes to read at current position from passed in file handle
 * @param	Dest		Pointer to data to read into
 *
 * @return	TRUE if read was successful, FALSE otherwise
 */	
UBOOL FAsyncIOSystemBase::InternalRead( FAsyncIOHandle FileHandle, INT Offset, INT Size, void* Dest )
{
	FScopeLock ScopeLock( ExclusiveReadCriticalSection );

	UBOOL bRetVal = FALSE;
	
	STAT(DOUBLE ReadTime = 0);
	{	
		SCOPE_SECONDS_COUNTER(ReadTime);
		PlatformReadDoNotCallDirectly( FileHandle, Offset, Size, Dest );
	}	
	INC_FLOAT_STAT_BY(STAT_AsyncIO_PlatformReadTime,(FLOAT)ReadTime);

	// The platform might actually read more than Size due to aligning and internal min read sizes
	// though we only really care about throttling requested bandwidth as it's not very accurate
	// to begin with.
	STAT(ConstrainBandwidth(Size, ReadTime));

	return bRetVal;
}

/**
 * This is made platform specific to allow ordering of read requests based on layout of files
 * on the physical media. The base implementation is FIFO while taking priority into account
 *
 * This function is being called while there is a scope lock on the critical section so it
 * needs to be fast in order to not block QueueIORequest and the likes.
 *
 * @return	index of next to be fulfilled request or INDEX_NONE if there is none
 */
INT FAsyncIOSystemBase::PlatformGetNextRequestIndex()
{
	// Find first index of highest priority request level. Basically FIFO per priority.
	INT HighestPriorityIndex = INDEX_NONE;
	EAsyncIOPriority HighestPriority = AIOP_MIN;
	for( INT CurrentRequestIndex=0; CurrentRequestIndex<OutstandingRequests.Num(); CurrentRequestIndex++ )
	{
		// Calling code already entered critical section so we can access OutstandingRequests.
		const FAsyncIORequest& IORequest = OutstandingRequests(CurrentRequestIndex);
		if( IORequest.Priority > HighestPriority )
		{
			HighestPriority = IORequest.Priority;
			HighestPriorityIndex = CurrentRequestIndex;
		}
	}
	return HighestPriorityIndex;
}


// If enabled allows tracking down crashes in decompression as it avoids using the async work queue.
#define BLOCK_ON_DECOMPRESSION 0

/**
 * Fulfills a compressed read request in a blocking fashion by falling back to using
 * PlatformSeek and various PlatformReads in combination with FAsyncUncompress to allow
 * decompression while we are waiting for file I/O.
 *
 * @note: the way this code works needs to be in line with FArchive::SerializeCompressed
 *
 * @param	IORequest	IO requewst to fulfill
 * @param	FileHandle	File handle to use
 */
void FAsyncIOSystemBase::FulfillCompressedRead( const FAsyncIORequest& IORequest, const FAsyncIOHandle& FileHandle )
{
	if (GbLogAsyncLoading == TRUE)
	{
		LogIORequest(TEXT("FulfillCompressedRead"), IORequest);
	}

	// Initialize variables.
	FAsyncUncompress*		Uncompressor			= NULL;
	BYTE*					UncompressedBuffer		= (BYTE*) IORequest.Dest;
	// First compression chunk contains information about total size so we skip that one.
	INT						CurrentChunkIndex		= 1;
	INT						CurrentBufferIndex		= 0;
	UBOOL					bHasProcessedAllData	= FALSE;

	// read the first two ints, which will contain the magic bytes (to detect byteswapping)
	// and the original size the chunks were compressed from
	INT						HeaderData[2];
	INT						HeaderSize = sizeof(HeaderData);

	InternalRead(FileHandle, IORequest.Offset, HeaderSize, HeaderData);
	RETURN_IF_EXIT_REQUESTED;

	// if the magic bytes don't match, then we are byteswapped (or corrupted)
	UBOOL bIsByteswapped = HeaderData[0] != PACKAGE_FILE_TAG;
	// if its potentially byteswapped, make sure it's not just corrupted
	if (bIsByteswapped)
	{
		// if it doesn't equal the swapped version, then data is corrupted
		if (HeaderData[0] != PACKAGE_FILE_TAG_SWAPPED)
		{
			warnf(NAME_Warning, TEXT("Detected data corruption [header] trying to read %i bytes at offset %i from '%s'. Please delete file and recook."),
				IORequest.UncompressedSize, 
				IORequest.Offset ,
				*IORequest.FileName );
			check(0);
			appHandleIOFailure(*IORequest.FileName);
		}
		// otherwise, we have a valid byteswapped file, so swap the chunk size
		else
		{
			HeaderData[1] = BYTESWAP_ORDER32(HeaderData[1]);
		}
	}

	INT						CompressionChunkSize	= HeaderData[1];
	
	// handle old packages that don't have the chunk size in the header, in which case
	// we can use the old hardcoded size
	if (CompressionChunkSize == PACKAGE_FILE_TAG)
	{
		CompressionChunkSize = LOADING_COMPRESSION_CHUNK_SIZE;
	}

	// calculate the number of chunks based on the size they were compressed from
	INT						TotalChunkCount = (IORequest.UncompressedSize + CompressionChunkSize - 1) / CompressionChunkSize + 1;

	// allocate chunk info data based on number of chunks
	FCompressedChunkInfo*	CompressionChunks		= (FCompressedChunkInfo*)PlatformAllocateIOBuffer(sizeof(FCompressedChunkInfo) * TotalChunkCount);
	INT						ChunkInfoSize			= (TotalChunkCount) * sizeof(FCompressedChunkInfo);
	void*					CompressedBuffer[2]		= { 0, 0 };
	
	// Read table of compression chunks after seeking to offset (after the initial header data)
	InternalRead( FileHandle, IORequest.Offset + HeaderSize, ChunkInfoSize, CompressionChunks );
	RETURN_IF_EXIT_REQUESTED;

	// Handle byte swapping. This is required for opening a cooked file on the PC.
	INT CalculatedUncompressedSize = 0;
	if (bIsByteswapped)
	{
		for( INT ChunkIndex=0; ChunkIndex<TotalChunkCount; ChunkIndex++ )
		{
			CompressionChunks[ChunkIndex].CompressedSize	= BYTESWAP_ORDER32(CompressionChunks[ChunkIndex].CompressedSize);
			CompressionChunks[ChunkIndex].UncompressedSize	= BYTESWAP_ORDER32(CompressionChunks[ChunkIndex].UncompressedSize);
			if (ChunkIndex > 0)
			{
				CalculatedUncompressedSize += CompressionChunks[ChunkIndex].UncompressedSize;
			}
		}
	}
	else
	{
		for( INT ChunkIndex=1; ChunkIndex<TotalChunkCount; ChunkIndex++ )
		{
			CalculatedUncompressedSize += CompressionChunks[ChunkIndex].UncompressedSize;
		}
	}

	if (CompressionChunks[0].UncompressedSize != CalculatedUncompressedSize)
	{
		warnf(NAME_Warning, TEXT("Detected data corruption [incorrect uncompressed size] calculated %i bytes, requested %i bytes at offset %i from '%s'. Please delete file and recook."),
			CalculatedUncompressedSize,
			IORequest.UncompressedSize, 
			IORequest.Offset ,
			*IORequest.FileName );
		check(0);
		appHandleIOFailure(*IORequest.FileName);
	}

	if (ChunkInfoSize + HeaderSize + CompressionChunks[0].CompressedSize > IORequest.Size )
	{
		warnf(NAME_Warning, TEXT("Detected data corruption [undershoot] trying to read %i bytes at offset %i from '%s'. Please delete file and recook."),
			IORequest.UncompressedSize, 
			IORequest.Offset ,
			*IORequest.FileName );
		check(0);
		appHandleIOFailure(*IORequest.FileName);
	}

	if (IORequest.UncompressedSize != CalculatedUncompressedSize)
	{
		warnf(NAME_Warning, TEXT("Detected data corruption [incorrect uncompressed size] calculated %i bytes, requested %i bytes at offset %i from '%s'. Please delete file and recook."),
			CalculatedUncompressedSize,
			IORequest.UncompressedSize, 
			IORequest.Offset ,
			*IORequest.FileName );
		check(0);
		appHandleIOFailure(*IORequest.FileName);
	}

	// Figure out maximum size of compressed data chunk.
	INT MaxCompressedSize = 0;
	for (INT ChunkIndex = 1; ChunkIndex < TotalChunkCount; ChunkIndex++)
	{
		MaxCompressedSize = Max(MaxCompressedSize, CompressionChunks[ChunkIndex].CompressedSize);
		// Verify the all chunks are 'full size' until the last one...
		if (CompressionChunks[ChunkIndex].UncompressedSize < CompressionChunkSize)
		{
			if (ChunkIndex != (TotalChunkCount - 1))
			{
				checkf(0, TEXT("Calculated too many chunks: %d should be last, there are %d from '%s'"), ChunkIndex, TotalChunkCount, *IORequest.FileName);
			}
		}
		check( CompressionChunks[ChunkIndex].UncompressedSize <= CompressionChunkSize );
	}

	INT Padding = 0;
#if PS3
	// This makes sure that there is a full valid cache line (and DMA read alignment) after the end
	// of the compressed data, so that the decompression function can safely over-read the source data.
	Padding = 128;
#endif

	// Allocate memory for compressed data.
	CompressedBuffer[0]	= PlatformAllocateIOBuffer( MaxCompressedSize + Padding );
	CompressedBuffer[1] = PlatformAllocateIOBuffer( MaxCompressedSize + Padding );

	// Initial read request.
	InternalRead( FileHandle, INDEX_NONE, CompressionChunks[CurrentChunkIndex].CompressedSize, CompressedBuffer[CurrentBufferIndex] );
	RETURN_IF_EXIT_REQUESTED;

	// Loop till we're done decompressing all data.
	while( !bHasProcessedAllData )
	{
		FAsyncTask<FAsyncUncompress> UncompressTask(
			IORequest.CompressionFlags,
			UncompressedBuffer,
			CompressionChunks[CurrentChunkIndex].UncompressedSize,
			CompressedBuffer[CurrentBufferIndex],
			CompressionChunks[CurrentChunkIndex].CompressedSize,
			(Padding > 0) ? TRUE : FALSE
			);

#if BLOCK_ON_DECOMPRESSION
		UncompressTask.StartSynchronousTask();
#else
		UncompressTask.StartBackgroundTask();
#endif

		// Advance destination pointer.
		UncompressedBuffer += CompressionChunks[CurrentChunkIndex].UncompressedSize;
	
		// Check whether we are already done reading.
		if( CurrentChunkIndex < TotalChunkCount-1 )
		{
			// Can't postincrement in if statement as we need it to remain at valid value for one more loop iteration to finish
		// the decompression.
			CurrentChunkIndex++;
			// Swap compression buffers to read into.
			CurrentBufferIndex = 1 - CurrentBufferIndex;
			// Read more data.
			InternalRead( FileHandle, INDEX_NONE, CompressionChunks[CurrentChunkIndex].CompressedSize, CompressedBuffer[CurrentBufferIndex] );
			RETURN_IF_EXIT_REQUESTED;
		}
		// We were already done reading the last time around so we are done processing now.
		else
		{
			bHasProcessedAllData = TRUE;
		}
		
		//@todo async loading: should use event for this
		STAT(DOUBLE UncompressorWaitTime = 0);
		{
			SCOPE_SECONDS_COUNTER(UncompressorWaitTime);
			UncompressTask.EnsureCompletion(); // just decompress on this thread if it isn't started yet
		}
		INC_FLOAT_STAT_BY(STAT_AsyncIO_UncompressorWaitTime,(FLOAT)UncompressorWaitTime);
	}

	PlatformDeallocateIOBuffer(CompressionChunks);
	PlatformDeallocateIOBuffer(CompressedBuffer[0]);
	PlatformDeallocateIOBuffer(CompressedBuffer[1] );
}

/**
 * Retrieves cached file handle or caches it if it hasn't been already
 *
 * @param	FileName	file name to retrieve cached handle for
 * @return	cached file handle
 */
FAsyncIOHandle FAsyncIOSystemBase::GetCachedFileHandle( const FString& FileName )
{
	// We can't make any assumptions about NULL being an invalid handle value so we need to use the indirection.
	FAsyncIOHandle*	FileHandlePtr = FindCachedFileHandle( FileName );
	FAsyncIOHandle FileHandle;

	// We have an already cached handle, let's use it.
	if( FileHandlePtr )
	{
		FileHandle = *FileHandlePtr;
	}
	// The filename doesn't have a handle associated with it.
	else
	{
		// So let the platform specific code create one.
		FileHandle = PlatformCreateHandle( *FileName );
		// Make sure it's valid before caching and using it.
		if( PlatformIsHandleValid( FileHandle ) )
		{
			NameToHandleMap.Set( *FileName, FileHandle );
		}
	}

	return FileHandle;
}

/**
 * Returns cached file handle if found, or NULL if not. This function does
 * NOT create any file handles and therefore is not blocking.
 *
 * @param	FileName	file name to retrieve cached handle for
 * @return	cached file handle, NULL if not cached
 */
FAsyncIOHandle* FAsyncIOSystemBase::FindCachedFileHandle( const FString& FileName )
{
	return NameToHandleMap.Find( FileName );
}

/**
 * Requests data to be loaded async. Returns immediately.
 *
 * @param	FileName	File name to load
 * @param	Offset		Offset into file
 * @param	Size		Size of load request
 * @param	Dest		Pointer to load data into
 * @param	Counter		Thread safe counter to decrement when loading has finished
 * @param	Priority	Priority of request
 *
 * @return Returns an index to the request that can be used for canceling or 0 if the request failed.
 */
QWORD FAsyncIOSystemBase::LoadData( 
	const FString& FileName, 
	INT Offset, 
	INT Size, 
	void* Dest, 
	FThreadSafeCounter* Counter,
	EAsyncIOPriority Priority )
{
	QWORD TheRequestIndex;
	{
		TheRequestIndex = QueueIORequest( FileName, Offset, Size, 0, Dest, COMPRESS_None, Counter, Priority );
	}
#if BLOCK_ON_ASYNCIO
	BlockTillAllRequestsFinished(); 
#endif
	return TheRequestIndex;
}

/**
 * Requests compressed data to be loaded async. Returns immediately.
 *
 * @param	FileName			File name to load
 * @param	Offset				Offset into file
 * @param	Size				Size of load request
 * @param	UncompressedSize	Size of uncompressed data
 * @param	Dest				Pointer to load data into
 * @param	CompressionFlags	Flags controlling data decompression
 * @param	Counter				Thread safe counter to decrement when loading has finished, can be NULL
 * @param	Priority			Priority of request
 *
 * @return Returns an index to the request that can be used for canceling or 0 if the request failed.
 */
QWORD FAsyncIOSystemBase::LoadCompressedData( 
	const FString& FileName, 
	INT Offset, 
	INT Size, 
	INT UncompressedSize, 
	void* Dest, 
	ECompressionFlags CompressionFlags, 
	FThreadSafeCounter* Counter,
	EAsyncIOPriority Priority )
{
	QWORD TheRequestIndex;
	{
		TheRequestIndex = QueueIORequest( FileName, Offset, Size, UncompressedSize, Dest, CompressionFlags, Counter, Priority );
	}
#if BLOCK_ON_ASYNCIO
	BlockTillAllRequestsFinished(); 
#endif
	return TheRequestIndex;
}

/**
 * Removes N outstanding requests from the queue and returns how many were canceled. We can't cancel
 * requests currently fulfilled and ones that have already been fulfilled.
 *
 * @param	RequestIndices	Indices of requests to cancel.
 * @return	The number of requests that were canceled
 */
INT FAsyncIOSystemBase::CancelRequests( QWORD* RequestIndices, INT NumIndices )
{
	FScopeLock ScopeLock( CriticalSection );

	// Iterate over all outstanding requests and cancel matching ones.
	INT RequestsCanceled = 0;
	for( INT OutstandingIndex=OutstandingRequests.Num()-1; OutstandingIndex>=0 && RequestsCanceled<NumIndices; OutstandingIndex-- )
	{
		// Iterate over all indices of requests to cancel
		for( INT TheRequestIndex=0; TheRequestIndex<NumIndices; TheRequestIndex++ )
		{
			// Look for matching request index in queue.
			const FAsyncIORequest IORequest = OutstandingRequests(OutstandingIndex);
			if( IORequest.RequestIndex == RequestIndices[TheRequestIndex] )
			{
				INC_DWORD_STAT( STAT_AsyncIO_CanceledReadCount );
				INC_DWORD_STAT_BY( STAT_AsyncIO_CanceledReadSize, IORequest.Size );
				DEC_DWORD_STAT( STAT_AsyncIO_OutstandingReadCount );
				DEC_DWORD_STAT_BY( STAT_AsyncIO_OutstandingReadSize, IORequest.Size );				
				// Decrement thread-safe counter to indicate that request has been "completed".
				IORequest.Counter->Decrement();
				// IORequest variable no longer valid after removal.
				OutstandingRequests.Remove( OutstandingIndex );
				RequestsCanceled++;
				// Break out of loop as we've modified OutstandingRequests AND it no longer is valid.
				break;
			}
		}
	}
	return RequestsCanceled;
}

/**
 * Removes all outstanding requests from the queue
 */
void FAsyncIOSystemBase::CancelAllOutstandingRequests()
{
	FScopeLock ScopeLock( CriticalSection );

	// simply toss all outstanding requests - the critical section will guarantee we aren't removing
	// while using elsewhere
	OutstandingRequests.Empty();
}

/**
 * Constrains bandwidth to value set in .ini
 *
 * @param BytesRead		Number of bytes read
 * @param ReadTime		Time it took to read
 */
void FAsyncIOSystemBase::ConstrainBandwidth( INT BytesRead, FLOAT ReadTime )
{
	// Constrain bandwidth if wanted. Value is in MByte/ sec.
	if( GSys->AsyncIOBandwidthLimit > 0 )
	{
		// Figure out how long to wait to throttle bandwidth.
		FLOAT WaitTime = BytesRead / (GSys->AsyncIOBandwidthLimit * 1024.f * 1024.f) - ReadTime;
		// Only wait if there is something worth waiting for.
		if( WaitTime > 0 )
		{
			// Time in seconds to wait.
			appSleep(WaitTime);
		}
	}
}

/**
 * Initializes critical section, event and other used variables. 
 *
 * This is called in the context of the thread object that aggregates 
 * this, not the thread that passes this runnable to a new thread.
 *
 * @return True if initialization was successful, false otherwise
 */
UBOOL FAsyncIOSystemBase::Init()
{
	CriticalSection				= GSynchronizeFactory->CreateCriticalSection();
	ExclusiveReadCriticalSection= GSynchronizeFactory->CreateCriticalSection();
	OutstandingRequestsEvent	= GSynchronizeFactory->CreateSynchEvent();
	RequestIndex				= 1;
	MinPriority					= AIOP_MIN;
	IsRunning.Increment();
	return TRUE;
}

/**
 * This is called if a thread is requested to suspend its' IO activity
 */
void FAsyncIOSystemBase::Suspend()
{
	SuspendCount.Increment();
	ExclusiveReadCriticalSection->Lock();
}

/**
 * This is called if a thread is requested to resume its' IO activity
 */
void FAsyncIOSystemBase::Resume()
{
	ExclusiveReadCriticalSection->Unlock();
	SuspendCount.Decrement();
}

/**
 * Sets the min priority of requests to fulfill. Lower priority requests will still be queued and
 * start to be fulfilled once the min priority is lowered sufficiently. This is only a hint and
 * implementations are free to ignore it.
 *
 * @param	InMinPriority		Min priority of requests to fulfill
 */
void FAsyncIOSystemBase::SetMinPriority( EAsyncIOPriority InMinPriority )
{
	FScopeLock ScopeLock( CriticalSection );
	// Trigger event telling IO thread to wake up to perform work if we are lowering the min priority.
	if( InMinPriority < MinPriority )
	{
		OutstandingRequestsEvent->Trigger();
	}
	// Update priority.
	MinPriority = InMinPriority;
}

/**
 * Give the IO system a hint that it is done with the file for now
 *
 * @param Filename File that was being async loaded from, but no longer is
 */
void FAsyncIOSystemBase::HintDoneWithFile(const FString& Filename)
{
	// let the platform handle it
	PlatformHandleHintDoneWithFile(Filename);
}


/**
 * Called in the context of the aggregating thread to perform cleanup.
 */
void FAsyncIOSystemBase::Exit()
{
	FlushHandles();
	GSynchronizeFactory->Destroy( CriticalSection );
	GSynchronizeFactory->Destroy( OutstandingRequestsEvent );
}

/**
 * This is called if a thread is requested to terminate early
 */
void FAsyncIOSystemBase::Stop()
{
	// Tell the thread to quit.
	IsRunning.Decrement();

	// Make sure that the thread awakens even if there is no work currently outstanding.
	OutstandingRequestsEvent->Trigger();
}

/**
 * This is where all the actual loading is done. This is only called
 * if the initialization was successful.
 *
 * @return always 0
 */
DWORD FAsyncIOSystemBase::Run()
{
	// IsRunning gets decremented by Stop.
	while( IsRunning.GetValue() > 0 )
	{
		// Sit and spin if requested, unless we are shutting down, in which case make sure we don't deadlock.
		while( !GIsRequestingExit && SuspendCount.GetValue() > 0 )
		{
			appSleep(0.005);
		}

		// Create file handles.
		{
			TArray<FString> FileNamesToCacheHandles; 
			// Only enter critical section for copying existing array over. We don't operate on the 
			// real array as creating file handles might take a while and we don't want to have other
			// threads stalling on submission of requests.
			{
				FScopeLock ScopeLock( CriticalSection );

				for( INT RequestIndex=0; RequestIndex<OutstandingRequests.Num(); RequestIndex++ )
				{
					// Early outs avoid unnecessary work and string copies with implicit allocator churn.
					FAsyncIORequest& OutstandingRequest = OutstandingRequests(RequestIndex);
					if( OutstandingRequest.bHasAlreadyRequestedHandleToBeCached == FALSE
					&&	OutstandingRequest.bIsDestroyHandleRequest == FALSE 
					&&	FindCachedFileHandle( OutstandingRequest.FileName ) == NULL )
					{
						new(FileNamesToCacheHandles)FString(*OutstandingRequest.FileName);
						OutstandingRequest.bHasAlreadyRequestedHandleToBeCached = TRUE;
					}
				}
			}
			// Create file handles for requests down the pipe. This is done here so we can later on
			// use the handles to figure out the sort keys.
			for( INT FileNameIndex=0; FileNameIndex<FileNamesToCacheHandles.Num(); FileNameIndex++ )
			{
				GetCachedFileHandle( FileNamesToCacheHandles(FileNameIndex) );
			}
		}

		// Copy of request.
		FAsyncIORequest IORequest;
		UBOOL			bIsRequestPending	= FALSE;
		{
			FScopeLock ScopeLock( CriticalSection );
			if( OutstandingRequests.Num() )
			{
				// Gets next request index based on platform specific criteria like layout on disc.
				INT TheRequestIndex = PlatformGetNextRequestIndex();
				if( TheRequestIndex != INDEX_NONE )
				{					
					// We need to copy as we're going to remove it...
					IORequest = OutstandingRequests( TheRequestIndex );
					// ...right here.
					// NOTE: this needs to be a Remove, not a RemoveSwap because the base implementation
					// of PlatformGetNextRequestIndex is a FIFO taking priority into account
					OutstandingRequests.Remove( TheRequestIndex );		
					// We're busy. Updated inside scoped lock to ensure BlockTillAllRequestsFinished works correctly.
					BusyWithRequest.Increment();
					bIsRequestPending = TRUE;
				}
			}
		}

		// We only have work to do if there's a request pending.
		if( bIsRequestPending )
		{
			// handle a destroy handle request from the queue
			if( IORequest.bIsDestroyHandleRequest )
			{
				FAsyncIOHandle*	FileHandlePtr = FindCachedFileHandle( IORequest.FileName );
				if( FileHandlePtr )
				{
					// destroy and remove the handle
					PlatformDestroyHandle(*FileHandlePtr);
					NameToHandleMap.Remove(IORequest.FileName);
				}
			}
			else
			{
				// Retrieve cached handle or create it if it wasn't cached. We purposefully don't look at currently
				// set value as it might be stale by now.
				FAsyncIOHandle FileHandle = GetCachedFileHandle( IORequest.FileName );
				if( PlatformIsHandleValid(FileHandle) )
				{
					if( IORequest.UncompressedSize )
					{
						// Data is compressed on disc so we need to also decompress.
						FulfillCompressedRead( IORequest, FileHandle );
					}
					else
					{
						// Read data after seeking.
						InternalRead( FileHandle, IORequest.Offset, IORequest.Size, IORequest.Dest );
					}
					INC_DWORD_STAT( STAT_AsyncIO_FulfilledReadCount );
					INC_DWORD_STAT_BY( STAT_AsyncIO_FulfilledReadSize, IORequest.Size );
				}
				else
				{
					//@todo streaming: add warning once we have thread safe logging.
				}

				DEC_DWORD_STAT( STAT_AsyncIO_OutstandingReadCount );
				DEC_DWORD_STAT_BY( STAT_AsyncIO_OutstandingReadSize, IORequest.Size );
			}

			// Request fulfilled.
			if( IORequest.Counter )
			{
				IORequest.Counter->Decrement(); 
			}
			// We're done reading for now.
			BusyWithRequest.Decrement();	
		}
		else
		{
			if( !OutstandingRequests.Num() )
			{
				// We're really out of requests now, wait till the calling thread signals further work
				OutstandingRequestsEvent->Wait();
			}
		}
		RETURN_VAL_IF_EXIT_REQUESTED(0);
	}
	return 0;
}

#if FLASH
/**
 * This is where all the actual loading is done. This is only called
 * if the initializationation was successful.
 */
void FAsyncIOSystemBase::ServiceRequestsSynchronously()
{
	while(OutstandingRequests.Num())
	{
		TArray<FString> FileNamesToCacheHandles; 
		{
			for( INT RequestIndex=0; RequestIndex<OutstandingRequests.Num(); RequestIndex++ )
			{
				// Early outs avoid unnecessary work and string copies with implicit allocator churn.
				FAsyncIORequest& OutstandingRequest = OutstandingRequests(RequestIndex);
				if( OutstandingRequest.bHasAlreadyRequestedHandleToBeCached == FALSE
				&&	OutstandingRequest.bIsDestroyHandleRequest == FALSE 
				&&	FindCachedFileHandle( OutstandingRequest.FileName ) == NULL )
				{
					new(FileNamesToCacheHandles)FString(*OutstandingRequest.FileName);
					OutstandingRequest.bHasAlreadyRequestedHandleToBeCached = TRUE;
				}
			}
		}
		// Create file handles for requests down the pipe. This is done here so we can later on
		// use the handles to figure out the sort keys.
		for( INT FileNameIndex=0; FileNameIndex<FileNamesToCacheHandles.Num(); FileNameIndex++ )
		{
			GetCachedFileHandle( FileNamesToCacheHandles(FileNameIndex) );
		}


		// Copy of request.
		FAsyncIORequest IORequest;
		UBOOL			bIsRequestPending	= FALSE;
		{
			if( OutstandingRequests.Num() )
			{
				// Gets next request index based on platform specific criteria like layout on disc.
				INT TheRequestIndex = PlatformGetNextRequestIndex();
				if( TheRequestIndex != INDEX_NONE )
				{					
					// We need to copy as we're going to remove it...
					IORequest = OutstandingRequests( TheRequestIndex );
					// ...right here.
					OutstandingRequests.Remove( TheRequestIndex );	
					bIsRequestPending = TRUE;	
				}
			}
		}

		// We only have work to do if there's a request pending.
		if( bIsRequestPending )
		{
			// handle a destroy handle request from the queue
			if( IORequest.bIsDestroyHandleRequest )
			{
				FAsyncIOHandle*	FileHandlePtr = FindCachedFileHandle( IORequest.FileName );
				if( FileHandlePtr )
				{
					// destroy and remove the handle
					PlatformDestroyHandle(*FileHandlePtr);
					NameToHandleMap.Remove(IORequest.FileName);
				}
			}
			else
			{
				// Retrieve cached handle or create it if it wasn't cached. We purposefully don't look at currently
				// set value as it might be stale by now.
				FAsyncIOHandle FileHandle = GetCachedFileHandle( IORequest.FileName );
				if( PlatformIsHandleValid(FileHandle) )
				{
					if( IORequest.UncompressedSize )
					{
						// Data is compressed on disc so we need to also decompress.
						FulfillCompressedRead( IORequest, FileHandle );
					}
					else
					{
						// Read data after seeking.
						InternalRead( FileHandle, IORequest.Offset, IORequest.Size, IORequest.Dest );
					}
					INC_DWORD_STAT( STAT_AsyncIO_FulfilledReadCount );
					INC_DWORD_STAT_BY( STAT_AsyncIO_FulfilledReadSize, IORequest.Size );
				}
				else
				{
					//@todo streaming: add warning once we have thread safe logging.
				}

				DEC_DWORD_STAT( STAT_AsyncIO_OutstandingReadCount );
				DEC_DWORD_STAT_BY( STAT_AsyncIO_OutstandingReadSize, IORequest.Size );
			}

			// Request fulfilled.
			if( IORequest.Counter )
			{
				IORequest.Counter->Decrement(); 
			}
	
		}
	}
}
#endif // FLASH

/**
 * Blocks till all requests are finished.
 *
 * @todo streaming: this needs to adjusted to signal the thread to not accept any new requests from other 
 * @todo streaming: threads while we are blocking till all requests are finished.
 */
void FAsyncIOSystemBase::BlockTillAllRequestsFinished()
{
	// Block till all requests are fulfilled.
	while( TRUE ) 
	{
		UBOOL bHasFinishedRequests = FALSE;
		{
			FScopeLock ScopeLock( CriticalSection );
			bHasFinishedRequests = (OutstandingRequests.Num() == 0) && (BusyWithRequest.GetValue() == 0);
		}	
		if( bHasFinishedRequests )
		{
			break;
		}
		else
		{
			SHUTDOWN_IF_EXIT_REQUESTED;

			//@todo streaming: this should be replaced by waiting for an event.
			appSleep( 0.01f );
		}
	}
}

/**
 * Blocks till all requests are finished and also flushes potentially open handles.
 */
void FAsyncIOSystemBase::BlockTillAllRequestsFinishedAndFlushHandles()
{
	// Block till all requests are fulfilled.
	BlockTillAllRequestsFinished();

	// Flush all file handles.
	FlushHandles();
}

/**
 * Flushes all file handles.
 */
void FAsyncIOSystemBase::FlushHandles()
{
	FScopeLock ScopeLock( CriticalSection );
	// Iterate over all file handles, destroy them and empty name to handle map.
	for( TMap<FString,FAsyncIOHandle>::TIterator It(NameToHandleMap); It; ++It )
	{
		PlatformDestroyHandle( It.Value() );
	}
	NameToHandleMap.Empty();
}

/*----------------------------------------------------------------------------
	FArchiveAsync.
----------------------------------------------------------------------------*/

/**
 * Constructor, initializing all member variables.
 */
FArchiveAsync::FArchiveAsync( const TCHAR* InFileName )
:	FileName					( InFileName	)
,	FileSize					( INDEX_NONE	)
,	UncompressedFileSize		( INDEX_NONE	)
,	CurrentPos					( 0				)
,	CompressedChunks			( NULL			)
,	CurrentChunkIndex			( 0				)
,	CompressionFlags			( COMPRESS_None	)
{
	ArIsLoading		= TRUE;
	ArIsPersistent	= TRUE;

	PrecacheStartPos[CURRENT]	= 0;
	PrecacheEndPos[CURRENT]		= 0;
	PrecacheBuffer[CURRENT]		= NULL;

	PrecacheStartPos[NEXT]		= 0;
	PrecacheEndPos[NEXT]		= 0;
	PrecacheBuffer[NEXT]		= NULL;

	// Relies on default constructor initializing to 0.
	check( PrecacheReadStatus[CURRENT].GetValue() == 0 );
	check( PrecacheReadStatus[NEXT].GetValue() == 0 );

	// Cache file size.
	FileSize = GFileManager->FileSize( *FileName );
	// Check whether file existed.
	if( FileSize >= 0 )
	{
		// No error.
		ArIsError	= FALSE;

		// Retrieved uncompressed file size.
		UncompressedFileSize = GFileManager->UncompressedFileSize( *FileName );

		// Package wasn't compressed so use regular file size.
		if( UncompressedFileSize == INDEX_NONE )
		{
			UncompressedFileSize = FileSize;
		}
	}
	else
	{
		// Couldn't open the file.
		ArIsError	= TRUE;
	}
}

/**
 * Flushes cache and frees internal data.
 */
void FArchiveAsync::FlushCache()
{
	// Wait on all outstanding requests.
	while( PrecacheReadStatus[CURRENT].GetValue() || PrecacheReadStatus[NEXT].GetValue() )
	{
		SHUTDOWN_IF_EXIT_REQUESTED;
		appSleep(0);
	}

	// Invalidate any precached data and free memory for current buffer.
	DEC_DWORD_STAT_BY(STAT_StreamingAllocSize, PrecacheEndPos[CURRENT] - PrecacheStartPos[CURRENT]);
	appFree( PrecacheBuffer[CURRENT] );
	PrecacheBuffer[CURRENT]		= NULL;
	PrecacheStartPos[CURRENT]	= 0;
	PrecacheEndPos[CURRENT]		= 0;
	
	// Invalidate any precached data and free memory for next buffer.
	DEC_DWORD_STAT_BY(STAT_StreamingAllocSize, PrecacheEndPos[NEXT] - PrecacheStartPos[NEXT]);
	appFree( PrecacheBuffer[NEXT] );
	PrecacheBuffer[NEXT]		= NULL;
	PrecacheStartPos[NEXT]		= 0;
	PrecacheEndPos[NEXT]		= 0;
}

/**
 * Virtual destructor cleaning up internal file reader.
 */
FArchiveAsync::~FArchiveAsync()
{
	// Invalidate any precached data and free memory.
	FlushCache();
}

/**
 * Close archive and return whether there has been an error.
 *
 * @return	TRUE if there were NO errors, FALSE otherwise
 */
UBOOL FArchiveAsync::Close()
{
	// Invalidate any precached data and free memory.
	FlushCache();
	// Return TRUE if there were NO errors, FALSE otherwise.
	return !ArIsError;
}

/**
 * Sets mapping from offsets/ sizes that are going to be used for seeking and serialization to what
 * is actually stored on disk. If the archive supports dealing with compression in this way it is 
 * going to return TRUE.
 *
 * @param	InCompressedChunks	Pointer to array containing information about [un]compressed chunks
 * @param	InCompressionFlags	Flags determining compression format associated with mapping
 *
 * @return TRUE if archive supports translating offsets & uncompressing on read, FALSE otherwise
 */
UBOOL FArchiveAsync::SetCompressionMap( TArray<FCompressedChunk>* InCompressedChunks, ECompressionFlags InCompressionFlags )
{
	// Set chunks. A value of NULL means to use direct reads again.
	CompressedChunks	= InCompressedChunks;
	CompressionFlags	= InCompressionFlags;
	CurrentChunkIndex	= 0;
	// Invalidate any precached data and free memory.
	FlushCache();

	// verify some assumptions
	check(UncompressedFileSize == FileSize);
	check(CompressedChunks->Num() > 0);

	// update the uncompressed filesize (which is the end of the uncompressed last chunk)
	FCompressedChunk& LastChunk = (*CompressedChunks)(CompressedChunks->Num() - 1);
	UncompressedFileSize = LastChunk.UncompressedOffset + LastChunk.UncompressedSize;

	// We support translation as requested.
	return TRUE;
}

/**
 * Swaps current and next buffer. Relies on calling code to ensure that there are no outstanding
 * async read operations into the buffers.
 */
void FArchiveAsync::BufferSwitcheroo()
{
	check( PrecacheReadStatus[CURRENT].GetValue() == 0 );
	check( PrecacheReadStatus[NEXT].GetValue() == 0 );

	// Switcheroo.
	DEC_DWORD_STAT_BY(STAT_StreamingAllocSize, PrecacheEndPos[CURRENT] - PrecacheStartPos[CURRENT]);
	appFree( PrecacheBuffer[CURRENT] );
	PrecacheBuffer[CURRENT]		= PrecacheBuffer[NEXT];
	PrecacheStartPos[CURRENT]	= PrecacheStartPos[NEXT];
	PrecacheEndPos[CURRENT]		= PrecacheEndPos[NEXT];

	// Next buffer is unused/ free.
	PrecacheBuffer[NEXT]		= NULL;
	PrecacheStartPos[NEXT]		= 0;
	PrecacheEndPos[NEXT]		= 0;
}

/**
 * Whether the current precache buffer contains the passed in request.
 *
 * @param	RequestOffset	Offset in bytes from start of file
 * @param	RequestSize		Size in bytes requested
 *
 * @return TRUE if buffer contains request, FALSE othwerise
 */
UBOOL FArchiveAsync::PrecacheBufferContainsRequest( INT RequestOffset, INT RequestSize )
{
	// TRUE if request is part of precached buffer.
	if( (RequestOffset >= PrecacheStartPos[CURRENT]) 
	&&  (RequestOffset+RequestSize <= PrecacheEndPos[CURRENT]) )
	{
		return TRUE;
	}
	// FALSE if it doesn't fit 100%.
	else
	{
		return FALSE;
	}
}

/**
 * Finds and returns the compressed chunk index associated with the passed in offset.
 *
 * @param	RequestOffset	Offset in file to find associated chunk index for
 *
 * @return Index into CompressedChunks array matching this offset
 */
INT FArchiveAsync::FindCompressedChunkIndex( INT RequestOffset )
{
	// Find base start point and size. @todo optimization: avoid full iteration
	CurrentChunkIndex = 0;
	while( CurrentChunkIndex < CompressedChunks->Num() )
	{
		const FCompressedChunk& Chunk = (*CompressedChunks)(CurrentChunkIndex);
		// Check whether request offset is encompassed by this chunk.
		if( Chunk.UncompressedOffset <= RequestOffset 
		&&  Chunk.UncompressedOffset + Chunk.UncompressedSize > RequestOffset )
		{
			break;
		}
		CurrentChunkIndex++;
	}
	check( CurrentChunkIndex < CompressedChunks->Num() );
	return CurrentChunkIndex;
}

/**
 * Precaches compressed chunk of passed in index using buffer at passed in index.
 *
 * @param	ChunkIndex	Index of compressed chunk
 * @param	BufferIndex	Index of buffer to precache into	
 */
void FArchiveAsync::PrecacheCompressedChunk( INT ChunkIndex, INT BufferIndex )
{
	// Request generic async IO system.
	FIOSystem* IO = GIOManager->GetIOSystem( IOSYSTEM_GenericAsync ); 
	check(IO);

	// Compressed chunk to request.
	FCompressedChunk ChunkToRead = (*CompressedChunks)(ChunkIndex);

	// Update start and end position...
	DEC_DWORD_STAT_BY(STAT_StreamingAllocSize, PrecacheEndPos[BufferIndex] - PrecacheStartPos[BufferIndex]);
	PrecacheStartPos[BufferIndex]	= ChunkToRead.UncompressedOffset;
	PrecacheEndPos[BufferIndex]		= ChunkToRead.UncompressedOffset + ChunkToRead.UncompressedSize;

	// In theory we could use appRealloc if it had a way to signal that we don't want to copy
	// the data (implicit realloc behavior).
	appFree( PrecacheBuffer[BufferIndex] );
	PrecacheBuffer[BufferIndex]		= (BYTE*) appMalloc( PrecacheEndPos[BufferIndex] - PrecacheStartPos[BufferIndex] );
	INC_DWORD_STAT_BY(STAT_StreamingAllocSize, PrecacheEndPos[BufferIndex] - PrecacheStartPos[BufferIndex]);

	// Increment read status, request load and make sure that request was possible (e.g. filename was valid).
	check( PrecacheReadStatus[BufferIndex].GetValue() == 0 );
	PrecacheReadStatus[BufferIndex].Increment();
	QWORD RequestId = IO->LoadCompressedData( 
							FileName, 
							ChunkToRead.CompressedOffset, 
							ChunkToRead.CompressedSize, 
							ChunkToRead.UncompressedSize, 
							PrecacheBuffer[BufferIndex], 
							CompressionFlags, 
							&PrecacheReadStatus[BufferIndex],
							AIOP_Normal);
	check(RequestId);
}

/**
 * Hint the archive that the region starting at passed in offset and spanning the passed in size
 * is going to be read soon and should be precached.
 *
 * The function returns whether the precache operation has completed or not which is an important
 * hint for code knowing that it deals with potential async I/O. The archive is free to either not 
 * implement this function or only partially precache so it is required that given sufficient time
 * the function will return TRUE. Archives not based on async I/O should always return TRUE.
 *
 * This function will not change the current archive position.
 *
 * @param	RequestOffset	Offset at which to begin precaching.
 * @param	RequestSize		Number of bytes to precache
 * @return	FALSE if precache operation is still pending, TRUE otherwise
 */
UBOOL FArchiveAsync::Precache( INT RequestOffset, INT RequestSize )
{
	// Check whether we're currently waiting for a read request to finish.
	UBOOL bFinishedReadingCurrent	= PrecacheReadStatus[CURRENT].GetValue()==0 ? TRUE : FALSE;
	UBOOL bFinishedReadingNext		= PrecacheReadStatus[NEXT].GetValue()==0 ? TRUE : FALSE;

	// Return read status if the current request fits entirely in the precached region.
	if( PrecacheBufferContainsRequest( RequestOffset, RequestSize ) )
	{
		return bFinishedReadingCurrent;
	}
	// We're not fitting into the precached region and we have a current read request outstanding
	// so wait till we're done with that. This can happen if we're skipping over large blocks in
	// the file because the object has been found in memory.
	// @todo async: implement cancelation
	else if( !bFinishedReadingCurrent )
	{
		return FALSE;
	}
	// We're still in the middle of fulfilling the next read request so wait till that is done.
	else if( !bFinishedReadingNext )
	{
		return FALSE;
	}
	// We need to make a new read request.
	else
	{
		// Compressed read. The passed in offset and size were requests into the uncompressed file and
		// need to be translated via the CompressedChunks map first.
		if( CompressedChunks )
		{
			// Switch to next buffer.
			BufferSwitcheroo();

			// Check whether region is precached after switcheroo.
			UBOOL	bIsRequestCached	= PrecacheBufferContainsRequest( RequestOffset, RequestSize );
			// Find chunk associated with request.
			INT		RequestChunkIndex	= FindCompressedChunkIndex( RequestOffset );

			// Precache chunk if it isn't already.
			if( !bIsRequestCached )
			{
				PrecacheCompressedChunk( RequestChunkIndex, CURRENT );
			}

			// Precache next chunk if there is one.
			if( RequestChunkIndex + 1 < CompressedChunks->Num() )
			{
				PrecacheCompressedChunk( RequestChunkIndex + 1, NEXT );
			}
		}
		// Regular read.
		else
		{
			// Request generic async IO system.
			FIOSystem* IO = GIOManager->GetIOSystem( IOSYSTEM_GenericAsync ); 
			check(IO);

			DEC_DWORD_STAT_BY(STAT_StreamingAllocSize, PrecacheEndPos[CURRENT] - PrecacheStartPos[CURRENT]);
			PrecacheStartPos[CURRENT]	= RequestOffset;
			// We always request at least a few KByte to be read/ precached to avoid going to disk for
			// a lot of little reads.
			PrecacheEndPos[CURRENT]		= RequestOffset + Max( RequestSize, DVD_ECC_BLOCK_SIZE );
			// Ensure that we're not trying to read beyond EOF.
			PrecacheEndPos[CURRENT]		= Min( PrecacheEndPos[CURRENT], FileSize );
			// In theory we could use appRealloc if it had a way to signal that we don't want to copy
			// the data (implicit realloc behavior).
			appFree( PrecacheBuffer[CURRENT] );

			PrecacheBuffer[CURRENT]		= (BYTE*) appMalloc( PrecacheEndPos[CURRENT] - PrecacheStartPos[CURRENT] );
			INC_DWORD_STAT_BY(STAT_StreamingAllocSize, PrecacheEndPos[CURRENT] - PrecacheStartPos[CURRENT]);

			// Increment read status, request load and make sure that request was possible (e.g. filename was valid).
			PrecacheReadStatus[CURRENT].Increment();
			QWORD RequestId = IO->LoadData( 
									FileName, 
									PrecacheStartPos[CURRENT], 
									PrecacheEndPos[CURRENT] - PrecacheStartPos[CURRENT], 
									PrecacheBuffer[CURRENT], 
									&PrecacheReadStatus[CURRENT],
									AIOP_Normal );
			check(RequestId);
		}

		return FALSE;
	}
}

/**
 * Serializes data from archive.
 *
 * @param	Data	Pointer to serialize to
 * @param	Count	Number of bytes to read
 */
void FArchiveAsync::Serialize( void* Data, INT Count )
{
	// Ensure we aren't reading beyond the end of the file
	checkf( CurrentPos + Count <= TotalSize(), TEXT("Seeked past end of file %s (%d / %d)"), *FileName, CurrentPos + Count, TotalSize(), UncompressedFileSize );

	DOUBLE	StartTime	= 0;
	UBOOL	bIOBlocked	= FALSE;

	// Make sure serialization request fits entirely in already precached region.
	if( !PrecacheBufferContainsRequest( CurrentPos, Count ) )
	{
		// Keep track of time we started to block.
		StartTime	= appSeconds();
		bIOBlocked	= TRUE;

		// Busy wait for region to be precached.
		while( !Precache( CurrentPos, Count ) )
		{
			SHUTDOWN_IF_EXIT_REQUESTED;
			appSleep(0);
		}

		// There shouldn't be any outstanding read requests for the main buffer at this point.
		check( PrecacheReadStatus[CURRENT].GetValue() == 0 );
	}
	
	// Make sure to wait till read request has finished before progressing. This can happen if PreCache interface
	// is not being used for serialization.
	while( PrecacheReadStatus[CURRENT].GetValue() != 0 )
	{
		SHUTDOWN_IF_EXIT_REQUESTED;
		// Only update StartTime if we haven't already started blocking I/O above.
		if( !bIOBlocked )
		{
			// Keep track of time we started to block.
			StartTime	= appSeconds();
			bIOBlocked	= TRUE;
		}
		appSleep(0);
	}

	// Update stats if we were blocked.
#if STATS
	if( bIOBlocked )
	{
		DOUBLE BlockingTime = appSeconds() - StartTime;
		INC_FLOAT_STAT_BY(STAT_AsyncIO_MainThreadBlockTime,(FLOAT)BlockingTime);
#if LOOKING_FOR_PERF_ISSUES
		debugf( NAME_PerfWarning, TEXT("FArchiveAsync::Serialize: %5.2fms blocking on read from '%s' (Offset: %i, Size: %i)"), 
			1000 * BlockingTime, 
			*FileName, 
			CurrentPos, 
			Count );
#endif
	}
#endif

	// Copy memory to destination.
	appMemcpy( Data, PrecacheBuffer[CURRENT] + (CurrentPos - PrecacheStartPos[CURRENT]), Count );
	// Serialization implicitly increases position in file.
	CurrentPos += Count;
}

/**
 * Returns the current position in the archive as offset in bytes from the beginning.
 *
 * @return	Current position in the archive (offset in bytes from the beginning)
 */
INT FArchiveAsync::Tell()
{
	return CurrentPos;
}

/**
 * Returns the total size of the archive in bytes.
 *
 * @return total size of the archive in bytes
 */
INT FArchiveAsync::TotalSize()
{
	return UncompressedFileSize;
}

/**
 * Sets the current position.
 *
 * @param InPos	New position (as offset from beginning in bytes)
 */
void FArchiveAsync::Seek( INT InPos )
{
	check( InPos >= 0 && InPos <= TotalSize() );
	CurrentPos = InPos;
}


/*----------------------------------------------------------------------------
	End.
----------------------------------------------------------------------------*/
