/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#ifndef __ConsolidateWindowCLR_h__
#define __ConsolidateWindowCLR_h__

#include "InteropShared.h"

#ifdef __cplusplus_cli

// Forward declarations
ref class MWPFFrame;
ref class MConsolidateObjectsPanel;

#endif // #ifdef __cplusplus_cli

/** Native wrapper singleton class for the managed consolidate window */
class FConsolidateWindow : public FSerializableObject, public FCallbackEventDevice
{
public:
	/** Enumeration representing the possible results of the consolidate window */
	enum ConsolidateResults
	{
		CR_Consolidate,
		CR_Cancel
	};
	
	/** Shutdown the singleton, freeing the allocated memory */
	static void Shutdown();

	/**
	 * Attempt to add objects to the consolidation window
	 *
	 * @param	InObjects			Objects to attempt to add to the consolidation window
	 * @param	InResourceTypes		Generic browser types associated with the passed in objects	
	 */
	static void AddConsolidationObjects( const TArray<UObject*>& InObjects, const TArray<UGenericBrowserType*>& InResourceTypes );

	/**
	 * Determine the compatibility of the passed in objects with the objects already present in the consolidation window
	 *
	 * @param	InProposedObjects		Objects to check compatibility with vs. the objects already present in the consolidation window
	 * @param	OutCompatibleObjects	[out]Objects from the passed in array which are compatible with those already present in the
	 *									consolidation window, if any
	 *
	 * @return	TRUE if all of the passed in objects are compatible, FALSE otherwise
	 */
	static UBOOL DetermineAssetCompatibility( const TArray<UObject*>& InProposedObjects, TArray<UObject*>& OutCompatibleObjects );

	/**
	 * FSerializableObject interface; Serialize object references
	 *
	 * @param	Ar	Archive used to serialize with
	 */
	virtual void Serialize( FArchive& Ar );

	/**
	 * FCallbackEventDevice interface; Respond to map change callbacks by clearing the consolidation window
	 *
	 * @param	InType	Type of callback event
	 * @param	InFlag	Flag associated with the callback
	 */
	virtual void Send( ECallbackEventType InType, DWORD InFlag );

private:

	/** Constructor; Construct an FConsolidateWindow */
	FConsolidateWindow();

	/** Destructor; Destruct an FConsolidateWindow */
	~FConsolidateWindow();

	// Copy constructor and assignment operator; intentionally left unimplemented
	FConsolidateWindow( const FConsolidateWindow& );
	FConsolidateWindow& operator=( const FConsolidateWindow& );

	/**
	 * Accessor for the private instance of the singleton; allocates the instance if it is NULL
	 *
	 * @note	Not really thread-safe, but should never be an issue given the use
	 * @return	The private instance of the singleton
	 */
	static FConsolidateWindow& GetInternalInstance();

	/** Parent WPF frame to the consolidation panel */
	GCRoot( MWPFFrame^ )				ConsolidateObjFrame;

	/** Consolidation panel that provides the specific consolidation features */
	GCRoot( MConsolidateObjectsPanel^ ) ConsolidateObjPanel;

	/** Static private instance of the window */
	static FConsolidateWindow*			Instance;
};

#endif // #ifndef __ConsolidateWindowCLR_h__
