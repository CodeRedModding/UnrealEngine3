/**
 * Bitwise animation compression only; performs no key reduction.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class AnimationCompressionAlgorithm_BitwiseCompressOnly extends AnimationCompressionAlgorithm
	native(Anim);

cpptext
{
protected:
	/**
	 * Bitwise animation compression only; performs no key reduction.
	 */
	virtual void DoReduction(class UAnimSequence* AnimSeq, class USkeletalMesh* SkelMesh, const TArray<class FBoneData>& BoneData);
}

defaultproperties
{
	Description="Bitwise Compress Only"
}
