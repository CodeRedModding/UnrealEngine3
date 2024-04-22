/*=============================================================================
	AnimationEncodingFormat_PerTrackCompression.h: Per-track decompressor.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/ 


#ifndef __ANIMATIONENCODINGFORMAT_PERTRACKCOMPRESSION_H__
#define __ANIMATIONENCODINGFORMAT_PERTRACKCOMPRESSION_H__

#include "AnimationEncodingFormat.h"

/**
 * Decompression codec for the per-track compressor.
 */
class AEFPerTrackCompressionCodec : public AnimationEncodingFormat
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
	virtual void GetPoseTranslations(	
		FBoneAtomArray& Atoms, 
		const BoneTrackArray& DesiredPairs,
		const UAnimSequence& Seq,
		FLOAT Time,
		UBOOL bLooping);
#endif

protected:
	/**
	 * Handles Byte-swapping a single track of animation data from a MemoryReader or to a MemoryWriter
	 *
	 * @param	Seq					The Animation Sequence being operated on.
	 * @param	MemoryStream		The MemoryReader or MemoryWriter object to read from/write to.
	 * @param	Offset				The starting offset into the compressed byte stream for this track (can be INDEX_NONE to indicate an identity track)
	 */
	static void ByteSwapOneTrack(UAnimSequence& Seq, FMemoryArchive& MemoryStream, INT Offset);

	/**
	 * Preserves 4 byte alignment within a stream
	 *
	 * @param	TrackData [inout]	The current data offset (will be returned four byte aligned from the start of the compressed byte stream)
	 * @param	MemoryStream		The MemoryReader or MemoryWriter object to read from/write to.
	 */
	static void PreservePadding(BYTE*& TrackData, FMemoryArchive& MemoryStream);

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
	static void GetBoneAtomRotation(	
		FBoneAtom& OutAtom,
		const UAnimSequence& Seq,
		INT Offset,
		FLOAT Time,
		FLOAT RelativePos,
		UBOOL bLooping);

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
	static void GetBoneAtomTranslation(	
		FBoneAtom& OutAtom,
		const UAnimSequence& Seq,
		INT Offset,
		FLOAT Time,
		FLOAT RelativePos,
		UBOOL bLooping);
};










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
FORCEINLINE_DEBUGGABLE void AEFPerTrackCompressionCodec::GetPoseRotations(
	FBoneAtomArray& Atoms, 
	const BoneTrackArray& DesiredPairs,
	const UAnimSequence& Seq,
	FLOAT Time,
	UBOOL bLooping)
{
	const INT PairCount = DesiredPairs.Num();
	const FLOAT RelativePos = Time / Seq.SequenceLength;

	for (INT PairIndex = 0; PairIndex < PairCount; ++PairIndex)
	{
		const BoneTrackPair& Pair = DesiredPairs(PairIndex);
		const INT TrackIndex = Pair.TrackIndex;
		const INT AtomIndex = Pair.AtomIndex;
		FBoneAtom& BoneAtom = Atoms(AtomIndex);

		const INT* RESTRICT TrackData = Seq.CompressedTrackOffsets.GetTypedData() + (TrackIndex*2);
		const INT RotKeysOffset	= *(TrackData+1);

		GetBoneAtomRotation(BoneAtom, Seq, RotKeysOffset, Time, RelativePos, bLooping);

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
FORCEINLINE_DEBUGGABLE void AEFPerTrackCompressionCodec::GetPoseTranslations(	
	FBoneAtomArray& Atoms, 
	const BoneTrackArray& DesiredPairs,
	const UAnimSequence& Seq,
	FLOAT Time,
	UBOOL bLooping)
{
	const INT PairCount = DesiredPairs.Num();
	const FLOAT RelativePos = Time / Seq.SequenceLength;

	for (INT PairIndex = 0; PairIndex < PairCount; ++PairIndex)
	{
		const BoneTrackPair& Pair = DesiredPairs(PairIndex);
		const INT TrackIndex = Pair.TrackIndex;
		const INT AtomIndex = Pair.AtomIndex;
		FBoneAtom& BoneAtom = Atoms(AtomIndex);

		const INT* RESTRICT TrackData = Seq.CompressedTrackOffsets.GetTypedData() + (TrackIndex*2);
		const INT PosKeysOffset	= *(TrackData+0);

		GetBoneAtomTranslation(BoneAtom, Seq, PosKeysOffset, Time, RelativePos, bLooping);
	}
}

#endif // USE_ANIMATION_CODEC_BATCH_SOLVER


#endif // __ANIMATIONENCODINGFORMAT_PERTRACKCOMPRESSION_H__
