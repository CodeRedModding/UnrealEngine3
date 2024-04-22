
/**
 *	Simple controller for multiplication node.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class SkelControl_Multiply extends SkelControlBase
	native(Anim);

cpptext
{
	// USkelControlBase interface
	virtual void GetAffectedBones(INT BoneIndex, USkeletalMeshComponent* SkelComp, TArray<INT>& OutBoneIndices);
	virtual void CalculateNewBoneTransforms(INT BoneIndex, USkeletalMeshComponent* SkelComp, TArray<FBoneAtom>& OutBoneTransforms);
	FQuat ExtractAngle(INT BoneIndex, USkeletalMeshComponent* SkelComp);
}

/** How much to scale the angle */
var()   FLOAT   Multiplier;

defaultproperties
{
	Multiplier=1.f
	CategoryDesc = "Single Bone"
 
	bIgnoreWhenNotRendered=TRUE // Twist Bone is safe to skip when not rendered.
}
