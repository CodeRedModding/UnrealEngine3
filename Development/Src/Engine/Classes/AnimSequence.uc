/**
 * One animation sequence of keyframes. Contains a number of tracks of data.
 * The Outer of AnimSequence is expected to be its AnimSet.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class AnimSequence extends Object
	native(Anim)
	config(Engine)
	hidecategories(Object);

/*
 * Triggers an animation notify.  Each AnimNotifyEvent contains an AnimNotify object
 * which has its Notify method called and passed to the animation.
 */
struct native AnimNotifyEvent
{
	var()	float						Time;
	var()	instanced AnimNotify		Notify;
	var()	editoronly Name				Comment;
	var()	float						Duration;
};

/**
 * Raw keyframe data for one track.  Each array will contain either NumFrames elements or 1 element.
 * One element is used as a simple compression scheme where if all keys are the same, they'll be
 * reduced to 1 key that is constant over the entire sequence.
 *
 * @warning: manually mirrored in UnMiscDeclerations.h due to mixed native/ script serialization
 */
struct RawAnimSequenceTrack
{
	/** Position keys. */
	var array<vector>	PosKeys;
	/** Rotation keys. */
	var array<quat>		RotKeys;
};

/** Name of the animation sequence. Used in AnimNodeSequence. */
var		name									SequenceName;

/** Animation notifies, sorted by time (earliest notification first). */
var()	editinline array<AnimNotifyEvent>		Notifies;

/** Animation Meta Data */
var()	editinline instanced Array<AnimMetaData>    MetaData;

/*
 * This is to support Skel Control Strength from animation data
 * For example, if you'd like to scale bones via SkelControlSingleBone from animation data.
 * This looks up SkelControlName in the Anim tree and apply strength at the point where it's set up
 */
struct native TimeModifier
{
	/** Time to apply **/
	var()   float           Time;
	/** Target Strength of the Skel Control at this time **/
	/** This will linearly interpolate between multiple strength within one anim data**/
	var()   float           TargetStrength;
};

/* DEPRECATED VER_ADDED_ANIM_METADATA_FIXED_QUATERROR
 * This contains all skel control modifiers 
 * The TimeModifier will be sorted by time
 * Initially it starts from what strength it was when animation started. 
 */
struct native SkelControlModifier
{
	/** SkelControl Node Name in the Anim Tree that would apply **/
	var()   name            SkelControlName;

	/** Modifiers for what time and what strength for this skelcontrol name**/
	var()  editinline array<TimeModifier>  Modifiers;
};

/** DEPRECATED VER_ADDED_ANIM_METADATA_FIXED_QUATERROR Bone Control Modifiers, sorted by time (earliest notification first). */
var	deprecated editinline array<SkelControlModifier>		BoneControlModifiers;

/** Length (in seconds) of this AnimSequence if played back with a speed of 1.0. */
var		float									SequenceLength;

/** Number of raw frames in this sequence (not used by engine - just for informational purposes). */
var		int										NumFrames;

/** Number for tweaking playback rate of this animation globally. */
var()	float									RateScale;

/**
 * if TRUE, disable interpolation between last and first frame when looping.
 */
var()	bool									bNoLoopingInterpolation;

/**
 * Raw uncompressed keyframe data. RawAnimData is deprecated and moved to RawAnimationData
 * to switch to native serialization. Down the road it should be switched to use lazy loading
 * as the data is only used in the editor. It is used pervasively enough to be a separate change.
 */
var	deprecated private const array<RawAnimSequenceTrack>	RawAnimData;
var native private const array<RawAnimSequenceTrack>		RawAnimationData;

/**
 * Keyframe position data for one track.  Pos(i) occurs at Time(i).  Pos.Num() always equals Time.Num().
 */
struct native TranslationTrack
{
	var array<vector>	PosKeys;
	var array<float>	Times;
};

/**
 * Keyframe rotation data for one track.  Rot(i) occurs at Time(i).  Rot.Num() always equals Time.Num().
 */
struct native RotationTrack
{
	var array<quat>		RotKeys;
	var array<float>	Times;
};

/**
 * Key frame curve data for one track
 * CurveName: Morph Target Name
 * CurveWeights: List of weights for each frame
 */
struct native CurveTrack
{
	var name			CurveName;
	var array<float>	CurveWeights;

	structcpptext
	{
	/** Returns TRUE if valid curve weight exists in the array*/
	UBOOL IsValidCurveTrack();
	/** This is very simple cut to 1 key method if all is same since I see so many redundant same value in every frame 
	 *  Eventually this can get more complicated 
	 *  Will return TRUE if compressed to 1. Return FALSE otherwise **/
	UBOOL CompressCurveWeights();
	}
};

