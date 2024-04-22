/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
 
/** 
 * A node to play sounds sequentially
 * WARNING: these are not seamless
 */
 
class SoundNodeConcatenator extends SoundNode
	native( Sound )
	hidecategories( Object )
	editinlinenew;

/** A volume for each input.  Automatically sized. */
var() export editfixedsize array<float>	InputVolume;

defaultproperties
{
}
