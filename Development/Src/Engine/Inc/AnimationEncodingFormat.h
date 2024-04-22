/*=============================================================================
	AnimationEncodingFormat.h: Skeletal mesh animation compression.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/ 

#ifndef __ANIMATIONENCODINGFORMAT_H__
#define __ANIMATIONENCODINGFORMAT_H__

// switches to toggle subsets of the new animation codec system
#define USE_ANIMATION_CODEC_BATCH_SOLVER 1

// all past encoding package version numbers should be listed here
#define ANIMATION_ENCODING_PACKAGE_ORIGINAL 0

// the current animation encoding package version
#define CURRENT_ANIMATION_ENCODING_PACKAGE_VERSION ANIMATION_ENCODING_PACKAGE_ORIGINAL

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Interfaces For Working With Encoded Animations
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 *	Structure to hold an Atom and Track index mapping for a requested bone. 
 *	Used in the bulk-animation solving process
 */
struct BoneTrackPair
{
	INT AtomIndex;
	INT TrackIndex;

	BoneTrackPair& operator=(const BoneTrackPair& Other)
	{
		this->AtomIndex = Other.AtomIndex;
		this->TrackIndex = Other.TrackIndex;
		return *this;
	}

	BoneTrackPair(){}
	BoneTrackPair(INT Atom, INT Track):AtomIndex(Atom),TrackIndex(Track){}
};

/**
 *	Fixed-size array of BoneTrackPair elements.
 *	Used in the bulk-animation solving process.
 */
#define MAX_BONES 256 // DesiredBones is passed to the decompression routines as a TArray<BYTE>, so we know this max is appropriate
typedef TPreallocatedArray<BoneTrackPair, MAX_BONES> BoneTrackArray;


/**
 * Extracts a single BoneAtom from an Animation Sequence.
 *
 * @param	OutAtom			The BoneAtom to fill with the extracted result.
 * @param	Seq				An Animation Sequence to extract the BoneAtom from.
 * @param	TrackIndex		The index of the track desired in the Animation Sequence.
 * @param	Time			The time (in seconds) to calculate the BoneAtom for.
 * @param	bLooping		TRUE if the animation should be played in a cyclic manner.
 */
void AnimationFormat_GetBoneAtom(	FBoneAtom& OutAtom,
									const UAnimSequence& Seq,
									INT TrackIndex,
									FLOAT Time,
									UBOOL bLooping);

#if USE_ANIMATION_CODEC_BATCH_SOLVER

/**
 * Extracts an array of BoneAtoms from an Animation Sequence representing an entire pose of the skeleton.
 *
 * @param	Atoms				The BoneAtoms to fill with the extracted result.
 * @param	RotationTracks		A BoneTrackArray element for each bone requesting rotation data. 
 * @param	TranslationTracks	A BoneTrackArray element for each bone requesting translation data. 
 * @param	Seq					An Animation Sequence to extract the BoneAtom from.
 * @param	Time				The time (in seconds) to calculate the BoneAtom for.
 * @param	bLooping			TRUE if the animation should be played in a cyclic manner.
 */
void AnimationFormat_GetAnimationPose(	
	FBoneAtomArray& Atoms, 
	const BoneTrackArray& RotationTracks,
	const BoneTrackArray& TranslationTracks,
	const UAnimSequence& Seq,
	FLOAT Time,
	UBOOL bLooping);

#endif

/**
 * Extracts statistics about a given Animation Sequence
 *
 * @param	Seq					An Animation Sequence.
 * @param	NumTransTracks		The total number of Translation Tracks found.
 * @param	NumRotTracks		The total number of Rotation Tracks found.
 * @param	TotalNumTransKeys	The total number of Translation Keys found.
 * @param	TotalNumRotKeys		The total number of Rotation Keys found.
 * @param	TranslationKeySize	The average size (in BYTES) of a single Translation Key.
 * @param	RotationKeySize		The average size (in BYTES) of a single Rotation Key.
 * @param	OverheadSize		The size (in BYTES) of overhead (offsets, scale tables, key->frame lookups, etc...)
 * @param	NumTransTracksWithOneKey	The total number of Translation Tracks found containing a single key.
 * @param	NumRotTracksWithOneKey		The total number of Rotation Tracks found containing a single key.
*/
void AnimationFormat_GetStats(	const UAnimSequence* Seq, 
  								INT& NumTransTracks,
								INT& NumRotTracks,
  								INT& TotalNumTransKeys,
								INT& TotalNumRotKeys,
								FLOAT& TranslationKeySize,
								FLOAT& RotationKeySize,
								INT& OverheadSize,
								INT& NumTransTracksWithOneKey,
								INT& NumRotTracksWithOneKey);


