/**
 * Removes trivial frames -- frames of tracks when position or orientation is constant
 * over the entire animation -- from the raw animation data.  If both position and rotation
 * go down to a single frame, the time is stripped out as well.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class AnimationCompressionAlgorithm_RemoveTrivialKeys extends AnimationCompressionAlgorithm
	native(Anim);

var()	float	MaxPosDiff;
var()	float	MaxAngleDiff;

cpptext
{
protected:
	/**
	 * Removes trivial frames -- frames of tracks when position or orientation is constant
	 * over the entire animation -- from the raw animation data.  If both position and rotation
	 * go down to a single frame, the time is stripped out as well.
	 *
	 * @return		TRUE if the keyframe reduction was successful.
	 */
	virtual void DoReduction(class UAnimSequence* AnimSeq, class USkeletalMesh* SkelMesh, const TArray<class FBoneData>& BoneData);
}

defaultproperties
{
	Description="Remove Trivial Keys"

	MaxPosDiff=0.0001
	MaxAngleDiff=0.0003
}
