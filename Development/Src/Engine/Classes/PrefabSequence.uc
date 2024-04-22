/**
 * Sequence for a PrefabInstance.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class PrefabSequence extends Sequence
	native(inherit);

/**
 * the PrefabInstance actor that created this PrefabSequence.
 */
var		protected{protected}	PrefabInstance	OwnerPrefab;

cpptext
{
	/* === USequenceObject interface === */
	/**
	 * Provides a way for non-deletable SequenceObjects (those with bDeletable=false) to be removed programatically.  The
	 * user will not be able to remove this object from the sequence via the UI, but calls to RemoveObject will succeed.
	 */
	virtual UBOOL IsDeletable() const { return TRUE; }

	/* === UObject interface === */
	virtual void PostLoad();

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

/**
 * Accessor for setting the value of OwnerPrefab.
 *
 * @param	InOwner		the PrefabInstance that created this PrefabSequence.
 */
native final function SetOwnerPrefab( PrefabInstance InOwner );

/**
 * Wrapper for retrieving the current value of OwnerPrefab.
 *
 * @return	a reference to the PrefabInstance that created this PrefabSequence
 */
native final function PrefabInstance GetOwnerPrefab() const;

DefaultProperties
{
	ObjName="PrefabSequence"
	bDeletable=false
}
