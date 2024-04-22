/*=============================================================================
	AnimationEncodingFormat.cpp: Skeletal mesh animation functions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/ 

#include "EnginePrivate.h"
#include "EngineAnimClasses.h"
#include "EngineAudioDeviceClasses.h"
#include "AnimationCompression.h"
#include "AnimationEncodingFormat.h"
#include "AnimationUtils.h"

// known codecs
#include "AnimationEncodingFormat_ConstantKeyLerp.h"
#include "AnimationEncodingFormat_VariableKeyLerp.h"
#include "AnimationEncodingFormat_PerTrackCompression.h"

/** Each CompresedTranslationData track's ByteStream will be byte swapped in chunks of this size. */
const INT CompressedTranslationStrides[ACF_MAX] =
{
	sizeof(FLOAT),						// ACF_None					(float X, float Y, float Z)
	sizeof(FLOAT),						// ACF_Float96NoW			(float X, float Y, float Z)
	sizeof(FLOAT),						// ACF_Fixed48NoW			(Illegal value for translation)
	sizeof(FVectorIntervalFixed32NoW),	// ACF_IntervalFixed32NoW	(compressed to 11-11-10 per-component interval fixed point)
	sizeof(FLOAT),						// ACF_Fixed32NoW			(Illegal value for translation)
	sizeof(FLOAT),						// ACF_Float32NoW			(Illegal value for translation)
	0									// ACF_Identity
};

/** Number of swapped chunks per element. */
const INT CompressedTranslationNum[ACF_MAX] =
{
	3,	// ACF_None					(float X, float Y, float Z)
	3,	// ACF_Float96NoW			(float X, float Y, float Z)
	3,	// ACF_Fixed48NoW			(Illegal value for translation)
	1,	// ACF_IntervalFixed32NoW	(compressed to 11-11-10 per-component interval fixed point)
	3,	// ACF_Fixed32NoW			(Illegal value for translation)
	3,	// ACF_Float32NoW			(Illegal value for translation)
	0	// ACF_Identity
};

/** Each CompresedRotationData track's ByteStream will be byte swapped in chunks of this size. */
const INT CompressedRotationStrides[ACF_MAX] =
{
	sizeof(FLOAT),						// ACF_None					(FQuats are serialized per element hence sizeof(FLOAT) rather than sizeof(FQuat).
	sizeof(FLOAT),						// ACF_Float96NoW			(FQuats with one component dropped and the remaining three uncompressed at 32bit floating point each
	sizeof(WORD),						// ACF_Fixed48NoW			(FQuats with one component dropped and the remaining three compressed to 16-16-16 fixed point.
	sizeof(FQuatIntervalFixed32NoW),	// ACF_IntervalFixed32NoW	(FQuats with one component dropped and the remaining three compressed to 11-11-10 per-component interval fixed point.
	sizeof(FQuatFixed32NoW),			// ACF_Fixed32NoW			(FQuats with one component dropped and the remaining three compressed to 11-11-10 fixed point.
	sizeof(FQuatFloat32NoW),			// ACF_Float32NoW			(FQuats with one component dropped and the remaining three compressed to 11-11-10 floating point.
	0	// ACF_Identity
};

/** Number of swapped chunks per element. */
const INT CompressedRotationNum[ACF_MAX] =
{
	4,	// ACF_None					(FQuats are serialized per element hence sizeof(FLOAT) rather than sizeof(FQuat).
	3,	// ACF_Float96NoW			(FQuats with one component dropped and the remaining three uncompressed at 32bit floating point each
	3,	// ACF_Fixed48NoW			(FQuats with one component dropped and the remaining three compressed to 16-16-16 fixed point.
	1,	// ACF_IntervalFixed32NoW	(FQuats with one component dropped and the remaining three compressed to 11-11-10 per-component interval fixed point.
	1,	// ACF_Fixed32NoW			(FQuats with one component dropped and the remaining three compressed to 11-11-10 fixed point.
	1,  // ACF_Float32NoW			(FQuats with one component dropped and the remaining three compressed to 11-11-10 floating point.
	0	// ACF_Identity
};

/** Number of swapped chunks per element, split out per component (high 3 bits) and flags (low 3 bits)
  *
  * Note: The entry for ACF_IntervalFixed32NoW is special, and actually indicates how many fixed components there are!
  **/
