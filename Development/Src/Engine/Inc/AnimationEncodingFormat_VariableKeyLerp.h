/*=============================================================================
	AnimationEncodingFormat_VariableKeyLerp.h: Variable key compression.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/ 


#ifndef __ANIMATIONENCODINGFORMAT_VARIABLEKEYLERP_H__
#define __ANIMATIONENCODINGFORMAT_VARIABLEKEYLERP_H__

#include "AnimationEncodingFormat.h"
#include "AnimationEncodingFormat_ConstantKeyLerp.h"



/**
 * Base class for all Animation Encoding Formats using variably-spaced key interpolation.
 */
class AEFVariableKeyLerpShared : public AEFConstantKeyLerpShared
{
public:
	/**
	 * Handles the ByteSwap of compressed rotation data on import
	 *
	 * @param	Seq				The UAnimSequence container.
	 * @param	MemoryReader	The FMemoryReader to read from.
	 * @param	RotTrackData	The compressed rotation data stream.
	 * @param	NumKeysRot		The number of keys present in the stream.
	 * @param	SourceArVersion	The version number of the source archive stream.
	 */
	void ByteSwapRotationIn(
		UAnimSequence& Seq, 
		FMemoryReader& MemoryReader,
		BYTE*& RotTrackData,
		INT NumKeysRot,
		INT SourceArVersion);

	/**
	 * Handles the ByteSwap of compressed translation data on import
	 *
	 * @param	Seq				The UAnimSequence container.
	 * @param	MemoryReader	The FMemoryReader to read from.
	 * @param	TransTrackData	The compressed translation data stream.
	 * @param	NumKeysTrans	The number of keys present in the stream.
	 * @param	SourceArVersion	The version number of the source archive stream.
	 */
	void ByteSwapTranslationIn(
		UAnimSequence& Seq, 
		FMemoryReader& MemoryReader,
		BYTE*& TransTrackData,
		INT NumKeysTrans,
		INT SourceArVersion);

	/**
	 * Handles the ByteSwap of compressed rotation data on export
	 *
	 * @param	Seq				The UAnimSequence container.
	 * @param	MemoryWriter	The FMemoryWriter to write to.
	 * @param	RotTrackData	The compressed rotation data stream.
	 * @param	NumKeysRot		The number of keys to write to the stream.
	 */
	void ByteSwapRotationOut(
		UAnimSequence& Seq, 
		FMemoryWriter& MemoryWriter,
		BYTE*& RotTrackData,
		INT NumKeysRot);

	/**
	 * Handles the ByteSwap of compressed translation data on export
	 *
	 * @param	Seq				The UAnimSequence container.
	 * @param	MemoryWriter	The FMemoryWriter to write to.
	 * @param	TransTrackData	The compressed translation data stream.
	 * @param	NumKeysTrans	The number of keys to write to the stream.
	 */
	void ByteSwapTranslationOut(
		UAnimSequence& Seq, 
		FMemoryWriter& MemoryWriter,
		BYTE*& TransTrackData,
		INT NumKeysTrans);
};

template<INT FORMAT>
class AEFVariableKeyLerp : public AEFVariableKeyLerpShared
{
public:
	/**
	 * Decompress the Rotation component of a BoneAtom
	 *
	 * @param	OutAtom			The FBoneAtom to fill in.
	 * @param	Stream			The compressed animation data.
	 * @param	NumKeys			The number of keys present in Stream.
	 * @param	Time			Current time to solve for.
	 * @param	RelativePos		Current position within the animation to solve for in the range [0.0,1.0].
	 * @param	bLooping		True when looping the stream in intended.
	 * @return					None. 
	 */
	void GetBoneAtomRotation(	
		FBoneAtom& OutAtom,
		const UAnimSequence& Seq,
		const BYTE* RESTRICT Stream,
		INT NumKeys,
		FLOAT Time,
		FLOAT RelativePos,
		UBOOL bLooping);

	/**
	 * Decompress the Translation component of a BoneAtom
	 *
	 * @param	OutAtom			The FBoneAtom to fill in.
	 * @param	Stream			The compressed animation data.
	 * @param	NumKeys			The number of keys present in Stream.
	 * @param	Time			Current time to solve for.
	 * @param	RelativePos		Current position within the animation to solve for in the range [0.0,1.0].
	 * @param	bLooping		True when looping the stream in intended.
	 * @return					None. 
	 */
	void GetBoneAtomTranslation(	
		FBoneAtom& OutAtom,
		const UAnimSequence& Seq,
		const BYTE* RESTRICT Stream,
		INT NumKeys,
		FLOAT Time,
		FLOAT RelativePos,
		UBOOL bLooping);

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
	void GetPoseRotations(	
		FBoneAtomArray& Atoms, 
		const BoneTrackArray& DesiredPairs,
		const UAnimSequence& Seq,
		FLOAT RelativePos,
		UBOOL bLooping);

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
	void GetPoseTranslations(	
		FBoneAtomArray& Atoms,
		const BoneTrackArray& DesiredPairs,
		const UAnimSequence& Seq,
		FLOAT RelativePos,
		UBOOL bLooping);
#endif

};


