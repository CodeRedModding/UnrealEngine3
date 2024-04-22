/**
 * Keyframe reduction algorithm that simply removes every second key.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class AnimationCompressionAlgorithm_RemoveEverySecondKey extends AnimationCompressionAlgorithm
	native(Anim);

/** Animations with fewer than MinKeys will not lose any keys. */
var()	int			MinKeys;

/**
 * If bStartAtSecondKey is TRUE, remove keys 1,3,5,etc.
 * If bStartAtSecondKey is FALSE, remove keys 0,2,4,etc.
 */
var()	bool		bStartAtSecondKey;

cpptext
{
protected:
	/**
	 * Keyframe reduction algorithm that simply removes every second key.
	 *
	 * @return		TRUE if the keyframe reduction was successful.
	 */
	virtual void DoReduction(class UAnimSequence* AnimSeq, class USkeletalMesh* SkelMesh, const TArray<class FBoneData>& BoneData);
}

defaultproperties
{
	Description="Remove Every Second Key"
	MinKeys=10
}
