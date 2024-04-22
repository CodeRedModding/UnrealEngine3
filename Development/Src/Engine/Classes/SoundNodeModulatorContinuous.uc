/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
 
/** 
 * Defines a random volume and pitch modification as a sound plays
 */
  
class SoundNodeModulatorContinuous extends SoundNode
	native( Sound )
	hidecategories( Object )
	editinlinenew;

/* 
 * NOTE:  If you have a looping sound the PlaybackTime will keep increasing.  And PlaybackTime
 * is what is used to get values from the Distributions.   So the Modulation will work the first
 * time through but subsequent times will not work for distributions with have a "size" to them.
 *
 * In short using a SoundNodeModulatorContinuous for looping sounds is not advised. 
 */
var()			rawdistributionfloat	PitchModulation;
var()			rawdistributionfloat	VolumeModulation;

defaultproperties
{
	// defaults
	Begin Object Class=DistributionFloatUniform Name=DistributionPitch
		Min=0.95
		Max=1.05
	End Object
	PitchModulation=(Distribution=DistributionPitch)

	Begin Object Class=DistributionFloatUniform Name=DistributionVolume
		Min=0.95
		Max=1.05
	End Object
	VolumeModulation=(Distribution=DistributionVolume)
}
