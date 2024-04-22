/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
 
/** 
 * Defines a delay
 */ 
 
class SoundNodeDelay extends SoundNode
	native( Sound )
	hidecategories( Object )
	editinlinenew;

var( Delay )			float					DelayMin<ToolTip=The lower bound of delay time in seconds>;
var( Delay )			float					DelayMax<ToolTip=The upper bound of delay time in seconds>;

var			deprecated	rawdistributionfloat	DelayDuration;

defaultproperties
{
	DelayMin=0
	DelayMax=0

	// deprecated defaults
	Begin Object Class=DistributionFloatUniform Name=DistributionDelayDuration
		Min=0
		Max=0
	End Object
	DelayDuration=(Distribution=DistributionDelayDuration)
}