/**
 * Sets the internal Animation Codec Interface Links within an Animation Sequence
 *
 * @param	Seq					An Animation Sequence to setup links within.
*/
void AnimationFormat_SetInterfaceLinks(UAnimSequence& Seq);

#if !CONSOLE
#define AC_UnalignedSwap( MemoryArchive, Data, Len )		\
	MemoryArchive.ByteOrderSerialize( (Data), (Len) );		\
	(Data) += (Len);
#else
	// No need to swap on consoles, as the cooker will have ordered bytes for the target platform.
#define AC_UnalignedSwap( MemoryArchive, Data, Len )		\
	MemoryArchive.Serialize( (Data), (Len) );				\
	(Data) += (Len);
#endif // !CONSOLE

extern const INT CompressedTranslationStrides[ACF_MAX];
extern const INT CompressedTranslationNum[ACF_MAX];
extern const INT CompressedRotationStrides[ACF_MAX];
extern const INT CompressedRotationNum[ACF_MAX];
extern const BYTE PerTrackNumComponentTable[ACF_MAX*8];

class FMemoryWriter;
class FMemoryReader;

void PadMemoryWriter(FMemoryWriter* MemoryWriter, BYTE*& TrackData, const INT Alignment);
void PadMemoryReader(FMemoryReader* MemoryReader, BYTE*& TrackData, const INT Alignment);


class AnimationEncodingFormat
{
public:
	/**
	 * Handles Byte-swapping incoming animation data from a MemoryReader
	 *
	 * @param	Seq					An Animation Sequence to contain the read data.
	 * @param	MemoryReader		The MemoryReader object to read from.
	 * @param	SourceArVersion		The version of the archive that the data is coming from.
	 */
	virtual void ByteSwapIn(
		UAnimSequence& Seq, 
		FMemoryReader& MemoryReader,
		INT SourceArVersion) PURE_VIRTUAL(AnimationEncodingFormat::ByteSwapIn,);

	/**
	 * Handles Byte-swapping outgoing animation data to an array of BYTEs
	 *
	 * @param	Seq					An Animation Sequence to write.
	 * @param	SerializedData		The output buffer.
	 * @param	ForceByteSwapping	TRUE is byte swapping is not optional.
	 */
	virtual void ByteSwapOut(
		UAnimSequence& Seq,
		TArray<BYTE>& SerializedData, 
		UBOOL ForceByteSwapping) PURE_VIRTUAL(AnimationEncodingFormat::ByteSwapOut,);

	/**
	 * Extracts a single BoneAtom from an Animation Sequence.
	 *
	 * @param	OutAtom			The BoneAtom to fill with the extracted result.
	 * @param	Seq				An Animation Sequence to extract the BoneAtom from.
	 * @param	TrackIndex		The index of the track desired in the Animation Sequence.
	 * @param	Time			The time (in seconds) to calculate the BoneAtom for.
	 * @param	bLooping		TRUE if the animation should be played in a cyclic manner.
	 */
	virtual void GetBoneAtom(
		FBoneAtom& OutAtom,
		const UAnimSequence& Seq,
		INT TrackIndex,
		FLOAT Time,
		UBOOL bLooping) PURE_VIRTUAL(AnimationEncodingFormat::GetBoneAtom,);

#if USE_ANIMATION_CODEC_BATCH_SOLVER

