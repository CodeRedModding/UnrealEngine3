/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class GameThirdPersonCameraMode_Default extends GameThirdPersonCameraMode
	config(Camera)
	native(Camera);

cpptext
{
	/**
	 * Returns location and rotation, in world space, of the camera's basis point.  The camera will rotate
	 * around this point, offsets are applied from here, etc.
	 */
	virtual void GetCameraOrigin(class APawn* TargetPawn, FVector& OriginLoc, FRotator& OriginRot);
};


/** Z adjustment to camera worst location if target pawn is in aiming stance */
var() protected const float WorstLocAimingZOffset;

var protected transient bool	bTemporaryOriginRotInterp;
var() protected const float		TemporaryOriginRotInterpSpeed;


defaultproperties
{
	TemporaryOriginRotInterpSpeed=12.f

	WorstLocOffset=(X=-8,Y=1,Z=95)
	WorstLocAimingZOffset=-10
	bValidateWorstLoc=FALSE

	ViewOffset={(
		OffsetHigh=(X=-128,Y=56,Z=40),
		OffsetLow=(X=-160,Y=48,Z=56),
		OffsetMid=(X=-160,Y=48,Z=16),
		)}
	ViewOffset_ViewportAdjustments(CVT_16to9_HorizSplit)={(
		OffsetHigh=(X=0,Y=0,Z=-12),
		OffsetLow=(X=0,Y=0,Z=-12),
		OffsetMid=(X=0,Y=0,Z=-12),
		)}
	ViewOffset_ViewportAdjustments(CVT_16to9_VertSplit)={(
		OffsetHigh=(X=0,Y=-20,Z=0),
		OffsetLow=(X=0,Y=-20,Z=0),
		OffsetMid=(X=0,Y=-20,Z=0),
		)}
	ViewOffset_ViewportAdjustments(CVT_4to3_Full)={(
		OffsetHigh=(X=0,Y=0,Z=17),
		OffsetLow=(X=0,Y=0,Z=17),
		OffsetMid=(X=0,Y=0,Z=17),
		)}
	ViewOffset_ViewportAdjustments(CVT_4to3_HorizSplit)={(
		OffsetHigh=(X=0,Y=0,Z=-15),
		OffsetLow=(X=0,Y=0,Z=-15),
		OffsetMid=(X=0,Y=0,Z=-15),
		)}
	ViewOffset_ViewportAdjustments(CVT_4to3_VertSplit)={(
		OffsetHigh=(X=0,Y=0,Z=0),
		OffsetLow=(X=0,Y=0,Z=0),
		OffsetMid=(X=0,Y=0,Z=0),
		)}

	StrafeLeftAdjustment=(X=0,Y=-15,Z=0)
	StrafeRightAdjustment=(X=0,Y=15,Z=0)
    StrafeOffsetScalingThreshold=200

	RunFwdAdjustment=(X=20,Y=0,Z=0)
	RunBackAdjustment=(X=-30,Y=0,Z=0)
	RunOffsetScalingThreshold=200

	BlendTime=0.25
}