const BYTE PerTrackNumComponentTable[ACF_MAX * 8] =
{
	4,4,4,4,4,4,4,4,	// ACF_None
	3,1,1,2,1,2,2,3,	// ACF_Float96NoW (0 is special, as uncompressed rotation gets 'mis'encoded with 0 instead of 7, so it's treated as a 3; a genuine 0 would use ACF_Identity)
	3,1,1,2,1,2,2,3,	// ACF_Fixed48NoW (ditto)
	6,2,2,4,2,4,4,6,	// ACF_IntervalFixed32NoW (special, indicates number of interval pairs stored in the fixed track)
	1,1,1,1,1,1,1,1,	// ACF_Fixed32NoW
	1,1,1,1,1,1,1,1,	// ACF_Float32NoW
	0,0,0,0,0,0,0,0		// ACF_Identity
};

/**
 * Compressed translation data will be byte swapped in chunks of this size.
 */
inline INT GetCompressedTranslationStride(AnimationCompressionFormat TranslationCompressionFormat)
{
	return CompressedTranslationStrides[TranslationCompressionFormat];
}

/**
 * Compressed rotation data will be byte swapped in chunks of this size.
 */
inline INT GetCompressedRotationStride(AnimationCompressionFormat RotationCompressionFormat)
{
	return CompressedRotationStrides[RotationCompressionFormat];
}


/**
 * Compressed translation data will be byte swapped in chunks of this size.
 */
inline INT GetCompressedTranslationStride(const UAnimSequence* Seq)
{
	return CompressedTranslationStrides[static_cast<AnimationCompressionFormat>(Seq->TranslationCompressionFormat)];
}

/**
 * Compressed rotation data will be byte swapped in chunks of this size.
 */
inline INT GetCompressedRotationStride(const UAnimSequence* Seq)
{
	return CompressedRotationStrides[static_cast<AnimationCompressionFormat>(Seq->RotationCompressionFormat)];
}

/**
 * Pads a specified number of bytes to the memory writer to maintain alignment
 */
void PadMemoryWriter(FMemoryWriter* MemoryWriter, BYTE*& TrackData, const INT Alignment)
{
	const PTRINT ByteStreamLoc = (PTRINT) TrackData;
	const INT Pad = static_cast<INT>( Align( ByteStreamLoc, Alignment ) - ByteStreamLoc );
	const BYTE PadSentinel = 85; // (1<<1)+(1<<3)+(1<<5)+(1<<7)
	
	for ( INT PadByteIndex = 0; PadByteIndex < Pad; ++PadByteIndex )
	{
		MemoryWriter->Serialize( (void*)&PadSentinel, sizeof(BYTE) );
	}
	TrackData += Pad;
}

/**
 * Skips a specified number of bytes in the memory reader to maintain alignment
 */
void PadMemoryReader(FMemoryReader* MemoryReader, BYTE*& TrackData, const INT Alignment)
{
	const PTRINT ByteStreamLoc = (PTRINT) TrackData;
	const INT Pad = static_cast<INT>( Align( ByteStreamLoc, Alignment ) - ByteStreamLoc );
	MemoryReader->Serialize( TrackData, Pad );
	TrackData += Pad;
}

/**
 * Extracts a single BoneAtom from an Animation Sequence.
 *
 * @param	OutAtom			The BoneAtom to fill with the extracted result.
 * @param	Seq				An Animation Sequence to extract the BoneAtom from.
 * @param	TrackIndex		The index of the track desired in the Animation Sequence.
 * @param	Time			The time (in seconds) to calculate the BoneAtom for.
 * @param	bLooping		TRUE if the animation should be played in a cyclic manner.
 */