	/**
	 * Decompress all requested rotation components from an Animation Sequence
	 *
	 * @param	Atoms			The FBoneAtom array to fill in.
	 * @param	DesiredPairs	Array of requested bone information
	 * @param	Seq				The animation sequence to use.
	 * @param	Time			Current time to solve for.
	 * @param	bLooping		True when looping the stream in intended.
	 * @return					None. 
	 */
	virtual void GetPoseRotations(	
		FBoneAtomArray& Atoms, 
		const BoneTrackArray& DesiredPairs,
		const UAnimSequence& Seq,
		FLOAT Time,
		UBOOL bLooping) PURE_VIRTUAL(AnimationEncodingFormat::GetPoseRotations,);

	/**
	 * Decompress all requested translation components from an Animation Sequence
	 *
	 * @param	Atoms			The FBoneAtom array to fill in.
	 * @param	DesiredPairs	Array of requested bone information
	 * @param	Seq				The animation sequence to use.
	 * @param	Time			Current time to solve for.
	 * @param	bLooping		True when looping the stream in intended.
	 * @return					None. 
	 */
	virtual void GetPoseTranslations(	
		FBoneAtomArray& Atoms, 
		const BoneTrackArray& DesiredPairs,
		const UAnimSequence& Seq,
		FLOAT Time,
		UBOOL bLooping) PURE_VIRTUAL(AnimationEncodingFormat::GetPoseTranslations,);
#endif
protected:

	/**
	 * Utility function to determine the two key indices to interpolate given a relative position in the animation
	 *
	 * @param	Seq				The UAnimSequence container.
	 * @param	RelativePos		The relative position to solve in the range [0,1] inclusive.
	 * @param	bLooping		TRUE if the animation should be consider cyclic (last frame interpolates back to the start)
	 * @param	NumKeys			The number of keys present in the track being solved.
	 * @param	PosIndex0Out	Output value for the closest key index before the RelativePos specified.
	 * @param	PosIndex1Out	Output value for the closest key index after the RelativePos specified.
	 * @return	The rate at which to interpolate the two keys returned to obtain the final result.
	 */
	static FLOAT TimeToIndex(
		const UAnimSequence& Seq,
		FLOAT RelativePos,
		UBOOL bLooping,
		INT NumKeys,
		INT &PosIndex0Out,
		INT &PosIndex1Out);

	/**
	 * Utility function to determine the two key indices to interpolate given a relative position in the animation
	 *
	 * @param	Seq				The UAnimSequence container.
	 * @param	FrameTable		The frame table containing a frame index for each key.
	 * @param	RelativePos		The relative position to solve in the range [0,1] inclusive.
	 * @param	bLooping		TRUE if the animation should be consider cyclic (last frame interpolates back to the start)
	 * @param	NumKeys			The number of keys present in the track being solved.
	 * @param	PosIndex0Out	Output value for the closest key index before the RelativePos specified.
	 * @param	PosIndex1Out	Output value for the closest key index after the RelativePos specified.
	 * @return	The rate at which to interpolate the two keys returned to obtain the final result.
	 */
	static FLOAT TimeToIndex(
		const UAnimSequence& Seq,
		const BYTE* FrameTable,
		FLOAT RelativePos,
		UBOOL bLooping,
		INT NumKeys,
		INT &PosIndex0Out,
		INT &PosIndex1Out);
};


/**
 * This class serves as the base to AEFConstantKeyLerpShared, introducing the per-track serialization methods called by
 * ByteSwapIn/ByteSwapOut and individual GetBoneAtomRotation / GetBoneAtomTranslation calls, which GetBoneAtom calls on
 * Seq.TranslationCodec or Seq.RotationCodec.
 */
class AnimationEncodingFormatLegacyBase : public AnimationEncodingFormat
{
public:
	/**
	 * Extracts a single BoneAtom from an Animation Sequence.
	 *
	 * @param	OutAtom			The BoneAtom to fill with the extracted result.
	 * @param	Seq				An Animation Sequence to extract the BoneAtom from.
	 * @param	TrackIndex		The index of the track desired in the Animation Sequence.
	 * @param	Time			The time (in seconds) to calculate the BoneAtom for.
	 * @param	bLooping		TRUE if the animation should be played in a cyclic manner.
	 */
	virtual void GetBoneAtom(
		FBoneAtom& OutAtom,
		const UAnimSequence& Seq,
		INT TrackIndex,
		FLOAT Time,
		UBOOL bLooping);

