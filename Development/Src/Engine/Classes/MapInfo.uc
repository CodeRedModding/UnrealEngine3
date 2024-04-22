/**
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */


/** contains game-specific map data, attached to the WorldInfo */
class MapInfo extends Object
	native
	abstract
	editinlinenew;

cpptext
{
#if WITH_EDITOR
	virtual void CheckForErrors() {}
#endif
}

