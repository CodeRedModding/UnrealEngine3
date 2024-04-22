/*=============================================================================
	AnimationUtils.cpp: Skeletal mesh animation utilities.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/ 

#include "EnginePrivate.h"
#include "EngineAnimClasses.h"
#include "AnimationUtils.h"
#include "AnimationCompression.h"
#include "AnimationEncodingFormat.h"

/** Array to keep track of SkeletalMeshes we have built metadata for, and log out the results just once. */
static TArray<USkeletalMesh*> UniqueSkelMeshesMetadataArray;

void FAnimationUtils::BuildSkeletonMetaData(USkeletalMesh* SkelMesh, TArray<FBoneData>& OutBoneData)
{
	const TArray<FMeshBone>& Skeleton = SkelMesh->RefSkeleton;

	// Disable logging by default. Except if we deal with a new SkelMesh. Then we log out its details. (just once).
	UBOOL bEnableLogging = FALSE;
// Uncomment to enable.
// 	if( UniqueSkelMeshesMetadataArray.FindItemIndex(SkelMesh) == INDEX_NONE )
// 	{
// 		bEnableLogging = TRUE;
// 		UniqueSkelMeshesMetadataArray.AddItem(SkelMesh);
// 	}

	// Assemble bone data.
	OutBoneData.Empty();
	OutBoneData.AddZeroed( Skeleton.Num() );

	TArray<FString> KeyEndEffectorsMatchNameArray;
	GConfig->GetArray( TEXT("AnimationCompression"), TEXT("KeyEndEffectorsMatchName"), KeyEndEffectorsMatchNameArray, GEngineIni );

	for ( INT BoneIndex = 0 ; BoneIndex < Skeleton.Num() ; ++BoneIndex )
	{
		FBoneData& BoneData = OutBoneData(BoneIndex);

		// Copy over data from the skeleton.
		const FMeshBone& SrcBone = Skeleton(BoneIndex);
		BoneData.Orientation = SrcBone.BonePos.Orientation;
		BoneData.Position = SrcBone.BonePos.Position;
		BoneData.Name = SrcBone.Name;

		if ( BoneIndex > 0 )
		{
			// Compute ancestry.
			INT ParentIndex = Skeleton(BoneIndex).ParentIndex;
			BoneData.BonesToRoot.AddItem( ParentIndex );
			while ( ParentIndex > 0 )
			{
				ParentIndex = Skeleton(ParentIndex).ParentIndex;
				BoneData.BonesToRoot.AddItem( ParentIndex );
			}
		}

		// See if a Socket is attached to that bone
		BoneData.bHasSocket = FALSE;
		for(INT SocketIndex=0; SocketIndex<SkelMesh->Sockets.Num(); SocketIndex++)
		{
			USkeletalMeshSocket* Socket = SkelMesh->Sockets(SocketIndex);
			if( Socket && Socket->BoneName == SrcBone.Name )
			{
				BoneData.bHasSocket = TRUE;
				break;
			}
		}
	}

	// Enumerate children (bones that refer to this bone as parent).
	for ( INT BoneIndex = 0 ; BoneIndex < OutBoneData.Num() ; ++BoneIndex )
	{
		FBoneData& BoneData = OutBoneData(BoneIndex);
		// Exclude the root bone as it is the child of nothing.
		for ( INT BoneIndex2 = 1 ; BoneIndex2 < OutBoneData.Num() ; ++BoneIndex2 )
		{
			if ( OutBoneData(BoneIndex2).GetParent() == BoneIndex )
			{
				BoneData.Children.AddItem(BoneIndex2);
			}
		}
	}

	// Enumerate end effectors.  For each end effector, propagate its index up to all ancestors.
	if( bEnableLogging )
	{
		warnf(TEXT("Enumerate End Effectors for %s"), *SkelMesh->GetFName().ToString());
	}
	for ( INT BoneIndex = 0 ; BoneIndex < OutBoneData.Num() ; ++BoneIndex )
	{
		FBoneData& BoneData = OutBoneData(BoneIndex);
		if ( BoneData.IsEndEffector() )
		{
			// End effectors have themselves as an ancestor.
			BoneData.EndEffectors.AddItem( BoneIndex );
			// Add the end effector to the list of end effectors of all ancestors.
			for ( INT i = 0 ; i < BoneData.BonesToRoot.Num() ; ++i )
			{
				const INT AncestorIndex = BoneData.BonesToRoot(i);
				OutBoneData(AncestorIndex).EndEffectors.AddItem( BoneIndex );
			}

			for(INT MatchIndex=0; MatchIndex<KeyEndEffectorsMatchNameArray.Num(); MatchIndex++)
			{
				// See if this bone has been defined as a 'key' end effector
				FString BoneString(BoneData.Name.ToString());
				if( BoneString.InStr(KeyEndEffectorsMatchNameArray(MatchIndex), FALSE, TRUE) != INDEX_NONE )
				{
					BoneData.bKeyEndEffector = TRUE;
					break;
				}
			}
			if( bEnableLogging )
			{
				warnf(TEXT("\t %s bKeyEndEffector: %d"), *BoneData.Name.ToString(), BoneData.bKeyEndEffector);
			}
		}
	}
#if 0
	debugf( TEXT("====END EFFECTORS:") );
	INT NumEndEffectors = 0;
	for ( INT BoneIndex = 0 ; BoneIndex < OutBoneData.Num() ; ++BoneIndex )
	{
		const FBoneData& BoneData = OutBoneData(BoneIndex);
		if ( BoneData.IsEndEffector() )
		{
			FString Message( FString::Printf(TEXT("%s(%i): "), *BoneData.Name, BoneData.GetDepth()) );
			for ( INT i = 0 ; i < BoneData.BonesToRoot.Num() ; ++i )
			{
				const INT AncestorIndex = BoneData.BonesToRoot(i);
				Message += FString::Printf( TEXT("%s "), *OutBoneData(AncestorIndex).Name );
			}
			debugf( *Message );
			++NumEndEffectors;
		}
	}
	debugf( TEXT("====NUM EFFECTORS %i(%i)"), NumEndEffectors, OutBoneData(0).Children.Num() );
	debugf( TEXT("====NON END EFFECTORS:") );
	for ( INT BoneIndex = 0 ; BoneIndex < OutBoneData.Num() ; ++BoneIndex )
	{
		const FBoneData& BoneData = OutBoneData(BoneIndex);
		if ( !BoneData.IsEndEffector() )
		{
			FString Message( FString::Printf(TEXT("%s(%i): "), *BoneData.Name, BoneData.GetDepth()) );
			Message += TEXT("Children: ");
			for ( INT i = 0 ; i < BoneData.Children.Num() ; ++i )
			{
				const INT ChildIndex = BoneData.Children(i);
				Message += FString::Printf( TEXT("%s "), *OutBoneData(ChildIndex).Name );
			}
			Message += TEXT("  EndEffectors: ");
			for ( INT i = 0 ; i < BoneData.EndEffectors.Num() ; ++i )
			{
				const INT EndEffectorIndex = BoneData.EndEffectors(i);
				Message += FString::Printf( TEXT("%s "), *OutBoneData(EndEffectorIndex).Name );
				check( OutBoneData(EndEffectorIndex).IsEndEffector() );
			}
			debugf( *Message );
		}
	}
	debugf( TEXT("===================") );
#endif
}

/**
* Builds the local-to-component transformation for all bones.
*/

void FAnimationUtils::BuildComponentSpaceTransforms(TArray<FBoneAtom>& OutTransforms,
												const TArray<FBoneAtom>& LocalAtoms,
												const TArray<BYTE>& RequiredBones,
												const TArray<FMeshBone>& RefSkel)
{
	OutTransforms.Empty();
	OutTransforms.Add( RefSkel.Num() );

	for ( INT i = 0 ; i < RequiredBones.Num() ; ++i )
	{
		const INT BoneIndex = RequiredBones(i);
		OutTransforms(BoneIndex) =  LocalAtoms(BoneIndex);

		// For all bones below the root, final component-space transform is relative transform * component-space transform of parent.
		if ( BoneIndex > 0 )
		{
			const INT ParentIndex = RefSkel(BoneIndex).ParentIndex;

			// Check the precondition that parents occur before children in the RequiredBones array.
			const INT ReqBoneParentIndex = RequiredBones.FindItemIndex(ParentIndex);
			check( ReqBoneParentIndex != INDEX_NONE );
			check( ReqBoneParentIndex < i );

			OutTransforms(BoneIndex) *= OutTransforms(ParentIndex);
		}
	}
}

/**
* Builds the local-to-component matrix for the specified bone.
*/
void FAnimationUtils::BuildComponentSpaceTransform(FBoneAtom& OutTransform,
											   INT BoneIndex,
											   const TArray<FBoneAtom>& LocalAtoms,
											   const TArray<FBoneData>& BoneData)
{
	// Put root-to-component in OutTransform.
	OutTransform = LocalAtoms(0);

	if ( BoneIndex > 0 )
	{
		const FBoneData& Bone = BoneData(BoneIndex);

		checkSlow( Bone.BonesToRoot.Num()-1 == 0 );

		// Compose BoneData.BonesToRoot down.
		for ( INT i = Bone.BonesToRoot.Num()-2 ; i >=0 ; --i )
		{
			const INT AncestorIndex = Bone.BonesToRoot(i);
			OutTransform = LocalAtoms(AncestorIndex)*OutTransform;
		}

		// Finally, include the bone's local-to-parent.
		OutTransform = LocalAtoms(BoneIndex)*OutTransform;
	}
}

/**
 * Utility function to measure the accuracy of a compressed animation. Each end-effector is checked for 
 * world-space movement as a result of compression.
 *
 * @param	AnimSet		The animset to calculate compression error for.
 * @param	SkelMesh	The skeletal mesh to use to check for world-space error (required)
 * @param	BoneData	BoneData array describing the hierarchy of the animated skeleton
 * @param	ErrorStats	Output structure containing the final compression error values
 * @return				None.
 */