	/**
	 * Decompress the Rotation component of a BoneAtom
	 *
	 * @param	OutAtom			The FBoneAtom to fill in.
	 * @param	Seq				The animation sequence to use.
	 * @param	Stream			The compressed animation data.
	 * @param	NumKeys			The number of keys present in Stream.
	 * @param	Time			Current time to solve for.
	 * @param	RelativePos		Current position within the animation to solve for in the range [0.0,1.0].
	 * @param	bLooping		True when looping the stream in intended.
	 * @return					None. 
	 */
	virtual void GetBoneAtomRotation(	
		FBoneAtom& OutAtom,
		const UAnimSequence& Seq,
		const BYTE* RESTRICT Stream,
		INT NumKeys,
		FLOAT Time,
		FLOAT RelativePos,
		UBOOL bLooping) PURE_VIRTUAL(AnimationEncodingFormat::GetBoneAtomRotation,);

	/**
	 * Decompress the Translation component of a BoneAtom
	 *
	 * @param	OutAtom			The FBoneAtom to fill in.
	 * @param	Seq				The animation sequence to use.
	 * @param	Stream			The compressed animation data.
	 * @param	NumKeys			The number of keys present in Stream.
	 * @param	Time			Current time to solve for.
	 * @param	RelativePos		Current position within the animation to solve for in the range [0.0,1.0].
	 * @param	bLooping		True when looping the stream in intended.
	 * @return					None. 
	 */
	virtual void GetBoneAtomTranslation(	
		FBoneAtom& OutAtom,
		const UAnimSequence& Seq,
		const BYTE* RESTRICT Stream,
		INT NumKeys,
		FLOAT Time,
		FLOAT RelativePos,
		UBOOL bLooping) PURE_VIRTUAL(AnimationEncodingFormat::GetBoneAtomTranslation,);

	/**
	 * Handles Byte-swapping incoming animation data from a MemoryReader
	 *
	 * @param	Seq					An Animation Sequence to contain the read data.
	 * @param	MemoryReader		The MemoryReader object to read from.
	 * @param	SourceArVersion		The version of the archive that the data is coming from.
	 */
	virtual void ByteSwapIn(
		UAnimSequence& Seq, 
		FMemoryReader& MemoryReader,
		INT SourceArVersion);

	/**
	 * Handles Byte-swapping outgoing animation data to an array of BYTEs
	 *
	 * @param	Seq					An Animation Sequence to write.
	 * @param	SerializedData		The output buffer.
	 * @param	ForceByteSwapping	TRUE is byte swapping is not optional.
	 */
	virtual void ByteSwapOut(
		UAnimSequence& Seq,
		TArray<BYTE>& SerializedData, 
		UBOOL ForceByteSwapping);

	/**
	 * Handles the ByteSwap of compressed animation data on import
	 *
	 * @param	Seq				The UAnimSequence container.
	 * @param	MemoryReader	The FMemoryReader to read from.
	 * @param	Stream			The compressed animation data.
	 * @param	NumKeys			The number of keys present in Stream.
	 * @param	SourceArVersion	The version number of the source archive stream.
	 * @return					The adjusted Stream position after import. 
	 */
	virtual void ByteSwapRotationIn(
		UAnimSequence& Seq, 
		FMemoryReader& MemoryReader,
		BYTE*& Stream,
		INT NumKeys,
		INT SourceArVersion) PURE_VIRTUAL(AnimationEncodingFormat::ByteSwapRotationIn,);

	/**
	 * Handles the ByteSwap of compressed animation data on import
	 *
	 * @param	Seq				The UAnimSequence container.
	 * @param	MemoryReader	The FMemoryReader to read from.
	 * @param	Stream			The compressed animation data.
	 * @param	NumKeys			The number of keys present in Stream.
	 * @param	SourceArVersion	The version number of the source archive stream.
	 * @return					The adjusted Stream position after import. 
	 */
	virtual void ByteSwapTranslationIn(
		UAnimSequence& Seq, 
		FMemoryReader& MemoryReader,
		BYTE*& Stream,
		INT NumKeys,
		INT SourceArVersion) PURE_VIRTUAL(AnimationEncodingFormat::ByteSwapTranslationIn,);