/**
 * Decompress the Rotation component of a BoneAtom
 *
 * @param	OutAtom			The FBoneAtom to fill in.
 * @param	Stream			The compressed animation data.
 * @param	NumKeys			The number of keys present in Stream.
 * @param	Time			Current time to solve for.
 * @param	RelativePos		Current position within the animation to solve for in the range [0.0,1.0].
 * @param	bLooping		True when looping the stream in intended.
 * @return					None. 
 */
template<INT FORMAT>
FORCEINLINE_DEBUGGABLE void AEFVariableKeyLerp<FORMAT>::GetBoneAtomRotation(	
	FBoneAtom& OutAtom,
	const UAnimSequence& Seq,
	const BYTE* RESTRICT RotStream,
	INT NumRotKeys,
	FLOAT Time,
	FLOAT RelativePos,
	UBOOL bLooping)
{
	if (NumRotKeys == 1)
	{
		// For a rotation track of n=1 keys, the single key is packed as an FQuatFloat96NoW.
		FQuat R0;
		DecompressRotation<ACF_Float96NoW>( R0 , RotStream, RotStream );
		OutAtom.SetRotation(R0);
	}
	else
	{
		const INT RotationStreamOffset = (FORMAT == ACF_IntervalFixed32NoW) ? (sizeof(FLOAT)*6) : 0; // offset past Min and Range data
		const BYTE* RESTRICT FrameTable= RotStream + RotationStreamOffset +(NumRotKeys*CompressedRotationStrides[FORMAT]*CompressedRotationNum[FORMAT]);
		FrameTable = Align(FrameTable, 4);

		INT Index0;
		INT Index1;
		FLOAT Alpha = TimeToIndex(Seq,FrameTable,RelativePos,bLooping,NumRotKeys,Index0,Index1);


		if (Index0 != Index1)
		{
			// unpack and lerp between the two nearest keys
			const BYTE* RESTRICT KeyData0= RotStream + RotationStreamOffset +(Index0*CompressedRotationStrides[FORMAT]*CompressedRotationNum[FORMAT]);
			const BYTE* RESTRICT KeyData1= RotStream + RotationStreamOffset +(Index1*CompressedRotationStrides[FORMAT]*CompressedRotationNum[FORMAT]);
			FQuat R0;
			FQuat R1;
			DecompressRotation<FORMAT>( R0, RotStream, KeyData0 );
			DecompressRotation<FORMAT>( R1, RotStream, KeyData1 );

			// Fast linear quaternion interpolation.
			// To ensure the 'shortest route', we make sure the dot product between the two keys is positive.
			const FLOAT DotResult = (R0 | R1);
			const FLOAT Bias = appFloatSelect(DotResult, 1.0f, -1.0f);
			FQuat RLerped((R0 * (1.f-Alpha)) + (R1 * (Alpha * Bias)));
			RLerped.Normalize();
			OutAtom.SetRotation(RLerped);
		}
		else // (Index0 == Index1)
		{
			// unpack a single key
			const BYTE* RESTRICT KeyData= RotStream + RotationStreamOffset +(Index0*CompressedRotationStrides[FORMAT]*CompressedRotationNum[FORMAT]);

			FQuat R0;
			DecompressRotation<FORMAT>( R0, RotStream, KeyData );

			OutAtom.SetRotation(R0);
		}
	}

}

/**
 * Decompress the Translation component of a BoneAtom
 *
 * @param	OutAtom			The FBoneAtom to fill in.
 * @param	Stream			The compressed animation data.
 * @param	NumKeys			The number of keys present in Stream.
 * @param	Time			Current time to solve for.
 * @param	RelativePos		Current position within the animation to solve for in the range [0.0,1.0].
 * @param	bLooping		True when looping the stream in intended.
 * @return					None. 
 */
