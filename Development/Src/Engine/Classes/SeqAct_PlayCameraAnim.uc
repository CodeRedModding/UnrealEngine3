/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_PlayCameraAnim extends SequenceAction
	dependson(CameraAnimInst)
	native(Sequence);

/** Reference to CameraAnim to play. */
var()	CameraAnim		CameraAnim;

/** True to loop the animation, false otherwise. */
var()	bool			bLoop;

/** Time to interpolate in from zero, for smooth starts. */
var()	float			BlendInTime<ClampMin=0.0>;

/** Time to interpolate out to zero, for smooth finishes. */
var()	float			BlendOutTime<ClampMin=0.0>;

/** Rate to play.  1.0 is normal. */
var()	float			Rate<ClampMin=0.001>;

/** Scalar for intensity.  1.0 is normal. */
var()	float			IntensityScale;

/** True to start the animation at a random time (good for things like looping shakes) */
var()	bool			bRandomStartTime;

/** Space in which to play the camera anim */
var()	ECameraAnimPlaySpace	PlaySpace;
/** Actor to use to specify the space for CAPS_UserDefined */
var()	Actor					UserDefinedSpaceActor;

cpptext
{
	void Activated();
};

/**
 * Return the version number for this class.  Child classes should increment this method by calling Super then adding
 * a individual class version to the result.  When a class is first created, the number should be 0; each time one of the
 * link arrays is modified (VariableLinks, OutputLinks, InputLinks, etc.), the number that is added to the result of
 * Super.GetObjClassVersion() should be incremented by 1.
 *
 * @return	the version number for this specific class.
 */
static event int GetObjClassVersion()
{
	return Super.GetObjClassVersion() + 1;
}

defaultproperties
{
	ObjName="Play CameraAnim"
	ObjCategory="Camera"

	InputLinks(0)=(LinkDesc="Play")
	InputLInks(1)=(LinkDesc="Stop")

	OutputLinks(0)=(LinkDesc="Out")

	BlendInTime=0.2f
	BlendOutTime=0.2f
	Rate=1.f
	IntensityScale=1.f
	PlaySpace=CAPS_CameraLocal
}




