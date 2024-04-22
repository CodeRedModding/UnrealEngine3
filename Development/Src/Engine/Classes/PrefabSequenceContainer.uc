/**
 * Special sequence class which acts as a container for any sequences of PrefabInstance actors.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class PrefabSequenceContainer extends Sequence
	native(inherit);

cpptext
{
	/* === USequenceObject interface === */
	/**
	 * Provides a way for non-deletable SequenceObjects (those with bDeletable=false) to be removed programatically.  The
	 * user will not be able to remove this object from the sequence via the UI, but calls to RemoveObject will succeed.
	 */
	virtual UBOOL IsDeletable() const { return TRUE; }

	/* === USequence interface === */
	/**
	 * @return	TRUE if this sequence is the special sequence which serves as the parent for all PrefabInstance sequences in a map.
	 */
	virtual UBOOL IsPrefabSequenceContainer() const { return TRUE; }

	/* === UObject interface === */
	/**
	 * Called after importing property values for this object (paste, duplicate or .t3d import)
	 * Allow the object to perform any cleanup for properties which shouldn't be duplicated or
	 * are unsupported by the script serialization
	 *
	 * Updates the value of ObjName to match the name of the sequence.
	 */
	virtual void PostEditImport();

	/**
	 * Called after this object is renamed; updates the value of ObjName to match the name of the sequence.
	 */
	virtual void PostRename();

	/**
	 * Called after duplication & serialization and before PostLoad.
	 *
	 * Updates the value of ObjName to match the name of the sequence.
	 */
	virtual void PostDuplicate();
}

DefaultProperties
{
	ObjName="Prefabs"
	bDeletable=false
}
