/**
 * Baseclass for animation compression algorithms.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class AnimationCompressionAlgorithm extends Object
	abstract
	native(Anim)
	DependsOn(AnimSequence)
	hidecategories(Object);

/** A human-readable name for this modifier; appears in editor UI. */
var		string		Description;

/** Compression algorithms requiring a skeleton should set this value to TRUE. */
var		bool		bNeedsSkeleton;

/** Format for bitwise compression of translation data. */
var		AnimationCompressionFormat		TranslationCompressionFormat;

/** Format for bitwise compression of rotation data. */
var()	AnimationCompressionFormat		RotationCompressionFormat;

cpptext
{
public:
	/**
	 * Reduce the number of keyframes and bitwise compress the specified sequence.
	 *
	 * @param	AnimSeq		The animation sequence to compress.
	 * @param	SkelMesh	The skeletal mesh against which to compress the animation.  Not needed by all compression schemes.
	 * @param	bOutput		If FALSE don't generate output or compute memory savings.
	 * @return				FALSE if a skeleton was needed by the algorithm but not provided.
	 */
	UBOOL Reduce(class UAnimSequence* AnimSeq, class USkeletalMesh* SkelMesh, UBOOL bOutput);

	/**
	 * Reduce the number of keyframes and bitwise compress all sequences in the specified animation set.
	 *
	 * @param	AnimSet		The animation set to compress.
	 * @param	SkelMesh	The skeletal mesh against which to compress the animation.  Not needed by all compression schemes.
	 * @param	bOutput		If FALSE don't generate output or compute memory savings.
	 * @return				FALSE if a skeleton was needed by the algorithm but not provided.
	 */
	UBOOL Reduce(class UAnimSet* AnimSet, class USkeletalMesh* SkelMesh, UBOOL bOutput);

protected:
	/**
	 * Implemented by child classes, this function reduces the number of keyframes in
	 * the specified sequence, given the specified skeleton (if needed).
	 *
	 * @return		TRUE if the keyframe reduction was successful.
	 */
	virtual void DoReduction(class UAnimSequence* AnimSeq, class USkeletalMesh* SkelMesh, const TArray<class FBoneData>& BoneData) PURE_VIRTUAL(UAnimationCompressionAlgorithm::DoReduction,);

	/**
	 * Common compression utility to remove 'redundant' position keys in a set of tracks based on the provided delta threshold
	 *
	 * @param	InputTracks		Array of position track elements to reduce
	 * @param	MaxPosDelta		Maximum local-space threshold for stationary motion
	 */
	static void FilterTrivialPositionKeys(
		TArray<struct FTranslationTrack>& Track,
		FLOAT MaxPosDelta);

	/**
	 * Common compression utility to remove 'redundant' position keys in a single track based on the provided delta threshold
	 *
	 * @param	Track			Track to reduce
	 * @param	MaxPosDelta		Maximum local-space threshold for stationary motion
	 */
	static void FilterTrivialPositionKeys(
		struct FTranslationTrack& Track,
		FLOAT MaxPosDelta);

	/**
	 * Common compression utility to remove 'redundant' rotation keys in a set of tracks based on the provided delta threshold
	 *
	 * @param	InputTracks		Array of rotation track elements to reduce
	 * @param	MaxRotDelta		Maximum angle threshold to consider stationary motion
	 */
	static void FilterTrivialRotationKeys(
		TArray<struct FRotationTrack>& InputTracks,
		FLOAT MaxRotDelta);

	/**
	 * Common compression utility to remove 'redundant' rotation keys in a set of tracks based on the provided delta threshold
	 *
	 * @param	Track			Track to reduce
	 * @param	MaxRotDelta		Maximum angle threshold to consider stationary motion
	 */
	static void FilterTrivialRotationKeys(
		struct FRotationTrack& Track,
		FLOAT MaxRotDelta);

	/**
	 * Common compression utility to remove 'redundant' keys based on the provided delta thresholds
	 *
	 * @param	PositionTracks	Array of position track elements to reduce
	 * @param	RotationTracks	Array of rotation track elements to reduce
	 * @param	MaxPosDelta		Maximum local-space threshold for stationary motion
	 * @param	MaxRotDelta		Maximum angle threshold to consider stationary motion
	 */
	static void FilterTrivialKeys(
		TArray<struct FTranslationTrack>& PositionTracks,
		TArray<struct FRotationTrack>& RotationTracks,
		FLOAT MaxPosDelta,
		FLOAT MaxRotDelta);

	/** 
	 * Remove translation keys from tracks marked bAnimRotationOnly.
	 *
	 * @param PositionTracks	Array of position track elements to reduce
	 * @param AnimSeq			AnimSequence the track is from.
	 * @param SkelMesh			SkeletalMesh associated with the AnimSerquence 
	 */
	static void FilterAnimRotationOnlyKeys(TArray<FTranslationTrack>& PositionTracks, UAnimSequence* AnimSeq, USkeletalMesh* SkelMesh);

	/**
	 * Common compression utility to retain only intermittent position keys. For example,
	 * calling with an Interval of 3 would keep every third key in the set and discard the rest
	 *
	 * @param	PositionTracks	Array of position track elements to reduce
	 * @param	StartIndex		Index at which to begin reduction
	 * @param	Interval		Interval of keys to retain
	 */
	static void FilterIntermittentPositionKeys(
		TArray<struct FTranslationTrack>& PositionTracks,
		INT StartIndex,
		INT Interval);

	/**
	 * Common compression utility to retain only intermittent position keys. For example,
	 * calling with an Interval of 3 would keep every third key in the set and discard the rest
	 *
	 * @param	Track			Track to reduce
	 * @param	StartIndex		Index at which to begin reduction
	 * @param	Interval		Interval of keys to retain
	 */
	static void FilterIntermittentPositionKeys(
		struct FTranslationTrack& Track,
		INT StartIndex,
		INT Interval);

	/**
	 * Common compression utility to retain only intermittent rotation keys. For example,
	 * calling with an Interval of 3 would keep every third key in the set and discard the rest
	 *
	 * @param	RotationTracks	Array of rotation track elements to reduce
	 * @param	StartIndex		Index at which to begin reduction
	 * @param	Interval		Interval of keys to retain
	 */
	static void FilterIntermittentRotationKeys(
		TArray<struct FRotationTrack>& RotationTracks,
		INT StartIndex,
		INT Interval);

	/**
	 * Common compression utility to retain only intermittent rotation keys. For example,
	 * calling with an Interval of 3 would keep every third key in the set and discard the rest
	 *
	 * @param	Track			Track to reduce
	 * @param	StartIndex		Index at which to begin reduction
	 * @param	Interval		Interval of keys to retain
	 */
	static void FilterIntermittentRotationKeys(
		struct FRotationTrack& Track,
		INT StartIndex,
		INT Interval);

	/**
	 * Common compression utility to retain only intermittent animation keys. For example,
	 * calling with an Interval of 3 would keep every third key in the set and discard the rest
	 *
	 * @param	PositionTracks	Array of position track elements to reduce
	 * @param	RotationTracks	Array of rotation track elements to reduce
	 * @param	StartIndex		Index at which to begin reduction
	 * @param	Interval		Interval of keys to retain
	 */
	static void FilterIntermittentKeys(
		TArray<struct FTranslationTrack>& PositionTracks,
		TArray<struct FRotationTrack>& RotationTracks,
		INT StartIndex,
		INT Interval);

	/**
	 * Common compression utility to populate individual rotation and translation track
	 * arrays from a set of raw animation tracks. Used as a precurser to animation compression.
	 *
	 * @param	RawAnimData			Array of raw animation tracks
	 * @param	SequenceLength		The duration of the animation in seconds
	 * @param	OutTranslationData	Translation tracks to fill
	 * @param	OutRotationData		Rotation tracks to fill
	 */
	static void SeparateRawDataIntoTracks(
		const TArray<struct FRawAnimSequenceTrack>& RawAnimData,
		FLOAT SequenceLength,
		TArray<struct FTranslationTrack>& OutTranslationData,
		TArray<struct FRotationTrack>& OutRotationData);

	/**
	 * Common compression utility to walk an array of rotation tracks and enforce
	 * that all adjacent rotation keys are represented by shortest-arc quaternion pairs.
	 *
	 * @param	RotationData	Array of rotation track elements to reduce.
	 */
	static void PrecalculateShortestQuaternionRoutes(TArray<struct FRotationTrack>& RotationData);

public:

	/**
	 * Encodes individual key arrays into an AnimSequence using the desired bit packing formats.
	 *
	 * @param	Seq							Pointer to an Animation Sequence which will contain the bit-packed data .
	 * @param	TargetTranslationFormat		The format to use when encoding translation keys.
	 * @param	TargetRotationFormat		The format to use when encoding rotation keys.
	 * @param	TranslationData				Translation Tracks to bit-pack into the Animation Sequence.
	 * @param	RotationData				Rotation Tracks to bit-pack into the Animation Sequence.
	 * @param	IncludeKeyTable				TRUE if the compressed data should also contain a table of frame indices for each key. (required by some codecs)
	 */
	static void BitwiseCompressAnimationTracks(
		class UAnimSequence* Seq,
		AnimationCompressionFormat TargetTranslationFormat,
		AnimationCompressionFormat TargetRotationFormat,
		const TArray<struct FTranslationTrack>& TranslationData,
		const TArray<struct FRotationTrack>& RotationData,
		UBOOL IncludeKeyTable = FALSE);
}

defaultproperties
{
	Description="None"
	TranslationCompressionFormat=ACF_None
	RotationCompressionFormat=ACF_Float96NoW
}
