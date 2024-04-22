class SkelControlSpline extends SkelControlBase
	native(Anim);
	
/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 *	Controller that configures the bones above the controlled one in the hierarchy into a smooth curve.
 */
 
cpptext
{
	// USkelControlBase interface
	virtual void GetAffectedBones(INT BoneIndex, USkeletalMeshComponent* SkelComp, TArray<INT>& OutBoneIndices);
	virtual void CalculateNewBoneTransforms(INT BoneIndex, USkeletalMeshComponent* SkelComp, TArray<FBoneAtom>& OutBoneTransforms);	
}

/** Number of bones above the active one in the hierarchy to modify to make into a smooth curve. */
var(Spline)		int		SplineLength;

/** Axis of the controlled bone (ie the end of the spline) to use as the direction for the curve. */
var(Spline)		EAxis	SplineBoneAxis;

/** Invert the direction we get for the start of the spline. */
var(Spline)		bool	bInvertSplineBoneAxis;

/** Strength of tangent at the controlled bone. */
var(Spline)		float	EndSplineTension;

/** Strength of tangent at the start of the chain. */
var(Spline)		float	StartSplineTension;

enum ESplineControlRotMode
{
	/** Do not modify rotation of bones along the spline. */
	SCR_NoChange,
	
	/** By applying the 'minimum' rotation needed, point the SplineBoneAxis of each bone along the direction of the spline. */
	SCR_AlongSpline,
	
	/** Interpolate the rotation of each bone between the rotation of the bone at the start and end of the spline. */
	SCR_Interpolate
};

/** Controls how the rotation of each bone along the length of the spline is modified. */
var(Spline)		ESplineControlRotMode	BoneRotMode;

defaultproperties
{
	SplineLength=2
	
	SplineBoneAxis=AXIS_X
	EndSplineTension=10.0
	StartSplineTension=10.0
}
