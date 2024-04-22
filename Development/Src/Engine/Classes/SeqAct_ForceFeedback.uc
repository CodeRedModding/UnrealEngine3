/**
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */


class SeqAct_ForceFeedback extends SequenceAction;

var() editinline ForceFeedbackWaveform FFWaveform;

/** A predefined WaveForm, only works with start, stopping a predefined doesn't do anything, can't use for looping **/
var() class<WaveFormBase> PredefinedWaveForm<AllowAbstract>;

defaultproperties
{
	ObjName="Force Feedback"
	ObjCategory="Misc"

	InputLinks(0)=(LinkDesc="Start")
	InputLinks(1)=(LinkDesc="Stop")
}
