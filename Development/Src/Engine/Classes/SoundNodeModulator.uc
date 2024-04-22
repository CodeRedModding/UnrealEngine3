/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
 
/** 
 * Defines a random volume and pitch modification when a sound starts
 */
 
class SoundNodeModulator extends SoundNode
	native( Sound )
	hidecategories( Object )
	editinlinenew;

var( Modulation )		float					PitchMin<ToolTip=The lower bound of pitch (1.0 is no change)>;
var( Modulation )		float					PitchMax<ToolTip=The upper bound of pitch (1.0 is no change)>;

var( Modulation )		float					VolumeMin<ToolTip=The lower bound of volume (1.0 is no change)>;
var( Modulation )		float					VolumeMax<ToolTip=The upper bound of volume (1.0 is no change)>;

var			deprecated	rawdistributionfloat	PitchModulation;
var			deprecated	rawdistributionfloat	VolumeModulation;

defaultproperties
{
	PitchMin=0.95
	PitchMax=1.05
	VolumeMin=0.95
	VolumeMax=1.05
	
	// deprecated defaults
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