/**
 * Translation data post keyframe reduction.  TranslationData.Num() is zero if keyframe reduction
 * has not yet been applied.
 */
var transient const array<TranslationTrack>		TranslationData;

/**
 * Rotation data post keyframe reduction.  RotationData.Num() is zero if keyframe reduction
 * has not yet been applied.
 */
var transient const array<RotationTrack>		RotationData;

/*
 * Curve data - no compression yet                                                                       
 */
var const array<CurveTrack>						CurveData;

/**
 * The compression scheme that was most recently used to compress this animation.
 * May be NULL.
 */
var() editinline editconst editoronly AnimationCompressionAlgorithm	CompressionScheme;

/**
 * Indicates animation data compression format.
 */
enum AnimationCompressionFormat
{
	ACF_None,
	ACF_Float96NoW,
	ACF_Fixed48NoW,
	ACF_IntervalFixed32NoW,
	ACF_Fixed32NoW,
	ACF_Float32NoW,
	ACF_Identity
};

/** The compression format that was used to compress translation tracks. */
var const AnimationCompressionFormat		TranslationCompressionFormat;

/** The compression format that was used to compress rotation tracks. */
var const AnimationCompressionFormat		RotationCompressionFormat;

struct native CompressedTrack
{
	var array<byte>		ByteStream;
	var array<float>	Times;
	var float			Mins[3];
	var float			Ranges[3];
};

/**
 * An array of 4*NumTrack ints, arranged as follows:
 *   [0] Trans0.Offset
 *   [1] Trans0.NumKeys
 *   [2] Rot0.Offset
 *   [3] Rot0.NumKeys
 *   [4] Trans1.Offset
 *   . . .
 */
var			array<int>		CompressedTrackOffsets;

/**
 * ByteStream for compressed animation data.
 * All keys are currently stored at evenly-spaced intervals (ie no explicit key times).
 *
 * For a translation track of n keys, data is packed as n uncompressed float[3]:
 *
 * For a rotation track of n>1 keys, the first 24 bytes are reserved for compression info
 * (eg Fixed32 stores float Mins[3]; float Ranges[3]), followed by n elements of the compressed type.
 * For a rotation track of n=1 keys, the single key is packed as an FQuatFloat96NoW.
 */
var native	array<byte>		CompressedByteStream;

/**
 * Indicates animation data compression format.
 */
enum AnimationKeyFormat
{
	AKF_ConstantKeyLerp,
	AKF_VariableKeyLerp,
	AKF_PerTrackCompression,
};

var const AnimationKeyFormat		KeyEncodingFormat;

/**
 * The runtime interface to decode and byte swap the compressed animation
 * May be NULL. Set at runtime - does not exist in editor
 */
var private transient native pointer	TranslationCodec;
var private transient native pointer	RotationCodec;

// Additive Animation Support
/** TRUE if this is an Additive Animation */
var	const	bool			bIsAdditive;
/** Reference pose for additive animation. Deprecated. */
var	const	deprecated Array<BoneAtom>	AdditiveRefPose;
/** Store Reference Pose animation used to create this additive one. For playback in editor only. */
var const Array<RawAnimSequenceTrack>   AdditiveBasePose;

/** Reference animation name */
var	const editoronly Name AdditiveRefName;
/** TRUE if additive animation was built with looping interpolation. */
var editoronly bool	bAdditiveBuiltLooping;
/** If this animation is Additive, this is the reference to the Base Pose used. For automatic rebuilding in the editor. Made into a list to handle duplicate animations. */
var editoronly Array<AnimSequence> AdditiveBasePoseAnimSeq;
/** If this animation is Additive, this is the reference to the Target Pose used. For automatic rebuilding in the editor. Made into a list to handle duplicate animations. */
var editoronly Array<AnimSequence> AdditiveTargetPoseAnimSeq;
/** If this animation was used, either as a Base or Target Pose, to build additive animations, they are referenced there. For automatic rebuilding in the editor. */
var editoronly Array<AnimSequence> RelatedAdditiveAnimSeqs;

// Versioning Support
/** The version of the global encoding package used at the time of import */
var	const	int				EncodingPkgVersion;
/** Saved version number with CompressAnimations commandlet. To help with doing it in multiple passes. */
var editoronly const int    CompressCommandletVersion;

	/**
 * Do not attempt to override compression scheme when running CompressAnimations commandlet.
 * Some high frequency animations are too sensitive and shouldn't be changed.
 */
var() editoronly const bool			bDoNotOverrideCompression;

