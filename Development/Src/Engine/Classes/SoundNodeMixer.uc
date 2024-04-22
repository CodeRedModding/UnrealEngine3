/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
 
/** 
 * Defines how concurrent sounds are mixed together
 */
 
class SoundNodeMixer extends SoundNode
	native( Sound )
	hidecategories( Object )
	editinlinenew;

/** A volume for each input.  Automatically sized. */
var()	export	editfixedsize	array<float>	InputVolume;

defaultproperties
{
}
