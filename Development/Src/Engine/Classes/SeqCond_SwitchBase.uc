/**
 * Base class for all condition sequence objects which act as switch constructs.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqCond_SwitchBase extends SequenceCondition
	native(inherit)
	abstract
	placeable;

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
	virtual UBOOL GetOutputLinksToActivate( TArray<INT>& out_LinksToActivate ) PURE_VIRTUAL(USeqCond_SwitchBase::GetOutputLinksToActivate,return FALSE;);

	/**
	 * Returns the index [into the switch op's array of values] that corresponds to the specified OutputLink.
	 *
	 * @param	OutputLinkIndex		index into [into the OutputLinks array] to find the corresponding value index for
	 *
	 * @return	INDEX_NONE if no value was found which matches the specified output link.
	 */
	virtual INT FindCaseValueIndex( INT OutputLinkIndex ) const PURE_VIRTUAL(USeqCond_SwitchBase::FindCaseValueIndex,return INDEX_NONE;);

	/** Returns the number of elements in this switch op's array of values. */
	virtual INT GetSupportedValueCount() const PURE_VIRTUAL(USeqCond_SwitchBase::GetSupportedValueCount,return 0;);

	/**
	 * Returns a string representation of the value at the specified index.  Used to populate the LinkDesc for the OutputLinks array.
	 */
	virtual FString GetCaseValueString( INT ValueIndex ) const PURE_VIRTUAL(USeqCond_SwitchBase::GetCaseValueString,return TEXT("NOT IMPLMENTED"););

	/* === USequenceOp interface === */
	/**
	 * Called when this sequence op is activated.  Determines which output link should be activated based on the value
	 * of the linked object var.
	 */
	virtual void Activated();
	virtual void UpdateDynamicLinks();

#if WITH_EDITOR
	/**
	 * Returns the color that should be used for an input, variable, or output link connector in the kismet editor.
	 *
	 * @param	ConnType	the type of connection this represents.  Valid values are:
	 *							LOC_INPUT		(input link)
	 *							LOC_OUTPUT		(output link)
	 *							LOC_VARIABLE	(variable link)
	 *							LOC_EVENT		(event link)
	 * @param	ConnIndex	the index [into the corresponding array (i.e. InputLinks, OutputLinks, etc.)] for the link
	 *						being queried.
	 * @param	MouseOverConnType
	 *						INDEX_NONE if the user is not currently mousing over the specified link connector.  One of the values
	 *						listed for ConnType otherwise.
	 * @param	MouseOverConnIndex
	 *						INDEX_NONE if the user is not currently mousing over the specified link connector.  The index for the
	 *						link being moused over otherwise.
	 */
	virtual FColor GetConnectionColor( INT ConnType, INT ConnIndex, INT MouseOverConnType, INT MouseOverConnIndex );
#endif
}

/* === Events === */
/**
 * Ensures that the last item in the value array represents the "default" item.  Child classes should override this method to ensure that
 * their value array stays synchronized with the OutputLinks array.
 */
event VerifyDefaultCaseValue();

/**
 * Returns whether fall through is enabled for the specified case value.
 */
event bool IsFallThruEnabled( int ValueIndex )
{
	// by default, fall thru is not enabled on anything
	return false;
}

/**
 * Insert an empty element into this switch's value array at the specified index.
 */
event InsertValueEntry( int InsertIndex );

/**
 * Remove an element from this switch's value array at the specified index.
 */
event RemoveValueEntry( int RemoveIndex );

DefaultProperties
{
	ObjCategory="Switch"
	OutputLinks(0)=(LinkDesc="Default")
	VariableLinks.Empty
}
