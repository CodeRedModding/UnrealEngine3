/**
 * Keyframe reduction algorithm that simply removes keys which are linear interpolations of surrounding keys.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class AnimationCompressionAlgorithm_RemoveLinearKeys extends AnimationCompressionAlgorithm
	native(Anim);

/** Maximum position difference to use when testing if an animation key may be removed. Lower values retain more keys, but yield less compression. */
var(LinearKeyRemoval)	float	MaxPosDiff;

/** Maximum angle difference to use when testing if an animation key may be removed. Lower values retain more keys, but yield less compression. */
var(LinearKeyRemoval)	float	MaxAngleDiff;

/** 
 * As keys are tested for removal, we monitor the effects all the way down to the end effectors. 
 * If their position changes by more than this amount as a result of removing a key, the key will be retained.
 * This value is used for all bones except the end-effectors parent.
 */
var(LinearKeyRemoval)	float	MaxEffectorDiff;

/** 
 * As keys are tested for removal, we monitor the effects all the way down to the end effectors. 
 * If their position changes by more than this amount as a result of removing a key, the key will be retained.
 * This value is used for the end-effectors parent, allowing tighter restrictions near the end of a skeletal chain.
 */
var(LinearKeyRemoval)	float	MinEffectorDiff;

/** 
 * Error threshold for End Effectors with Sockets attached to them.
 * Typically more important bone, where we want to be less aggressive with compression.
 */
var(LinearKeyRemoval)   float   EffectorDiffSocket;

/** 
 * A scale value which increases the likelihood that a bone will retain a key if it's parent also had a key at the same time position. 
 * Higher values can remove shaking artifacts from the animation, at the cost of compression.
 */
var(LinearKeyRemoval)	float	ParentKeyScale;

/** 
 * TRUE = As the animation is compressed, adjust animated nodes to compensate for compression error.
 * FALSE= Do not adjust animated nodes.
 */
var(LinearKeyRemoval)	bool	bRetarget;

/**
  * Controls whether the final filtering step will occur, or only the retargetting after bitwise compression.
  * If both this and bRetarget are false, then the linear compressor will do no better than the underlying bitwise compressor, extremely slowly.
  */
var(LinearKeyRemoval)	bool	bActuallyFilterLinearKeys;