void AnimationFormat_GetBoneAtom(	
	FBoneAtom& OutAtom,
	const UAnimSequence& Seq,
	INT TrackIndex,
	FLOAT Time,
	UBOOL bLooping)
{
	checkSlow(Seq.RotationCodec != NULL);
	((AnimationEncodingFormat*)Seq.RotationCodec)->GetBoneAtom(OutAtom, Seq, TrackIndex, Time, bLooping);
}

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
	const BoneTrackArray& RotationPairs,
	const BoneTrackArray& TranslationPairs,
	const UAnimSequence& Seq,
	FLOAT Time,
	UBOOL bLooping)
{
	// decompress the translation component using the proper method
	checkSlow(Seq.TranslationCodec != NULL);
	((AnimationEncodingFormat*)Seq.TranslationCodec)->GetPoseTranslations(Atoms, TranslationPairs, Seq, Time, bLooping);

	// decompress the rotation component using the proper method
	checkSlow(Seq.RotationCodec != NULL);
	((AnimationEncodingFormat*)Seq.RotationCodec)->GetPoseRotations(Atoms, RotationPairs, Seq, Time, bLooping);
}
#endif

/**
 * Extracts a single BoneAtom from an Animation Sequence.
 *
 * @param	OutAtom			The BoneAtom to fill with the extracted result.
 * @param	Seq				An Animation Sequence to extract the BoneAtom from.
 * @param	TrackIndex		The index of the track desired in the Animation Sequence.
 * @param	Time			The time (in seconds) to calculate the BoneAtom for.
 * @param	bLooping		TRUE if the animation should be played in a cyclic manner.
 */
void AnimationEncodingFormatLegacyBase::GetBoneAtom(
	FBoneAtom& OutAtom,
	const UAnimSequence& Seq,
	INT TrackIndex,
	FLOAT Time,
	UBOOL bLooping)
{
	// Initialize to identity to set the scale and in case of a missing rotation or translation codec
	OutAtom.SetIdentity();

	// Use the CompressedTrackOffsets stream to find the data addresses
	const INT* RESTRICT TrackData= Seq.CompressedTrackOffsets.GetTypedData() + (TrackIndex*4);
	INT TransKeysOffset = *(TrackData+0);
	INT NumTransKeys	= *(TrackData+1);
	INT RotKeysOffset	= *(TrackData+2);
	INT NumRotKeys		= *(TrackData+3);
	const BYTE* RESTRICT TransStream	= Seq.CompressedByteStream.GetTypedData()+TransKeysOffset;
	const BYTE* RESTRICT RotStream		= Seq.CompressedByteStream.GetTypedData()+RotKeysOffset;

	const FLOAT RelativePos = Time / (FLOAT)Seq.SequenceLength;

	// decompress the translation component using the proper method
	checkSlow(Seq.TranslationCodec != NULL);
	((AnimationEncodingFormatLegacyBase*)Seq.TranslationCodec)->GetBoneAtomTranslation(OutAtom, Seq, TransStream, NumTransKeys, Time, RelativePos, bLooping);

	// decompress the rotation component using the proper method
	checkSlow(Seq.RotationCodec != NULL);
	((AnimationEncodingFormatLegacyBase*)Seq.RotationCodec)->GetBoneAtomRotation(OutAtom, Seq, RotStream, NumRotKeys, Time, RelativePos, bLooping);
}

/**
 * Handles Byte-swapping incoming animation data from a MemoryReader
 *
 * @param	Seq					An Animation Sequence to contain the read data.
 * @param	MemoryReader		The MemoryReader object to read from.
 * @param	SourceArVersion		The version of the archive that the data is coming from.
 */
//@todo.VC10: Apparent VC10 compiler bug here causes an access violation in optimized builds
#if _MSC_VER
	PRAGMA_DISABLE_OPTIMIZATION
