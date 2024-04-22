/** 
 * This class is the base class for Gear waveforms
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class GameWaveForms extends Object;

/** The forcefeedback waveform to play with a particular camera shakes */
var ForceFeedbackWaveform CameraShakeMediumShort;

/** The forcefeedback waveform to play with a particular camera shakes */
var ForceFeedbackWaveform CameraShakeMediumLong;

/** The forcefeedback waveform to play with a particular camera shakes */
var ForceFeedbackWaveform CameraShakeBigShort;

/** The forcefeedback waveform to play with a particular camera shakes */
var ForceFeedbackWaveform CameraShakeBigLong;

defaultproperties
{
	Begin Object Class=ForceFeedbackWaveform Name=ForceFeedbackWaveform7
	    Samples(0)=(LeftAmplitude=60,RightAmplitude=60,LeftFunction=WF_LinearDecreasing,RightFunction=WF_LinearDecreasing,Duration=0.500)
	End Object
	CameraShakeMediumShort=ForceFeedbackWaveform7

	Begin Object Class=ForceFeedbackWaveform Name=ForceFeedbackWaveform8
	    Samples(0)=(LeftAmplitude=60,RightAmplitude=60,LeftFunction=WF_LinearDecreasing,RightFunction=WF_LinearDecreasing,Duration=1.500)
	End Object
	CameraShakeMediumLong=ForceFeedbackWaveform8

    Begin Object Class=ForceFeedbackWaveform Name=ForceFeedbackWaveform9
      	Samples(0)=(LeftAmplitude=100,RightAmplitude=100,LeftFunction=WF_LinearDecreasing,RightFunction=WF_LinearDecreasing,Duration=0.500)
	End Object
	CameraShakeBigShort=ForceFeedbackWaveform9

	Begin Object Class=ForceFeedbackWaveform Name=ForceFeedbackWaveform10
	     Samples(0)=(LeftAmplitude=100,RightAmplitude=100,LeftFunction=WF_LinearDecreasing,RightFunction=WF_LinearDecreasing,Duration=1.500)
	End Object
	CameraShakeBigLong=ForceFeedbackWaveform10

}