template<INT FORMAT>
FORCEINLINE_DEBUGGABLE void AEFVariableKeyLerp<FORMAT>::GetBoneAtomTranslation(	
	FBoneAtom& OutAtom,
	const UAnimSequence& Seq,
	const BYTE* RESTRICT TransStream,
	INT NumTransKeys,
	FLOAT Time,
	FLOAT RelativePos,
	UBOOL bLooping)
{
	const BYTE* RESTRICT FrameTable= TransStream +(NumTransKeys*CompressedTranslationStrides[FORMAT]*CompressedTranslationNum[FORMAT]);
	FrameTable= Align(FrameTable, 4);

	INT Index0;
	INT Index1;
	FLOAT Alpha = TimeToIndex(Seq,FrameTable,RelativePos,bLooping,NumTransKeys,Index0,Index1);
	const INT TransStreamOffset = ((FORMAT == ACF_IntervalFixed32NoW) && NumTransKeys > 1) ? (sizeof(FLOAT)*6) : 0; // offset past Min and Range data

	if (Index0 != Index1)
	{
		FVector P0;
		FVector P1;
		const BYTE* RESTRICT KeyData0 = TransStream + TransStreamOffset + Index0*CompressedTranslationStrides[FORMAT]*CompressedTranslationNum[FORMAT];
		const BYTE* RESTRICT KeyData1 = TransStream + TransStreamOffset + Index1*CompressedTranslationStrides[FORMAT]*CompressedTranslationNum[FORMAT];
		DecompressTranslation<FORMAT>( P0, TransStream, KeyData0 );
		DecompressTranslation<FORMAT>( P1, TransStream, KeyData1 );
		OutAtom.SetTranslation( Lerp( P0, P1, Alpha ) );
	}
	else // (Index0 == Index1)
	{
		// unpack a single key
		FVector P0;
		const BYTE* RESTRICT KeyData = TransStream + TransStreamOffset + Index0*CompressedTranslationStrides[FORMAT]*CompressedTranslationNum[FORMAT];
		DecompressTranslation<FORMAT>( P0, TransStream, KeyData);
		OutAtom.SetTranslation( P0 );
	}
}

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
template<INT FORMAT>
FORCEINLINE_DEBUGGABLE void AEFVariableKeyLerp<FORMAT>::GetPoseRotations(	
	FBoneAtomArray& Atoms, 
	const BoneTrackArray& DesiredPairs,
	const UAnimSequence& Seq,
	FLOAT Time,
	UBOOL bLooping)
{
	const INT PairCount = DesiredPairs.Num();
	const FLOAT RelativePos = Time / (FLOAT)Seq.SequenceLength;

	for (INT PairIndex=0; PairIndex<PairCount; ++PairIndex)
	{
		const BoneTrackPair& Pair = DesiredPairs(PairIndex);
		const INT TrackIndex = Pair.TrackIndex;
		const INT AtomIndex = Pair.AtomIndex;
		FBoneAtom& BoneAtom = Atoms(AtomIndex);

		const INT* RESTRICT TrackData = Seq.CompressedTrackOffsets.GetTypedData() + (TrackIndex*4);
		const INT RotKeysOffset	= *(TrackData+2);
		const INT NumRotKeys	= *(TrackData+3);
		const BYTE* RESTRICT RotStream		= Seq.CompressedByteStream.GetTypedData()+RotKeysOffset;

		// call the decoder directly (not through the vtable)
		AEFVariableKeyLerp<FORMAT>::GetBoneAtomRotation(BoneAtom, Seq, RotStream, NumRotKeys, Time, RelativePos, bLooping);

		// Apply quaternion fix for ActorX-exported quaternions.
		BoneAtom.FlipSignOfRotationW();
	}
}

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
template<INT FORMAT>
FORCEINLINE_DEBUGGABLE void AEFVariableKeyLerp<FORMAT>::GetPoseTranslations(	
	FBoneAtomArray& Atoms, 
	const BoneTrackArray& DesiredPairs,
	const UAnimSequence& Seq,
	FLOAT Time,
	UBOOL bLooping)
{
	const INT PairCount= DesiredPairs.Num();
	const FLOAT RelativePos = Time / (FLOAT)Seq.SequenceLength;

	for (INT PairIndex=0; PairIndex<PairCount; ++PairIndex)
	{
		const BoneTrackPair& Pair = DesiredPairs(PairIndex);
		const INT TrackIndex = Pair.TrackIndex;
		const INT AtomIndex = Pair.AtomIndex;
		FBoneAtom& BoneAtom = Atoms(AtomIndex);

		const INT* RESTRICT TrackData = Seq.CompressedTrackOffsets.GetTypedData() + (TrackIndex*4);
		const INT TransKeysOffset	= *(TrackData+0);
		const INT NumTransKeys		= *(TrackData+1);
		const BYTE* RESTRICT TransStream = Seq.CompressedByteStream.GetTypedData()+TransKeysOffset;

		// call the decoder directly (not through the vtable)
		AEFVariableKeyLerp<FORMAT>::GetBoneAtomTranslation(BoneAtom, Seq, TransStream, NumTransKeys, Time, RelativePos, bLooping);
	}
}
#endif

#endif // __ANIMATIONENCODINGFORMAT_VARIABLEKEYLERP_H__