#endif
void AnimationEncodingFormatLegacyBase::ByteSwapIn(
	UAnimSequence& Seq, 
	FMemoryReader& MemoryReader,
	INT SourceArVersion)
{
	const INT NumTracks = Seq.CompressedTrackOffsets.Num() / 4;

	INT OriginalNumBytes = MemoryReader.TotalSize();
	Seq.CompressedByteStream.Empty(OriginalNumBytes);
	Seq.CompressedByteStream.Add(OriginalNumBytes);

	//@compatibility:
	// This variable keeps track of the number of bounds bytes skipped from older files that
	// didn't need the bounds.  They're located at the start of each rotation track with
	// more than one key, but are only needed if the format is ACF_IntervalFixed32NoW.
	INT SkippedBoundsBytes = 0;

	// Read and swap
	BYTE* StreamBase = Seq.CompressedByteStream.GetTypedData();

	for ( INT TrackIndex = 0; TrackIndex < NumTracks; ++TrackIndex )
	{
		//@compatibility:
		// Correct subsequent offsets in the rotation stream for the removed bounds
		if (SkippedBoundsBytes > 0)
		{
			Seq.CompressedTrackOffsets(TrackIndex*4+0) -= SkippedBoundsBytes;
			Seq.CompressedTrackOffsets(TrackIndex*4+2) -= SkippedBoundsBytes;
		}

		const INT OffsetTrans	= Seq.CompressedTrackOffsets(TrackIndex*4+0);
		const INT NumKeysTrans	= Seq.CompressedTrackOffsets(TrackIndex*4+1);
		const INT OffsetRot		= Seq.CompressedTrackOffsets(TrackIndex*4+2);
		const INT NumKeysRot	= Seq.CompressedTrackOffsets(TrackIndex*4+3);

		// Translation data.
		checkSlow( (OffsetTrans % 4) == 0 && "CompressedByteStream not aligned to four bytes" );
		BYTE* TransTrackData = StreamBase + OffsetTrans;
		checkSlow(Seq.TranslationCodec != NULL);
		((AnimationEncodingFormatLegacyBase*)Seq.TranslationCodec)->ByteSwapTranslationIn(Seq, MemoryReader, TransTrackData, NumKeysTrans, SourceArVersion);

		// Like the compressed byte stream, pad the serialization stream to four bytes.
		// As a sanity check, each pad byte can be checked to be the PadSentinel.
		PadMemoryReader(&MemoryReader, TransTrackData, 4); 

		//@compatibility:
		if ((SourceArVersion < VER_OPTIMIZED_ANIMSEQ) && (Seq.RotationCompressionFormat != ACF_IntervalFixed32NoW) && (NumKeysRot > 1))
		{
			SkippedBoundsBytes += sizeof(FLOAT) * 6;
			// Skip the Mins and Ranges data from animation streams that don't need it
			MemoryReader.Seek(MemoryReader.Tell() + sizeof(FLOAT) * 6);
		}
		//@endcompatibility


		// Rotation data.
		checkSlow( (OffsetRot % 4) == 0 && "CompressedByteStream not aligned to four bytes" );
		BYTE* RotTrackData = StreamBase + OffsetRot;
		checkSlow(Seq.RotationCodec != NULL);
		((AnimationEncodingFormatLegacyBase*)Seq.RotationCodec)->ByteSwapRotationIn(Seq, MemoryReader, RotTrackData, NumKeysRot, SourceArVersion);

		// Like the compressed byte stream, pad the serialization stream to four bytes.
		// As a sanity check, each pad byte can be checked to be the PadSentinel.
		PadMemoryReader(&MemoryReader, RotTrackData, 4); 
	}

	//@compatibility:
	if (SkippedBoundsBytes > 0)
	{
		// Resize the compressed byte stream to exact size if some obsolete data was thrown away during serialization
		Seq.CompressedByteStream.Remove(Seq.CompressedByteStream.Num() - SkippedBoundsBytes, SkippedBoundsBytes);
		Seq.CompressedByteStream.Shrink();

#if CHECK_SLOW
		// Verify that all of the offsets and sizes are within the array, since data was repacked
		for (INT TrackIndex = 0; TrackIndex < NumTracks; ++TrackIndex)
		{
			const INT OffsetTrans	= Seq.CompressedTrackOffsets(TrackIndex*4+0);
			const INT NumKeysTrans	= Seq.CompressedTrackOffsets(TrackIndex*4+1);
			const INT EffectiveTransFormat = (NumKeysTrans == 1) ? ACF_None : Seq.TranslationCompressionFormat;
			const INT TransKeySize = CompressedTranslationStrides[EffectiveTransFormat] * CompressedTranslationNum[EffectiveTransFormat];
			checkSlow(OffsetTrans + NumKeysTrans * TransKeySize <= Seq.CompressedByteStream.Num());

			const INT OffsetRot		= Seq.CompressedTrackOffsets(TrackIndex*4+2);
			const INT NumKeysRot	= Seq.CompressedTrackOffsets(TrackIndex*4+3);
			const INT EffectiveRotFormat = (NumKeysRot == 1) ? ACF_Float96NoW : Seq.RotationCompressionFormat;
			const INT RotKeySize = CompressedRotationStrides[EffectiveRotFormat] * CompressedRotationNum[EffectiveRotFormat];
			checkSlow(OffsetRot + NumKeysRot * RotKeySize <= Seq.CompressedByteStream.Num());
		}
#endif
	}
}
	//@todo.VC10: Apparent VC10 compiler bug here causes an access violation in optimized builds
