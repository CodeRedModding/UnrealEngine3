/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 * 
 * Object that encapsulates parameters for defining a camera shake.
 * a code-driven (oscillating) screen shake.
 */

class CameraShake extends Object
	editinlinenew
	native(Camera);

/** 
 *  TRUE to only allow a single instance of this shake to play at any given time. 
 *  Subsequents attempts to play this shake will simply restart the timer.
 */
var() bool			bSingleInstance;


/************************************************************
 * Parameters for defining oscillating camera shakes
 ************************************************************/

/** Shake start offset parameter */
enum EInitialOscillatorOffset
{
	EOO_OffsetRandom,	// Start with random offset (default)
	EOO_OffsetZero,		// Start with zero offset
};

/** Defines oscillation of a single number. */
struct native FOscillator
{
	var() float Amplitude;
	var() float Frequency;
	var() EInitialOscillatorOffset InitialOffset;
};

/** Defines rotator oscillation. */
struct native ROscillator
{
	var() FOscillator Pitch;
	var() FOscillator Yaw;
	var() FOscillator Roll;
};

/** Defines vector oscillation. */
struct native VOscillator
{
	var() FOscillator X;
	var() FOscillator Y;
	var() FOscillator Z;
};


/** Duration in seconds of current screen shake. <0 means indefinite, 0 means no oscillation */
var(Oscillation)	float	OscillationDuration;

var(Oscillation)	float   OscillationBlendInTime<ClampMin=0.0>;
var(Oscillation)	float   OscillationBlendOutTime<ClampMin=0.0>;

/** Rotational oscillation */
var(Oscillation) ROscillator RotOscillation;
/** Positional oscillation */
var(Oscillation) VOscillator LocOscillation;
/** FOV oscillation */
var(Oscillation) FOscillator FOVOscillation;



/************************************************************
 * Parameters for defining CameraAnim-driven camera shakes
 ************************************************************/

var(AnimShake) CameraAnim	Anim;

/** Scalar defining how fast to play the anim. */
var(AnimShake) float		AnimPlayRate<ClampMin=0.001>;
/** Scalar defining how "intense" to play the anim. */
var(AnimShake) float		AnimScale<ClampMin=0.0>;
/** Linear blend-in time. */
var(AnimShake) float		AnimBlendInTime<ClampMin=0.0>;
/** Linear blend-out time. */
var(AnimShake) float		AnimBlendOutTime<ClampMin=0.0>;

/**
 * If TRUE, play a random snippet of the animation of length Duration.  Implies bLoop and bRandomStartTime = TRUE for the CameraAnim.
 * If FALSE, play the full anim once, non-looped.
 */
var(AnimShake) bool			bRandomAnimSegment;
/** When bRandomAnimSegment=true, this defines how long the anim should play. */
var(AnimShake) float		RandomAnimSegmentDuration<ClampMin=0.0 | EditCondition=bRandomAnimSegment>;


simulated function float GetRotOscillationMagnitude()
{
	local vector V;
	V.X = RotOscillation.Pitch.Amplitude;
	V.Y = RotOscillation.Yaw.Amplitude;
	V.Z = RotOscillation.Roll.Amplitude;
	return VSize(V);
}
simulated function float GetLocOscillationMagnitude()
{
	local vector V;
	V.X = LocOscillation.X.Amplitude;
	V.Y = LocOscillation.Y.Amplitude;
	V.Z = LocOscillation.Z.Amplitude;
	return VSize(V);
}


defaultproperties
{
	AnimPlayRate=1.f
	AnimScale=1.f
	AnimBlendInTime=0.2f
	AnimBlendOutTime=0.2f
	OscillationBlendInTime=0.1f
	OscillationBlendOutTime=0.2f
}
