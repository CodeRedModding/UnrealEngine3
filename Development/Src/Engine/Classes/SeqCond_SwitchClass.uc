/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqCond_SwitchClass extends SeqCond_SwitchBase
	native(Sequence);

cpptext
{
	/**
	 * Returns the index of the OutputLink to activate for the specified object.
	 *
	 * @param	out_LinksToActivate
	 *						the indexes [into the OutputLinks array] for the most appropriate OutputLinks to activate
	 *						for the specified object, or INDEX_NONE if none are found.  Should only contain 0 or 1 elements
	 *						unless one of the matching cases is configured to fall through.
	 *
	 * @return	TRUE if at least one match was found, FALSE otherwise.
	 */
	virtual UBOOL GetOutputLinksToActivate( TArray<INT>& out_LinksToActivate );

	/**
	 * Returns the index [into the switch op's array of values] that corresponds to the specified OutputLink.
	 *
	 * @param	OutputLinkIndex		index into [into the OutputLinks array] to find the corresponding value index for
	 *
	 * @return	INDEX_NONE if no value was found which matches the specified output link.
	 */
	virtual INT FindCaseValueIndex( INT OutputLinkIndex ) const;

	/** Returns the number of elements in this switch op's array of values. */
	virtual INT GetSupportedValueCount() const;

	/**
	 * Returns a string representation of the value at the specified index.  Used to populate the LinkDesc for the OutputLinks array.
	 */
	virtual FString GetCaseValueString( INT ValueIndex ) const;
}

/** Stores class name to compare for each output link and whether it should fall through to next node */
struct native SwitchClassInfo
{
	var() Name ClassName;
	var() Byte bFallThru;
};
var() array<SwitchClassInfo> ClassArray;

/* === Events === */
/**
 * Ensures that the last item in the value array represents the "default" item.  Child classes should override this method to ensure that
 * their value array stays synchronized with the OutputLinks array.
 */
event VerifyDefaultCaseValue()
{
	Super.VerifyDefaultCaseValue();

	ClassArray.Length = OutputLinks.Length;
	ClassArray[ClassArray.Length-1].ClassName = 'Default';
	ClassArray[ClassArray.Length-1].bFallThru = 0;
}

/**
 * Returns whether fall through is enabled for the specified case value.
 */
event bool IsFallThruEnabled( int ValueIndex )
{
	// by default, fall thru is not enabled on anything
	return ValueIndex >= 0 && ValueIndex < ClassArray.Length && ClassArray[ValueIndex].bFallThru != 0;
}

/**
 * Insert an empty element into this switch's value array at the specified index.
 */
event InsertValueEntry( int InsertIndex )
{
	InsertIndex = Clamp(InsertIndex, 0, ClassArray.Length);

	ClassArray.Insert(InsertIndex, 1);
}

/**
 * Remove an element from this switch's value array at the specified index.
 */
event RemoveValueEntry( int RemoveIndex )
{
	if ( RemoveIndex >= 0 && RemoveIndex < ClassArray.Length )
	{
		ClassArray.Remove(RemoveIndex, 1);
	}
}

defaultproperties
{
	ObjName="Switch Class"

	InputLinks(0)=(LinkDesc="In")
	OutputLinks(0)=(LinkDesc="Default")
	ClassArray(0)=(ClassName=Default)

	VariableLinks(0)=(ExpectedType=class'SeqVar_Object',LinkDesc="Object")
}
