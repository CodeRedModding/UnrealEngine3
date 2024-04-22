/**
 * Base class for all switch condition ops which use an object value for branching.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqCond_SwitchObject extends SeqCond_SwitchBase
	native(inherit);

cpptext
{
	/* === USeqCond_SwitchBase interface === */
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
struct native SwitchObjectCase
{
	/** the value of this case statement */
	var() Object	ObjectValue;

	/** indicates whether control should fall through to the next case upon a match*/
	var() bool		bFallThru;

	/** true if this represents the default value option */
	var() bool		bDefaultValue;
};

/**
 * Stores the list of values which are handled by this switch object.
 */
var() array<SwitchObjectCase>	SupportedValues;

/* === Events === */
/**
 * Ensures that the last item in the value array represents the "default" item.  Child classes should override this method to ensure that
 * their value array stays synchronized with the OutputLinks array.
 */
event VerifyDefaultCaseValue()
{
	local int i;

	Super.VerifyDefaultCaseValue();

	SupportedValues.Length = OutputLinks.Length;
	for ( i = 0; i < SupportedValues.Length - 1; i++ )
	{
		SupportedValues[i].bDefaultValue = false;
	}

	SupportedValues[SupportedValues.Length-1].ObjectValue = None;
	SupportedValues[SupportedValues.Length-1].bFallThru = false;
	SupportedValues[SupportedValues.Length-1].bDefaultValue = true;
}

/**
 * Returns whether fall through is enabled for the specified case value.
 */
event bool IsFallThruEnabled( int ValueIndex )
{
	// by default, fall thru is not enabled on anything
	return ValueIndex >= 0 && ValueIndex < SupportedValues.Length && SupportedValues[ValueIndex].bFallThru;
}

/**
 * Insert an empty element into this switch's value array at the specified index.
 */
event InsertValueEntry( int InsertIndex )
{
	InsertIndex = Clamp(InsertIndex, 0, SupportedValues.Length);

	SupportedValues.Insert(InsertIndex, 1);
}

/**
 * Remove an element from this switch's value array at the specified index.
 */
event RemoveValueEntry( int RemoveIndex )
{
	if ( RemoveIndex >= 0 && RemoveIndex < SupportedValues.Length )
	{
		SupportedValues.Remove(RemoveIndex, 1);
	}
}

DefaultProperties
{
	SupportedValues(0)=(bDefaultValue=true)

	ObjName="Switch Object"
	VariableLinks(0)=(ExpectedType=class'SeqVar_Object',LinkDesc="Object")
}
