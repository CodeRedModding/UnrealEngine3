/*=============================================================================
	AnimationCompressionAlgorithm_LeastDestructive.cpp: Uses the Bitwise compressor with really light settings
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/ 

#include "EnginePrivate.h"
#include "EngineAnimClasses.h"
#include "AnimationUtils.h"


IMPLEMENT_CLASS(UAnimationCompressionAlgorithm_LeastDestructive);

/**
 * Uses the Bitwise compressor, with really light settings, so it acts pretty close to "no compression at all"
 */
void UAnimationCompressionAlgorithm_LeastDestructive::DoReduction(UAnimSequence* AnimSeq, USkeletalMesh* SkelMesh, const TArray<FBoneData>& BoneData)
{
	UAnimationCompressionAlgorithm* BitwiseCompressor = ConstructObject<UAnimationCompressionAlgorithm_BitwiseCompressOnly>( UAnimationCompressionAlgorithm_BitwiseCompressOnly::StaticClass() );
	BitwiseCompressor->RotationCompressionFormat = ACF_Float96NoW;
	BitwiseCompressor->TranslationCompressionFormat = ACF_None;
	BitwiseCompressor->Reduce(AnimSeq, SkelMesh, FALSE);
}
