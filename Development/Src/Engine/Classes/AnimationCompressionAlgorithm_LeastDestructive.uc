/**
 * Reverts any animation compression, restoring the animation to the raw data.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class AnimationCompressionAlgorithm_LeastDestructive extends AnimationCompressionAlgorithm
	native(Anim);

cpptext
{
protected:
	/**
	 * Uses the Bitwise compressor, with really light settings, so it acts pretty close to "no compression at all"
	 */
	virtual void DoReduction(class UAnimSequence* AnimSeq, class USkeletalMesh* SkelMesh, const TArray<class FBoneData>& BoneData);
}

defaultproperties
{
	Description="Least Destructive"
	TranslationCompressionFormat=ACF_None
	RotationCompressionFormat=ACF_None
}
