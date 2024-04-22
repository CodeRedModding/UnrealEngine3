/*=============================================================================
	CrossLevelReferences.h: Classes used for safe cross level references
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __CROSSLEVELREFERENCES_H__
#define __CROSSLEVELREFERENCES_H__


// expose the global reference manager
extern class FCrossLevelReferenceManager* GCrossLevelReferenceManager;

/**
 * Structure to hold information about an object and a property offset - used to fix up pointers to delayed cross-level references
 */
struct FDelayedCrossLevelRef
{
	FDelayedCrossLevelRef(UObject* InObject, DWORD InOffset)
		: Object(InObject)
		, Offset(InOffset)
	{
	}

	/**
	* Comparison operator for use in TMaps, etc
	*/
	UBOOL operator==(const FDelayedCrossLevelRef& Other) const
	{
		return Other.Object == Object && Other.Offset == Offset;
	}

	/** The object that holds the cross level reference that needs fixing */
	UObject* Object;

	/** The offset describing the location in the object */
	DWORD Offset;
};

class FCrossLevelReferenceManager
{
public:

	/**
	 * Makes the stnadard default manager the active GCrossLevelReferenceManager
	 */
	static void SwitchToStandardManager();

	/**
	 * Makes the PIE manager the active GCrossLevelReferenceManager
	 */
	static void SwitchToPIEManager();

	/**
	 * Sets the current manager to be the manager that contains the object passed in
	 *
	 * @param Object Object used to control which manager to make active
	 */
	static void SwitchToManagerWithObject(UObject* Object);

	/**
	 * Dumps out memory usage statistics to Ar
	 *
	 * @param Ar Archive to print stats to
	 */
	static void DumpMemoryUsage(FOutputDevice& Ar);

	/**
	 * Empties out all information from the manager
	 */
	void Reset();

	/**
	 * Sets an internal prefix for all level names when dealing with PIE, because the levels will be loaded as UEDPIExxx (or similar),
	 * but objects still are using xxx for level names
	 *
	 * @param Prefix Prefix before all levels currently being saved out for PIE
	 */
	void SetPIEPrefix(const TCHAR* Prefix)
	{
		PIEPrefix = Prefix;
	}

	/** 
	 * @return the current PIE prefix, or "" if this isn't PIE or if it's not set yet
	 */
	const FString& GetPIEPrefix()
	{
		return PIEPrefix;
	}

#if !CONSOLE
	/** Track all objects that have been assigned a Guid for cross-level reference purposes (only needed when saving packages) */
	TMap<UObject*, FGuid> CrossLevelObjectToGuidMap;
#endif

	/**
	 * Map of Guids used by cross level references to locations in other objects that attempted
	 * to point to this object, but the object wasn't already loaded. This will allow properties
	 * to be automatically fixed up to point to this object after the level is loaded.
	 */
	TMultiMap<FGuid, FDelayedCrossLevelRef> DelayedCrossLevelFixupMap;

	/**
	 * List of objects that are being pointed to by external objects. This is used 
	 * so when an object is unstreamed, it NULLs out pointers pointing to it, as well 
	 * as readies them to be streamed in again using the above DelayedCrossLevelFixup map
	 */
	TMultiMap<UObject*, FDelayedCrossLevelRef> DelayedCrossLevelTeardownMap;

protected:
	/** Prefix used before file names */
	FString PIEPrefix;
};



#endif