	/**
	 * Handles the ByteSwap of compressed animation data on export
	 *
	 * @param	Seq				The UAnimSequence container.
	 * @param	MemoryWriter	The FMemoryReader to write to.
	 * @param	Stream			The compressed animation data.
	 * @param	NumKeys			The number of keys present in Stream.
	 * @return					The adjusted Stream position after export. 
	 */
	virtual void ByteSwapRotationOut(
		UAnimSequence& Seq, 
		FMemoryWriter& MemoryWriter,
		BYTE*& Stream,
		INT NumKeys) PURE_VIRTUAL(AnimationEncodingFormat::ByteSwapRotationOut,);

	/**
	 * Handles the ByteSwap of compressed animation data on export
	 *
	 * @param	Seq				The UAnimSequence container.
	 * @param	MemoryWriter	The FMemoryReader to write to.
	 * @param	Stream			The compressed animation data.
	 * @param	NumKeys			The number of keys present in Stream.
	 * @return					The adjusted Stream position after export. 
	 */
	virtual void ByteSwapTranslationOut(
		UAnimSequence& Seq, 
		FMemoryWriter& MemoryWriter,
		BYTE*& Stream,
		INT NumKeys) PURE_VIRTUAL(AnimationEncodingFormat::ByteSwapTranslationOut,);
};






/**
 * Utility function to determine the two key indices to interpolate given a relative position in the animation
 *
 * @param	Seq				The UAnimSequence container.
 * @param	RelativePos		The relative position to solve in the range [0,1] inclusive.
 * @param	bLooping		TRUE if the animation should be consider cyclic (last frame interpolates back to the start)
 * @param	NumKeys			The number of keys present in the track being solved.
 * @param	PosIndex0Out	Output value for the closest key index before the RelativePos specified.
 * @param	PosIndex1Out	Output value for the closest key index after the RelativePos specified.
 * @return	The rate at which to interpolate the two keys returned to obtain the final result.
 */
