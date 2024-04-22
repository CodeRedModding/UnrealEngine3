class SkelControlHandlebars extends SkelControlSingleBone
	hidecategories(Translation,Rotation)
	native(Anim);

/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 *	Controller used by vehicle system for rotating a steering wheel.
 *  Automatically orients a handlebar/steeringwheel based on the wheel orientation
 */

cpptext
{
	// SkelControl interface
	void CalculateNewBoneTransforms(INT BoneIndex, USkeletalMeshComponent* SkelComp, TArray<FBoneAtom>& OutBoneTransforms);

	// Editor modification
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
}

/** Axis around which wheel rolling occurs. */
var(Handlebars)		EAxis	WheelRollAxis;

/** Name of the bone whose rotation will control the steering */
var(Handlebars)		name	WheelBoneName;

/** Axis around which steering occurs. */
var(Handlebars)		EAxis	HandlebarRotateAxis;

var(HandleBars)		bool	bInvertRotation;

/** Cached index of the wheel bone */ 
var int SteerWheelBoneIndex;

defaultproperties
{
	bApplyTranslation=false
	bAddTranslation=false
	BoneTranslationSpace=BCS_BoneSpace

	bApplyRotation=true
	bAddRotation=false
	BoneRotationSpace=BCS_BoneSpace

	WheelRollAxis=AXIS_Y
	bIgnoreWhenNotRendered=true

	HandlebarRotateAxis=AXIS_Z
	bInvertRotation = false
	SteerWheelBoneIndex=-1
}
