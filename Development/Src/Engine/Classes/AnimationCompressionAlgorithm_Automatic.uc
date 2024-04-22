/**
 * Animation compression algorithm that is just a shell for trying the range of other compression schemes and pikcing the
 * smallest result within a configurable error threshold.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class AnimationCompressionAlgorithm_Automatic extends AnimationCompressionAlgorithm
	hidecategories(AnimationCompressionAlgorithm)
	native(Anim);

/** Maximum amount of error that a compression technique can introduce in an end effector */
var() float	MaxEndEffectorError;

var() bool bTryFixedBitwiseCompression;
var() bool bTryPerTrackBitwiseCompression;
var() bool bTryLinearKeyRemovalCompression;
var() bool bTryIntervalKeyRemoval;

var() bool bRunCurrentDefaultCompressor;

var() bool bAutoReplaceIfExistingErrorTooGreat;
var() bool bRaiseMaxErrorToExisting;

cpptext
{
protected:
	/**
	 * Animation compression algorithm that optionally:
	 *   1) Forcefully removes a portion of keys (every other key, 2 out of every 3, etc...)
	 *   2) Removes any keys which can be linearly approximated by neighboring keys
	 * but always packs each track using per-track compression settings.
	 */
	virtual void DoReduction(class UAnimSequence* AnimSeq, class USkeletalMesh* SkelMesh, const TArray<class FBoneData>& BoneData);
}

defaultproperties
{
	Description="Automatic"
	
	// Error threshold
	MaxEndEffectorError = 1.0
	bTryFixedBitwiseCompression = TRUE
	bTryPerTrackBitwiseCompression = TRUE
	bTryLinearKeyRemovalCompression = TRUE
	bTryIntervalKeyRemoval = TRUE

	bRunCurrentDefaultCompressor = FALSE
	bAutoReplaceIfExistingErrorTooGreat = FALSE
	bRaiseMaxErrorToExisting = FALSE
}
