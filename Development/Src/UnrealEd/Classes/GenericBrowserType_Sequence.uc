/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
//=============================================================================
// GenericBrowserType_Sequence: Sequences
//=============================================================================

class GenericBrowserType_Sequence
	extends GenericBrowserType
	native;

cpptext
{
	virtual void Init();

	/**
	 * Determines whether the specified object is a USequence class that should be handled by this generic browser type.
	 *
	 * @param	Object	a pointer to a USequence object.
	 *
	 * @return	TRUE if this generic browser type supports to object specified.
	 */
	static UBOOL IsSequenceTypeSupported( UObject* Object );
}

defaultproperties
{
	Description="Sequences"
}
