/*=============================================================================
TrackAllocSections.h: Utils for tracking alloc by scoped sections
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _TRACK_ALLOC_SECTIONS
#define _TRACK_ALLOC_SECTIONS

/** 
* Keeps track of the current alloc section name. 
* Used by FMallocProxySimpleTrack and set by FScopeAllocSection 
*/
class FGlobalAllocSectionState
{
public:
	enum { MAX_THREAD_DATA_INSTANCES=100 };


	/** Per-thread data */
	struct FAllocThreadData
	{
		/** Section ID */
		UINT SectionID;

		/** Maps section IDs to section names, constructed dynamically as memory is allocated.
		    We store this per-thread to eliminate an extra critical section. */
		TMap< INT, FString > SectionIDtoSectionNameMap;
	};


	/** 
	* Dtor. Free any memory allocated for the per thread global section name 
	*/
	~FGlobalAllocSectionState();

	/** 
	* Gets the global data for this thread via TLS entry.  Also initializes the class on demand. 
	*/
	FAllocThreadData& GetThreadData();

	/**
	* Gets the global section name via the thread's TLS entry. Allocates a new
	* section string if one doesn't exist for the current TLS entry.
	* @return ptr to the global section name for the running thread 
	*/
	const TCHAR* GetCurrentSectionName();

	/**
	* Gets the global section ID via the thread's TLS entry. Allocates a new
	* section if one doesn't exist for the current TLS entry.
	* @return ptr to the global section name for the running thread 
	*/
	UINT& GetCurrentSectionID();

private:
	/** each thread needs its own instance set accessed via a TLS entry */
	FAllocThreadData PerThreadData[ MAX_THREAD_DATA_INSTANCES ];
	
	/** keeps track of the next entry in PerThreadData */
	INT NextAvailInstance;

	/** TLS index for the per thread entry of the alloc section name data */
	DWORD PerThreadSectionDataTLS;
};

/** Global state to keep track of current section name. */
extern FGlobalAllocSectionState	GAllocSectionState;

/** Utility class for setting GAllocSectionName while this object is in scope. */
class FScopeAllocSection
{
public:

	/** Ctor, sets the section name. */
	FScopeAllocSection( const UINT InSectionID, const TCHAR* InSection );

	/** Dtor, restores old section name. */
	~FScopeAllocSection();


private:

	/** Previous section ID - to restore when this object goes out of scope. */
	UINT OldSectionID;

};


#endif //#if _TRACK_ALLOC_SECTIONS



