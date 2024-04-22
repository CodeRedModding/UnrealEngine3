/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
 
/** 
 * Defines how a sound loops; either indefinitely, or for a set number of times
 */
  
class SoundNodeLooping extends SoundNode
	native( Sound )
	hidecategories( Object )
	editinlinenew;

var( Looping )			bool					bLoopIndefinitely;
var( Looping )			float					LoopCountMin<ToolTip=The lower bound of number of times to loop>;
var( Looping )			float					LoopCountMax<ToolTip=The upper bound of number of times to loop>;

var			deprecated	rawdistributionfloat	LoopCount;

defaultproperties
{
	bLoopIndefinitely=TRUE
	LoopCountMin=1000000
	LoopCountMax=1000000
	
	// deprecated defaults
	Begin Object Class=DistributionFloatUniform Name=DistributionLoopCount
		Min=1000000
		Max=1000000
	End Object
	LoopCount=(Distribution=DistributionLoopCount)
}
