class SkelControlFootPlacement extends SkelControlLimb
	hidecategories(Effector)
	native(Anim);
	
/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 *	SkelControlLimb subclass designed for placing feet on the ground.
 *	The SkeletalMeshComponent must be adjusted in gameplay code so that the feet can pass into some world geometry,
 *	and then line-checks in the controller will detect where the line hits and pull the leg back accordingly, and orient
 *	the foor bone if desired.
 */

/** Vertical offset to apply to foot bone. This is applied along the vector between the hip position and the foot bone position. */
var(FootPlacement)	float	FootOffset;

/** Axis of the foot bone to align to ground normal (if bOrientFootToGround is true). */
var(FootPlacement)	EAxis	FootUpAxis;

/** Rotation offset applied to foot matrix before taking the FootUpAxis. */
var(FootPlacement)	rotator	FootRotOffset;

/** If we should invert the axis used for aligning the foot to the floor, defined by FootUpAxis. */
var(FootPlacement)	bool	bInvertFootUpAxis;

/** If we should attempt to align the foot bone with the surface normal of the ground. */
var(FootPlacement)	bool	bOrientFootToGround;

/** This control should be completely disabled if we are not doing an upwards adjustment on the foot. */ 
var(FootPlacement)	bool	bOnlyEnableForUpAdjustment;

/** Maximum distance from animated post that foot will be moved up. */
var(FootPlacement)	float	MaxUpAdjustment;

/** Maximum distance from animated post that foot will be moved down. */
var(FootPlacement)	float	MaxDownAdjustment;

/** Maximum angle (in degrees) that we will rotate the foot from the animated orientation in an attempt to match the ground normal. */
var(FootPlacement)	float	MaxFootOrientAdjust;

cpptext
{
	// USkelControlBase interface
	virtual void CalculateNewBoneTransforms(INT BoneIndex, USkeletalMeshComponent* SkelComp, TArray<FBoneAtom>& OutBoneTransforms);	
}

defaultproperties
{
	FootUpAxis=AXIS_X
	bOrientFootToGround=true
	
	MaxUpAdjustment=50.0
	MaxDownAdjustment=0.0
	MaxFootOrientAdjust=45.0
}
