/*=============================================================================
	AnimationCompressionAlgorithm_BitwiseCompressionOnly.cpp: Bitwise animation compression only; performs no key reduction.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/ 

#include "EnginePrivate.h"
#include "EngineAnimClasses.h"
#include "AnimationUtils.h"
#include "AnimationEncodingFormat.h"
#include "AnimationCompression.h"

IMPLEMENT_CLASS(UAnimationCompressionAlgorithm_BitwiseCompressOnly);

/**
 * Bitwise animation compression only; performs no key reduction.
 */
void UAnimationCompressionAlgorithm_BitwiseCompressOnly::DoReduction(UAnimSequence* AnimSeq, USkeletalMesh* SkelMesh, const TArray<FBoneData>& BoneData)
{
#if WITH_EDITORONLY_DATA
	// split the raw data into tracks
	TArray<FTranslationTrack> TranslationData;
	TArray<FRotationTrack> RotationData;
	SeparateRawDataIntoTracks( AnimSeq->RawAnimationData, AnimSeq->SequenceLength, TranslationData, RotationData );

	// Remove Translation Keys from tracks marked bAnimRotationOnly
	FilterAnimRotationOnlyKeys(TranslationData, AnimSeq, SkelMesh);

	// remove obviously redundant keys from the source data
	FilterTrivialKeys(TranslationData, RotationData, TRANSLATION_ZEROING_THRESHOLD, QUATERNION_ZEROING_THRESHOLD);

	// bitwise compress the tracks into the anim sequence buffers
	BitwiseCompressAnimationTracks(
		AnimSeq,
		static_cast<AnimationCompressionFormat>(TranslationCompressionFormat),
		static_cast<AnimationCompressionFormat>(RotationCompressionFormat),
		TranslationData,
		RotationData);

	// record the proper runtime decompressor to use
	AnimSeq->KeyEncodingFormat = AKF_ConstantKeyLerp;
	AnimationFormat_SetInterfaceLinks(*AnimSeq);
	AnimSeq->CompressionScheme = static_cast<UAnimationCompressionAlgorithm*>( StaticDuplicateObject( this, this, AnimSeq, TEXT("None"), ~RF_RootSet ) );
#endif // WITH_EDITORONLY_DATA
}
