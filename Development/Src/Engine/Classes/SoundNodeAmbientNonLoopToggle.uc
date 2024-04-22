/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/** 
 * Defines the parameters for an in world non looping ambient sound e.g. a car driving by
 */

class SoundNodeAmbientNonLoopToggle extends SoundNodeAmbientNonLoop
	native( Sound )
	hidecategories( Object )
	dependson( SoundNodeAttenuation )
	editinlinenew;

defaultproperties
{
}
