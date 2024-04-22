/*=============================================================================
      Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __SCOPEDOBJECTSTATECHANGE_H__
#define __SCOPEDOBJECTSTATECHANGE_H__


/**
 * This class is used for encapsulating tasks performed when the value of a property is changed outside of an editor property window.
 * It creates and cleans up an archetype propagation archive to ensure that changes made to archetype objects are propagated to instances
 * of that archetype, and sends Pre/PostEditChange notifications to the modified object.
 */
class FScopedObjectStateChange
{
public:
	/**
	 * Constructor
	 *
	 * @param	InModifiedObject	the object that will be modified.
	 */
	FScopedObjectStateChange( class UObject* InModifiedObject=NULL );

	/** Destructor */
	~FScopedObjectStateChange();

	/**
	 * Assigns the object that this struct will operate on; only used when NULL is passed to the constructor.  Cannot
	 * be used to change the object associated with this struct - asserts if ModifiedObject is non-NULL or if PreEditChange
	 * has already been called on ModifiedObject.
	 *
	 * @param	InModifiedObject	the object that will be modified.
	 */
	void SetObject( class UObject* InModifiedObject );

	/**
	 * Creates the archetype propagation archive and send the PreEditChange notifications.
	 */
	void BeginEdit();

	/**
	 * Sends the PostEditChange notifications and deletes the archetype propagation archive.
	 *
	 * @param	bCancelled	specify TRUE if archetype changes should NOT be propagated to instances (i.e. if the modification failed)
	 */
	void FinishEdit( UBOOL bCancelled=FALSE );
	void CancelEdit()
	{
		FinishEdit(TRUE);
	}

private:

	/**
	 * The object that has been modified.
	 */
	class UObject* ModifiedObject;

	/**
	 * An archive used for propagating property value changes to instances of archetypes.
	 */
	class FArchetypePropagationArc* PropagationArchive;

	/**
	 * Stores a reference to the previous value of GMemoryArchive so that it can be restored when FinishEdit is called.
	 */
	class FReloadObjectArc* PreviousPropagationArchive;

	/**
	 * TRUE if PreEditChange has already been called on ModifiedObject.
	 */
	UBOOL bActive;
};

#endif

