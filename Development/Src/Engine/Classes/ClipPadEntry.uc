/* epic ===============================================
* class ClipPadEntry
*
* A block of text that can be pasted into the level.
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ClipPadEntry extends Object
	native
	hidecategories(Object);

/** User specified name */
var() string	Title;

/** The text copied/pasted */
var() string	Text;

cpptext
{
}

defaultproperties
{
	Title="Untitled"
}
