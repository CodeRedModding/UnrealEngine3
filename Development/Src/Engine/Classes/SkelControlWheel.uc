class SkelControlWheel extends SkelControlSingleBone
	hidecategories(Translation,Rotation)
	native(Anim);

/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 *	Controller used by vehicle system for moving/rotating wheel.
 */


cpptext
{
	// SkelControlWheel interface
	void UpdateWheelControl( FLOAT InDisplacement, FLOAT InRoll, FLOAT InSteering );
	void CalculateNewBoneTransforms(INT BoneIndex, USkeletalMeshComponent* SkelComp, TArray<FBoneAtom>& OutBoneTransforms);
}

/** Units to move the wheel up vertically. */
var(Wheel)		transient float	WheelDisplacement;

/** Maximum displacement that the wheel will be rendered at. Used to avoid graphical clipping of wheel into chassis etc */
var(Wheel)		float	WheelMaxRenderDisplacement;

/** Current rolling angle of wheel. In degrees. */
var(Wheel)		transient float	WheelRoll;

/** Axis around which the wheel rolls. */
var(Wheel)		EAxis	WheelRollAxis;

/** Steering angle of wheel. In degrees. */
var(Wheel)		transient float	WheelSteering;

/** Axis around which wheel steering occurs. */
var(Wheel)		EAxis	WheelSteeringAxis;

/** If we should invert the rotation applied to the wheel for rolling motion. */
var(Wheel)		bool	bInvertWheelRoll;

/** If we should invert rotation applied to the wheel for steering. */
var(Wheel)		bool	bInvertWheelSteering;

defaultproperties
{
	bApplyTranslation=true
	bAddTranslation=true
	BoneTranslationSpace=BCS_BoneSpace

	bApplyRotation=true
	bAddRotation=true
	BoneRotationSpace=BCS_BoneSpace

	WheelRollAxis=AXIS_X
	WheelSteeringAxis=AXIS_Z
	WheelMaxRenderDisplacement=50.0
	bIgnoreWhenNotRendered=true
}
