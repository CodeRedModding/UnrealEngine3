/* epic ===============================================
* class EdCoordSystem
*
* A custom coordinate system used by the editor.
*
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class EdCoordSystem extends Object
	hidecategories(Object)
	editinlinenew
	native;

/** The matrix that defines this coordinate system. */
var()	matrix	M;

/* A human readable description for use in the editor UI. */
var()	string	Desc;

defaultproperties
{
	Desc="Coord System"
	M=(XPlane=(X=1,Y=0,Z=0,W=0),YPlane=(X=0,Y=1,Z=0,W=0),ZPlane=(X=0,Y=0,Z=1,W=0),WPlane=(X=0,Y=0,Z=0,W=1))
}
