/*=============================================================================
	AnimationCompressionAlgorithm_Automatic.cpp: Try various compression methods
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/ 

#include "EnginePrivate.h"
#include "EngineAnimClasses.h"
#include "AnimationUtils.h"

IMPLEMENT_CLASS(UAnimationCompressionAlgorithm_Automatic);

/**
 * Try compressing using other compressors.
 */
void UAnimationCompressionAlgorithm_Automatic::DoReduction(UAnimSequence* AnimSeq, USkeletalMesh* SkelMesh, const TArray<FBoneData>& BoneData)
{
#if WITH_EDITORONLY_DATA
	FAnimationUtils::CompressAnimSequenceExplicit(
		AnimSeq,
		SkelMesh,
		MaxEndEffectorError,
		FALSE, // bOutput
		bRunCurrentDefaultCompressor,
		bAutoReplaceIfExistingErrorTooGreat,
		bRaiseMaxErrorToExisting,
		bTryFixedBitwiseCompression,
		bTryPerTrackBitwiseCompression,
		bTryLinearKeyRemovalCompression,
		bTryIntervalKeyRemoval);
	AnimSeq->CompressionScheme = static_cast<UAnimationCompressionAlgorithm*>( StaticDuplicateObject( AnimSeq->CompressionScheme, AnimSeq->CompressionScheme, AnimSeq, TEXT("None"), ~RF_RootSet ) );
#endif // WITH_EDITORONLY_DATA
}
