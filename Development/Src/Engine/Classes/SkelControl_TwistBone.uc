
/**
 *	Simple controller for Twist/Roll bones.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class SkelControl_TwistBone extends SkelControlBase
	native(Anim);

cpptext
{
	// USkelControlBase interface
	virtual void GetAffectedBones(INT BoneIndex, USkeletalMeshComponent* SkelComp, TArray<INT>& OutBoneIndices);
	virtual void CalculateNewBoneTransforms(INT BoneIndex, USkeletalMeshComponent* SkelComp, TArray<FBoneAtom>& OutBoneTransforms);
	FQuat ExtractRollAngle(INT BoneIndex, USkeletalMeshComponent* SkelComp);
}

/** Name of source bone to use. For a forearm twist, that would be the hand bone name. */
var()   Name    SourceBoneName;
/** How much to scale down the roll angle */
var()   FLOAT   TwistAngleScale;

defaultproperties
{
	TwistAngleScale=-0.5f
	CategoryDesc = "Single Bone"

	bIgnoreWhenNotRendered=TRUE // Twist Bone is safe to skip when not rendered.
}
