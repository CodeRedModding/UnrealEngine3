/*=============================================================================
	CrossLevelManager.cpp: Cross level reference implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "CorePrivate.h"
#include "CrossLevelReferences.h"

// create the standard clr manager, used by the game, editor, everything but PIE
FCrossLevelReferenceManager StandardManager;

// create the PIE clr manager, used only when PIE is active, and PIE world is GWorld
FCrossLevelReferenceManager PIEManager;

// set the global manager to the empty one, this will be overridden later to something useful
FCrossLevelReferenceManager* GCrossLevelReferenceManager = &StandardManager;


/**
 * Makes the stnadard default manager the active GCrossLevelReferenceManager
 */
void FCrossLevelReferenceManager::SwitchToStandardManager()
{
	GCrossLevelReferenceManager = &StandardManager;
}

/**
 * Makes the PIE manager the active GCrossLevelReferenceManager
 */
void FCrossLevelReferenceManager::SwitchToPIEManager()
{
	check(GIsEditor);
	GCrossLevelReferenceManager = &PIEManager;
}

/**
 * Sets the current manager to be the manager that contains the object passed in
 *
 * @param Object Object used to control which manager to make active
 */
void FCrossLevelReferenceManager::SwitchToManagerWithObject(UObject* Object)
{
#if !CONSOLE
	if (StandardManager.CrossLevelObjectToGuidMap.Find(Object))
	{
		SwitchToStandardManager();
	}
	else if (PIEManager.CrossLevelObjectToGuidMap.Find(Object))
	{
		SwitchToPIEManager();
	}
	else
	{
		SwitchToStandardManager();
	}
#endif
}

/**
 * Empties out all information from the manager
 */
void FCrossLevelReferenceManager::Reset()
{
	// flush out all maps
	DelayedCrossLevelFixupMap.Empty();
	DelayedCrossLevelTeardownMap.Empty();;
#if !CONSOLE
	CrossLevelObjectToGuidMap.Empty();
#endif
}


/**
 * Dumps out memory usage statistics to Ar
 *
 * @param Ar Archive to print stats to
 */
void FCrossLevelReferenceManager::DumpMemoryUsage(FOutputDevice& Ar)
{
	{
		Ar.Logf(TEXT("Standard CrossLevelReferenceManager:"));
#if !CONSOLE
		FArchiveCountMem ObjectToGuidMapMem(NULL);
		StandardManager.CrossLevelObjectToGuidMap.CountBytes(ObjectToGuidMapMem);
		Ar.Logf(TEXT("  Object To Guid Map: %dK, %dK, %d entries"), ObjectToGuidMapMem.GetNum() / 1024, ObjectToGuidMapMem.GetMax() / 1024, StandardManager.CrossLevelObjectToGuidMap.Num());
#endif

		FArchiveCountMem FixupMapMem(NULL);
		StandardManager.DelayedCrossLevelFixupMap.CountBytes(FixupMapMem);
		Ar.Logf(TEXT("  DelayedCrossLevelFixup Map: %dK, %dK, %d entries"), FixupMapMem.GetNum() / 1024, FixupMapMem.GetMax() / 1024, StandardManager.DelayedCrossLevelFixupMap.Num());

		FArchiveCountMem TeardownMapMem(NULL);
		StandardManager.DelayedCrossLevelTeardownMap.CountBytes(TeardownMapMem);
		Ar.Logf(TEXT("  DelayedCrossLevelTeardown Map: %dK, %dK, %d entries"), TeardownMapMem.GetNum() / 1024, TeardownMapMem.GetMax() / 1024, StandardManager.DelayedCrossLevelTeardownMap.Num());
	}

	if (GIsEditor)
	{
		Ar.Logf(TEXT("PIE CrossLevelReferenceManager:"));
#if !CONSOLE
		FArchiveCountMem ObjectToGuidMapMem(NULL);
		PIEManager.CrossLevelObjectToGuidMap.CountBytes(ObjectToGuidMapMem);
		Ar.Logf(TEXT("  Object To Guid Map: %dK, %dK, %d entries"), ObjectToGuidMapMem.GetNum() / 1024, ObjectToGuidMapMem.GetMax() / 1024, PIEManager.CrossLevelObjectToGuidMap.Num());
#endif

		FArchiveCountMem FixupMapMem(NULL);
		PIEManager.DelayedCrossLevelFixupMap.CountBytes(FixupMapMem);
		Ar.Logf(TEXT("  DelayedCrossLevelFixup Map: %dK, %dK, %d entries"), FixupMapMem.GetNum() / 1024, FixupMapMem.GetMax() / 1024, PIEManager.DelayedCrossLevelFixupMap.Num());

		FArchiveCountMem TeardownMapMem(NULL);
		PIEManager.DelayedCrossLevelTeardownMap.CountBytes(TeardownMapMem);
		Ar.Logf(TEXT("  DelayedCrossLevelTeardown Map: %dK, %dK, %d entries"), TeardownMapMem.GetNum() / 1024, TeardownMapMem.GetMax() / 1024, PIEManager.DelayedCrossLevelTeardownMap.Num());
	}

}