void FAnimationUtils::ComputeCompressionError(const UAnimSequence* AnimSeq, USkeletalMesh* SkelMesh, const TArray<FBoneData>& BoneData, AnimationErrorStats& ErrorStats)
{
	check(SkelMesh != NULL);
	check(SkelMesh->SkelMeshRUID > 0);
	ErrorStats.AverageError = 0.0f;
	ErrorStats.MaxError = 0.0f;
	ErrorStats.MaxErrorBone = 0;
	ErrorStats.MaxErrorTime = 0.0f;
	INT MaxErrorTrack = -1;

	if (AnimSeq->NumFrames > 0)
	{
		const FLOAT TimeStep = (FLOAT)AnimSeq->SequenceLength/(FLOAT)AnimSeq->NumFrames;
		const INT NumBones = BoneData.Num();
		
		FLOAT ErrorCount = 0.0f;
		FLOAT ErrorTotal = 0.0f;

		UAnimSet* AnimSet						= AnimSeq->GetAnimSet();
		const INT AnimLinkupIndex				= AnimSet->GetMeshLinkupIndex( SkelMesh );
		check( AnimLinkupIndex != INDEX_NONE );
		check( AnimLinkupIndex < AnimSet->LinkupCache.Num() );

		const FAnimSetMeshLinkup& AnimLinkup	= AnimSet->LinkupCache( AnimLinkupIndex );
		const TArray<FMeshBone>& RefSkel		= SkelMesh->RefSkeleton;
		check( AnimLinkup.BoneToTrackTable.Num() == RefSkel.Num() );

		TArray<FBoneAtom> RawAtoms;
		TArray<FBoneAtom> NewAtoms;
		TArray<FBoneAtom> RawTransforms;
		TArray<FBoneAtom> NewTransforms;

		RawAtoms.AddZeroed(NumBones);
		NewAtoms.AddZeroed(NumBones);
		RawTransforms.AddZeroed(NumBones);
		NewTransforms.AddZeroed(NumBones);

		FBoneAtom const DummyBone(FQuat::Identity, FVector(END_EFFECTOR_SOCKET_DUMMY_BONE_SIZE, END_EFFECTOR_SOCKET_DUMMY_BONE_SIZE, END_EFFECTOR_SOCKET_DUMMY_BONE_SIZE));

		// for each whole increment of time (frame stepping)
		for( FLOAT Time = 0.0f; Time < AnimSeq->SequenceLength; Time+= TimeStep )
		{
			// get the raw and compressed atom for each bone
			for( INT BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex )
			{
				INT const TrackIndex = AnimLinkup.BoneToTrackTable(BoneIndex);

				if( TrackIndex == INDEX_NONE )
				{
					// No track for the bone was found, so use the reference pose.
					RawAtoms(BoneIndex)	= FBoneAtom( RefSkel(BoneIndex).BonePos.Orientation, RefSkel(BoneIndex).BonePos.Position, 1.f );
					NewAtoms(BoneIndex) = RawAtoms(BoneIndex);
				}
				else
				{
					AnimSeq->GetBoneAtom(RawAtoms(BoneIndex), TrackIndex, Time, FALSE, TRUE);
					AnimSeq->GetBoneAtom(NewAtoms(BoneIndex), TrackIndex, Time, FALSE, FALSE);

					// Apply quaternion fix for ActorX-exported quaternions.
					if( BoneIndex > 0 ) 
					{
						RawAtoms(BoneIndex).FlipSignOfRotationW();
						NewAtoms(BoneIndex).FlipSignOfRotationW();
					}

					// apply the reference bone atom (if needed)
					if( AnimSeq->bIsAdditive )
					{
						FBoneAtom RefBoneAtom;
						AnimSeq->GetAdditiveBasePoseBoneAtom(RefBoneAtom, TrackIndex, Time, FALSE);
						
						// Apply quaternion fix for ActorX-exported quaternions.
						if( BoneIndex > 0 )
						{
							RefBoneAtom.FlipSignOfRotationW();
						}

						// apply additive amount to the reference pose
						RawAtoms(BoneIndex).AddToTranslation(RefBoneAtom.GetTranslationV());
						NewAtoms(BoneIndex).AddToTranslation(RefBoneAtom.GetTranslationV());

						// Add ref pose relative animation to base animation
						RawAtoms(BoneIndex).ConcatenateRotation(RefBoneAtom.GetRotationV());
						NewAtoms(BoneIndex).ConcatenateRotation(RefBoneAtom.GetRotationV());
					}

					UBOOL bSkipTranslationTrack = FALSE;

					// If we don't care about this translation track, because it's going to get skipped, then use RefSkel translation for error measurement.
#if( SKIP_FORCEMESHTRANSLATION_TRACKS )		
					bSkipTranslationTrack = AnimSet->ForceUseMeshTranslation(TrackIndex);
#endif

#if( SKIP_ANIMROTATIONONLY_TRACKS )
					bSkipTranslationTrack = bSkipTranslationTrack || (BoneIndex > 0 && AnimSet->bAnimRotationOnly && !AnimSet->BoneUseAnimTranslation(TrackIndex));
#endif

					// If we forcibly reduced the translation track to one key, make sure we don't introduce error if it was animated previously.
					// So short-circuit RAW data for error measuring past that first key.
					UBOOL bReducedTranslationTrack = FALSE;
#if( REDUCE_ANIMROTATIONONLY_TRACKS )
					bReducedTranslationTrack = (BoneIndex > 0 && AnimSet->bAnimRotationOnly && !AnimSet->BoneUseAnimTranslation(TrackIndex));
#endif

					// bAnimRotationOnly tracks - ignore translation data always use Ref Skeleton.
					if( bSkipTranslationTrack || (bReducedTranslationTrack && Time > 0.f) )
					{
						RawAtoms(BoneIndex).SetTranslation( RefSkel(BoneIndex).BonePos.Position );
						NewAtoms(BoneIndex).SetTranslation( RefSkel(BoneIndex).BonePos.Position );
					}
				}

				RawTransforms(BoneIndex) = RawAtoms(BoneIndex);
				NewTransforms(BoneIndex) = NewAtoms(BoneIndex);

				// For all bones below the root, final component-space transform is relative transform * component-space transform of parent.
				if( BoneIndex > 0 )
				{
					const INT ParentIndex = RefSkel(BoneIndex).ParentIndex;

					// Check the precondition that parents occur before children in the RequiredBones array.
					check( ParentIndex != INDEX_NONE );
					check( ParentIndex < BoneIndex );

					RawTransforms(BoneIndex) *= RawTransforms(ParentIndex);
					NewTransforms(BoneIndex) *= NewTransforms(ParentIndex);
				}
				
				if( BoneData(BoneIndex).IsEndEffector() )
				{
					// If this is an EndEffector with a Socket attached to it, add an extra bone, to measure error introduced by effector rotation compression.
					if( BoneData(BoneIndex).bHasSocket || BoneData(BoneIndex).bKeyEndEffector )
					{
						RawTransforms(BoneIndex) = DummyBone * RawTransforms(BoneIndex);
						NewTransforms(BoneIndex) = DummyBone * NewTransforms(BoneIndex);
					}

					FLOAT Error = (RawTransforms(BoneIndex).GetOrigin() - NewTransforms(BoneIndex).GetOrigin()).Size();

					ErrorTotal += Error;
					ErrorCount += 1.0f;

					if( Error > ErrorStats.MaxError )
					{
						ErrorStats.MaxError		= Error;
						ErrorStats.MaxErrorBone = BoneIndex;
						MaxErrorTrack = TrackIndex;
						ErrorStats.MaxErrorTime = Time;
					}
				}
			}
		}

		if (ErrorCount > 0.0f)
		{
			ErrorStats.AverageError = ErrorTotal / ErrorCount;
		}

#if 0		
		// That's a big error, log out some information!
 		if( ErrorStats.MaxError > 10.f )
 		{
			debugf(TEXT("!!! Big error found: %f, Time: %f, BoneIndex: %d, Track: %d, CompressionScheme: %s, additive: %d"), 
 				ErrorStats.MaxError,
				ErrorStats.MaxErrorTime,
				ErrorStats.MaxErrorBone,
				MaxErrorTrack,
				AnimSeq->CompressionScheme ? *AnimSeq->CompressionScheme->GetFName().ToString() : TEXT("NULL"),
				AnimSeq->bIsAdditive );
 			debugf(TEXT("   RawOrigin: %s, NormalOrigin: %s"), *RawTransforms(ErrorStats.MaxErrorBone).GetOrigin().ToString(), *NewTransforms(ErrorStats.MaxErrorBone).GetOrigin().ToString());
 
 			// We shouldn't have a big error with no compression.
 			check( AnimSeq->CompressionScheme != NULL );
 		}	
#endif
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Default animation compression algorithm.
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace {

/**
 * @return		A new instance of the default animation compression algorithm singleton, attached to the root set.
 */
static inline UAnimationCompressionAlgorithm* ConstructDefaultCompressionAlgorithm()
{
	// Algorithm.
	FString DefaultCompressionAlgorithm( TEXT("AnimationCompressionAlgorithm_BitwiseCompressOnly") );
	GConfig->GetString( TEXT("AnimationCompression"), TEXT("DefaultCompressionAlgorithm"), DefaultCompressionAlgorithm, GEngineIni );

	// Rotation compression format.
	AnimationCompressionFormat RotationCompressionFormat = ACF_Float96NoW;
	GConfig->GetInt( TEXT("AnimationCompression"), TEXT("RotationCompressionFormat"), (INT&)RotationCompressionFormat, GEngineIni );
	Clamp( RotationCompressionFormat, ACF_None, ACF_MAX );

	// Translation compression format.
	AnimationCompressionFormat TranslationCompressionFormat = ACF_None;
	GConfig->GetInt( TEXT("AnimationCompression"), TEXT("TranslationCompressionFormat"), (INT&)TranslationCompressionFormat, GEngineIni );
	Clamp( TranslationCompressionFormat, ACF_None, ACF_MAX );

	// Find a class that inherits
	UClass* CompressionAlgorithmClass = NULL;
	for ( TObjectIterator<UClass> It ; It ; ++It )
	{
		UClass* Class = *It;
		if( !(Class->ClassFlags & CLASS_Abstract) && !(Class->ClassFlags & CLASS_Deprecated) )
		{
			if ( Class->IsChildOf(UAnimationCompressionAlgorithm::StaticClass()) && DefaultCompressionAlgorithm == Class->GetName() )
			{
				CompressionAlgorithmClass = Class;
				break;

			}
		}
	}

	if ( !CompressionAlgorithmClass )
	{
		appErrorf( TEXT("Couldn't find animation compression algorithm named %s"), *DefaultCompressionAlgorithm );
	}

	UAnimationCompressionAlgorithm* NewAlgorithm = ConstructObject<UAnimationCompressionAlgorithm>( CompressionAlgorithmClass );
	NewAlgorithm->RotationCompressionFormat = RotationCompressionFormat;
	NewAlgorithm->TranslationCompressionFormat = TranslationCompressionFormat;
	NewAlgorithm->AddToRoot();
	return NewAlgorithm;
}

} // namespace

/**
 * @return		The default animation compression algorithm singleton, instantiating it if necessary.
 */
UAnimationCompressionAlgorithm* FAnimationUtils::GetDefaultAnimationCompressionAlgorithm()
{
	static UAnimationCompressionAlgorithm* SAlgorithm = ConstructDefaultCompressionAlgorithm();
	return SAlgorithm;
}

/**
 * Determines the current setting for world-space error tolerance in the animation compressor.
 * When requested, animation being compressed will also consider an alternative compression
 * method if the end result of that method produces less error than the AlternativeCompressionThreshold.
 * The default tolerance value is 0.0f (no alternatives allowed) but may be overridden using a field in the base engine INI file.
 *
 * @return				World-space error tolerance for considering an alternative compression method
 */
FLOAT FAnimationUtils::GetAlternativeCompressionThreshold()
{
	// Allow the Engine INI file to provide a new override
	FLOAT AlternativeCompressionThreshold = 0.0f;
	GConfig->GetFloat( TEXT("AnimationCompression"), TEXT("AlternativeCompressionThreshold"), (FLOAT&)AlternativeCompressionThreshold, GEngineIni );

	return AlternativeCompressionThreshold;
}

/**
 * Determines the current setting for recompressing all animations upon load. The default value 
 * is False, but may be overridden by an optional field in the base engine INI file. 
 *
 * @return				TRUE if the engine settings request that all animations be recompiled
 */
UBOOL FAnimationUtils::GetForcedRecompressionSetting()
{
	// Allow the Engine INI file to provide a new override
	UBOOL ForcedRecompressionSetting = FALSE;
	GConfig->GetBool( TEXT("AnimationCompression"), TEXT("ForceRecompression"), (UBOOL&)ForcedRecompressionSetting, GEngineIni );

	return ForcedRecompressionSetting;
}

#if CONSOLE
	#define TRYCOMPRESSION_INNER(compressionname,winningcompressor_count,winningcompressor_error,winningcompressor_margin,compressionalgorithm)	
#else
	#define TRYCOMPRESSION_INNER(compressionname,winningcompressor_count,winningcompressor_error,winningcompressor_margin,compressionalgorithm)				\
{																																							\
	/* try the alternative compressor	*/																													\
	(##compressionalgorithm)->Reduce( AnimSeq, DefaultSkeletalMesh, bOutput );																				\
	INT NewSize = AnimSeq->GetResourceSize();																												\
																																							\
	/* compute the savings and compression error*/																											\
	INT MemorySavingsFromOriginal = OriginalSize - NewSize;																									\
	INT MemorySavingsFromPrevious = CurrentSize - NewSize;																									\
	PctSaving = 0.f;																																		\
	/* figure out our new compression error*/																												\
	FAnimationUtils::ComputeCompressionError(AnimSeq, DefaultSkeletalMesh, BoneData, NewErrorStats);														\
																																							\
	const UBOOL bLowersError = NewErrorStats.MaxError < WinningCompressorError;																				\
	const UBOOL bErrorUnderThreshold = NewErrorStats.MaxError <= MasterTolerance;																			\
																																							\
	/* keep it if it we want to force the error below the threshold and it reduces error */																	\
	bKeepNewCompressionMethod = FALSE;																												\
	bKeepNewCompressionMethod |= (bLowersError && (WinningCompressorError > MasterTolerance) && bForceBelowThreshold);										\
	/* or if has an acceptable error and saves space  */																									\
	bKeepNewCompressionMethod |= bErrorUnderThreshold && (MemorySavingsFromPrevious > 0);																	\
	/* or if saves the same amount and an acceptable error that is lower than the previous best */															\
	bKeepNewCompressionMethod |= bErrorUnderThreshold && bLowersError && (MemorySavingsFromPrevious >= 0);													\
																																							\
	if (bKeepNewCompressionMethod)																															\
	{																																						\
		WinningCompressorMarginalSavings = MemorySavingsFromPrevious;																						\
		WinningCompressorCounter = &(##winningcompressor_count);																							\
		WinningCompressorErrorSum = &(##winningcompressor_error);																							\
		WinningCompressorMarginalSavingsSum = &(##winningcompressor_margin);																				\
		WinningCompressorName = ##compressionname;																											\
		CurrentSize = NewSize;																																\
		WinningCompressorSavings = MemorySavingsFromOriginal;																								\
		WinningCompressorError = NewErrorStats.MaxError;																									\
	}																																						\
																																							\
	PctSaving = OriginalSize > 0 ? 100.f - (100.f * FLOAT(NewSize) / FLOAT(OriginalSize)) : 0.f;															\
	warnf(TEXT("- %s - bytes saved: %i (%3.1f%% saved), maxdiff: %f %s"),																					\
##compressionname, MemorySavingsFromOriginal, PctSaving, NewErrorStats.MaxError, bKeepNewCompressionMethod ? TEXT("(**Best so far**)") : TEXT(""));		\
																																							\
	if( !bKeepNewCompressionMethod )																														\
	{																																						\
		/* revert back to the old method by copying back the data we cached */																				\
		AnimSeq->TranslationData = SavedTranslationData;																									\
		AnimSeq->RotationData = SavedRotationData;																											\
		AnimSeq->CompressionScheme = SavedCompressionScheme;																								\
		AnimSeq->TranslationCompressionFormat = SavedTranslationCompressionFormat;																			\
		AnimSeq->RotationCompressionFormat = SavedRotationCompressionFormat;																				\
		AnimSeq->KeyEncodingFormat = SavedKeyEncodingFormat;																								\
		AnimSeq->CompressedTrackOffsets = SavedCompressedTrackOffsets;																						\
		AnimSeq->CompressedByteStream = SavedCompressedByteStream;																							\
		AnimSeq->TranslationCodec = SavedTranslationCodec;																									\
		AnimSeq->RotationCodec = SavedRotationCodec;																										\
		AnimationFormat_SetInterfaceLinks(*AnimSeq);																										\
																																							\
		INT RestoredSize = AnimSeq->GetResourceSize();																										\
		check(RestoredSize == CurrentSize);																													\
	}																																						\
	else																																					\
	{																																						\
		/* backup key information from the sequence */																										\
		SavedTranslationData				= AnimSeq->TranslationData;																						\
		SavedRotationData					= AnimSeq->RotationData;																						\
		SavedCompressionScheme				= AnimSeq->CompressionScheme;																					\
		SavedTranslationCompressionFormat	= AnimSeq->TranslationCompressionFormat;																		\
		SavedRotationCompressionFormat		= AnimSeq->RotationCompressionFormat;																			\
		SavedKeyEncodingFormat				= AnimSeq->KeyEncodingFormat;																					\
		SavedCompressedTrackOffsets			= AnimSeq->CompressedTrackOffsets;																				\
		SavedCompressedByteStream			= AnimSeq->CompressedByteStream;																				\
		SavedTranslationCodec				= AnimSeq->TranslationCodec;																					\
		SavedRotationCodec					= AnimSeq->RotationCodec;																						\
	}																																						\
}
#endif

#define TRYCOMPRESSION(Name, CompressionAlgorithm) TRYCOMPRESSION_INNER(TEXT(#Name), Name ## CompressorWins, Name ## CompressorSumError, Name ## CompressorWinMargin, CompressionAlgorithm)

#define WARN_COMPRESSION_STATUS(Name) \
	warnf(TEXT("\t\tWins for '%32s': %4i\t\t%f\t%i bytes"), TEXT(#Name), Name ## CompressorWins, (Name ## CompressorWins > 0) ? Name ## CompressorSumError / Name ## CompressorWins : 0.0f, Name ## CompressorWinMargin)

#define DECLARE_ANIM_COMP_ALGORITHM(Algorithm) \
	static INT Algorithm ## CompressorWins = 0; \
	static FLOAT Algorithm ## CompressorSumError = 0.0f; \
	static INT Algorithm ## CompressorWinMargin = 0

/** Control animation recompression upon load. */
UBOOL GDisableAnimationRecompression	= FALSE;

/**
 * Utility function to compress an animation. If the animation is currently associated with a codec, it will be used to 
 * compress the animation. Otherwise, the default codec will be used. If AllowAlternateCompressor is TRUE, an
 * alternative compression codec will also be tested. If the alternative codec produces better compression and 
 * the accuracy of the compressed animation remains within tolerances, the alternative codec will be used. 
 * See GetAlternativeCompressionThreshold for information on the tolerance value used.
 *
 * @param	AnimSet		The animset to compress.
 * @param	SkelMesh	The skeletal mesh against which to compress the animation.  Not needed by all compression schemes.
 * @param	AllowAlternateCompressor	TRUE if an alternative compressor is permitted.
 * @param	bOutput		If FALSE don't generate output or compute memory savings.
 * @return				None.
 */
void FAnimationUtils::CompressAnimSequence(UAnimSequence* AnimSeq, USkeletalMesh* SkelMesh, UBOOL bAllowAlternateCompressor, UBOOL bOutput)
{
#if !CONSOLE
	// get the master tolerance we will use to guide recompression
	FLOAT MasterTolerance = GetAlternativeCompressionThreshold(); 

	UBOOL bOnlyCheckForMissingSkeletalMeshes = FALSE;
	GConfig->GetBool( TEXT("AnimationCompression"), TEXT("bOnlyCheckForMissingSkeletalMeshes"), (UBOOL&)bOnlyCheckForMissingSkeletalMeshes, GEngineIni );

	if (bOnlyCheckForMissingSkeletalMeshes)
	{
		TestForMissingMeshes(AnimSeq, SkelMesh);
	}
	else
	{
		UBOOL bForceBelowThreshold = FALSE;
		UBOOL bFirstRecompressUsingCurrentOrDefault = TRUE;
		UBOOL bRaiseMaxErrorToExisting = FALSE;
		GConfig->GetBool( TEXT("AnimationCompression"), TEXT("bForceBelowThreshold"), (UBOOL&)bForceBelowThreshold, GEngineIni );
		GConfig->GetBool( TEXT("AnimationCompression"), TEXT("bFirstRecompressUsingCurrentOrDefault"), (UBOOL&)bFirstRecompressUsingCurrentOrDefault, GEngineIni );
		// If we don't allow alternate compressors, and just want to recompress with default/existing, then make sure we do so.
		if( !bAllowAlternateCompressor )
		{
			bFirstRecompressUsingCurrentOrDefault = TRUE;
		}
		GConfig->GetBool( TEXT("AnimationCompression"), TEXT("bRaiseMaxErrorToExisting"), (UBOOL&)bRaiseMaxErrorToExisting, GEngineIni );

		UBOOL bTryFixedBitwiseCompression = TRUE;
		UBOOL bTryPerTrackBitwiseCompression = TRUE;
		UBOOL bTryLinearKeyRemovalCompression = TRUE;
		UBOOL bTryIntervalKeyRemoval = TRUE;
		GConfig->GetBool( TEXT("AnimationCompression"), TEXT("bTryFixedBitwiseCompression"), bTryFixedBitwiseCompression, GEngineIni );
		GConfig->GetBool( TEXT("AnimationCompression"), TEXT("bTryPerTrackBitwiseCompression"), bTryPerTrackBitwiseCompression, GEngineIni );
		GConfig->GetBool( TEXT("AnimationCompression"), TEXT("bTryLinearKeyRemovalCompression"), bTryLinearKeyRemovalCompression, GEngineIni );
		GConfig->GetBool( TEXT("AnimationCompression"), TEXT("bTryIntervalKeyRemoval"), bTryIntervalKeyRemoval, GEngineIni );

		CompressAnimSequenceExplicit(
			AnimSeq,
			SkelMesh,
			bAllowAlternateCompressor ? MasterTolerance : 0.0f,
			bOutput,
			bFirstRecompressUsingCurrentOrDefault,
			bForceBelowThreshold,
			bRaiseMaxErrorToExisting,
			bTryFixedBitwiseCompression,
			bTryPerTrackBitwiseCompression,
			bTryLinearKeyRemovalCompression,
			bTryIntervalKeyRemoval);
	}
#endif
}

/**
 * Utility function to compress an animation. If the animation is currently associated with a codec, it will be used to 
 * compress the animation. Otherwise, the default codec will be used. If AllowAlternateCompressor is TRUE, an
 * alternative compression codec will also be tested. If the alternative codec produces better compression and 
 * the accuracy of the compressed animation remains within tolerances, the alternative codec will be used. 
 * See GetAlternativeCompressionThreshold for information on the tolerance value used.
 *
 * @param	AnimSet		The animset to compress.
 * @param	SkelMesh	The skeletal mesh against which to compress the animation.  Not needed by all compression schemes.
 * @param	MasterTolerance	The alternate error threshold (0.0 means don't try anything other than the current / default scheme)
 * @param	bOutput		If FALSE don't generate output or compute memory savings.
 * @return				None.
 */
void FAnimationUtils::CompressAnimSequenceExplicit(
	UAnimSequence* AnimSeq,
	USkeletalMesh* SkelMesh,
	FLOAT MasterTolerance,
	UBOOL bOutput,
	UBOOL bFirstRecompressUsingCurrentOrDefault,
	UBOOL bForceBelowThreshold,
	UBOOL bRaiseMaxErrorToExisting,
	UBOOL bTryFixedBitwiseCompression,
	UBOOL bTryPerTrackBitwiseCompression,
	UBOOL bTryLinearKeyRemovalCompression,
	UBOOL bTryIntervalKeyRemoval)
{
#if WITH_EDITORONLY_DATA && !PLATFORM_MACOSX // @todo Mac
	if( GDisableAnimationRecompression )
	{
		return;
	}

	DECLARE_ANIM_COMP_ALGORITHM(BitwiseACF_Float96);
	DECLARE_ANIM_COMP_ALGORITHM(BitwiseACF_Fixed48);
	DECLARE_ANIM_COMP_ALGORITHM(BitwiseACF_IntervalFixed32);
	DECLARE_ANIM_COMP_ALGORITHM(BitwiseACF_Fixed32);

	DECLARE_ANIM_COMP_ALGORITHM(HalfOddACF_Float96);
	DECLARE_ANIM_COMP_ALGORITHM(HalfOddACF_Fixed48);
	DECLARE_ANIM_COMP_ALGORITHM(HalfOddACF_IntervalFixed32);
	DECLARE_ANIM_COMP_ALGORITHM(HalfOddACF_Fixed32);

	DECLARE_ANIM_COMP_ALGORITHM(HalfEvenACF_Float96);
	DECLARE_ANIM_COMP_ALGORITHM(HalfEvenACF_Fixed48);
	DECLARE_ANIM_COMP_ALGORITHM(HalfEvenACF_IntervalFixed32);
	DECLARE_ANIM_COMP_ALGORITHM(HalfEvenACF_Fixed32);

	DECLARE_ANIM_COMP_ALGORITHM(LinearACF_Float96);
	DECLARE_ANIM_COMP_ALGORITHM(LinearACF_Fixed48);
	DECLARE_ANIM_COMP_ALGORITHM(LinearACF_IntervalFixed32);
	DECLARE_ANIM_COMP_ALGORITHM(LinearACF_Fixed32);

	DECLARE_ANIM_COMP_ALGORITHM(Progressive_PerTrack);
	DECLARE_ANIM_COMP_ALGORITHM(Bitwise_PerTrack);
	DECLARE_ANIM_COMP_ALGORITHM(Linear_PerTrack);
	DECLARE_ANIM_COMP_ALGORITHM(Adaptive1_LinPerTrack);
	DECLARE_ANIM_COMP_ALGORITHM(Adaptive1_LinPerTrackNoRT);

	DECLARE_ANIM_COMP_ALGORITHM(Adaptive2_PerTrack);
	DECLARE_ANIM_COMP_ALGORITHM(Adaptive2_LinPerTrack);
	DECLARE_ANIM_COMP_ALGORITHM(Adaptive2_LinPerTrackNoRT);

	DECLARE_ANIM_COMP_ALGORITHM(Downsample20Hz_PerTrack);
	DECLARE_ANIM_COMP_ALGORITHM(Downsample15Hz_PerTrack);
	DECLARE_ANIM_COMP_ALGORITHM(Downsample10Hz_PerTrack);
	DECLARE_ANIM_COMP_ALGORITHM(Downsample5Hz_PerTrack);

	DECLARE_ANIM_COMP_ALGORITHM(Adaptive1_15Hz_LinPerTrack);
	DECLARE_ANIM_COMP_ALGORITHM(Adaptive1_10Hz_LinPerTrack);
	DECLARE_ANIM_COMP_ALGORITHM(Adaptive1_5Hz_LinPerTrack);

	DECLARE_ANIM_COMP_ALGORITHM(Adaptive2_15Hz_LinPerTrack);
	DECLARE_ANIM_COMP_ALGORITHM(Adaptive2_10Hz_LinPerTrack);

	DECLARE_ANIM_COMP_ALGORITHM(Linear_PerTrackExp1);
	DECLARE_ANIM_COMP_ALGORITHM(Linear_PerTrackExp2);

	check(AnimSeq != NULL);
	UAnimSet* AnimSet = AnimSeq->GetAnimSet();
	check(AnimSet != NULL);

	// attempt to find the default skeletal mesh associated with this sequence
	USkeletalMesh* DefaultSkeletalMesh = SkelMesh;
	if( !DefaultSkeletalMesh && AnimSet->PreviewSkelMeshName != NAME_None )
	{
		DefaultSkeletalMesh = LoadObject<USkeletalMesh>(NULL, *AnimSet->PreviewSkelMeshName.ToString(), NULL, LOAD_None, NULL);
	}

	// Make sure that animation can be played on that skeletal mesh
	if( DefaultSkeletalMesh && !AnimSet->CanPlayOnSkeletalMesh(DefaultSkeletalMesh) )
	{
		warnf(TEXT("%s cannot be played on %s (%s)."), *AnimSeq->SequenceName.ToString(), *DefaultSkeletalMesh->GetFName().ToString(), *AnimSet->GetFullName());
		DefaultSkeletalMesh = NULL;
	}

	UBOOL bValidSkeletalMesh = TRUE;
	if( !DefaultSkeletalMesh || DefaultSkeletalMesh->SkelMeshRUID == 0 )
	{
		// our SkeletalMesh is not valid, so unfortunately we can't use RemoveLinearKeys reliably. :(
		bValidSkeletalMesh = FALSE;
		// RemoveLinearKeys needs bone hierarchy, so we require a valid SkelMesh to recompress.
		// If we call this during PostLoad() SkelMesh might not be valid yet, so don't attempt to recompress.
		warnf(TEXT("FAnimationUtils::CompressAnimSequence %s (%s) SkelMesh not valid, won't be able to use RemoveLinearKeys."), *AnimSeq->SequenceName.ToString(), *AnimSet->GetFullName());
	}

	static INT TotalRecompressions = 0;

	static INT TotalNoWinnerRounds = 0;

	static INT AlternativeCompressorLossesFromSize = 0;
	static INT AlternativeCompressorLossesFromError = 0;
	static INT AlternativeCompressorSavings = 0;
	static INT64 TotalSizeBefore = 0;
	static INT64 TotalSizeNow = 0;
	static INT64 TotalUncompressed = 0;

	// we must have raw data to continue
	if( AnimSeq->RawAnimationData.Num() > 0 )
	{
		// See if we're trying alternate compressors
		UBOOL const bTryAlternateCompressor = MasterTolerance > 0.0f;

		// Get the current size
		INT OriginalSize = AnimSeq->GetResourceSize();
		TotalSizeBefore += OriginalSize;

		// Estimate total uncompressed
		TotalUncompressed += ((sizeof(FVector) + sizeof(FQuat)) * AnimSeq->RawAnimationData.Num() * AnimSeq->NumFrames);

		// Filter RAW data to get rid of mismatched tracks (translation/rotation data with a different number of keys than there are frames)
		// No trivial key removal is done at this point (impossible error metrics of -1), since all of the techniques will perform it themselves
		AnimSeq->CompressRawAnimData(-1.0f, -1.0f);

		// start with the current technique, or the default if none exists.
		// this will serve as our fallback if no better technique can be found
		INT OriginalKeyEncodingFormat = AnimSeq->KeyEncodingFormat;
		INT OriginalTranslationFormat = AnimSeq->TranslationCompressionFormat;
		INT OriginalRotationFormat = AnimSeq->RotationCompressionFormat;

		AnimationErrorStats OriginalErrorStats;
		AnimationErrorStats TrueOriginalErrorStats;
		TArray<FBoneData> BoneData;
		if (bValidSkeletalMesh)
		{
			// Build skeleton metadata to use during the key reduction.
			FAnimationUtils::BuildSkeletonMetaData( DefaultSkeletalMesh, BoneData );
			FAnimationUtils::ComputeCompressionError(AnimSeq, DefaultSkeletalMesh, BoneData, TrueOriginalErrorStats);
		}
		else
		{
			TrueOriginalErrorStats.MaxError = 0.0f;
		}

		INT AfterOriginalRecompression = 0;
		if( (bFirstRecompressUsingCurrentOrDefault && !bTryAlternateCompressor) || !bValidSkeletalMesh )
		{
			UAnimationCompressionAlgorithm* OriginalCompressionAlgorithm = AnimSeq->CompressionScheme ? AnimSeq->CompressionScheme : FAnimationUtils::GetDefaultAnimationCompressionAlgorithm();

			if( OriginalCompressionAlgorithm->IsA(UDEPRECATED_AnimationCompressionAlgorithm_RevertToRaw::StaticClass()) )
			{
				warnf(TEXT("FAnimationUtils::CompressAnimSequence %s (%s) Not allowed to revert to RAW. Using default compression scheme."), *AnimSeq->SequenceName.ToString(), *AnimSet->GetFullName());
				OriginalCompressionAlgorithm = FAnimationUtils::GetDefaultAnimationCompressionAlgorithm();
			}
			else if( OriginalCompressionAlgorithm->IsA(UAnimationCompressionAlgorithm_LeastDestructive::StaticClass()) )
			{
				warnf(TEXT("FAnimationUtils::CompressAnimSequence %s (%s) Not allowed to least destructive. Using default compression scheme."), *AnimSeq->SequenceName.ToString(), *AnimSet->GetFullName());
				OriginalCompressionAlgorithm = FAnimationUtils::GetDefaultAnimationCompressionAlgorithm();
			}

			// If we don't have a valid SkelMesh, and our current Scheme requires one, fall back to bitwise compression...
			if(!bValidSkeletalMesh && OriginalCompressionAlgorithm->bNeedsSkeleton)
			{
				if (bFirstRecompressUsingCurrentOrDefault)
				{
					warnf(TEXT("FAnimationUtils::CompressAnimSequence %s (%s) SkelMesh not valid, reverting default compression scheme to Bitwise compression!"), *AnimSeq->SequenceName.ToString(), *AnimSet->GetFullName());
				}
				OriginalCompressionAlgorithm = ConstructObject<UAnimationCompressionAlgorithm_BitwiseCompressOnly>( UAnimationCompressionAlgorithm_BitwiseCompressOnly::StaticClass() );
				OriginalCompressionAlgorithm->RotationCompressionFormat = ACF_Float96NoW;
				OriginalCompressionAlgorithm->TranslationCompressionFormat = ACF_None;
			}

			OriginalCompressionAlgorithm->Reduce( AnimSeq, DefaultSkeletalMesh, bOutput );
			AfterOriginalRecompression = AnimSeq->GetResourceSize();

			// figure out our current compression error
			if (bValidSkeletalMesh)
			{
				FAnimationUtils::ComputeCompressionError(AnimSeq, DefaultSkeletalMesh, BoneData, OriginalErrorStats);
			}
			else
			{
				OriginalErrorStats.MaxError = 1e6f; // Unknown error, no mesh to check against
			}
		}
		else
		{
			AfterOriginalRecompression = AnimSeq->GetResourceSize();
			OriginalErrorStats = TrueOriginalErrorStats;
		}
 
		// Check for global permission to try an alternative compressor
		if( !bValidSkeletalMesh )
		{
			if( !DefaultSkeletalMesh )
			{
				warnf(TEXT("%s %s couldn't be compressed! Default Mesh is NULL! PreviewSkelMeshName: %s"), *AnimSet->GetFullName(), *AnimSeq->SequenceName.ToString(), *AnimSet->PreviewSkelMeshName.ToString());
			}
			else
			{
				warnf(TEXT("%s %s couldn't be compressed! Default Mesh is %s, PreviewSkelMeshName: %s"), *AnimSet->GetFullName(), *DefaultSkeletalMesh->GetFullName(), *AnimSeq->SequenceName.ToString(), *AnimSet->PreviewSkelMeshName.ToString());
			}
		}
		else if( bTryAlternateCompressor && !AnimSeq->bDoNotOverrideCompression )
		{
			AnimationErrorStats NewErrorStats = OriginalErrorStats;
			if (bRaiseMaxErrorToExisting)
			{
				if (NewErrorStats.MaxError > MasterTolerance)
				{
					warnf(TEXT("  Boosting MasterTolerance to %f, as existing MaxDiff was higher than %f and bRaiseMaxErrorToExisting=TRUE"), NewErrorStats.MaxError, MasterTolerance);
					MasterTolerance = NewErrorStats.MaxError;
				}
			}

			{
				// backup key information from the sequence
				TArrayNoInit<struct FTranslationTrack> SavedTranslationData = AnimSeq->TranslationData;
				TArrayNoInit<struct FRotationTrack> SavedRotationData = AnimSeq->RotationData;
				class UAnimationCompressionAlgorithm* SavedCompressionScheme = AnimSeq->CompressionScheme;
				BYTE SavedTranslationCompressionFormat = AnimSeq->TranslationCompressionFormat;
				BYTE SavedRotationCompressionFormat = AnimSeq->RotationCompressionFormat;
				BYTE SavedKeyEncodingFormat = AnimSeq->KeyEncodingFormat;
				TArrayNoInit<INT> SavedCompressedTrackOffsets = AnimSeq->CompressedTrackOffsets;
				TArrayNoInit<BYTE> SavedCompressedByteStream = AnimSeq->CompressedByteStream;
				FPointer SavedTranslationCodec = AnimSeq->TranslationCodec;
				FPointer SavedRotationCodec = AnimSeq->RotationCodec;

				// count all attempts for debugging
				++TotalRecompressions;

				// Prepare to compress
				INT CurrentSize = AnimSeq->GetResourceSize();
				INT* WinningCompressorCounter = NULL;
				FLOAT* WinningCompressorErrorSum = NULL;
				INT* WinningCompressorMarginalSavingsSum = NULL;
				INT WinningCompressorMarginalSavings = 0;
				FString WinningCompressorName;
				INT WinningCompressorSavings = 0;
				FLOAT PctSaving = 0.f;
				FLOAT WinningCompressorError = OriginalErrorStats.MaxError;
				UBOOL bKeepNewCompressionMethod = FALSE;

				warnf(TEXT("Compressing %s (%s)\n\tSkelMesh: %s\n\tOriginal Size: %i   MaxDiff: %f"),
					*AnimSeq->SequenceName.ToString(),
					*AnimSet->GetFullName(),
					DefaultSkeletalMesh ? *DefaultSkeletalMesh->GetFName().ToString() : TEXT("NULL - Not all compression techniques can be used!"),
					OriginalSize,
					TrueOriginalErrorStats.MaxError);

				warnf(TEXT("Original Key Encoding: %s\n\tOriginal Rotation Format: %s\n\tOriginal Translation Format: %s\n\tNumFrames: %i\n\tSequenceLength: %f (%2.1f fps)"),
					*GetAnimationKeyFormatString(static_cast<AnimationKeyFormat>(OriginalKeyEncodingFormat)),
					*FAnimationUtils::GetAnimationCompressionFormatString(static_cast<AnimationCompressionFormat>(OriginalRotationFormat)),
					*FAnimationUtils::GetAnimationCompressionFormatString(static_cast<AnimationCompressionFormat>(OriginalTranslationFormat)),
					AnimSeq->NumFrames,
					AnimSeq->SequenceLength,
					(AnimSeq->NumFrames > 1) ? AnimSeq->NumFrames / AnimSeq->SequenceLength : 30.0f);

				if (bFirstRecompressUsingCurrentOrDefault)
				{
					warnf(TEXT("Recompressed using current/default\n\tRecompress Size: %i   MaxDiff: %f\n\tRecompress Scheme: %s"),
						AfterOriginalRecompression,
						OriginalErrorStats.MaxError,
						AnimSeq->CompressionScheme ? *AnimSeq->CompressionScheme->GetClass()->GetName() : TEXT("NULL"));
				}

				// Progressive Algorithm
				if( bTryPerTrackBitwiseCompression )
				{
					UAnimationCompressionAlgorithm_PerTrackCompression* PerTrackCompressor = ConstructObject<UAnimationCompressionAlgorithm_PerTrackCompression>(UAnimationCompressionAlgorithm_PerTrackCompression::StaticClass());

					// Start not too aggressive
					PerTrackCompressor->MaxPosDiffBitwise /= 10.f;
					PerTrackCompressor->MaxAngleDiffBitwise /= 10.f;
					PerTrackCompressor->bUseAdaptiveError2 = TRUE;

					// Try default compressor first
					TRYCOMPRESSION(Progressive_PerTrack, PerTrackCompressor);

					if( NewErrorStats.MaxError >= MasterTolerance )
					{
						warnf(TEXT("\tStandard bitwise compressor too aggressive, lower default settings."));
					}
					else
					{
						// First, start by finding most downsampling factor.
						if( bTryIntervalKeyRemoval && (AnimSeq->NumFrames >= PerTrackCompressor->MinKeysForResampling) )
						{
							PerTrackCompressor->bResampleAnimation = TRUE;
				
							// Try PerTrackCompression, down sample to 5 Hz
							PerTrackCompressor->ResampledFramerate = 5.0f;
							warnf(TEXT("\tResampledFramerate: %f"), PerTrackCompressor->ResampledFramerate);
							TRYCOMPRESSION(Progressive_PerTrack, PerTrackCompressor);

							// If too much error, try 6Hz
							if( NewErrorStats.MaxError >= MasterTolerance )
							{
								PerTrackCompressor->ResampledFramerate = 6.0f;
								warnf(TEXT("\tResampledFramerate: %f"), PerTrackCompressor->ResampledFramerate);
								TRYCOMPRESSION(Progressive_PerTrack, PerTrackCompressor);

								// if too much error go 10Hz, 15Hz, 20Hz.
								if( NewErrorStats.MaxError >= MasterTolerance )
								{
									PerTrackCompressor->ResampledFramerate = 5.0f;
									// Keep trying until we find something that works (or we just don't downsample)
									while( PerTrackCompressor->ResampledFramerate < 20.f && NewErrorStats.MaxError >= MasterTolerance )
									{
										PerTrackCompressor->ResampledFramerate += 5.f;
										warnf(TEXT("\tResampledFramerate: %f"), PerTrackCompressor->ResampledFramerate);
										TRYCOMPRESSION(Progressive_PerTrack, PerTrackCompressor);
									}
								}
							}
							
							// Give up downsampling if it didn't work.
							if( NewErrorStats.MaxError >= MasterTolerance )
							{
								warnf(TEXT("\tDownsampling didn't work."));
								PerTrackCompressor->bResampleAnimation = FALSE;
							}
						}
						
						// Now do Linear Key Removal
						if( bValidSkeletalMesh && (AnimSeq->NumFrames > 1) )
						{
							PerTrackCompressor->bActuallyFilterLinearKeys = TRUE;
							PerTrackCompressor->bRetarget = TRUE;
							
							INT const TestSteps = 16;
							FLOAT const MaxScale = 2^(TestSteps);

							// Start with the least aggressive first. if that one doesn't succeed, don't bother going through all the steps.
							PerTrackCompressor->MaxPosDiff /= MaxScale;
							PerTrackCompressor->MaxAngleDiff /= MaxScale;
							PerTrackCompressor->MaxEffectorDiff /= MaxScale;
							PerTrackCompressor->MinEffectorDiff /= MaxScale;
							PerTrackCompressor->EffectorDiffSocket /= MaxScale;
							warnf(TEXT("\tLinearKeys. MaxPosDiff: %f, MaxAngleDiff: %f"), PerTrackCompressor->MaxPosDiff, PerTrackCompressor->MaxAngleDiff);
							TRYCOMPRESSION(Progressive_PerTrack, PerTrackCompressor);
							PerTrackCompressor->MaxPosDiff *= MaxScale;
							PerTrackCompressor->MaxAngleDiff *= MaxScale;
							PerTrackCompressor->MaxEffectorDiff *= MaxScale;
							PerTrackCompressor->MinEffectorDiff *= MaxScale;
							PerTrackCompressor->EffectorDiffSocket *= MaxScale;

							if( NewErrorStats.MaxError < MasterTolerance )
							{
								// Start super aggressive, and go down until we find something that works.
								warnf(TEXT("\tLinearKeys. MaxPosDiff: %f, MaxAngleDiff: %f"), PerTrackCompressor->MaxPosDiff, PerTrackCompressor->MaxAngleDiff);
								TRYCOMPRESSION(Progressive_PerTrack, PerTrackCompressor);

								for(INT Step=0; Step<TestSteps && (NewErrorStats.MaxError >= MasterTolerance); Step++)
								{
									PerTrackCompressor->MaxPosDiff /= 2.f;
									PerTrackCompressor->MaxAngleDiff /= 2.f;
									PerTrackCompressor->MaxEffectorDiff /= 2.f;
									PerTrackCompressor->MinEffectorDiff /= 2.f;
									PerTrackCompressor->EffectorDiffSocket /= 2.f;
									warnf(TEXT("\tLinearKeys. MaxPosDiff: %f, MaxAngleDiff: %f"), PerTrackCompressor->MaxPosDiff, PerTrackCompressor->MaxAngleDiff);
									TRYCOMPRESSION(Progressive_PerTrack, PerTrackCompressor);
								}
							}

							// Give up Linear Key Compression if it didn't work
							if( NewErrorStats.MaxError >= MasterTolerance )
							{
								PerTrackCompressor->bActuallyFilterLinearKeys = FALSE;
								PerTrackCompressor->bRetarget = FALSE;
							}
						}

						// Finally tighten up bitwise compression
						PerTrackCompressor->MaxPosDiffBitwise *= 10.f;
						PerTrackCompressor->MaxAngleDiffBitwise *= 10.f;
						{
							INT const TestSteps = 16;
							FLOAT const MaxScale = 2^(TestSteps/2);

							PerTrackCompressor->MaxPosDiffBitwise *= MaxScale;
							PerTrackCompressor->MaxAngleDiffBitwise *= MaxScale;
							warnf(TEXT("\tBitwise. MaxPosDiffBitwise: %f, MaxAngleDiffBitwise: %f"), PerTrackCompressor->MaxPosDiffBitwise, PerTrackCompressor->MaxAngleDiffBitwise);
							TRYCOMPRESSION(Progressive_PerTrack, PerTrackCompressor);
							PerTrackCompressor->MaxPosDiffBitwise /= 2.f;
							PerTrackCompressor->MaxAngleDiffBitwise /= 2.f;
							for(INT Step=0; Step<TestSteps && (NewErrorStats.MaxError >= MasterTolerance) && (PerTrackCompressor->MaxPosDiffBitwise >= PerTrackCompressor->MaxZeroingThreshold); Step++)
							{
								warnf(TEXT("\tBitwise. MaxPosDiffBitwise: %f, MaxAngleDiffBitwise: %f"), PerTrackCompressor->MaxPosDiffBitwise, PerTrackCompressor->MaxAngleDiffBitwise);
								TRYCOMPRESSION(Progressive_PerTrack, PerTrackCompressor);
								PerTrackCompressor->MaxPosDiffBitwise /= 2.f;
								PerTrackCompressor->MaxAngleDiffBitwise /= 2.f;
							}
						}
					}
				}

				// Start with Bitwise Compress only
				if( bTryFixedBitwiseCompression )
				{
					UAnimationCompressionAlgorithm_BitwiseCompressOnly* BitwiseCompressor = ConstructObject<UAnimationCompressionAlgorithm_BitwiseCompressOnly>( UAnimationCompressionAlgorithm_BitwiseCompressOnly::StaticClass() );

					// Try ACF_Float96NoW
					BitwiseCompressor->RotationCompressionFormat = ACF_Float96NoW;
					BitwiseCompressor->TranslationCompressionFormat = ACF_None;
					TRYCOMPRESSION(BitwiseACF_Float96, BitwiseCompressor);

					// Try ACF_Fixed48NoW
					BitwiseCompressor->RotationCompressionFormat = ACF_Fixed48NoW;
					BitwiseCompressor->TranslationCompressionFormat = ACF_None;
					TRYCOMPRESSION(BitwiseACF_Fixed48,BitwiseCompressor);

// 32bits currently unusable due to creating too much error
// 					// Try ACF_IntervalFixed32NoW
// 					BitwiseCompressor->RotationCompressionFormat = ACF_IntervalFixed32NoW;
// 					BitwiseCompressor->TranslationCompressionFormat = ACF_None;
// 					TRYCOMPRESSION(BitwiseACF_IntervalFixed32,BitwiseCompressor);
// 
// 					// Try ACF_Fixed32NoW
// 					BitwiseCompressor->RotationCompressionFormat = ACF_Fixed32NoW;
// 					BitwiseCompressor->TranslationCompressionFormat = ACF_None;
// 					TRYCOMPRESSION(BitwiseACF_Fixed32,BitwiseCompressor);
				}

				// Start with Bitwise Compress only
				// this compressor has a minimum number of frames requirement. So no need to go there if we don't meet that...
				if( bTryFixedBitwiseCompression && bTryIntervalKeyRemoval )
				{
					UAnimationCompressionAlgorithm_RemoveEverySecondKey* RemoveEveryOtherKeyCompressor = ConstructObject<UAnimationCompressionAlgorithm_RemoveEverySecondKey>( UAnimationCompressionAlgorithm_RemoveEverySecondKey::StaticClass() );
					if( AnimSeq->NumFrames > RemoveEveryOtherKeyCompressor->MinKeys )
					{
						RemoveEveryOtherKeyCompressor->bStartAtSecondKey = FALSE;
						{
							// Try ACF_Float96NoW
							RemoveEveryOtherKeyCompressor->RotationCompressionFormat = ACF_Float96NoW;	
							RemoveEveryOtherKeyCompressor->TranslationCompressionFormat = ACF_None;		
							TRYCOMPRESSION(HalfOddACF_Float96, RemoveEveryOtherKeyCompressor);

							// Try ACF_Fixed48NoW
							RemoveEveryOtherKeyCompressor->RotationCompressionFormat = ACF_Fixed48NoW;	
							RemoveEveryOtherKeyCompressor->TranslationCompressionFormat = ACF_None;		
							TRYCOMPRESSION(HalfOddACF_Fixed48, RemoveEveryOtherKeyCompressor);

// 32bits currently unusable due to creating too much error
// 							// Try ACF_IntervalFixed32NoW
// 							RemoveEveryOtherKeyCompressor->RotationCompressionFormat = ACF_IntervalFixed32NoW;	
// 							RemoveEveryOtherKeyCompressor->TranslationCompressionFormat = ACF_None;		
// 							TRYCOMPRESSION(HalfOddACF_IntervalFixed32, RemoveEveryOtherKeyCompressor);
// 
// 							// Try ACF_Fixed32NoW
// 							RemoveEveryOtherKeyCompressor->RotationCompressionFormat = ACF_Fixed32NoW;	
// 							RemoveEveryOtherKeyCompressor->TranslationCompressionFormat = ACF_None;		
// 							TRYCOMPRESSION(HalfOddACF_Fixed32, RemoveEveryOtherKeyCompressor);
						}
						RemoveEveryOtherKeyCompressor->bStartAtSecondKey = TRUE;
						{
							// Try ACF_Float96NoW
							RemoveEveryOtherKeyCompressor->RotationCompressionFormat = ACF_Float96NoW;	
							RemoveEveryOtherKeyCompressor->TranslationCompressionFormat = ACF_None;		
							TRYCOMPRESSION(HalfEvenACF_Float96,RemoveEveryOtherKeyCompressor);

							// Try ACF_Fixed48NoW
							RemoveEveryOtherKeyCompressor->RotationCompressionFormat = ACF_Fixed48NoW;	
							RemoveEveryOtherKeyCompressor->TranslationCompressionFormat = ACF_None;		
							TRYCOMPRESSION(HalfEvenACF_Fixed48,RemoveEveryOtherKeyCompressor);

// 32bits currently unusable due to creating too much error
// 							// Try ACF_IntervalFixed32NoW
// 							RemoveEveryOtherKeyCompressor->RotationCompressionFormat = ACF_IntervalFixed32NoW;	
// 							RemoveEveryOtherKeyCompressor->TranslationCompressionFormat = ACF_None;		
// 							TRYCOMPRESSION(HalfEvenACF_IntervalFixed32,RemoveEveryOtherKeyCompressor);
// 
// 							// Try ACF_Fixed32NoW
// 							RemoveEveryOtherKeyCompressor->RotationCompressionFormat = ACF_Fixed32NoW;	
// 							RemoveEveryOtherKeyCompressor->TranslationCompressionFormat = ACF_None;		
// 							TRYCOMPRESSION(HalfEvenACF_Fixed32,RemoveEveryOtherKeyCompressor);
						}
					}
				}

				// construct the proposed compressor		
				if( bTryLinearKeyRemovalCompression && bValidSkeletalMesh && (AnimSeq->NumFrames > 1) )
				{
					UAnimationCompressionAlgorithm_RemoveLinearKeys* LinearKeyRemover = ConstructObject<UAnimationCompressionAlgorithm_RemoveLinearKeys>( UAnimationCompressionAlgorithm_RemoveLinearKeys::StaticClass() );
					{
						// Try ACF_Float96NoW
						LinearKeyRemover->RotationCompressionFormat = ACF_Float96NoW;
						LinearKeyRemover->TranslationCompressionFormat = ACF_None;	
						TRYCOMPRESSION(LinearACF_Float96,LinearKeyRemover);

						// Try ACF_Fixed48NoW
						LinearKeyRemover->RotationCompressionFormat = ACF_Fixed48NoW;
						LinearKeyRemover->TranslationCompressionFormat = ACF_None;	
						TRYCOMPRESSION(LinearACF_Fixed48,LinearKeyRemover);

// Error is too bad w/ 32bits
// 						// Try ACF_IntervalFixed32NoW
// 						LinearKeyRemover->RotationCompressionFormat = ACF_IntervalFixed32NoW;
// 						LinearKeyRemover->TranslationCompressionFormat = ACF_None;
// 						TRYCOMPRESSION(LinearACF_IntervalFixed32,LinearKeyRemover);
// 
// 						// Try ACF_Fixed32NoW
// 						LinearKeyRemover->RotationCompressionFormat = ACF_Fixed32NoW;
// 						LinearKeyRemover->TranslationCompressionFormat = ACF_None;
// 						TRYCOMPRESSION(LinearACF_Fixed32,LinearKeyRemover);
					}
				}

				if( bTryPerTrackBitwiseCompression )
				{
					UAnimationCompressionAlgorithm_PerTrackCompression* PerTrackCompressor = ConstructObject<UAnimationCompressionAlgorithm_PerTrackCompression>(UAnimationCompressionAlgorithm_PerTrackCompression::StaticClass());

					// Straight PerTrackCompression, no key decimation and no linear key removal
					TRYCOMPRESSION(Bitwise_PerTrack, PerTrackCompressor);
					PerTrackCompressor->bUseAdaptiveError = TRUE;

					// Full blown linear
					PerTrackCompressor->bActuallyFilterLinearKeys = TRUE;
					PerTrackCompressor->bRetarget = TRUE;
					TRYCOMPRESSION(Linear_PerTrack, PerTrackCompressor);

					// Adaptive retargetting based on height within the skeleton
					PerTrackCompressor->bActuallyFilterLinearKeys = TRUE;
					PerTrackCompressor->bRetarget = FALSE;
					PerTrackCompressor->ParentingDivisor = 2.0f;
					PerTrackCompressor->ParentingDivisorExponent = 1.6f;
					TRYCOMPRESSION(Adaptive1_LinPerTrackNoRT, PerTrackCompressor);
					PerTrackCompressor->ParentingDivisor = 1.0f;
					PerTrackCompressor->ParentingDivisorExponent = 1.0f;

					PerTrackCompressor->bActuallyFilterLinearKeys = TRUE;
					PerTrackCompressor->bRetarget = TRUE;
					PerTrackCompressor->ParentingDivisor = 2.0f;
					PerTrackCompressor->ParentingDivisorExponent = 1.6f;
					TRYCOMPRESSION(Adaptive1_LinPerTrack, PerTrackCompressor);
					PerTrackCompressor->ParentingDivisor = 1.0f;
					PerTrackCompressor->ParentingDivisorExponent = 1.0f;
				}


				if( bTryPerTrackBitwiseCompression )
				{
					UAnimationCompressionAlgorithm_PerTrackCompression* PerTrackCompressor = ConstructObject<UAnimationCompressionAlgorithm_PerTrackCompression>(UAnimationCompressionAlgorithm_PerTrackCompression::StaticClass());
					PerTrackCompressor->bUseAdaptiveError = TRUE;

					if (bValidSkeletalMesh && (AnimSeq->NumFrames > 1))
					{
						PerTrackCompressor->bActuallyFilterLinearKeys = TRUE;
						PerTrackCompressor->bRetarget = TRUE;

						PerTrackCompressor->MaxPosDiff = 0.1;
// 						PerTrackCompressor->MaxAngleDiff = 0.1;
						PerTrackCompressor->ParentingDivisor = 2.0f;
						PerTrackCompressor->ParentingDivisorExponent = 1.0f;
						TRYCOMPRESSION(Linear_PerTrackExp1, PerTrackCompressor);

						PerTrackCompressor->MaxPosDiff = 0.01;
// 						PerTrackCompressor->MaxAngleDiff = 0.025;
						PerTrackCompressor->ParentingDivisor = 2.0f;
						PerTrackCompressor->ParentingDivisorExponent = 1.0f;
						TRYCOMPRESSION(Linear_PerTrackExp2, PerTrackCompressor);

						PerTrackCompressor->bRetarget = FALSE;
						PerTrackCompressor->MaxPosDiff = 0.1;
// 						PerTrackCompressor->MaxAngleDiff = 0.025;
						PerTrackCompressor->ParentingDivisor = 1.0f;
						PerTrackCompressor->ParentingDivisorExponent = 1.0f;
					}
				}

				if( bTryPerTrackBitwiseCompression )
				{
					UAnimationCompressionAlgorithm_PerTrackCompression* PerTrackCompressor = ConstructObject<UAnimationCompressionAlgorithm_PerTrackCompression>(UAnimationCompressionAlgorithm_PerTrackCompression::StaticClass());
					PerTrackCompressor->bUseAdaptiveError = TRUE;

					// Try the decimation algorithms
					if (bTryIntervalKeyRemoval && (AnimSeq->NumFrames >= PerTrackCompressor->MinKeysForResampling))
					{
						PerTrackCompressor->bActuallyFilterLinearKeys = FALSE;
						PerTrackCompressor->bRetarget = FALSE;
						PerTrackCompressor->bUseAdaptiveError = FALSE;
						PerTrackCompressor->bResampleAnimation = TRUE;

						// Try PerTrackCompression, downsample to 20 Hz
						PerTrackCompressor->ResampledFramerate = 20.0f;
						TRYCOMPRESSION(Downsample20Hz_PerTrack, PerTrackCompressor);

						// Try PerTrackCompression, downsample to 15 Hz
						PerTrackCompressor->ResampledFramerate = 15.0f;
						TRYCOMPRESSION(Downsample15Hz_PerTrack, PerTrackCompressor);

						// Try PerTrackCompression, downsample to 10 Hz
						PerTrackCompressor->ResampledFramerate = 10.0f;
						TRYCOMPRESSION(Downsample10Hz_PerTrack, PerTrackCompressor);

						// Try PerTrackCompression, downsample to 5 Hz
						PerTrackCompressor->ResampledFramerate = 5.0f;
						TRYCOMPRESSION(Downsample5Hz_PerTrack, PerTrackCompressor);


						// Downsampling with linear key removal and adaptive error metrics
						PerTrackCompressor->bActuallyFilterLinearKeys = TRUE;
						PerTrackCompressor->bRetarget = FALSE;
						PerTrackCompressor->bUseAdaptiveError = TRUE;
						PerTrackCompressor->ParentingDivisor = 2.0f;
						PerTrackCompressor->ParentingDivisorExponent = 1.6f;

						PerTrackCompressor->ResampledFramerate = 15.0f;
						TRYCOMPRESSION(Adaptive1_15Hz_LinPerTrack, PerTrackCompressor);

						PerTrackCompressor->ResampledFramerate = 10.0f;
						TRYCOMPRESSION(Adaptive1_10Hz_LinPerTrack, PerTrackCompressor);

						PerTrackCompressor->ResampledFramerate = 5.0f;
						TRYCOMPRESSION(Adaptive1_5Hz_LinPerTrack, PerTrackCompressor);
					}
				}


				if( bTryPerTrackBitwiseCompression && bTryIntervalKeyRemoval)
				{
					// Try the decimation algorithms
					if (AnimSeq->NumFrames >= 3)
					{
						UAnimationCompressionAlgorithm_PerTrackCompression* NewPerTrackCompressor = ConstructObject<UAnimationCompressionAlgorithm_PerTrackCompression>(UAnimationCompressionAlgorithm_PerTrackCompression::StaticClass());

						// Downsampling with linear key removal and adaptive error metrics v2
						NewPerTrackCompressor->MinKeysForResampling = 3;
						NewPerTrackCompressor->bUseAdaptiveError2 = TRUE;
						NewPerTrackCompressor->MaxPosDiffBitwise = 0.05;
						NewPerTrackCompressor->MaxAngleDiffBitwise = 0.02;
						NewPerTrackCompressor->bActuallyFilterLinearKeys = TRUE;
						NewPerTrackCompressor->bRetarget = TRUE;

						NewPerTrackCompressor->ResampledFramerate = 15.0f;
						TRYCOMPRESSION(Adaptive2_15Hz_LinPerTrack, NewPerTrackCompressor);

						NewPerTrackCompressor->ResampledFramerate = 10.0f;
						TRYCOMPRESSION(Adaptive2_10Hz_LinPerTrack, NewPerTrackCompressor);
					}
				}


				if( bTryPerTrackBitwiseCompression)
				{
					// Adaptive error through probing the effect of perturbations at each track
					UAnimationCompressionAlgorithm_PerTrackCompression* NewPerTrackCompressor = ConstructObject<UAnimationCompressionAlgorithm_PerTrackCompression>(UAnimationCompressionAlgorithm_PerTrackCompression::StaticClass());
					NewPerTrackCompressor->bUseAdaptiveError2 = TRUE;
					NewPerTrackCompressor->MaxPosDiffBitwise = 0.05;
					NewPerTrackCompressor->MaxAngleDiffBitwise = 0.02;

					TRYCOMPRESSION(Adaptive2_PerTrack, NewPerTrackCompressor);

					NewPerTrackCompressor->bActuallyFilterLinearKeys = TRUE;
					NewPerTrackCompressor->bRetarget = TRUE;
					TRYCOMPRESSION(Adaptive2_LinPerTrack, NewPerTrackCompressor);

					NewPerTrackCompressor->bActuallyFilterLinearKeys = TRUE;
					NewPerTrackCompressor->bRetarget = FALSE;
					TRYCOMPRESSION(Adaptive2_LinPerTrackNoRT, NewPerTrackCompressor);
				}

				// Increase winning compressor.
				if( CurrentSize != OriginalSize )
				{
					INT SizeDecrease = OriginalSize - CurrentSize;
					if (WinningCompressorCounter)
					{
						++(*WinningCompressorCounter);
						(*WinningCompressorErrorSum) += WinningCompressorError;
						AlternativeCompressorSavings += WinningCompressorSavings;
						*WinningCompressorMarginalSavingsSum += WinningCompressorMarginalSavings;
						checkf(WinningCompressorSavings == SizeDecrease);

					warnf(TEXT("  Recompressing '%s' with compressor '%s' saved %i bytes (%i -> %i -> %i) (max diff=%f)\n"),
							*AnimSeq->SequenceName.ToString(),
							*WinningCompressorName,
							SizeDecrease,
							OriginalSize, AfterOriginalRecompression, CurrentSize,
							WinningCompressorError);
					}
					else
					{
						TotalNoWinnerRounds++;
						warnf(TEXT("  Recompressing '%s' with original/default compressor saved %i bytes (%i -> %i -> %i) (max diff=%f)\n"), 
							*AnimSeq->SequenceName.ToString(),
							SizeDecrease,
							OriginalSize, AfterOriginalRecompression, CurrentSize,
							WinningCompressorError);
					}


					// Update the memory stats
#if STATS
					if( GIsGame && !GIsEditor )
					{
						if (SizeDecrease > 0)
						{
							DEC_DWORD_STAT_BY( STAT_AnimationMemory, SizeDecrease );
						}
						else
						{
							INC_DWORD_STAT_BY( STAT_AnimationMemory, -SizeDecrease );
						}
					}
#endif
				}

				// Make sure we got that right.
				check(CurrentSize == AnimSeq->GetResourceSize());
				TotalSizeNow += CurrentSize;

				PctSaving = TotalSizeBefore > 0 ? 100.f - (100.f * FLOAT(TotalSizeNow) / FLOAT(TotalSizeBefore)) : 0.f;
				warnf(TEXT("Compression Stats Summary [%i total, %i Bytes saved, %i before, %i now, %3.1f%% savings. Uncompressed: %i TotalRatio: %i:1]"), 
				TotalRecompressions,
				AlternativeCompressorSavings,
				TotalSizeBefore, 
				TotalSizeNow,
				PctSaving,
				TotalUncompressed,
				(TotalUncompressed / TotalSizeNow));

				warnf(TEXT("\t\tDefault compressor wins:                      %i"), TotalNoWinnerRounds);

				if (bTryFixedBitwiseCompression)
				{
					WARN_COMPRESSION_STATUS(BitwiseACF_Float96);
					WARN_COMPRESSION_STATUS(BitwiseACF_Fixed48);
// 					WARN_COMPRESSION_STATUS(BitwiseACF_IntervalFixed32);
// 					WARN_COMPRESSION_STATUS(BitwiseACF_Fixed32);
				}

				if (bTryFixedBitwiseCompression && bTryIntervalKeyRemoval)
				{
					WARN_COMPRESSION_STATUS(HalfOddACF_Float96);
					WARN_COMPRESSION_STATUS(HalfOddACF_Fixed48);
// 					WARN_COMPRESSION_STATUS(HalfOddACF_IntervalFixed32);
// 					WARN_COMPRESSION_STATUS(HalfOddACF_Fixed32);

					WARN_COMPRESSION_STATUS(HalfEvenACF_Float96);
					WARN_COMPRESSION_STATUS(HalfEvenACF_Fixed48);
// 					WARN_COMPRESSION_STATUS(HalfEvenACF_IntervalFixed32);
// 					WARN_COMPRESSION_STATUS(HalfEvenACF_Fixed32);
				}

				if (bTryLinearKeyRemovalCompression)
				{
					WARN_COMPRESSION_STATUS(LinearACF_Float96);
					WARN_COMPRESSION_STATUS(LinearACF_Fixed48);
// 					WARN_COMPRESSION_STATUS(LinearACF_IntervalFixed32);
// 					WARN_COMPRESSION_STATUS(LinearACF_Fixed32);
				}

				if (bTryPerTrackBitwiseCompression)
				{
					WARN_COMPRESSION_STATUS(Progressive_PerTrack);
					WARN_COMPRESSION_STATUS(Bitwise_PerTrack);
					WARN_COMPRESSION_STATUS(Linear_PerTrack);
					WARN_COMPRESSION_STATUS(Adaptive1_LinPerTrackNoRT);
					WARN_COMPRESSION_STATUS(Adaptive1_LinPerTrack);

					WARN_COMPRESSION_STATUS(Linear_PerTrackExp1);
					WARN_COMPRESSION_STATUS(Linear_PerTrackExp2);
				}

				if (bTryPerTrackBitwiseCompression && bTryIntervalKeyRemoval)
				{
					WARN_COMPRESSION_STATUS(Downsample20Hz_PerTrack);
					WARN_COMPRESSION_STATUS(Downsample15Hz_PerTrack);
					WARN_COMPRESSION_STATUS(Downsample10Hz_PerTrack);
					WARN_COMPRESSION_STATUS(Downsample5Hz_PerTrack);

					WARN_COMPRESSION_STATUS(Adaptive1_15Hz_LinPerTrack);
					WARN_COMPRESSION_STATUS(Adaptive1_10Hz_LinPerTrack);
					WARN_COMPRESSION_STATUS(Adaptive1_5Hz_LinPerTrack);

					WARN_COMPRESSION_STATUS(Adaptive2_15Hz_LinPerTrack);
					WARN_COMPRESSION_STATUS(Adaptive2_10Hz_LinPerTrack);
				}

				if (bTryPerTrackBitwiseCompression)
				{
					WARN_COMPRESSION_STATUS(Adaptive2_PerTrack);
					WARN_COMPRESSION_STATUS(Adaptive2_LinPerTrack);
					WARN_COMPRESSION_STATUS(Adaptive2_LinPerTrackNoRT);
				}
			}
		}
		// Do not recompress - Still take into account size for stats.
		else
		{
			TotalSizeNow += AnimSeq->GetResourceSize();
		}
	}
	else
	{
		warnf(TEXT("Compression Requested for Empty Animation %s"), *AnimSeq->SequenceName.ToString() );
	}
#endif // WITH_EDITORONLY_DATA
}


void FAnimationUtils::TestForMissingMeshes(UAnimSequence* AnimSeq, USkeletalMesh* SkelMesh)
{
#if !CONSOLE
	check(AnimSeq != NULL);
	UAnimSet* AnimSet = AnimSeq->GetAnimSet();
	check(AnimSet != NULL);

	// attempt to find the default skeletal mesh associated with this sequence
	USkeletalMesh* DefaultSkeletalMesh = SkelMesh;
	if( !DefaultSkeletalMesh && AnimSet->PreviewSkelMeshName != NAME_None )
	{
		DefaultSkeletalMesh = LoadObject<USkeletalMesh>(NULL, *AnimSet->PreviewSkelMeshName.ToString(), NULL, LOAD_None, NULL);
	}

	// Make sure that animation can be played on that skeletal mesh
	if( DefaultSkeletalMesh && !AnimSet->CanPlayOnSkeletalMesh(DefaultSkeletalMesh) )
	{
		warnf(TEXT("%s cannot be played on %s (%s)."), *AnimSeq->SequenceName.ToString(), *DefaultSkeletalMesh->GetFName().ToString(), *AnimSet->GetFullName());
		DefaultSkeletalMesh = NULL;
	}

	UBOOL bValidSkeletalMesh = TRUE;
	if (!DefaultSkeletalMesh || (DefaultSkeletalMesh->SkelMeshRUID == 0))
	{
		bValidSkeletalMesh = FALSE;
	}


	static INT MissingSkelMeshCount = 0;
	static TArray<FString> MissingSkelMeshArray;

	// Check for global permission to try an alternative compressor
	if (!bValidSkeletalMesh)
	{
		++MissingSkelMeshCount;

		if (!DefaultSkeletalMesh)
		{
			warnf(TEXT("%s %s couldn't be compressed! Default Mesh is NULL! PreviewSkelMeshName: %s"), *AnimSet->GetFullName(), *AnimSeq->SequenceName.ToString(), *AnimSet->PreviewSkelMeshName.ToString());
		}
		else
		{
			warnf(TEXT("%s %s couldn't be compressed! Default Mesh is %s, PreviewSkelMeshName: %s"), *AnimSet->GetFullName(), *DefaultSkeletalMesh->GetFullName(), *AnimSeq->SequenceName.ToString(), *AnimSet->PreviewSkelMeshName.ToString());
		}

		MissingSkelMeshArray.AddUniqueItem(AnimSeq->GetOuter()->GetFullName());
		for(INT i=0; i<MissingSkelMeshArray.Num(); i++)
		{
			warnf(TEXT("[%i] %s"), i, *MissingSkelMeshArray(i));
		}
		warnf(TEXT("Missing Mesh Count: %i"), MissingSkelMeshCount);
	}
#endif
}

void FAnimationUtils::InternalSetAnimRebuildInfoForAdditiveAnim(UAnimSequence* AdditiveAnimSeq, TArray<AdditiveAnimRebuildInfo> &AdditiveAnimRebuildList)
{
#if WITH_EDITORONLY_DATA
	AdditiveAnimRebuildInfo RebuildInfo;
	RebuildInfo.AdditivePose = AdditiveAnimSeq;
	RebuildInfo.BasePose = NULL;
	RebuildInfo.TargetPose = NULL;
	RebuildInfo.SkelMesh = NULL;

	if( AdditiveAnimSeq->GetAnimSet()->PreviewSkelMeshName != NAME_None )
	{
		RebuildInfo.SkelMesh = LoadObject<USkeletalMesh>(NULL, *AdditiveAnimSeq->GetAnimSet()->PreviewSkelMeshName.ToString(), NULL, LOAD_None, NULL);
	}

	// Look up a base pose
	for(INT AnimSeqIdx=0; AnimSeqIdx<AdditiveAnimSeq->AdditiveBasePoseAnimSeq.Num(); AnimSeqIdx++)
	{
		UAnimSequence *BasePoseAnimSeq = AdditiveAnimSeq->AdditiveBasePoseAnimSeq(AnimSeqIdx);
		check(BasePoseAnimSeq);
		RebuildInfo.BasePose = BasePoseAnimSeq;
		break;
	}

	// Look up target pose.
	for(INT AnimSeqIdx=0; AnimSeqIdx<AdditiveAnimSeq->AdditiveTargetPoseAnimSeq.Num(); AnimSeqIdx++)
	{
		UAnimSequence *TargetPoseAnimSeq = AdditiveAnimSeq->AdditiveTargetPoseAnimSeq(AnimSeqIdx);
		check(TargetPoseAnimSeq);
		RebuildInfo.TargetPose = TargetPoseAnimSeq;
		break;
	}

	if( AdditiveAnimSeq->AdditiveRefName == FName(TEXT("Bind Pose")) )
	{
		RebuildInfo.BuildMethod = CTA_RefPose;
	}
	else
	{
		// Figure out how many frames we used from the base pose.
		INT BasePoseNumFrames = 0;
		for(INT TrackIdx=0; TrackIdx<AdditiveAnimSeq->AdditiveBasePose.Num() && BasePoseNumFrames<=1; TrackIdx++)
		{
			BasePoseNumFrames = Max<INT>(BasePoseNumFrames, Max<INT>(AdditiveAnimSeq->AdditiveBasePose(TrackIdx).PosKeys.Num(), AdditiveAnimSeq->AdditiveBasePose(TrackIdx).RotKeys.Num()));
		}

		RebuildInfo.BuildMethod = (BasePoseNumFrames > 1) ? CTA_AnimScaled : CTA_AnimFirstFrame;
	}

	// If we have a valid rebuild setup, then add the animation to be rebuilt.
	if( RebuildInfo.SkelMesh && RebuildInfo.TargetPose 
		&& (RebuildInfo.BasePose || RebuildInfo.BuildMethod == CTA_RefPose) )
	{
		AdditiveAnimRebuildList.AddItem( RebuildInfo );
	}
#endif // WITH_EDITORONLY_DATA
}

void FAnimationUtils::GetAdditiveAnimRebuildList(UAnimSequence *AnimSeq, TArray<AdditiveAnimRebuildInfo> &AdditiveAnimRebuildList)
{
#if WITH_EDITORONLY_DATA
	check( AnimSeq );

	// Make sure list is empty.
	AdditiveAnimRebuildList.Empty();
	
	// if it's a Target or Base Pose
	if( !AnimSeq->bIsAdditive )
	{
		for(INT AdditiveIndex=0; AdditiveIndex<AnimSeq->RelatedAdditiveAnimSeqs.Num(); AdditiveIndex++)
		{
			UAnimSequence *AdditiveAnimSeq = AnimSeq->RelatedAdditiveAnimSeqs(AdditiveIndex);
			check( AdditiveAnimSeq );
			check( AdditiveAnimSeq->bIsAdditive );

			FAnimationUtils::InternalSetAnimRebuildInfoForAdditiveAnim(AdditiveAnimSeq, AdditiveAnimRebuildList);
		}
	}
	// If we're already dealing with an Additive Animation.
	else
	{
		FAnimationUtils::InternalSetAnimRebuildInfoForAdditiveAnim(AnimSeq, AdditiveAnimRebuildList);
	}
#endif // WITH_EDITORONLY_DATA
}

// Turn on to enable logging
#define DEBUG_ADDITIVE_CREATION 0

void FAnimationUtils::RebuildAdditiveAnimations(TArray<AdditiveAnimRebuildInfo> &AdditiveAnimRebuildList)
{
#if WITH_EDITORONLY_DATA
	TArray<FName> BuildChoices;
	BuildChoices.Add(CTA_MAX);
	BuildChoices(CTA_RefPose) = FName(*LocalizeUnrealEd("AACT_ReferencePose"));
	BuildChoices(CTA_AnimFirstFrame) = FName(*LocalizeUnrealEd("AACT_AnimationFirstFrame"));
	BuildChoices(CTA_AnimScaled) = FName(*LocalizeUnrealEd("AACT_AnimationScaled"));

	for(INT i=0; i<AdditiveAnimRebuildList.Num(); i++)
	{
		AdditiveAnimRebuildInfo &RebuildInfo = AdditiveAnimRebuildList(i);
		debugf(TEXT("RebuildAdditiveAnimations Additive: %s, Build Method: %s"), *RebuildInfo.AdditivePose->SequenceName.ToString(), *BuildChoices(RebuildInfo.BuildMethod).ToString());
		ConvertAnimSeqToAdditive(RebuildInfo.TargetPose, RebuildInfo.AdditivePose, RebuildInfo.BasePose, RebuildInfo.SkelMesh, RebuildInfo.BuildMethod, RebuildInfo.AdditivePose->bAdditiveBuiltLooping, TRUE);
	}
#endif // WITH_EDITORONLY_DATA
}

static void GetBindPoseAtom(FBoneAtom &OutBoneAtom, INT BoneIndex, USkeletalMesh *SkelMesh)
{
	const FMeshBone& RefSkelBone = SkelMesh->RefSkeleton(BoneIndex);
	OutBoneAtom.SetComponents(
		RefSkelBone.BonePos.Orientation,
		RefSkelBone.BonePos.Position);
// #if DEBUG_ADDITIVE_CREATION
// 	debugf(TEXT("GetBindPoseAtom BoneIndex: %d, OutBoneAtom: %s"), BoneIndex, *OutBoneAtom.ToString());
// #endif
}

static void GetRawAnimTrackKey(FBoneAtom &OutBoneAtom, INT BoneIndex, INT KeyIdx, UAnimSequence *AnimSeq, FAnimSetMeshLinkup *AnimLinkup, USkeletalMesh *SkelMesh)
{
	const INT SrcTrackIndex = AnimLinkup->BoneToTrackTable(BoneIndex);
	if( SrcTrackIndex != INDEX_NONE )
	{
		FRawAnimSequenceTrack& SourceRawTrack = AnimSeq->RawAnimationData(SrcTrackIndex);
		OutBoneAtom.SetComponents(
			SourceRawTrack.RotKeys( KeyIdx < SourceRawTrack.RotKeys.Num() ? KeyIdx : 0 ),
			SourceRawTrack.PosKeys( KeyIdx < SourceRawTrack.PosKeys.Num() ? KeyIdx : 0 ));
// #if DEBUG_ADDITIVE_CREATION
// 		debugf(TEXT("GetRawAnimTrackKey BoneIndex: %d, SrcTrackIndex: %d, KeyIdx: %d, OutBoneAtom: %s"), BoneIndex, SrcTrackIndex, KeyIdx, *OutBoneAtom.ToString());
// #endif
	}
	else
	{
		GetBindPoseAtom(OutBoneAtom, BoneIndex, SkelMesh);
		// Bind Pose Atom doesn't need W flipped. So doing ActorX quat conversion here so the correct rotation is computed below.
		if( BoneIndex > 0 )
		{
			OutBoneAtom.FlipSignOfRotationW();
		}
	}
}

UBOOL FAnimationUtils::ConvertAnimSeqToAdditive
(
	UAnimSequence* SourceAnimSeq, 
	UAnimSequence* DestAnimSeq, 
	UAnimSequence* RefAnimSeq, 
	USkeletalMesh* SkelMesh, 
	EConvertToAdditive BuildMethod,
	UBOOL bIsLoopingAnim,
	UBOOL bRebuildExisting
)
{
#if WITH_EDITORONLY_DATA
	// We need a reference animation unless we use the reference pose.
	check(RefAnimSeq || BuildMethod == CTA_RefPose);
	check( SkelMesh );

	// Make sure source anim sequence is not already additive.
	if( SourceAnimSeq->bIsAdditive )
	{
		appMsgf( AMT_OK, LocalizeSecure(LocalizeUnrealEd("AnimSequenceAlreadyAdditive"), *SourceAnimSeq->SequenceName.ToString()) );
		return FALSE;
	}

	// Check if tracks between source and destination match.
	UAnimSet* SourceAnimSet = SourceAnimSeq->GetAnimSet();
	INT SourceAnimLinkupIndex = SourceAnimSet->GetMeshLinkupIndex( SkelMesh );
	FAnimSetMeshLinkup* SourceAnimLinkup = &SourceAnimSet->LinkupCache(SourceAnimLinkupIndex);

	UAnimSet* DestAnimSet = DestAnimSeq->GetAnimSet();
	INT DestAnimLinkupIndex = DestAnimSet->GetMeshLinkupIndex( SkelMesh );
	FAnimSetMeshLinkup* DestAnimLinkup = &DestAnimSet->LinkupCache(DestAnimLinkupIndex);

	UAnimSet* RefAnimSet = NULL;
	INT RefAnimLinkupIndex = INDEX_NONE;
	FAnimSetMeshLinkup* RefAnimLinkup = NULL;
	if( RefAnimSeq )
	{
		RefAnimSet = RefAnimSeq->GetAnimSet();
		RefAnimLinkupIndex = RefAnimSet->GetMeshLinkupIndex( SkelMesh );
		RefAnimLinkup = &RefAnimSet->LinkupCache(RefAnimLinkupIndex);
	}

	// Make sure we can find all of the destination tracks in the SkelMesh.
	for(INT DestTrackIdx=0; DestTrackIdx<DestAnimSet->TrackBoneNames.Num(); DestTrackIdx++)
	{
		// Figure out which bone this track is mapped to
		const INT BoneIndex = SkelMesh->MatchRefBone(DestAnimSet->TrackBoneNames(DestTrackIdx));
		if( BoneIndex == INDEX_NONE )
		{
			appMsgf(AMT_OK, LocalizeSecure(LocalizeUnrealEd("Error_CouldNotFindPatchBone"), *DestAnimSet->TrackBoneNames(DestTrackIdx).ToString()));
			return FALSE;
		}
	}

	// Make sure all the tracks from Source exist in Dest. Otherwise we'll lose some data.
	UBOOL bAskAboutPatching = TRUE;
	UBOOL bDoPatching = FALSE;
	for(INT SrcTrackIdx=0; SrcTrackIdx<SourceAnimSet->TrackBoneNames.Num(); SrcTrackIdx++)
	{
		INT DestTrackIndex = INDEX_NONE;
		if( !DestAnimSet->TrackBoneNames.FindItem( SourceAnimSet->TrackBoneNames(SrcTrackIdx), DestTrackIndex ) )
		{
			if (bAskAboutPatching)
			{
				UINT MsgResult = appMsgf(AMT_YesNoYesAllNoAll, LocalizeSecure(LocalizeUnrealEd("Error_CouldNotFindTrack"), *SourceAnimSet->TrackBoneNames(SrcTrackIdx).ToString()));
				bDoPatching = MsgResult == ART_Yes || MsgResult == ART_YesAll;
				bAskAboutPatching = MsgResult == ART_No || MsgResult == ART_Yes;
			}
			if( bDoPatching )
			{
				// Check the selected SkelMesh has a bone called that. If we can't find it - fail.
				INT PatchBoneIndex = SkelMesh->MatchRefBone(SourceAnimSet->TrackBoneNames(SrcTrackIdx));
				if( PatchBoneIndex == INDEX_NONE )
				{
					appMsgf(AMT_OK, LocalizeSecure(LocalizeUnrealEd("Error_CouldNotFindPatchBone"), *SourceAnimSet->TrackBoneNames(SrcTrackIdx).ToString()));
					return FALSE;
				}

				DestAnimSet->TrackBoneNames.AddItem(SourceAnimSet->TrackBoneNames(SrcTrackIdx));

				// Iterate over all existing sequences in this set and add an extra track to the end.
				for(INT SetAnimIndex=0; SetAnimIndex<DestAnimSet->Sequences.Num(); SetAnimIndex++)
				{
					UAnimSequence* ExtendSeq = DestAnimSet->Sequences(SetAnimIndex);

					// Remove any compression on the sequence so that it will be recomputed with the new track.
					if( ExtendSeq->CompressedTrackOffsets.Num() > 0 )
					{
						ExtendSeq->CompressedTrackOffsets.Empty();
					}

					// Add an extra track to the end, based on the ref skeleton.
					ExtendSeq->RawAnimationData.AddZeroed();
					FRawAnimSequenceTrack& RawTrack = ExtendSeq->RawAnimationData( ExtendSeq->RawAnimationData.Num()-1 );

					// Create 1-frame animation from the reference pose of the skeletal mesh.
					// This is basically what the compression does, so should be fine.
					if( ExtendSeq->bIsAdditive )
					{
						RawTrack.PosKeys.AddItem(FVector(0.f));

						FQuat RefOrientation = FQuat::Identity;
						// To emulate ActorX-exported animation quat-flipping, we do it here.
						if( PatchBoneIndex > 0 )
						{
							RefOrientation.W *= -1.f;
						}
						RawTrack.RotKeys.AddItem(RefOrientation);

						FBoneAtom RefBoneAtom;
						GetBindPoseAtom(RefBoneAtom, PatchBoneIndex, SkelMesh);

						if( PatchBoneIndex > 0)
						{
							RefBoneAtom.FlipSignOfRotationW(); // As above - flip if necessary
						}

						// Save off RefPose into destination AnimSequence
						ExtendSeq->AdditiveBasePose.AddZeroed();
						FRawAnimSequenceTrack& BasePoseTrack = ExtendSeq->AdditiveBasePose( ExtendSeq->AdditiveBasePose.Num()-1 );
						BasePoseTrack.PosKeys.AddItem(RefBoneAtom.GetTranslation());
						BasePoseTrack.RotKeys.AddItem(RefBoneAtom.GetRotation());
					}
					else
					{
						const FVector RefPosition = SkelMesh->RefSkeleton(PatchBoneIndex).BonePos.Position;
						RawTrack.PosKeys.AddItem(RefPosition);

						FQuat RefOrientation = SkelMesh->RefSkeleton(PatchBoneIndex).BonePos.Orientation;
						// To emulate ActorX-exported animation quat-flipping, we do it here.
						if( PatchBoneIndex > 0 )
						{
							RefOrientation.W *= -1.f;
						}
						RawTrack.RotKeys.AddItem(RefOrientation);
					}
				}

				// Update LinkupCache
				DestAnimSet->LinkupCache.Empty();
				DestAnimSet->SkelMesh2LinkupCache.Empty();
				DestAnimLinkupIndex = DestAnimSet->GetMeshLinkupIndex( SkelMesh );
				DestAnimLinkup = &DestAnimSet->LinkupCache(DestAnimLinkupIndex);

				// We need to re-init any skeletal mesh components now, because they might still have references to linkups in this set.
				for(TObjectIterator<USkeletalMeshComponent> It;It;++It)
				{
					USkeletalMeshComponent* SkelComp = *It;
					if(!SkelComp->IsPendingKill() && !SkelComp->IsTemplate())
					{
						SkelComp->InitAnimTree();
					}
				}

				// Recompress any sequences that need it.
				for( INT SequenceIndex = 0 ; SequenceIndex <DestAnimSet->Sequences.Num(); ++SequenceIndex )
				{
					UAnimSequence* AnimSeq = DestAnimSet->Sequences( SequenceIndex );
					FAnimationUtils::CompressAnimSequence(AnimSeq, NULL, FALSE, FALSE);
				}
			}
			else
			{
				return FALSE;
			}
		}
	}

	// Make sure destination is setup correctly
	DestAnimSeq->RecycleAnimSequence();

	// Copy properties of Source into Dest.
	UAnimSequence::CopyAnimSequenceProperties(SourceAnimSeq, DestAnimSeq, bRebuildExisting);

	// New name
	DestAnimSeq->SequenceName = FName( *FString::Printf(TEXT("ADD_%s"), *SourceAnimSeq->SequenceName.ToString()) );
	DestAnimSeq->bIsAdditive = TRUE;
	DestAnimSeq->bAdditiveBuiltLooping = bIsLoopingAnim;
	DestAnimSeq->AdditiveRefName = RefAnimSeq ? RefAnimSeq->SequenceName : FName( TEXT("Bind Pose") );

	// DestAnimSeq is now becoming an additive animation. So we need to update the references accordingly

	// First, remove all references to this new animation, as its status as changed. 
	// We copied the information from SourceAnimSeq, which could be a base or target pose. That wouldn't apply to this new additive animation.
	DestAnimSeq->ClearAdditiveAnimReferences();

	// Now setup our references
	if( RefAnimSeq )
	{
		DestAnimSeq->AdditiveBasePoseAnimSeq.AddUniqueItem( RefAnimSeq );
		RefAnimSeq->RelatedAdditiveAnimSeqs.AddUniqueItem( DestAnimSeq );
	}
	DestAnimSeq->AdditiveTargetPoseAnimSeq.AddUniqueItem( SourceAnimSeq );
	SourceAnimSeq->RelatedAdditiveAnimSeqs.AddUniqueItem( DestAnimSeq );

	// Make sure data is zeroed
	DestAnimSeq->RawAnimationData.AddZeroed( DestAnimSet->TrackBoneNames.Num() );
	if( DestAnimSeq->bIsAdditive )
	{
		DestAnimSeq->AdditiveBasePose.AddZeroed( DestAnimSet->TrackBoneNames.Num() );
	}

	// Two path for construction
	// 1) Base Pose is a fixed frame. Then SourceAnim dictates the number of keys to do.
	// 2) Base Pose is animated. Then whoever has the most keys dictates how many keys are going to end up there.
	//		Also requires simple key reduction at the end, to turn some tracks to a single key when needed.

	// Base Pose is a single frame.
	if( BuildMethod == CTA_RefPose || BuildMethod == CTA_AnimFirstFrame || RefAnimSeq->NumFrames == 1 )
	{
		FBoneAtom BindPoseAtom = FBoneAtom::Identity;
		FBoneAtom RefBoneAtom = FBoneAtom::Identity;
		FBoneAtom SourceBoneAtom = FBoneAtom::Identity;

		// Import each track.
		for(INT DestTrackIdx=0; DestTrackIdx<DestAnimSet->TrackBoneNames.Num(); DestTrackIdx++)
		{
			// Figure out which bone this track is mapped to
			const INT BoneIndex = SkelMesh->MatchRefBone(DestAnimSet->TrackBoneNames(DestTrackIdx));
			check( BoneIndex != INDEX_NONE );

			// Bind Pose Atom
			GetBindPoseAtom(BindPoseAtom, BoneIndex, SkelMesh);
			// Bind Pose Atom doesn't need W flipped. So doing ActorX quat conversion here so the correct rotation is computed below.
			if( BoneIndex > 0 )
			{
				BindPoseAtom.FlipSignOfRotationW();
			}

			// Ref pose 
			if( BuildMethod == CTA_RefPose )
			{
				RefBoneAtom = BindPoseAtom;
			}
			// First frame of reference animation
			else if( BuildMethod == CTA_AnimFirstFrame || RefAnimSeq->NumFrames == 1 )
			{
				GetRawAnimTrackKey(RefBoneAtom, BoneIndex, 0, RefAnimSeq, RefAnimLinkup, SkelMesh);

				// Check AnimRotationOnly settings, and adjust Translation component accordingly
				if( RefAnimSet->bAnimRotationOnly 
					&& (!RefAnimSet->UseTranslationBoneNames.ContainsItem(DestAnimSet->TrackBoneNames(DestTrackIdx))
						|| RefAnimSet->ForceMeshTranslationBoneNames.ContainsItem(DestAnimSet->TrackBoneNames(DestTrackIdx))) )
				{
					RefBoneAtom.SetTranslation( BindPoseAtom.GetTranslation() );
				}
			}
			else
			{
				check(FALSE);
			}
			
			// Go through all keys and turns them into additive
			// That is the difference to the Reference pose.
			FRawAnimSequenceTrack& DestRawTrack = DestAnimSeq->RawAnimationData(DestTrackIdx);
			FRawAnimSequenceTrack& BasePoseTrack = DestAnimSeq->AdditiveBasePose(DestTrackIdx);

			DestRawTrack.PosKeys.Reset();
			DestRawTrack.RotKeys.Reset();

			FCurveKeyArray DummyCurveKeys;
			DestRawTrack.PosKeys.Add( SourceAnimSeq->NumFrames );
			DestRawTrack.RotKeys.Add( SourceAnimSeq->NumFrames );

			// Size AdditiveBasePose Array
			BasePoseTrack.PosKeys.Add( 1 );
			BasePoseTrack.RotKeys.Add( 1 );

			// Save off Base Pose into destination AnimSequence for preview in editor
			BasePoseTrack.PosKeys(0) = RefBoneAtom.GetTranslation();
			BasePoseTrack.RotKeys(0) = RefBoneAtom.GetRotation();

			for(INT KeyIdx=0; KeyIdx<SourceAnimSeq->NumFrames; KeyIdx++)
			{
				GetRawAnimTrackKey(SourceBoneAtom, BoneIndex, KeyIdx, SourceAnimSeq, SourceAnimLinkup, SkelMesh);

				// Check AnimRotationOnly settings, and adjust Translation component accordingly
				if( SourceAnimSet->bAnimRotationOnly 
					&& (!SourceAnimSet->UseTranslationBoneNames.ContainsItem(DestAnimSet->TrackBoneNames(DestTrackIdx)) 
						|| SourceAnimSet->ForceMeshTranslationBoneNames.ContainsItem(DestAnimSet->TrackBoneNames(DestTrackIdx))) )
				{
					SourceBoneAtom.SetTranslation( BindPoseAtom.GetTranslation() );
				}

				// For rotation part. We have this annoying thing to work around...
				// See UAnimNodeSequence::GetAnimationPose()
				// Make delta with "quaternion fix for ActorX exported quaternions". Then revert back.
				if( BoneIndex > 0 )
				{
					SourceBoneAtom.FlipSignOfRotationW();
					RefBoneAtom.FlipSignOfRotationW();
				}

				// Actual delta.
				DestRawTrack.RotKeys(KeyIdx) = SourceBoneAtom.GetRotation() * (-RefBoneAtom.GetRotation());
				DestRawTrack.PosKeys(KeyIdx) = SourceBoneAtom.GetTranslation() - RefBoneAtom.GetTranslation();
				
				// Convert back to non "quaternion fix for ActorX exported quaternions".
				if( BoneIndex > 0 )
				{
					DestRawTrack.RotKeys(KeyIdx).W *= -1.f;
					RefBoneAtom.FlipSignOfRotationW();
				}

				// Normalize resulting quaternion.
				DestRawTrack.RotKeys(KeyIdx).Normalize();
			}
		}
	}
	// Base Pose is animated
	else if( BuildMethod == CTA_AnimScaled )
	{
		FBoneAtom BindPoseAtom = FBoneAtom::Identity;
		FBoneAtom RefBoneAtom = FBoneAtom::Identity;
		FBoneAtom SourceBoneAtom = FBoneAtom::Identity;
	
		// Uncomment below to be able to scale Source to size of Reference, if Reference is bigger.
// 		if( RefAnimSeq->NumFrames > SourceAnimSeq->NumFrames )
// 		{
// 			DestAnimSeq->NumFrames = RefAnimSeq->NumFrames;
// 			DestAnimSeq->SequenceLength = RefAnimSeq->SequenceLength;
// 		}

		const INT NumKeys = DestAnimSeq->NumFrames;
		debugf(TEXT("Creating additive animation %s. bIsLoopingAnim: %d"), *DestAnimSeq->SequenceName.ToString(), bIsLoopingAnim);
		debugf(TEXT("Destination. %d frames, length: %f"), NumKeys, DestAnimSeq->SequenceLength);
		debugf(TEXT("Source: %s. %d frames, length: %f"), *SourceAnimSeq->SequenceName.ToString(), SourceAnimSeq->NumFrames, SourceAnimSeq->SequenceLength);
		if( RefAnimSeq )
		{
			debugf(TEXT("Reference %s. %d frames, length: %f"), *RefAnimSeq->SequenceName.ToString(), RefAnimSeq->NumFrames, RefAnimSeq->SequenceLength);
		}
		else
		{
			debugf(TEXT("Reference Bind Pose."));
		}

		// Import each track.
		for(INT DestTrackIdx=0; DestTrackIdx<DestAnimSet->TrackBoneNames.Num(); DestTrackIdx++)
		{
			// Figure out which bone this track is mapped to
			const INT BoneIndex = SkelMesh->MatchRefBone(DestAnimSet->TrackBoneNames(DestTrackIdx));
			check( BoneIndex != INDEX_NONE );

			// Bind Pose Atom
			GetBindPoseAtom(BindPoseAtom, BoneIndex, SkelMesh);
			// Bind Pose Atom doesn't need W flipped. So doing ActorX quat conversion here so the correct rotation is computed below.
			if( BoneIndex > 0 )
			{
				BindPoseAtom.FlipSignOfRotationW();
			}

			// Go through all keys and turns them into additive
			// That is the difference to the Reference pose.
			FRawAnimSequenceTrack& DestRawTrack = DestAnimSeq->RawAnimationData(DestTrackIdx);
			FRawAnimSequenceTrack& BasePoseTrack = DestAnimSeq->AdditiveBasePose(DestTrackIdx);

			DestRawTrack.PosKeys.Add(NumKeys);
			DestRawTrack.RotKeys.Add(NumKeys);

			BasePoseTrack.PosKeys.Add(NumKeys);
			BasePoseTrack.RotKeys.Add(NumKeys);

#if DEBUG_ADDITIVE_CREATION
			debugf(TEXT("\tTrack #%3d, Bone #3d, %s"), DestTrackIdx, BoneIndex, *DestAnimSet->TrackBoneNames(DestTrackIdx).ToString());
#endif
			for(INT KeyIdx=0; KeyIdx<NumKeys; KeyIdx++)
			{
				// Get pose from ref animation
				// If we have the same number of keys, we can directly use those
				if( RefAnimSeq->NumFrames == DestAnimSeq->NumFrames )
				{
#if DEBUG_ADDITIVE_CREATION
					debugf(TEXT("\t\t Getting Ref Pose, GetRawAnimTrackKey"));
#endif
					GetRawAnimTrackKey(RefBoneAtom, BoneIndex, KeyIdx, RefAnimSeq, RefAnimLinkup, SkelMesh);
				}
				// Otherwise we have to scale the animation
				else
				{
					const INT RefTrackIndex = RefAnimLinkup->BoneToTrackTable(BoneIndex);
					if( RefTrackIndex != INDEX_NONE )
					{
#if DEBUG_ADDITIVE_CREATION
						debugf(TEXT("\t\t Getting Ref Pose, GetBoneAtom. RefTrackIndex %d"), RefTrackIndex);
#endif
						const FLOAT Position = (RefAnimSeq->SequenceLength * FLOAT(KeyIdx)) / FLOAT((bIsLoopingAnim || NumKeys==1) ? NumKeys : (NumKeys-1));

						RefAnimSeq->GetBoneAtom(RefBoneAtom, RefTrackIndex, Position, bIsLoopingAnim, TRUE);
					}
					else
					{
#if DEBUG_ADDITIVE_CREATION
						debugf(TEXT("\t\t Getting Ref Pose, GetBindPoseAtom"));
#endif
						GetBindPoseAtom(RefBoneAtom, BoneIndex, SkelMesh);
						// Bind Pose Atom doesn't need W flipped. So doing ActorX quat conversion here so the correct rotation is computed below.
						if( BoneIndex > 0 )
						{
							RefBoneAtom.FlipSignOfRotationW();
						}
					}
				}

				// Check AnimRotationOnly settings, and adjust Translation component accordingly
				if( RefAnimSet->bAnimRotationOnly 
					&& (!RefAnimSet->UseTranslationBoneNames.ContainsItem(DestAnimSet->TrackBoneNames(DestTrackIdx)) 
						|| RefAnimSet->ForceMeshTranslationBoneNames.ContainsItem(DestAnimSet->TrackBoneNames(DestTrackIdx))) )
				{
					RefBoneAtom.SetTranslation( BindPoseAtom.GetTranslation() );
				}

				// Get pose from source animation
				// If we have the same number of keys, we can directly use those
				if( SourceAnimSeq->NumFrames == DestAnimSeq->NumFrames )
				{
#if DEBUG_ADDITIVE_CREATION
					debugf(TEXT("\t\t Getting Source Pose, GetRawAnimTrackKey"));
#endif
					GetRawAnimTrackKey(SourceBoneAtom, BoneIndex, KeyIdx, SourceAnimSeq, SourceAnimLinkup, SkelMesh);
				}
				// Otherwise we have to scale the animation
				else
				{
					const INT SourceTrackIndex = SourceAnimLinkup->BoneToTrackTable(BoneIndex);
					if( SourceTrackIndex != INDEX_NONE )
					{
#if DEBUG_ADDITIVE_CREATION
						debugf(TEXT("\t\t Getting Source Pose, GetBoneAtom. RefTrackIndex %d"), SourceTrackIndex);
#endif
						const FLOAT Position = (SourceAnimSeq->SequenceLength * FLOAT(KeyIdx)) / FLOAT((bIsLoopingAnim || NumKeys==1) ? NumKeys : (NumKeys-1));

						SourceAnimSeq->GetBoneAtom(SourceBoneAtom, SourceTrackIndex, Position, bIsLoopingAnim, TRUE);
					}
					else
					{
#if DEBUG_ADDITIVE_CREATION
						debugf(TEXT("\t\t Getting Ref Pose, GetBindPoseAtom"));
#endif
						GetBindPoseAtom(SourceBoneAtom, BoneIndex, SkelMesh);
						// Bind Pose Atom doesn't need W flipped. So doing ActorX quat conversion here so the correct rotation is computed below.
						if( BoneIndex > 0 )
						{
							SourceBoneAtom.FlipSignOfRotationW();
						}
					}
				}

				// Check AnimRotationOnly settings, and adjust Translation component accordingly
				if( SourceAnimSet->bAnimRotationOnly 
					&& (!SourceAnimSet->UseTranslationBoneNames.ContainsItem(DestAnimSet->TrackBoneNames(DestTrackIdx)) 
						|| SourceAnimSet->ForceMeshTranslationBoneNames.ContainsItem(DestAnimSet->TrackBoneNames(DestTrackIdx))) )
				{
					SourceBoneAtom.SetTranslation( BindPoseAtom.GetTranslation() );
				}

				// Save pose for editor display.
				BasePoseTrack.PosKeys(KeyIdx) = RefBoneAtom.GetTranslation();
				BasePoseTrack.RotKeys(KeyIdx) = RefBoneAtom.GetRotation();

				// For rotation part. We have this annoying thing to work around...
				// See UAnimNodeSequence::GetAnimationPose()
				// Make delta with "quaternion fix for ActorX exported quaternions". Then revert back.
				if( BoneIndex > 0 )
				{
					SourceBoneAtom.FlipSignOfRotationW();
					RefBoneAtom.FlipSignOfRotationW();
				}

				// Actual delta.
				DestRawTrack.RotKeys(KeyIdx) = SourceBoneAtom.GetRotation() * (-RefBoneAtom.GetRotation());
				DestRawTrack.PosKeys(KeyIdx) = SourceBoneAtom.GetTranslation() - RefBoneAtom.GetTranslation();
				
				// Convert back to non "quaternion fix for ActorX exported quaternions".
				if( BoneIndex > 0 )
				{
					DestRawTrack.RotKeys(KeyIdx).W *= -1.f;
					RefBoneAtom.FlipSignOfRotationW();
				}

				// Normalize resulting quaternion.
				DestRawTrack.RotKeys(KeyIdx).Normalize();
			}
		}
	}
	else
	{
		check(FALSE && TEXT("UnSupported Build Method"));
	}

	// Compress Raw Anim data.
	DestAnimSeq->CompressRawAnimData();

	// See if SourceAnimSeq had a compression Scheme
	FAnimationUtils::CompressAnimSequence(DestAnimSeq, NULL, FALSE, FALSE);
#endif // WITH_EDITORONLY_DATA

	return TRUE;
}

/**
 * Converts an animation compression type into a human readable string
 *
 * @param	InFormat	The compression format to convert into a string
 * @return				The format as a string
 */
FString FAnimationUtils::GetAnimationCompressionFormatString(AnimationCompressionFormat InFormat)
{
	switch(InFormat)
	{
	case ACF_None:
		return FString(TEXT("ACF_None"));
	case ACF_Float96NoW:
		return FString(TEXT("ACF_Float96NoW"));
	case ACF_Fixed48NoW:
		return FString(TEXT("ACF_Fixed48NoW"));
	case ACF_IntervalFixed32NoW:
		return FString(TEXT("ACF_IntervalFixed32NoW"));
	case ACF_Fixed32NoW:
		return FString(TEXT("ACF_Fixed32NoW"));
	case ACF_Float32NoW:
		return FString(TEXT("ACF_Float32NoW"));
	case ACF_Identity:
		return FString(TEXT("ACF_Identity"));
	default:
		warnf( TEXT("AnimationCompressionFormat was not found:  %i"), static_cast<INT>(InFormat) );
	}

	return FString(TEXT("Unknown"));
}

/**
 * Converts an animation codec format into a human readable string
 *
 * @param	InFormat	The format to convert into a string
 * @return				The format as a string
 */
FString FAnimationUtils::GetAnimationKeyFormatString(AnimationKeyFormat InFormat)
{
	switch(InFormat)
	{
	case AKF_ConstantKeyLerp:
		return FString(TEXT("AKF_ConstantKeyLerp"));
	case AKF_VariableKeyLerp:
		return FString(TEXT("AKF_VariableKeyLerp"));
	case AKF_PerTrackCompression:
		return FString(TEXT("AKF_PerTrackCompression"));
	default:
		warnf( TEXT("AnimationKeyFormat was not found:  %i"), static_cast<INT>(InFormat) );
	}

	return FString(TEXT("Unknown"));
}


/**
 * Computes the 'height' of each track, relative to a given animation linkup.
 *
 * The track height is defined as the minimal number of bones away from an end effector (end effectors are 0, their parents are 1, etc...)
 *
 * @param AnimLinkup			The animation linkup
 * @param BoneData				The bone data to check
 * @param NumTracks				The number of tracks
 * @param TrackHeights [OUT]	The computed track heights
 *
 */
void FAnimationUtils::CalculateTrackHeights(const FAnimSetMeshLinkup& AnimLinkup, const TArray<FBoneData>& BoneData, INT NumTracks, TArray<INT>& TrackHeights)
{
	TrackHeights.Empty();
	TrackHeights.Add(NumTracks);
	for (INT TrackIndex = 0; TrackIndex < NumTracks; ++TrackIndex)
	{
		TrackHeights(TrackIndex) = 0;
	}

	// Populate the bone 'height' table (distance from closest end effector, with 0 indicating an end effector)
	// setup the raw bone transformation and find all end effectors
	for (INT BoneIndex = 0; BoneIndex < BoneData.Num(); ++BoneIndex)
	{
		// also record all end-effectors we find
		const FBoneData& Bone = BoneData(BoneIndex);
		if (Bone.IsEndEffector())
		{
			const FBoneData& EffectorBoneData = BoneData(BoneIndex);

			for (INT FamilyIndex = 0; FamilyIndex < EffectorBoneData.BonesToRoot.Num(); ++FamilyIndex)
			{
				const INT NextParentBoneIndex = EffectorBoneData.BonesToRoot(FamilyIndex);
				const INT NextParentTrackIndex = (NextParentBoneIndex != INDEX_NONE) ? AnimLinkup.BoneToTrackTable(NextParentBoneIndex) : 0;
				if (NextParentTrackIndex != INDEX_NONE)
				{
					const INT CurHeight = TrackHeights(NextParentTrackIndex);
					TrackHeights(NextParentTrackIndex) = (CurHeight > 0) ? Min<INT>(CurHeight, (FamilyIndex+1)) : (FamilyIndex+1);
				}
			}
		}
	}
}

/**
 * Checks a set of key times to see if the spacing is uniform or non-uniform.
 * Note: If there are as many times as frames, they are automatically assumed to be uniformly spaced.
 * Note: If there are two or fewer times, they are automatically assumed to be uniformly spaced.
 *
 * @param AnimSeq		The animation sequence the Times array is associated with
 * @param Times			The array of key times
 *
 * @return				TRUE if the keys are uniformly spaced (or one of the trivial conditions is detected).  FALSE if any key spacing is greater than 1e-4 off.
 */
UBOOL FAnimationUtils::HasUniformKeySpacing(UAnimSequence* AnimSeq, const TArrayNoInit<FLOAT>& Times)
{
	if ((Times.Num() <= 2) || (Times.Num() == AnimSeq->NumFrames))
	{
		return TRUE;
	}

	FLOAT FirstDelta = Times(1) - Times(0);
	for (INT i = 2; i < Times.Num(); ++i)
	{
		FLOAT DeltaTime = Times(i) - Times(i-1);

		if (fabs(DeltaTime - FirstDelta) > KINDA_SMALL_NUMBER)
		{
			return FALSE;
		}
	}

	return FALSE;
}

/**
 * Perturbs the bone(s) associated with each track in turn, measuring the maximum error introduced in end effectors as a result
 */
void FAnimationUtils::TallyErrorsFromPerturbation(
	const UAnimSequence* AnimSeq,
	INT NumTracks,
	USkeletalMesh* SkelMesh,
	const FAnimSetMeshLinkup& AnimLinkup,
	const TArray<FBoneData>& BoneData,
	const FVector& PositionNudge,
	const FQuat& RotationNudge,
	TArray<FAnimPerturbationError>& InducedErrors)
{
	const FLOAT TimeStep = (FLOAT)AnimSeq->SequenceLength / (FLOAT)AnimSeq->NumFrames;
	const INT NumBones = BoneData.Num();

	UAnimSet* AnimSet = AnimSeq->GetAnimSet();
	const TArray<FMeshBone>& RefSkel = SkelMesh->RefSkeleton;
	check( AnimLinkup.BoneToTrackTable.Num() == RefSkel.Num() );

	TArray<FBoneAtom> RawAtoms;
	TArray<FBoneAtom> NewAtomsT;
	TArray<FBoneAtom> NewAtomsR;
	TArray<FBoneAtom> RawTransforms;
	TArray<FBoneAtom> NewTransformsT;
	TArray<FBoneAtom> NewTransformsR;

	RawAtoms.AddZeroed(NumBones);
	NewAtomsT.AddZeroed(NumBones);
	NewAtomsR.AddZeroed(NumBones);
	RawTransforms.AddZeroed(NumBones);
	NewTransformsT.AddZeroed(NumBones);
	NewTransformsR.AddZeroed(NumBones);

	InducedErrors.Add(NumTracks);

	FBoneAtom Perturbation(RotationNudge, PositionNudge);

	for (INT TrackUnderTest = 0; TrackUnderTest < NumTracks; ++TrackUnderTest)
	{
		FLOAT MaxErrorT_DueToT = 0.0f;
		FLOAT MaxErrorR_DueToT = 0.0f;
		FLOAT MaxErrorT_DueToR = 0.0f;
		FLOAT MaxErrorR_DueToR = 0.0f;

		// for each whole increment of time (frame stepping)
		for (FLOAT Time = 0.0f; Time < AnimSeq->SequenceLength; Time += TimeStep)
		{
			// get the raw and compressed atom for each bone
			for (INT BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
			{
				const INT TrackIndex = AnimLinkup.BoneToTrackTable(BoneIndex);

				if (TrackIndex == INDEX_NONE)
				{
					// No track for the bone was found, so use the reference pose.
					RawAtoms(BoneIndex)	= FBoneAtom(RefSkel(BoneIndex).BonePos.Orientation, RefSkel(BoneIndex).BonePos.Position, 1.f);
					NewAtomsT(BoneIndex) = RawAtoms(BoneIndex);
					NewAtomsR(BoneIndex) = RawAtoms(BoneIndex);
				}
				else
				{
					AnimSeq->GetBoneAtom(RawAtoms(BoneIndex), TrackIndex, Time, FALSE, TRUE);
					if (BoneIndex > 0) 
					{
						// Apply quaternion fix for ActorX-exported quaternions.
						RawAtoms(BoneIndex).FlipSignOfRotationW();
					}

					// apply the reference bone atom (if needed)
					if (AnimSeq->bIsAdditive)
					{
						FBoneAtom RefBoneAtom;
						AnimSeq->GetAdditiveBasePoseBoneAtom(RefBoneAtom, TrackIndex, Time, FALSE);

						if (BoneIndex > 0)
						{
							// Apply quaternion fix for ActorX-exported quaternions.
							RefBoneAtom.FlipSignOfRotationW();
						}

						RawAtoms(BoneIndex).AddToTranslation(RefBoneAtom.GetTranslationV());
						RawAtoms(BoneIndex).ConcatenateRotation(RefBoneAtom.GetRotationV());
					}

					NewAtomsT(BoneIndex) = RawAtoms(BoneIndex);
					NewAtomsR(BoneIndex) = RawAtoms(BoneIndex);

					// Perturb the bone under test
					if (TrackIndex == TrackUnderTest)
					{
						NewAtomsT(BoneIndex).AddToTranslation(PositionNudge);

						FQuat NewR = NewAtomsR(BoneIndex).GetRotation();
						NewR += RotationNudge;
						NewR.Normalize();
						NewAtomsR(BoneIndex).SetRotation(NewR);
					}
				}

				RawTransforms(BoneIndex) = RawAtoms(BoneIndex);
				NewTransformsT(BoneIndex) = NewAtomsT(BoneIndex);
				NewTransformsR(BoneIndex) = NewAtomsR(BoneIndex);

				// For all bones below the root, final component-space transform is relative transform * component-space transform of parent.
				if ( BoneIndex > 0 )
				{
					const INT ParentIndex = RefSkel(BoneIndex).ParentIndex;

					// Check the precondition that parents occur before children in the RequiredBones array.
					check( ParentIndex != INDEX_NONE );
					check( ParentIndex < BoneIndex );

					RawTransforms(BoneIndex) *= RawTransforms(ParentIndex);
					NewTransformsT(BoneIndex) *= NewTransformsT(ParentIndex);
					NewTransformsR(BoneIndex) *= NewTransformsR(ParentIndex);
				}

				// Only look at the error that occurs in end effectors
				if (BoneData(BoneIndex).IsEndEffector())
				{
					MaxErrorT_DueToT = Max(MaxErrorT_DueToT, (RawTransforms(BoneIndex).GetOrigin() - NewTransformsT(BoneIndex).GetOrigin()).Size());
					MaxErrorT_DueToR = Max(MaxErrorT_DueToR, (RawTransforms(BoneIndex).GetOrigin() - NewTransformsR(BoneIndex).GetOrigin()).Size());
					MaxErrorR_DueToT = Max(MaxErrorR_DueToT, FQuatErrorAutoNormalize(RawTransforms(BoneIndex).GetRotation(), NewTransformsT(BoneIndex).GetRotation()));
					MaxErrorR_DueToR = Max(MaxErrorR_DueToR, FQuatErrorAutoNormalize(RawTransforms(BoneIndex).GetRotation(), NewTransformsR(BoneIndex).GetRotation()));
				}
			} // for each bone
		} // for each time

		// Save the worst errors
		FAnimPerturbationError& TrackError = InducedErrors(TrackUnderTest);
		TrackError.MaxErrorInTransDueToTrans = MaxErrorT_DueToT;
		TrackError.MaxErrorInRotDueToTrans = MaxErrorR_DueToT;
		TrackError.MaxErrorInTransDueToRot = MaxErrorT_DueToR;
		TrackError.MaxErrorInRotDueToRot = MaxErrorR_DueToR;
	}
}