#if _MSC_VER
	PRAGMA_ENABLE_OPTIMIZATION
#endif

/**
 * Handles Byte-swapping outgoing animation data to an array of BYTEs
 *
 * @param	Seq					An Animation Sequence to write.
 * @param	SerializedData		The output buffer.
 * @param	ForceByteSwapping	TRUE is byte swapping is not optional.
 */
void AnimationEncodingFormatLegacyBase::ByteSwapOut(
	UAnimSequence& Seq,
	TArray<BYTE>& SerializedData, 
	UBOOL ForceByteSwapping)
{
	FMemoryWriter MemoryWriter( SerializedData, TRUE );
	MemoryWriter.SetByteSwapping( ForceByteSwapping );

	BYTE* StreamBase		= Seq.CompressedByteStream.GetTypedData();
	const INT NumTracks		= Seq.CompressedTrackOffsets.Num()/4;

	for ( INT TrackIndex = 0; TrackIndex < NumTracks; ++TrackIndex )
	{
		const INT OffsetTrans	= Seq.CompressedTrackOffsets(TrackIndex*4);
		const INT NumKeysTrans	= Seq.CompressedTrackOffsets(TrackIndex*4+1);
		const INT OffsetRot		= Seq.CompressedTrackOffsets(TrackIndex*4+2);
		const INT NumKeysRot	= Seq.CompressedTrackOffsets(TrackIndex*4+3);

		// Translation data.
		checkSlow( (OffsetTrans % 4) == 0 && "CompressedByteStream not aligned to four bytes" );
		BYTE* TransTrackData = StreamBase + OffsetTrans;
		if (Seq.TranslationCodec != NULL)
		{
			((AnimationEncodingFormatLegacyBase*)Seq.TranslationCodec)->ByteSwapTranslationOut(Seq, MemoryWriter, TransTrackData, NumKeysTrans);
		}
		else
		{
			appErrorf( TEXT("%i: unknown or unsupported animation format"), (INT)Seq.KeyEncodingFormat );
		};

		// Like the compressed byte stream, pad the serialization stream to four bytes.
		PadMemoryWriter(&MemoryWriter, TransTrackData, 4);

		// Rotation data.
		checkSlow( (OffsetRot % 4) == 0 && "CompressedByteStream not aligned to four bytes" );
		BYTE* RotTrackData = StreamBase + OffsetRot;
		checkSlow(Seq.RotationCodec != NULL);
		((AnimationEncodingFormatLegacyBase*)Seq.RotationCodec)->ByteSwapRotationOut(Seq, MemoryWriter, RotTrackData, NumKeysRot);

		// Like the compressed byte stream, pad the serialization stream to four bytes.
		PadMemoryWriter(&MemoryWriter, RotTrackData, 4);
	}
}

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
void AnimationFormat_GetStats(	
	const UAnimSequence* Seq, 
	INT& NumTransTracks,
	INT& NumRotTracks,
	INT& TotalNumTransKeys,
	INT& TotalNumRotKeys,
	FLOAT& TranslationKeySize,
	FLOAT& RotationKeySize,
	INT& OverheadSize,
	INT& NumTransTracksWithOneKey,
	INT& NumRotTracksWithOneKey)
{
	if (Seq)
	{
		OverheadSize = Seq->CompressedTrackOffsets.Num() * sizeof(INT);
		const size_t KeyFrameLookupSize = (Seq->NumFrames > 0xFF) ? sizeof(WORD) : sizeof(BYTE);

		if (Seq->KeyEncodingFormat != AKF_PerTrackCompression)
		{
			const INT TransStride	= GetCompressedTranslationStride(Seq);
			const INT RotStride		= GetCompressedRotationStride(Seq);
			const INT TransNum		= CompressedTranslationNum[Seq->TranslationCompressionFormat];
			const INT RotNum		= CompressedRotationNum[Seq->RotationCompressionFormat];

			TranslationKeySize = TransStride * TransNum;
			RotationKeySize = RotStride * RotNum;

			// Track number of tracks.
			NumTransTracks	= Seq->CompressedTrackOffsets.Num()/4;
			NumRotTracks	= Seq->CompressedTrackOffsets.Num()/4;

			// Track total number of keys.
			TotalNumTransKeys = 0;
			TotalNumRotKeys = 0;

			// Track number of tracks with a single key.
			NumTransTracksWithOneKey = 0;
			NumRotTracksWithOneKey = 0;

			// Translation.
			for ( INT TrackIndex = 0; TrackIndex < NumTransTracks; ++TrackIndex )
			{
				const INT NumKeys = Seq->CompressedTrackOffsets(TrackIndex*4+1);
				TotalNumTransKeys += NumKeys;
				if ( NumKeys == 1 )
				{
					++NumTransTracksWithOneKey;
				}
				else
				{
					OverheadSize += (Seq->KeyEncodingFormat == AKF_VariableKeyLerp) ? NumKeys * KeyFrameLookupSize : 0;
				}
			}

			// Rotation.
			for ( INT TrackIndex = 0; TrackIndex < NumRotTracks; ++TrackIndex )
			{
				const INT NumKeys = Seq->CompressedTrackOffsets(TrackIndex*4+3);
				TotalNumRotKeys += NumKeys;
				if ( NumKeys == 1 )
				{
					++NumRotTracksWithOneKey;
				}
				else
				{
					OverheadSize += (Seq->KeyEncodingFormat == AKF_VariableKeyLerp) ? NumKeys * KeyFrameLookupSize : 0;
				}
			}

			// Add in scaling values (min+range for interval encoding)
			OverheadSize += (Seq->RotationCompressionFormat == ACF_IntervalFixed32NoW) ? (NumRotTracks - NumRotTracksWithOneKey) * sizeof(FLOAT) * 6 : 0;
			OverheadSize += (Seq->TranslationCompressionFormat == ACF_IntervalFixed32NoW) ? (NumTransTracks - NumTransTracksWithOneKey) * sizeof(FLOAT) * 6 : 0;
		}
		else
		{
			INT TotalTransKeysThatContributedSize = 0;
			INT TotalRotKeysThatContributedSize = 0;

			TranslationKeySize = 0;
			RotationKeySize = 0;

			// Track number of tracks.
			NumTransTracks = Seq->CompressedTrackOffsets.Num() / 2;
			NumRotTracks = Seq->CompressedTrackOffsets.Num() / 2;

			// Track total number of keys.
			TotalNumTransKeys = 0;
			TotalNumRotKeys = 0;

			// Track number of tracks with a single key.
			NumTransTracksWithOneKey = 0;
			NumRotTracksWithOneKey = 0;

			// Translation.
			for (INT TrackIndex = 0; TrackIndex < NumTransTracks; ++TrackIndex)
			{
				const INT TransOffset = Seq->CompressedTrackOffsets(TrackIndex*2+0);
				if (TransOffset == INDEX_NONE)
				{
					++TotalNumTransKeys;
					++NumTransTracksWithOneKey;
				}
				else
				{
					const INT Header = *((INT*)(Seq->CompressedByteStream.GetTypedData() + TransOffset));

					INT KeyFormat;
					INT FormatFlags;
					INT TrackBytesPerKey;
					INT TrackFixedBytes;
					INT NumKeys;

					FAnimationCompression_PerTrackUtils::DecomposeHeader(Header, /*OUT*/ KeyFormat, /*OUT*/ NumKeys, /*OUT*/ FormatFlags, /*OUT*/ TrackBytesPerKey, /*OUT*/ TrackFixedBytes);
					TranslationKeySize += TrackBytesPerKey * NumKeys;
					TotalTransKeysThatContributedSize += NumKeys;
					OverheadSize += TrackFixedBytes;
					OverheadSize += ((FormatFlags & 0x08) != 0) ? NumKeys * KeyFrameLookupSize : 0;
					
					TotalNumTransKeys += NumKeys;
					if (NumKeys <= 1)
					{
						++NumTransTracksWithOneKey;
					}
				}
			}

			// Rotation.
			for (INT TrackIndex = 0; TrackIndex < NumRotTracks; ++TrackIndex)
			{
				const INT RotOffset = Seq->CompressedTrackOffsets(TrackIndex*2+1);
				if (RotOffset == INDEX_NONE)
				{
					++TotalNumRotKeys;
					++NumRotTracksWithOneKey;
				}
				else
				{
					const INT Header = *((INT*)(Seq->CompressedByteStream.GetTypedData() + RotOffset));

					INT KeyFormat;
					INT FormatFlags;
					INT TrackBytesPerKey;
					INT TrackFixedBytes;
					INT NumKeys;

					FAnimationCompression_PerTrackUtils::DecomposeHeader(Header, /*OUT*/ KeyFormat, /*OUT*/ NumKeys, /*OUT*/ FormatFlags, /*OUT*/ TrackBytesPerKey, /*OUT*/ TrackFixedBytes);
					RotationKeySize += TrackBytesPerKey * NumKeys;
					TotalRotKeysThatContributedSize += NumKeys;
					OverheadSize += TrackFixedBytes;
					OverheadSize += ((FormatFlags & 0x08) != 0) ? NumKeys * KeyFrameLookupSize : 0;

					TotalNumRotKeys += NumKeys;
					if (NumKeys <= 1)
					{
						++NumRotTracksWithOneKey;
					}
				}
			}

			// Average key sizes
			if (TotalRotKeysThatContributedSize > 0)
			{
				RotationKeySize = RotationKeySize / TotalRotKeysThatContributedSize;
			}

			if (TotalTransKeysThatContributedSize > 0)
			{
				TranslationKeySize = TranslationKeySize / TotalTransKeysThatContributedSize;
			}
		}
	}
}

