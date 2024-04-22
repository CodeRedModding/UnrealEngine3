/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/** 
 * Defines how a sound oscillates
 */

class SoundNodeOscillator extends SoundNode
	native( Sound )
	hidecategories( Object )
	editinlinenew;

var( Oscillator )		bool					bModulateVolume<ToolTip=Whether to oscillate volume>;
var( Oscillator )		bool					bModulatePitch<ToolTip=Whether to oscillate pitch>;

var( Oscillator )		float					AmplitudeMin<ToolTip=An amplitude of 0.25 would oscillate between 0.75 and 1.25>;
var( Oscillator )		float					AmplitudeMax<ToolTip=An amplitude of 0.25 would oscillate between 0.75 and 1.25>;
var( Oscillator )		float					FrequencyMin<ToolTip=A frequency of 20 would oscillate at 10Hz>;
var( Oscillator )		float					FrequencyMax<ToolTip=A frequency of 20 would oscillate at 10Hz>;
var( Oscillator )		float					OffsetMin<ToolTip=Offset into the sine wave. Value modded by 2 * PI>;
var( Oscillator )		float					OffsetMax<ToolTip=Offset into the sine wave. Value modded by 2 * PI>;
var( Oscillator )		float					CenterMin<ToolTip=A center of 0.5 would oscillate around 0.5>;
var( Oscillator )		float					CenterMax<ToolTip=A center of 0.5 would oscillate around 0.5>;

var			deprecated	rawdistributionfloat	Amplitude;
var			deprecated	rawdistributionfloat	Frequency;
var			deprecated	rawdistributionfloat	Offset;
var			deprecated	rawdistributionfloat	Center;

defaultproperties
{
	AmplitudeMin=0
	AmplitudeMax=0
	FrequencyMin=0
	FrequencyMax=0
	OffsetMin=0
	OffsetMax=0
	CenterMin=0
	CenterMax=0
	bModulateVolume=false
	bModulatePitch=false

	// deprecated defaults
	Begin Object Class=DistributionFloatUniform Name=DistributionAmplitude
		Min=0
		Max=0
	End Object
	Amplitude=(Distribution=DistributionAmplitude)

	Begin Object Class=DistributionFloatUniform Name=DistributionFrequency
		Min=0
		Max=0
	End Object
	Frequency=(Distribution=DistributionFrequency)

	Begin Object Class=DistributionFloatUniform Name=DistributionOffset
		Min=0
		Max=0
	End Object
	Offset=(Distribution=DistributionOffset)

	Begin Object Class=DistributionFloatUniform Name=DistributionCenter
		Min=0
		Max=0
	End Object
	Center=(Distribution=DistributionCenter)
}
