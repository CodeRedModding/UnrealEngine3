/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/** 
 * Defines the parameters for an in world non looping ambient sound e.g. a car driving by
 */

class SoundNodeAmbientNonLoop extends SoundNodeAmbient
	native( Sound )
	hidecategories( Object )
	AutoExpandCategories( Delay )
	dontsortcategories( Delay )
	dependson( SoundNodeAttenuation )
	editinlinenew;

var( Delay )			float					DelayMin<ToolTip=The lower bound of the delay in seconds>;
var( Delay )			float					DelayMax<ToolTip=The upper bound of the delay in seconds>;

var			deprecated	rawdistributionfloat	DelayTime;

defaultproperties
{
	Begin Object Class=DistributionFloatUniform Name=DistributionDelayTime
		Min=1
		Max=1
	End Object
	DelayTime=(Distribution=DistributionDelayTime)
}
