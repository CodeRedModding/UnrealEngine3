/** 
 * This class is the base class for waveforms that can be played via AnimNotify_Rumble or any of the other PlayWaveform functions that exist
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class WaveFormBase extends Object
	native
	abstract;

/** This is the waveform data **/
var ForceFeedbackWaveform TheWaveForm;


DefaultProperties
{
	Begin Object Class=ForceFeedbackWaveform Name=ForceFeedbackWaveform
	End Object
	TheWaveForm=ForceFeedbackWaveform
}