FORCEINLINE_DEBUGGABLE FLOAT AnimationEncodingFormat::TimeToIndex(
	const UAnimSequence& Seq,
	FLOAT RelativePos,
	UBOOL bLooping,
	INT NumKeys,
	INT &PosIndex0Out,
	INT &PosIndex1Out)
{
	static INT		NumKeysCache = 0; // this value is guaranteed to not be used for valid data
	static FLOAT	TimeCache;
	static FLOAT	SequenceLengthCache;
	static UBOOL	LoopingCache;
	static INT		PosIndex0CacheOut; 
	static INT		PosIndex1CacheOut; 
	static FLOAT	AlphaCacheOut;

	const FLOAT SequenceLength= Seq.SequenceLength;

	if (NumKeys < 2)
	{
		checkSlow(NumKeys == 1); // check if data is empty for some reason.
		PosIndex0Out = 0;
		PosIndex1Out = 0;
		return 0.0f;
	}
	if (
		NumKeysCache		!= NumKeys ||
		LoopingCache		!= bLooping ||
		SequenceLengthCache != SequenceLength ||
		TimeCache			!= RelativePos
		)
	{
		NumKeysCache		= NumKeys;
		LoopingCache		= bLooping;
		SequenceLengthCache = SequenceLength;
		TimeCache			= RelativePos;
		// Check for before-first-frame case.
		if( RelativePos <= 0.f )
		{
			PosIndex0CacheOut = 0;
			PosIndex1CacheOut = 0;
			AlphaCacheOut = 0.0f;
		}
		else
		{
			if (!bLooping)
			{
				NumKeys -= 1; // never used without the minus one in this case
				// Check for after-last-frame case.
				if( RelativePos >= 1.0f )
				{
					// If we're not looping, key n-1 is the final key.
					PosIndex0CacheOut = NumKeys;
					PosIndex1CacheOut = NumKeys;
					AlphaCacheOut = 0.0f;
				}
				else
				{
					// For non-looping animation, the last frame is the ending frame, and has no duration.
					const FLOAT KeyPos = RelativePos * FLOAT(NumKeys);
					checkSlow(KeyPos >= 0.0f);
					const FLOAT KeyPosFloor = floorf(KeyPos);
					PosIndex0CacheOut = Min( appTrunc(KeyPosFloor), NumKeys );
					AlphaCacheOut = KeyPos - KeyPosFloor;
					PosIndex1CacheOut = Min( PosIndex0CacheOut + 1, NumKeys );
				}
			}
			else // we are looping
			{
				// Check for after-last-frame case.
				if( RelativePos >= 1.0f )
				{
					// If we're looping, key 0 is the final key.
					PosIndex0CacheOut = 0;
					PosIndex1CacheOut = 0;
					AlphaCacheOut = 0.0f;
				}
				else
				{
					// Work with animation total frames, to handle looping last->first
					// Our track might have a different number of frames, and we handle that below.
					INT const NumFrames = Seq.NumFrames;

					{
						// For looping animation, the last frame has duration, and interpolates back to the first one.
						const FLOAT KeyPos = RelativePos * FLOAT(NumFrames);
						checkSlow(KeyPos >= 0.0f);
						const FLOAT KeyPosFloor = floorf(KeyPos);
						PosIndex0CacheOut = Min( appTrunc(KeyPosFloor), NumFrames - 1 );
						AlphaCacheOut = KeyPos - KeyPosFloor;
						PosIndex1CacheOut = PosIndex0CacheOut + 1;
					}

					// Handle Looping
					if( PosIndex1CacheOut == NumFrames )
					{
						PosIndex0CacheOut = NumKeys - 1;
						PosIndex1CacheOut = 0;
					}
					// Non Looping! Special treatment if NumKeys is not the same as NumFrames...
					else if( NumFrames != NumKeys )
					{
						// Since we're not looping first to last, chop off the last chunk
						// And do a simple non looping interp between those keys.
						FLOAT const AdjustedPosition = (RelativePos * FLOAT(NumFrames)) / FLOAT(NumFrames-1);
						FLOAT const KeyPos = AdjustedPosition * FLOAT(NumKeys-1);
						checkSlow(KeyPos >= 0.0f);
						FLOAT const KeyPosFloor = floorf(KeyPos);
						PosIndex0CacheOut = Min( appTrunc(KeyPosFloor), NumKeys - 1 );
						AlphaCacheOut = KeyPos - KeyPosFloor;
						PosIndex1CacheOut = Min( PosIndex0CacheOut + 1, NumKeys - 1 );
					}
				}
			}
		}
	}
	PosIndex0Out = PosIndex0CacheOut;
	PosIndex1Out = PosIndex1CacheOut;
	return AlphaCacheOut;
}

/**
 * Utility function to find the key before the specified search value.
 *
 * @param	FrameTable		The frame table, containing on frame index value per key.
 * @param	NumKeys			The total number of keys in the table.
 * @param	SearchFrame		The Frame we are attempting to find.
 * @param	KeyEstimate		An estimate of the best location to search from in the KeyTable.
 * @return	The index of the first key immediately below the specified search frame.
 */
template <typename TABLE_TYPE>
FORCEINLINE_DEBUGGABLE INT FindLowKeyIndex(
	const TABLE_TYPE* FrameTable, 
	INT NumKeys, 
	INT SearchFrame, 
	INT KeyEstimate)
{
	const INT LastKeyIndex = NumKeys-1;
	INT LowKeyIndex = KeyEstimate;

	if (FrameTable[KeyEstimate] <= SearchFrame)
	{
		// unless we find something better, we'll default to the last key
		LowKeyIndex = LastKeyIndex;

		// search forward from the estimate for the first value greater than our search parameter
		// if found, this is the high key and we want the one just prior to it
		for (INT i = KeyEstimate+1; i <= LastKeyIndex; ++i)
		{
			if (FrameTable[i] > SearchFrame)
			{
				LowKeyIndex= i-1;
				break;
			}
		}
	}
	else
	{
		// unless we find something better, we'll default to the first key
		LowKeyIndex = 0;

		// search backward from the estimate for the first value less than or equal to the search parameter
		// if found, this is the low key we are searching for
		for (INT i = KeyEstimate-1; i > 0; --i)
		{
			if (FrameTable[i] <= SearchFrame)
			{
				LowKeyIndex= i;
				break;
			}
		}
	}

	return LowKeyIndex;
}

