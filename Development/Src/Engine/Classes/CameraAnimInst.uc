
/**
 *	CameraAnim: defines a pre-packaged animation to be played on a camera.
 * 	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class CameraAnimInst extends Object
	notplaceable
	native(Camera);

/** which CameraAnim this is an instance of */
var CameraAnim					CamAnim;

/** the InterpGroupInst used to do the interpolation */
var protected instanced InterpGroupInst	InterpGroupInst;

/** Current time for the animation */
var protected transient float	CurTime;
/** True if the animation should loop, false otherwise. */
var protected transient bool	bLooping;
/** True if the animation has finished, false otherwise. */
var transient bool				bFinished;
/** True if it's ok for the system to auto-release this instance upon completion. */
var transient bool				bAutoReleaseWhenFinished;

/** Time to interpolate in from zero, for smooth starts. */
var protected float				BlendInTime;
/** Time to interpolate out to zero, for smooth finishes. */
var protected float				BlendOutTime;
/** True if currently blending in. */
var protected transient bool	bBlendingIn;
/** True if currently blending out. */
var protected transient bool	bBlendingOut;
/** Current time for the blend-in.  I.e. how long we have been blending. */
var protected transient float	CurBlendInTime;
/** Current time for the blend-out.  I.e. how long we have been blending. */
var protected transient float	CurBlendOutTime;

/** Multiplier for playback rate.  1.0 = normal. */
var protected float				PlayRate;

/** "Intensity" scalar.  This is the scale at which the anim was first played.  */
var float						BasePlayScale;
/** A supplemental scale factor, allowing external systems to scale this anim as necessary.  This is reset to 1.f each frame. */
var float						TransientScaleModifier;


/* Number in range [0..1], controlling how much this influence this instance should have. */
var float						CurrentBlendWeight;

/** How much longer to play the anim, if a specific duration is desired.  Has no effect if 0.  */
var protected transient float	RemainingTime;

/** cached movement track from the currently playing anim so we don't have to go find it every frame */
var transient InterpTrackMove		MoveTrack;
var transient InterpTrackInstMove	MoveInst;

/** Ref to the AnimNodeSequence that's instigating this anim.  Can be None. */
var protected transient AnimNodeSequence	SourceAnimNode;	

var protectedwrite ECameraAnimPlaySpace PlaySpace;
/** The user-defined space for CAPS_UserDefined */
var transient matrix					UserPlaySpaceMatrix;

/** PP settings stored for this inst, to be applied at the proper time */
var transient PostProcessSettings		LastPPSettings;
var transient float						LastPPSettingsAlpha;

/** Camera Anim debug variable to trace back to previous location **/
var transient vector                    LastCameraLoc;

cpptext
{
	void RegisterAnimNode(class UAnimNodeSequence* AnimNode);
};

/**
 * Starts this instance playing the specified CameraAnim.
 *
 * CamAnim:		The animation that should play on this instance.
 * CamActor:	The Actor that will be modified by this animation.
 * InRate:		How fast to play the animation.  1.f is normal.
 * InScale:		How intense to play the animation.  1.f is normal.
 * InBlendInTime:	Time over which to linearly ramp in.
 * InBlendInTime:	Time over which to linearly ramp out.
 * bInLoop:			Whether or not to loop the animation.
 * bRandomStartTime:	Whether or not to choose a random time to start playing.  Only really makes sense for bLoop = TRUE;
 * Duration:	optional specific playtime for this animation.  This is total time, including blends.
 */
native final function Play(CameraAnim Anim, Actor CamActor, float InRate, float InScale, float InBlendInTime, float InBlendOutTime, bool bInLoop, bool bRandomStartTime, optional float Duration);

/** Update this instance with new parameters. */
native final function Update(float NewRate, float NewScale, float NewBlendInTime, float NewBlendOutTime, optional float NewDuration);

/** advances the animation by the specified time - updates any modified interp properties, moves the group actor, etc */
native final function AdvanceAnim(float DeltaTime, bool bJump);

/** Stops this instance playing whatever animation it is playing. */
native final function Stop(optional bool bImmediate);

/** Applies given scaling factor to the playing animation for the next update only. */
native final function ApplyTransientScaling(float Scalar);

/** Sets this anim to play in an alternate playspace */
native final function SetPlaySpace(ECameraAnimPlaySpace NewSpace, optional rotator UserPlaySpace);


defaultproperties
{
	bFinished=true
	bAutoReleaseWhenFinished=true
	PlayRate=1.f
	TransientScaleModifier=1.f

	PlaySpace=CAPS_CameraLocal

	Begin Object Class=InterpGroupInst Name=InterpGroupInst0
	End Object
	InterpGroupInst=InterpGroupInst0
}
