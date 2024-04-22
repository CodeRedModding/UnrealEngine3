/*=============================================================================
	ScopedObjectStateChange.cpp: Implementation of the FScopedObjectState class.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

// Includes
#include "EnginePrivate.h"
#include "ScopedObjectStateChange.h"


/* ==========================================================================================================
	FScopedObjectStateChange
========================================================================================================== */
/**
 * Constructor
 *
 * @param	InModifiedObject	the object that was modified.
 */
FScopedObjectStateChange::FScopedObjectStateChange( UObject* InModifiedObject )
: ModifiedObject(NULL), PropagationArchive(NULL), PreviousPropagationArchive(NULL), bActive(FALSE)
{
	SetObject(InModifiedObject);
	BeginEdit();
}

/** Destructor */
FScopedObjectStateChange::~FScopedObjectStateChange()
{
	FinishEdit();
}

/**
 * Assigns the object that this struct will operate on; only used when NULL is passed to the constructor.  Cannot
 * be used to change the object associated with this struct - asserts if ModifiedObject is non-NULL or if PreEditChange
 * has already been called on ModifiedObject.
 *
 * @param	InModifiedObject	the object that will be modified.
 */
void FScopedObjectStateChange::SetObject( UObject* InModifiedObject )
{
	checkf(ModifiedObject == NULL, TEXT("Not allowed to change the object associated with an FScopedObjectStateChange!"));
	checkf(!bActive, TEXT("Cannot change the object associated with an FScopedObjectStateChange if BeginEdit() has been called without a matching call to FinishEdit()!"));

	ModifiedObject = InModifiedObject;
}

/**
 * Creates the archetype propagation archive and send the PreEditChange notifications.
 */
void FScopedObjectStateChange::BeginEdit()
{
	if ( ModifiedObject != NULL )
	{
		if ( ModifiedObject->HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject) )
		{
			UObject* PropagationManager = ModifiedObject;

			// if this archetype uses managed archetype propagation, don't create the default propagation archive
			if ( !ModifiedObject->UsesManagedArchetypePropagation(&PropagationManager) )
			{
				// store the existing value of GMemoryArchive, so that we don't clobber it if this class is used in a recursive method
				PreviousPropagationArchive = GMemoryArchive;

				// Create an FArchetypePropagationArc to propagate the updated property values from archetypes to instances of that archetype
				GMemoryArchive = PropagationArchive = new FArchetypePropagationArc();
			}

			// get a list of all objects which will be affected by this change; 
			TArray<UObject*> Objects;
			PropagationManager->GetArchetypeInstances(Objects);

			// If we're modifying an archetype, save the the property data for all instances and child classes
			// of this archetype before the archetype's property values are changed.  Once the archetype's values
			// have been changed, we'll refresh each instances values with the new values from the archetype, then
			// reload the custom values for each object that were stored in the memory archive.
			PropagationManager->SaveInstancesIntoPropagationArchive(Objects);
		}

		// notify the object that this property's value is about to be changed
		ModifiedObject->PreEditChange(NULL);
	}
}

/**
 * Sends the PostEditChange notifications and deletes the archetype propagation archive.
 *
 * @param	bCancelled	specify TRUE if archetype changes should NOT be propagated to instances (i.e. if the modification failed)
 */
void FScopedObjectStateChange::FinishEdit( UBOOL bCancelled/*=FALSE*/ )
{
	if ( ModifiedObject != NULL )
	{
		// notify the object that this property's value has been changed
		ModifiedObject->PostEditChange();

		if ( ModifiedObject->HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject) )
		{
			if ( !bCancelled )
			{
				UObject* PropagationManager = ModifiedObject;

				// if this archetype uses managed archetype propagation, we don't create the default propagation archive so don't try
				// to clean it up
				if ( !ModifiedObject->UsesManagedArchetypePropagation(&PropagationManager) )
				{
					check(PropagationArchive);

					// now change the propagation archive to read mode
					PropagationArchive->ActivateReader();
				}

				// first, get a list of all objects which will be affected by this change; 
				TArray<UObject*> Objects;
				PropagationManager->GetArchetypeInstances(Objects);

				// If we're modifying an archetype, reload the property data for all instances and child classes
				// of this archetype, then re-import the property data for the modified properties of each object.
				PropagationManager->LoadInstancesFromPropagationArchive(Objects);
			}

			// if GMemoryArchive is still pointing to the one we created, restore it to the previous value
			if ( GMemoryArchive == PropagationArchive )
			{
				GMemoryArchive = PreviousPropagationArchive;
			}

			// clean up the FArchetypePropagationArc we created
			delete PropagationArchive;
			PropagationArchive = NULL;
			PreviousPropagationArchive = NULL;
		}
		else
		{
			// this assertion is to detect cases where the ModifiedObject had RF_ArchetypeObject or RF_ClassDefaultObject flags
			// in BeginEdit but they were removed between BeginEdit and EndEdit
			check(PropagationArchive == NULL);
		}
		ModifiedObject = NULL;
	}
}


// EOF



