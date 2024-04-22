/**
 * Keyframe reduction algorithm that removes keys which are linear interpolations of surrounding keys, as
 * well as choosing the best bitwise compression for each track independently.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class AnimationCompressionAlgorithm_PerTrackCompression extends AnimationCompressionAlgorithm_RemoveLinearKeys
	hidecategories(AnimationCompressionAlgorithm)
	native(Anim);

/** Maximum threshold to use when replacing a component with zero. Lower values retain more keys, but yield less compression. */
var(PerTrack) float	MaxZeroingThreshold;

/** Maximum position difference to use when testing if an animation key may be removed. Lower values retain more keys, but yield less compression. */
var(PerTrack) float	MaxPosDiffBitwise;

/** Maximum angle difference to use when testing if an animation key may be removed. Lower values retain more keys, but yield less compression. */
var(PerTrack) float	MaxAngleDiffBitwise;

/** Which encoding formats is the per-track compressor allowed to try on rotation keys */
var(PerTrack) array<AnimationCompressionFormat>	AllowedRotationFormats;

/** Which encoding formats is the per-track compressor allowed to try on translation keys */
var(PerTrack) array<AnimationCompressionFormat>	AllowedTranslationFormats;


/** If TRUE, resample the animation to ResampleFramerate frames per second */
var(Resampling)	bool	bResampleAnimation;

/** When bResampleAnimation is true, this defines the desired framerate */
var(Resampling)	float	ResampledFramerate<ClampMin=1.0 | ClampMax=30.0 | EditCondition=bResampleAnimation>;

/** Animations with fewer keys than MinKeysForResampling will not be resampled. */
var(Resampling)	int		MinKeysForResampling;


/** If TRUE, adjust the error thresholds based on the 'height' within the skeleton */
var(AdaptiveError) bool	bUseAdaptiveError;

/** If TRUE, uses MinEffectorDiff as the threhsold for end effectors */
var(AdaptiveError) bool	bUseOverrideForEndEffectors;

/** A bias added to the track height before using it to calculate the adaptive error */
var(AdaptiveError) int	TrackHeightBias;

/**
 * Reduces the error tolerance the further up the tree that a key occurs
 *    EffectiveErrorTolerance = Max(BaseErrorTolerance / Power(ParentingDivisor, Max(Height+Bias,0) * ParentingDivisorExponent), ZeroingThreshold)
 * Only has an effect bUseAdaptiveError is TRUE
 */
var(AdaptiveError) float	ParentingDivisor<ClampMin = 1.0>;

/**
 * Reduces the error tolerance the further up the tree that a key occurs
 *    EffectiveErrorTolerance = Max(BaseErrorTolerance / Power(ParentingDivisor, Max(Height+Bias,0) * ParentingDivisorExponent), ZeroingThreshold)
 * Only has an effect bUseAdaptiveError is TRUE
 */
var(AdaptiveError) float	ParentingDivisorExponent<ClampMin = 0.1>;


/**
 * If true, the adaptive error system will determine how much error to allow for each track, based on the
 * error introduced in end effectors due to errors in the track.
 */
var(AdaptiveError2) bool	bUseAdaptiveError2;

/**
 * This ratio determines how much error in end effector rotation can come from a given track's rotation error or translation error.
 * If 1, all of it must come from rotation error, if 0.5, half can come from each, and if 0.0, all must come from translation error.
 */
var(AdaptiveError2) float	RotationErrorSourceRatio<ClampMin = 0.0 | ClampMax = 1.0>;

/**
 * This ratio determines how much error in end effector translation can come from a given track's rotation error or translation error.
 * If 1, all of it must come from rotation error, if 0.5, half can come from each, and if 0.0, all must come from translation error.
 */
var(AdaptiveError2) float	TranslationErrorSourceRatio<ClampMin = 0.0 | ClampMax = 1.0>;

/**
 * A fraction that determines how much of the total error budget can be introduced by any particular track
 */
var(AdaptiveError2) float	MaxErrorPerTrackRatio<ClampMin = 0.0 | ClampMax = 1.0>;

/**
 * How big of a perturbation should be made when probing error propagation
 */
var float	PerturbationProbeSize;

/**
 * Cached metastructures used within DoReduction, tied to a particular sequence and mesh
 */
var const native transient pointer PerReductionCachedData{struct FPerTrackCachedInfo};

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
	 * Pre-filters the tracks before running the main key removal algorithm
	 */
	virtual void FilterBeforeMainKeyRemoval(
		UAnimSequence* AnimSeq, 
		USkeletalMesh* SkelMesh, 
		const struct FAnimSetMeshLinkup& AnimLinkup,
		const TArray<FBoneData>& BoneData, 
		TArray<FTranslationTrack>& TranslationData,
		TArray<FRotationTrack>& RotationData);
}

defaultproperties
{
	Description="Compress each track independently"
	
	// Bitwise settings
	MaxPosDiffBitwise = 0.007
	MaxAngleDiffBitwise = 0.002
	MaxZeroingThreshold = 0.0002

	// Settings for resampling
	ResampledFramerate = 15.0
	bResampleAnimation = FALSE
	MinKeysForResampling = 10;

	// Settings for linear key removal (disabled by default)
	bRetarget = FALSE
	bActuallyFilterLinearKeys = FALSE

	// Settings for adaptive error thresholds
	bUseAdaptiveError = FALSE
	ParentingDivisor = 1.0
	ParentingDivisorExponent = 1.0
	TrackHeightBias = 1
	bUseOverrideForEndEffectors = FALSE


	// Settings for adaptive error mode 2
	bUseAdaptiveError2 = FALSE
	RotationErrorSourceRatio = 0.8
	TranslationErrorSourceRatio = 0.8
	MaxErrorPerTrackRatio = 0.3
	PerturbationProbeSize = 0.001


	// Allowed rotation formats
	AllowedRotationFormats[0] = ACF_Identity
	AllowedRotationFormats[1] = ACF_Fixed48NoW

	// Those below produce too much error (shaking), and have the side effect of producing worse overall compression, so they are being removed for now.
// 	AllowedRotationFormats[2] = ACF_IntervalFixed32NoW
// 	AllowedRotationFormats[3] = ACF_Fixed32NoW
// 	AllowedRotationFormats[4] = ACF_Float32NoW

	// Allowed translation formats
	AllowedTranslationFormats[0] = ACF_Identity
	AllowedTranslationFormats[1] = ACF_IntervalFixed32NoW
	AllowedTranslationFormats[2] = ACF_Fixed48NoW
}
