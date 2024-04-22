/*=============================================================================
	AnimationCompressionAlgorithm_SeparateToTracks.cpp: Reverts any animation compression, restoring the animation to the raw data.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/ 

#include "EnginePrivate.h"
#include "EngineAnimClasses.h"
#include "AnimationUtils.h"

IMPLEMENT_CLASS(UDEPRECATED_AnimationCompressionAlgorithm_RevertToRaw);

/**
 * Reverts any animation compression, restoring the animation to the raw data.
 */
void UDEPRECATED_AnimationCompressionAlgorithm_RevertToRaw::DoReduction(UAnimSequence* AnimSeq, USkeletalMesh* SkelMesh, const TArray<FBoneData>& BoneData)
{
	/*
	AnimSeq->TranslationData.Empty();
	AnimSeq->RotationData.Empty();
	AnimSeq->CompressedTrackOffsets.Empty();
	AnimSeq->CompressedByteStream.Empty();

	// Separate to raw data to tracks so that the bitwise compressor will have a data to operate on.
	// AnimSeq->SeparateRawDataToTracks( AnimSeq->RawAnimData, AnimSeq->SequenceLength, AnimSeq->TranslationData, AnimSeq->RotationData );

	// record the proper runtime decompressor to use
	AnimSeq->KeyEncodingFormat = AKF_ConstantKeyLerp;
	AnimationFormat_SetInterfaceLinks(*AnimSeq);
	AnimSeq->CompressionScheme = static_cast<UAnimationCompressionAlgorithm*>( StaticDuplicateObject( this, this, AnimSeq, TEXT("None"), ~RF_RootSet ) );
	*/

	// Disable "Revert To RAW" to prevent uncompressed animation data.
	// We instead compress using the Bitwise compressor, with really light settings, so it acts pretty close to "no compression at all."
	UAnimationCompressionAlgorithm* BitwiseCompressor = ConstructObject<UAnimationCompressionAlgorithm_BitwiseCompressOnly>( UAnimationCompressionAlgorithm_BitwiseCompressOnly::StaticClass() );
	BitwiseCompressor->RotationCompressionFormat = ACF_Float96NoW;
	BitwiseCompressor->TranslationCompressionFormat = ACF_None;
	BitwiseCompressor->Reduce(AnimSeq, SkelMesh, FALSE);
}