/**
 * Debug flag to trace if this anim sequence was played.
 */
var const transient bool	bHasBeenUsed;
/**
 * Debug score to find out animation usage
 */
var const transient float	UseScore;

/**
 * Used to track whether, or not, this sequence was compressed with it's full translation tracks
 */
var editoronly bool bWasCompressedWithoutTranslations;

cpptext
{
	// UObject interface

	virtual void Serialize(FArchive& Ar);
	virtual void PreSave();
	virtual void PostLoad();
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
	virtual void BeginDestroy();

	/**
	 * Used by various commandlets to purge editor only and platform-specific data from various objects
	 * 
	 * @param PlatformsToKeep Platforms for which to keep platform-specific data
	 * @param bStripLargeEditorData If TRUE, data used in the editor, but large enough to bloat download sizes, will be removed
	 */
	virtual void StripData(UE3::EPlatformType PlatformsToKeep, UBOOL bStripLargeEditorData);

	// AnimSequence interface

	/**
	 * Reconstructs a bone atom from key-reduced tracks.
	 */
	static void ReconstructBoneAtom(FBoneAtom& OutAtom,
									const FTranslationTrack& TranslationTrack,
									const FRotationTrack& RotationTrack,
									FLOAT SequenceLength,
									FLOAT Time,
									UBOOL bLooping);

	/**
	 * Reconstructs a bone atom from compressed tracks.
	 */
	static void ReconstructBoneAtom(FBoneAtom& OutAtom,
									const FCompressedTrack& TranslationTrack,
									const FCompressedTrack& RotationTrack,
									AnimationCompressionFormat TranslationCompressionFormat,
									AnimationCompressionFormat RotationCompressionFormat,
									FLOAT SequenceLength,
									FLOAT Time,
									UBOOL bLooping);

	/**
	 * Reconstructs a bone atom from compressed tracks.
	 */
	static void ReconstructBoneAtom(FBoneAtom& OutAtom,
									const BYTE* TransStream,
									INT NumTransKeys,
									const BYTE* RotStream,
									INT NumRotKeys,
									AnimationCompressionFormat TranslationCompressionFormat,
									AnimationCompressionFormat RotationCompressionFormat,
									FLOAT SequenceLength,
									FLOAT Time,
									UBOOL bLooping);

	/**
	 * Decompresses a translation key from the specified compressed translation track.
	 */
	static void ReconstructTranslation(class FVector& Out, const BYTE* Stream, INT KeyIndex, AnimationCompressionFormat TranslationCompressionFormat);

	/**
	 * Decompresses a rotation key from the specified compressed rotation track.
	 */
	static void ReconstructRotation(class FQuat& Out, const BYTE* Stream, INT KeyIndex, AnimationCompressionFormat RotationCompressionFormat, const FLOAT *Mins, const FLOAT *Ranges);

	/**
	 * Decompresses a translation key from the specified compressed translation track.
	 */
	static void ReconstructTranslation(class FVector& Out, const BYTE* Stream, INT KeyIndex);

	/**
	 * Decompresses a rotation key from the specified compressed rotation track.
	 */
	static void ReconstructRotation(class FQuat& Out, const BYTE* Stream, INT KeyIndex, UBOOL bTrackHasCompressionInfo, AnimationCompressionFormat RotationCompressionFormat);

	/**
	 * Populates the key reduced arrays from raw animation data.
	 */
	static void SeparateRawDataToTracks(const TArray<FRawAnimSequenceTrack>& RawAnimData,
										FLOAT SequenceLength,
										TArray<FTranslationTrack>& OutTranslationData,
										TArray<FRotationTrack>& OutRotationData);

	/**
	 * Interpolate keyframes in this sequence to find the bone transform (relative to parent).
	 *
	 * @param	OutAtom			[out] Output bone transform.
	 * @param	TrackIndex		Index of track to interpolate.
	 * @param	Time			Time on track to interpolate to.
	 * @param	bLooping		TRUE if the animation is looping.
	 * @param	bUseRawData		If TRUE, use raw animation data instead of compressed data.
	 * @param	CurveKeys		List of Curve Keys if exists
	 */
	void GetBoneAtom(FBoneAtom& OutAtom, INT TrackIndex, FLOAT Time, UBOOL bLooping, UBOOL bUseRawData, FCurveKeyArray* CurveKeys = NULL) const;
	
	/**
	 * Interpolate keyframes in this sequence to find the bone transform (relative to parent).
	 * This returns the base pose used to create the additive animation.
	 *
	 * @param	OutAtom			[out] Output bone transform.
	 * @param	TrackIndex		Index of track to interpolate.
	 * @param	Time			Time on track to interpolate to.
	 * @param	bLooping		TRUE if the animation is looping.
	 */
	void GetAdditiveBasePoseBoneAtom(FBoneAtom& OutAtom, INT TrackIndex, FLOAT Time, UBOOL bLooping) const;

	/**
	 * Interpolate curve weights of the Time in this sequence if curve data exists
	 *
	 * @param	Time			Time on track to interpolate to.
	 * @param	bLooping		TRUE if the animation is looping.
	 * @param	CurveKeys		Add the curve keys if exists
	 */
	void GetCurveData(FLOAT Time, UBOOL bLooping, FCurveKeyArray& CurveKeys) const;

	/** Sort the Notifies array by time, earliest first. */
	void SortNotifies();

	/**
	 * @return		A reference to the AnimSet this sequence belongs to.
	 */
	UAnimSet* GetAnimSet() const;

	/**
	 * Returns the size of the object/ resource for display to artists/ LDs in the Editor.
	 *
	 * @return size of resource as to be displayed to artists/ LDs in the Editor.
	 */
	virtual INT GetResourceSize();

	/**
	 * @return		The approximate size of raw animation data.
	 */
	INT GetApproxRawSize() const;

	/**
	 * @return		The approximate size of key-reduced animation data.
	 */
	INT GetApproxReducedSize() const;

	/**
	 * @return		The approximate size of compressed animation data.
	 */
	INT GetApproxCompressedSize() const;

	/**
	 * Crops the raw anim data either from Start to CurrentTime or CurrentTime to End depending on
	 * value of bFromStart.  Can't be called against cooked data.
	 *
	 * @param	CurrentTime		marker for cropping (either beginning or end)
	 * @param	bFromStart		whether marker is begin or end marker
	 * @return					TRUE if the operation was successful.
	 */
	UBOOL CropRawAnimData( FLOAT CurrentTime, UBOOL bFromStart );
	/** 
	 *  Utility function for lossless compression of a FRawAnimSequenceTrack 
	 *  @return TRUE if keys were removed.
	 **/
	UBOOL CompressRawAnimSequenceTrack(FRawAnimSequenceTrack& RawTrack, float MaxPosDiff, float MaxAngleDiff);
	/**
	 * Removes trivial frames -- frames of tracks when position or orientation is constant
	 * over the entire animation -- from the raw animation data.  If both position and rotation
	 * go down to a single frame, the time is stripped out as well.
	 * @return TRUE if keys were removed.
	 */
	UBOOL CompressRawAnimData(float MaxPosDiff, float MaxAngleDiff);
	/**
	 * Removes trivial frames -- frames of tracks when position or orientation is constant
	 * over the entire animation -- from the raw animation data.  If both position and rotation
	 * go down to a single frame, the time is stripped out as well.
	 * @return TRUE if keys were removed.
	 */
	UBOOL CompressRawAnimData();

	/** Clears any data in the AnimSequence, so it can be recycled when importing a new animation with same name over it. */
	void RecycleAnimSequence();

	/** 
	 * Clear references to additive animations.
	 * This is the following arrays: AdditiveBasePoseAnimSeq, AdditiveTargetPoseAnimSeq and RelatedAdditiveAnimSeqs.
	 * Handles dependencies, and removes us properly.
	 */
	void ClearAdditiveAnimReferences();
	void FixUpBadAnimNotifiers();

	static UBOOL CopyAnimSequenceProperties(UAnimSequence* SourceAnimSeq, UAnimSequence* DestAnimSeq, UBOOL bSkipCopyingNotifies=FALSE);
	static void CopyMetadata(UAnimSequence* SourceAnimSeq, UAnimSequence* DestAnimSeq);
	static UBOOL CopyNotifies(UAnimSequence* SourceAnimSeq, UAnimSequence* DestAnimSeq);
}

/** 
 *	Get the time (in seconds) from the start of the animation that the first notify of the given class would fire 
 *
 *	@param	NotifyClass		Class of AnimNotify we are looking for (ie AnimNotify_Sound)
 *	@param	PlayRate		Rate that animation would be played at
 *	@param	StartPosition	Initial position in the animation to start checking from
 *	@return					Time in seconds that notify would fire if anim was played at given rate 
 *							Returns -1.f if no notify is found
 */
native function float GetNotifyTimeByClass( class<AnimNotify> NotifyClass, optional float PlayRate = 1.f, optional float StartPosition = -1.f, optional out AnimNotify out_Notify, optional out float out_Duration );

defaultproperties
{
	RateScale=1.0
}
