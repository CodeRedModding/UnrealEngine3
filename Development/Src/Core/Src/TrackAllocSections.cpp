/*=============================================================================
TrackAllocSections.cpp: Utils for tracking alloc by scoped sections
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "CorePrivate.h"
#include "TrackAllocSections.h"

/*----------------------------------------------------------------------------
Globals
----------------------------------------------------------------------------*/

/** 
* Global state to keep track of current section name. 
* Used by FMallocProxySimpleTrack and set by FScopeAllocSection
*/
FGlobalAllocSectionState GAllocSectionState;
/** TRUE if the global GAllocSectionState has been initialized */
UBOOL GAllocSectionState_IsInitialized = FALSE;


/*-----------------------------------------------------------------------------
FGlobalAllocSectionState
-----------------------------------------------------------------------------*/

/** 
* Dtor. Free any memory allocated for the per thread global section name 
*/
FGlobalAllocSectionState::~FGlobalAllocSectionState()
{
	if (GAllocSectionState_IsInitialized)
	{
		appFreeTlsSlot(PerThreadSectionDataTLS);	
	}
}

/**
* Gets the global data for this thread.  Also initializes the class on demand. 
*/
FGlobalAllocSectionState::FAllocThreadData& FGlobalAllocSectionState::GetThreadData()
{
	// NOTE: Because of static initialization order issues, it's possible for this method to be called
	//		on GAllocSectionState before the constructor is even run yet!  Need to handle this carefully here.
	if( !GAllocSectionState_IsInitialized )
	{
		NextAvailInstance = 0;

		// Set initial section IDs
		appMemzero( PerThreadData, sizeof( PerThreadData ) );
		for( INT CurDataIndex = 0; CurDataIndex < MAX_THREAD_DATA_INSTANCES; ++CurDataIndex )
		{
			PerThreadData[ CurDataIndex ].SectionID = (UINT)INDEX_NONE;
		}

		// Allocate the TLS slot and zero its contents
		PerThreadSectionDataTLS = appAllocTlsSlot();
		appSetTlsValue(PerThreadSectionDataTLS,NULL);

		GAllocSectionState_IsInitialized = TRUE;
	}

	FAllocThreadData* Entry = (FAllocThreadData*)appGetTlsValue(PerThreadSectionDataTLS);
	if( !Entry )
	{
		INT CurAvailInstance = NextAvailInstance;
		do 
		{	
			// update the cur available instance atomically
			CurAvailInstance = NextAvailInstance;			
		} while( appInterlockedCompareExchange(&NextAvailInstance,CurAvailInstance+1,CurAvailInstance) != CurAvailInstance );

		check(CurAvailInstance < MAX_THREAD_DATA_INSTANCES);
		Entry = &PerThreadData[CurAvailInstance];
		appSetTlsValue(PerThreadSectionDataTLS,(void*)Entry);		
	}
	return *Entry;
}


/**
* Gets the global section name via the thread's TLS entry. Allocates a new
* section string if one doesn't exist for the current TLS entry.
* @return ptr to the global section name for the running thread 
*/
const TCHAR* FGlobalAllocSectionState::GetCurrentSectionName()
{
	const FAllocThreadData& ThreadData = GetThreadData();

	// Lookup the name for this section
	const FString* FoundSectionName = ThreadData.SectionIDtoSectionNameMap.Find( ThreadData.SectionID );
	if( FoundSectionName != NULL )
	{
		return **FoundSectionName;
	}
	return TEXT( "<Total Untracked>" );
}

/**
* Gets the global section ID via the thread's TLS entry. Allocates a new
* section if one doesn't exist for the current TLS entry.
* @return ptr to the global section name for the running thread 
*/
UINT& FGlobalAllocSectionState::GetCurrentSectionID()
{
	return GetThreadData().SectionID;
}


/*-----------------------------------------------------------------------------
FScopeAllocSection
-----------------------------------------------------------------------------*/

/** Ctor, sets the section name. */
FScopeAllocSection::FScopeAllocSection( const UINT InSectionID, const TCHAR* InSection )
{
	FGlobalAllocSectionState::FAllocThreadData& ThreadData = GAllocSectionState.GetThreadData();

	// Add this section name to our list of sections if we don't have it already
	if( ThreadData.SectionIDtoSectionNameMap.Find( InSectionID ) == NULL )
	{
		FString SectionName( InSection );
		ThreadData.SectionIDtoSectionNameMap.Set( InSectionID, *SectionName );
	}

	// Capture current section ID so we can restore it after our scope exits
	UINT& CurSectionID = ThreadData.SectionID;
	OldSectionID = CurSectionID;

	// Now set the current section ID to this scope's section!
	CurSectionID = InSectionID;
}

/** Dtor, restores old section name. */
FScopeAllocSection::~FScopeAllocSection()
{
	// Restore old section ID
	UINT& CurSectionID = GAllocSectionState.GetCurrentSectionID();
	CurSectionID = OldSectionID;
}



