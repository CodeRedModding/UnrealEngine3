/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 * Deletes selected objects.
 */
class GeomModifier_Delete
	extends GeomModifier_Edit
	native;
	
cpptext
{
	/**
	 * @return		TRUE if this modifier will work on the currently selected sub objects.
	 */
	virtual UBOOL Supports();

protected:
	/**
	 * Implements the modifier application.
	 */
 	virtual UBOOL OnApply();
}
	
defaultproperties
{
	Description="Delete"
	bPushButton=True
}