/**
 * Sets the internal Animation Codec Interface Links within an Animation Sequence
 *
 * @param	Seq					An Animation Sequence to setup links within.
*/
void AnimationFormat_SetInterfaceLinks(UAnimSequence& Seq)
{
	Seq.TranslationCodec = NULL;
	Seq.RotationCodec = NULL;

	if (Seq.KeyEncodingFormat == AKF_ConstantKeyLerp)
	{
		static AEFConstantKeyLerp<ACF_None>					AEFConstantKeyLerp_None;
		static AEFConstantKeyLerp<ACF_Float96NoW>			AEFConstantKeyLerp_Float96NoW;
		static AEFConstantKeyLerp<ACF_Fixed48NoW>			AEFConstantKeyLerp_Fixed48NoW;
		static AEFConstantKeyLerp<ACF_IntervalFixed32NoW>	AEFConstantKeyLerp_IntervalFixed32NoW;
		static AEFConstantKeyLerp<ACF_Fixed32NoW>			AEFConstantKeyLerp_Fixed32NoW;
		static AEFConstantKeyLerp<ACF_Float32NoW>			AEFConstantKeyLerp_Float32NoW;
		static AEFConstantKeyLerp<ACF_Identity>				AEFConstantKeyLerp_Identity;

		// setup translation codec
		switch(Seq.TranslationCompressionFormat)
		{
			case ACF_None:
				Seq.TranslationCodec = &AEFConstantKeyLerp_None;
				break;
			case ACF_Float96NoW:
				Seq.TranslationCodec = &AEFConstantKeyLerp_Float96NoW;
				break;
			case ACF_IntervalFixed32NoW:
				Seq.TranslationCodec = &AEFConstantKeyLerp_IntervalFixed32NoW;
				break;
			case ACF_Identity:
				Seq.TranslationCodec = &AEFConstantKeyLerp_Identity;
				break;

			default:
				appErrorf( TEXT("%i: unknown or unsupported translation compression"), (INT)Seq.TranslationCompressionFormat );
		};

		// setup rotation codec
		switch(Seq.RotationCompressionFormat)
		{
			case ACF_None:
				Seq.RotationCodec = &AEFConstantKeyLerp_None;
				break;
			case ACF_Float96NoW:
				Seq.RotationCodec = &AEFConstantKeyLerp_Float96NoW;
				break;
			case ACF_Fixed48NoW:
				Seq.RotationCodec = &AEFConstantKeyLerp_Fixed48NoW;
				break;
			case ACF_IntervalFixed32NoW:
				Seq.RotationCodec = &AEFConstantKeyLerp_IntervalFixed32NoW;
				break;
			case ACF_Fixed32NoW:
				Seq.RotationCodec = &AEFConstantKeyLerp_Fixed32NoW;
				break;
			case ACF_Float32NoW:
				Seq.RotationCodec = &AEFConstantKeyLerp_Float32NoW;
				break;
			case ACF_Identity:
				Seq.RotationCodec = &AEFConstantKeyLerp_Identity;
				break;

			default:
				appErrorf( TEXT("%i: unknown or unsupported rotation compression"), (INT)Seq.RotationCompressionFormat );
		};
	}
	else if (Seq.KeyEncodingFormat == AKF_VariableKeyLerp)
	{
		static AEFVariableKeyLerp<ACF_None>					AEFVariableKeyLerp_None;
		static AEFVariableKeyLerp<ACF_Float96NoW>			AEFVariableKeyLerp_Float96NoW;
		static AEFVariableKeyLerp<ACF_Fixed48NoW>			AEFVariableKeyLerp_Fixed48NoW;
		static AEFVariableKeyLerp<ACF_IntervalFixed32NoW>	AEFVariableKeyLerp_IntervalFixed32NoW;
		static AEFVariableKeyLerp<ACF_Fixed32NoW>			AEFVariableKeyLerp_Fixed32NoW;
		static AEFVariableKeyLerp<ACF_Float32NoW>			AEFVariableKeyLerp_Float32NoW;
		static AEFVariableKeyLerp<ACF_Identity>				AEFVariableKeyLerp_Identity;

		// setup translation codec
		switch(Seq.TranslationCompressionFormat)
		{
			case ACF_None:
				Seq.TranslationCodec = &AEFVariableKeyLerp_None;
				break;
			case ACF_Float96NoW:
				Seq.TranslationCodec = &AEFVariableKeyLerp_Float96NoW;
				break;
			case ACF_IntervalFixed32NoW:
				Seq.TranslationCodec = &AEFVariableKeyLerp_IntervalFixed32NoW;
				break;
			case ACF_Identity:
				Seq.TranslationCodec = &AEFVariableKeyLerp_Identity;
				break;

			default:
				appErrorf( TEXT("%i: unknown or unsupported translation compression"), (INT)Seq.TranslationCompressionFormat );
		};

		// setup rotation codec
		switch(Seq.RotationCompressionFormat)
		{
			case ACF_None:
				Seq.RotationCodec = &AEFVariableKeyLerp_None;
				break;
			case ACF_Float96NoW:
				Seq.RotationCodec = &AEFVariableKeyLerp_Float96NoW;
				break;
			case ACF_Fixed48NoW:
				Seq.RotationCodec = &AEFVariableKeyLerp_Fixed48NoW;
				break;
			case ACF_IntervalFixed32NoW:
				Seq.RotationCodec = &AEFVariableKeyLerp_IntervalFixed32NoW;
				break;
			case ACF_Fixed32NoW:
				Seq.RotationCodec = &AEFVariableKeyLerp_Fixed32NoW;
				break;
			case ACF_Float32NoW:
				Seq.RotationCodec = &AEFVariableKeyLerp_Float32NoW;
				break;
			case ACF_Identity:
				Seq.RotationCodec = &AEFVariableKeyLerp_Identity;
				break;

			default:
				appErrorf( TEXT("%i: unknown or unsupported rotation compression"), (INT)Seq.RotationCompressionFormat );
		};
	}
	else if (Seq.KeyEncodingFormat == AKF_PerTrackCompression)
	{
		static AEFPerTrackCompressionCodec StaticCodec;

		Seq.RotationCodec = &StaticCodec;
		Seq.TranslationCodec = &StaticCodec;

		checkf(Seq.RotationCompressionFormat == ACF_Identity);
		checkf(Seq.TranslationCompressionFormat == ACF_Identity);
	}
	else
	{
		appErrorf( TEXT("%i: unknown or unsupported animation format"), (INT)Seq.KeyEncodingFormat );
	}
}
