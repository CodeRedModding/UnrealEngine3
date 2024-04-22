/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class CurveEdPreset_Nothing extends CurveEdPresetBase
	native
	editinlinenew
	hidecategories(Object);

/** Virtual function to get the user-readable name for the curve	*/
function string GetDisplayName()
{
	local string RetVal;
	
	RetVal = "Do not preset";
	
	return RetVal;
}

/** */
cpptext
{
}

/** */
defaultproperties
{
}
