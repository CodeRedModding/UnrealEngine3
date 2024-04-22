/**
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */


class SeqAct_SetMotionBlurParams extends SeqAct_Latent
	native(Sequence);
	
/** This is a scalar on the blur */
var() float MotionBlurAmount;
/** Time to interpolate values over */
var() float InterpolateSeconds;
/** Elapsed interpolation time */
var float InterpolateElapsed;

// Previous values, used in lerp()
var float OldMotionBlurAmount;

cpptext
{
	void Activated();
	void DeActivated();
	virtual UBOOL UpdateOp(FLOAT DeltaTime);
};


defaultproperties
{
	InterpolateElapsed = 0
	InterpolateSeconds = 2
	ObjName="Motion Blur"
	ObjCategory="Camera"

	// typical settings
	MotionBlurAmount = 0.1f;

	InputLinks(0)=(LinkDesc="Enable")
	InputLinks(1)=(LinkDesc="Disable")
}