cpptext
{
protected:
	/**
	 * Keyframe reduction algorithm that removes any keys which can be linearly approximated by neighboring keys.
	 */
	virtual void DoReduction(class UAnimSequence* AnimSeq, class USkeletalMesh* SkelMesh, const TArray<class FBoneData>& BoneData);

	/**
	 * Pre-filters the tracks before running the main key removal algorithm
	 */
	virtual void FilterBeforeMainKeyRemoval(
		UAnimSequence* AnimSeq, 
		USkeletalMesh* SkelMesh, 
		const struct FAnimSetMeshLinkup& AnimLinkup,
		const TArray<FBoneData>& BoneData, 
		TArray<FTranslationTrack>& TranslationData,
		TArray<FRotationTrack>& RotationData);

	/**
	 * Compresses the tracks passed in using the underlying compressor for this key removal codec
	 */
	virtual void CompressUsingUnderlyingCompressor(
		UAnimSequence* AnimSeq, 
		USkeletalMesh* SkelMesh,
		const struct FAnimSetMeshLinkup& AnimLinkup,
		const TArray<FBoneData>& BoneData, 
		const TArray<FTranslationTrack>& TranslationData,
		const TArray<FRotationTrack>& RotationData,
		const UBOOL bFinalPass);

	/**
	  * Updates the world bone transforms for a range of bone indices
	  */
	void UpdateWorldBoneTransformRange(
		UAnimSequence* AnimSeq, 
		USkeletalMesh* SkelMesh, 
		const TArray<FBoneData>& BoneData, 
		const struct FAnimSetMeshLinkup& AnimLinkup,
		const TArray<FMeshBone>& RefSkel,
		const TArray<FTranslationTrack>& PositionTracks,
		const TArray<FRotationTrack>& RotationTracks,
		INT StartingBoneIndex,
		INT EndingBoneIndex,
		UBOOL UseRaw,
		TArray<FMatrix>& OutputWorldBones);

	/**
	 * To guide the key removal process, we need to maintain a table of world transforms
	 * for the bones we are investigating. This helper function fills a row of the 
	 * table for a specified bone.
	 */
	void UpdateWorldBoneTransformTable(
		UAnimSequence* AnimSeq, 
		USkeletalMesh* SkelMesh, 
		const TArray<FBoneData>& BoneData, 
		const struct FAnimSetMeshLinkup& AnimLinkup,
		const TArray<FMeshBone>& RefSkel,
		INT BoneIndex,
		UBOOL UseRaw,
		TArray<FMatrix>& OutputWorldBones);

	/**
	 * Creates a list of the bone atom result for every frame of a given track
	 */
	static void UpdateBoneAtomList(
		UAnimSequence* AnimSeq, 
		INT BoneIndex,
		INT TrackIndex,
		INT NumFrames,
		FLOAT TimePerFrame,
		TArray<FBoneAtom>& BoneAtoms,
		const struct FAnimSetMeshLinkup& AnimLinkup,
		const TArray<FMeshBone>& RefSkel);

	/**
	 * If the passed in animation sequence is additive, converts it to absolute (using the frame 0 pose) and returns TRUE
	 * (indicating it should be converted back to relative later with ConvertToRelativeSpace)
	 *
	 * @param AnimSeq			The animation sequence being compressed
	 *
	 * @return TRUE if the animation was additive and has been converted to absolute space.
	 */
	UBOOL ConvertFromRelativeSpace(UAnimSequence* AnimSeq, const struct FAnimSetMeshLinkup& AnimLinkup);

	/**
	 * Converts an absolute animation sequence to a relative (additive) one.
	 *
	 * @param AnimSeq			The animation sequence being compressed
	 * @param TranslationData	Translation Tracks to convert to relative space
	 * @param RotationData		Rotation Tracks  to convert to relative space
	 *
	 */
	void ConvertToRelativeSpace(UAnimSequence* AnimSeq, TArray<FTranslationTrack>& TranslationData, TArray<FRotationTrack>& RotationData, const struct FAnimSetMeshLinkup& AnimLinkup);

	/**
	 * Locates spans of keys within the position and rotation tracks provided which can be estimated
	 * through linear interpolation of the surrounding keys. The remaining key values are bit packed into
	 * the animation sequence provided
	 *
	 * @param	AnimSeq		The animation sequence being compressed
	 * @param	SkelMesh	The skeletal mesh to use to guide the compressor
	 * @param	AnimLinkup	The linkup between skeletal mesh an animation
	 * @param	BoneData	BoneData array describing the hierarchy of the animated skeleton
	 * @param	TranslationData		Translation Tracks to compress and bit-pack into the Animation Sequence.
	 * @param	RotationData		Rotation Tracks to compress and bit-pack into the Animation Sequence.
	 * @return				None.
	 */
	void ProcessAnimationTracks(
		UAnimSequence* AnimSeq, 
		USkeletalMesh* SkelMesh, 
		const struct FAnimSetMeshLinkup& AnimLinkup,
		const TArray<FBoneData>& BoneData, 
		TArray<FTranslationTrack>& PositionTracks,
		TArray<FRotationTrack>& RotationTracks);
}

defaultproperties
{
	bNeedsSkeleton = TRUE
	Description = "Remove Linear Keys"
	MaxPosDiff = 0.001f
	MaxAngleDiff = 0.00075f
	MaxEffectorDiff = 0.001f	// used to be 0.2
	MinEffectorDiff = 0.001f	// used to be 0.1
	EffectorDiffSocket = 0.001f
	ParentKeyScale = 2.0
	bRetarget = TRUE
	bActuallyFilterLinearKeys = TRUE
}
