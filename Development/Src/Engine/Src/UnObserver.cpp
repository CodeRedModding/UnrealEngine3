/*=============================================================================
	UnObserver.cpp: Unreal Observer interface implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"

//-----------------------------------------------------------------------------
// FObserver Interface
//-----------------------------------------------------------------------------

/**
 * FObserverInterface::AddObserver
 * Add this Observer to the current world's list of observers
 */
void FObserverInterface::AddObserver()
{
	if(GWorld)
	{
		GWorld->Observers.AddUniqueItem(this);
	}
}

/**
 * FObserverInterface::RemoveObserver
 * Remove this Observer to the current world's list of observers
 */
void FObserverInterface::RemoveObserver()
{
	if(GWorld)
	{
		GWorld->Observers.RemoveItem(this);
	}
}