/**
 * Utility function to determine the two key indices to interpolate given a relative position in the animation
 *
 * @param	Seq				The UAnimSequence container.
 * @param	FrameTable		The frame table containing a frame index for each key.
 * @param	RelativePos		The relative position to solve in the range [0,1] inclusive.
 * @param	bLooping		TRUE if the animation should be consider cyclic (last frame interpolates back to the start)
 * @param	NumKeys			The number of keys present in the track being solved.
 * @param	PosIndex0Out	Output value for the closest key index before the RelativePos specified.
 * @param	PosIndex1Out	Output value for the closest key index after the RelativePos specified.
 * @return	The rate at which to interpolate the two keys returned to obtain the final result.
 */
FORCEINLINE_DEBUGGABLE FLOAT AnimationEncodingFormat::TimeToIndex(
	const UAnimSequence& Seq,
	const BYTE* FrameTable,
	FLOAT RelativePos,
	UBOOL bLooping,
	INT NumKeys,
	INT &PosIndex0Out,
	INT &PosIndex1Out)
{
	const FLOAT SequenceLength = Seq.SequenceLength;
	FLOAT Alpha = 0.0f;

	check(NumKeys != 0);
	
	const INT LastKey= NumKeys-1;
	
	INT TotalFrames = Seq.NumFrames-1;
	INT EndingKey = LastKey;
	if (bLooping)
	{
		TotalFrames = Seq.NumFrames;
		EndingKey = 0;
	}

	if (NumKeys < 2 || RelativePos <= 0.f)
	{
		// return the first key
		PosIndex0Out = 0;
		PosIndex1Out = 0;
		Alpha = 0.0f;
	}
	else if( RelativePos >= 1.0f )
	{
		// return the ending key
		PosIndex0Out = EndingKey;
		PosIndex1Out = EndingKey;
		Alpha = 0.0f;
	}
	else
	{
		// find the proper key range to return
		const INT LastFrame= TotalFrames-1;
		const FLOAT KeyPos = RelativePos * (FLOAT)LastKey;
		const FLOAT FramePos = RelativePos * (FLOAT)TotalFrames;
		const INT FramePosFloor = Clamp(appTrunc(FramePos), 0, LastFrame);
		const INT KeyEstimate = Clamp(appTrunc(KeyPos), 0, LastKey);

		INT LowFrame = 0;
		INT HighFrame = 0;
		
		// find the pair of keys which surround our target frame index
		if (Seq.NumFrames > 0xFF)
		{
			const WORD* Frames= (WORD*)FrameTable;
			PosIndex0Out = FindLowKeyIndex<WORD>(Frames, NumKeys, FramePosFloor, KeyEstimate);
			LowFrame = Frames[PosIndex0Out];

			PosIndex1Out = PosIndex0Out + 1;
			if (PosIndex1Out > LastKey)
			{
				PosIndex1Out= EndingKey;
			}
			HighFrame= Frames[PosIndex1Out];
		}
		else
		{
			const BYTE* Frames= (BYTE*)FrameTable;
			PosIndex0Out = FindLowKeyIndex<BYTE>(Frames, NumKeys, FramePosFloor, KeyEstimate);
			LowFrame = Frames[PosIndex0Out];

			PosIndex1Out = PosIndex0Out + 1;
			if (PosIndex1Out > LastKey)
			{
				PosIndex1Out= EndingKey;
			}
			HighFrame= Frames[PosIndex1Out];
		}

		// compute the blend parameters for the keys we have found
		INT Delta= Max(HighFrame - LowFrame, 1);
		const FLOAT Remainder = (FramePos - (FLOAT)LowFrame);
		Alpha = Remainder / (FLOAT)Delta;
	}
	
	return Alpha;
}

#endif // __ANIMATIONENCODINGFORMAT_H__
