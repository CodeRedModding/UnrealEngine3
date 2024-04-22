/*=============================================================================
	UnSkeletalAnim.cpp: Skeletal mesh animation functions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/ 

#include "EnginePrivate.h"
#include "EngineAnimClasses.h"
#include "EngineAudioDeviceClasses.h"
#include "EngineSequenceClasses.h"
#include "AnimationCompression.h"
#include "AnimationEncodingFormat.h"
#include "AnimationUtils.h"
#include "PerfMem.h"
#include "EngineParticleClasses.h"
#include "EngineForceFieldClasses.h"
// Priority with which to display sounds triggered by sound notifies.
#define SUBTITLE_PRIORITY_ANIMNOTIFY	10000

IMPLEMENT_CLASS(UAnimSequence)
IMPLEMENT_CLASS(UAnimSet)
IMPLEMENT_CLASS(UAnimNotify)

IMPLEMENT_CLASS(UAnimMetaData)
IMPLEMENT_CLASS(UAnimMetaData_SkelControl)
IMPLEMENT_CLASS(UAnimMetaData_SkelControlKeyFrame)

IMPLEMENT_CLASS(UHeadTrackingComponent)

#define USE_SLERP 0

//@deprecated with VER_REPLACED_LAZY_ARRAY_WITH_UNTYPED_BULK_DATA
struct FRawAnimSequenceTrackNativeDeprecated
{
    TArray<FVector> PosKeys;
    TArray<FQuat>	RotKeys;
	friend FArchive& operator<<(FArchive& Ar, FRawAnimSequenceTrackNativeDeprecated& T)
	{
		return	Ar << T.PosKeys << T.RotKeys;
	}
};

/** Returns TRUE if valid curve weight exists in the array*/
UBOOL FCurveTrack::IsValidCurveTrack()
{
	UBOOL bValid = FALSE;

	if ( CurveName != NAME_None )
	{
		for (INT I=0; I<CurveWeights.Num(); ++I)
		{
			// it has valid weight
			if (CurveWeights(I)>KINDA_SMALL_NUMBER)
			{
				bValid = TRUE;
				break;
			}
		}
	}

	return bValid;
}

/** This is very simple cut to 1 key method if all is same since I see so many redundant same value in every frame 
 *  Eventually this can get more complicated 
 *  Will return TRUE if compressed to 1. Return FALSE otherwise **/
UBOOL FCurveTrack::CompressCurveWeights()
{
	// if always 1, no reason to do this
	if ( CurveWeights.Num() > 1 )
	{
		UBOOL bCompress = TRUE;
		// first weight
		FLOAT FirstWeight = CurveWeights(0);

		for (INT I=1; I<CurveWeights.Num(); ++I)
		{
			// see if my key is same as previous
			if (fabs(FirstWeight - CurveWeights(I)) > SMALL_NUMBER)
			{
				// if not same, just get out, you don't like to compress this to 1 key
				bCompress = FALSE;
				break;
			}
		} 

		if (bCompress)
		{
			CurveWeights.Empty();
			CurveWeights.AddItem(FirstWeight);
			CurveWeights.Shrink();
		}

		return bCompress;
	}

	// nothing changed
	return FALSE;
}

/**
 * Returns the size of the object/ resource for display to artists/ LDs in the Editor.
 *
 * @return size of resource as to be displayed to artists/ LDs in the Editor.
 */
INT UAnimSequence::GetResourceSize()
{
	if (GExclusiveResourceSizeMode)
	{
		// Sizes below are fully covered by the count serializer used when determining Num
		return 0;
	}
	else
	{
		const INT ResourceSize = CompressedTrackOffsets.Num() == 0 ? GetApproxRawSize() : GetApproxCompressedSize();
		return ResourceSize;
	}
}

/**
 * @return		The approximate size of raw animation data.
 */
INT UAnimSequence::GetApproxRawSize() const
{
	INT Total = sizeof(FRawAnimSequenceTrack) * RawAnimationData.Num();
	for (INT i=0;i<RawAnimationData.Num();++i)
	{
		const FRawAnimSequenceTrack& RawTrack = RawAnimationData(i);
		Total +=
			sizeof( FVector ) * RawTrack.PosKeys.Num() +
			sizeof( FQuat ) * RawTrack.RotKeys.Num();
	}
	return Total;
}

/**
 * @return		The approximate size of key-reduced animation data.
 */
INT UAnimSequence::GetApproxReducedSize() const
{
	INT Total =
		sizeof(FTranslationTrack) * TranslationData.Num() +
		sizeof(FRotationTrack) * RotationData.Num();

	for (INT i=0;i<TranslationData.Num();++i)
	{
		const FTranslationTrack& TranslationTrack = TranslationData(i);
		Total +=
			sizeof( FVector ) * TranslationTrack.PosKeys.Num() +
			sizeof( FLOAT ) * TranslationTrack.Times.Num();
	}

	for (INT i=0;i<RotationData.Num();++i)
	{
		const FRotationTrack& RotationTrack = RotationData(i);
		Total +=
			sizeof( FQuat ) * RotationTrack.RotKeys.Num() +
			sizeof( FLOAT ) * RotationTrack.Times.Num();
	}
	return Total;
}


/**
 * @return		The approximate size of compressed animation data.
 */
INT UAnimSequence::GetApproxCompressedSize() const
{
	const INT Total = sizeof(INT)*CompressedTrackOffsets.Num() + CompressedByteStream.Num();
	return Total;
}

/**
 * Deserializes old compressed track formats from the specified archive.
 */
static void LoadOldCompressedTrack(FArchive& Ar, FCompressedTrack& Dst, INT ByteStreamStride)
{
	// Serialize from the archive to a buffer.
	INT NumBytes = 0;
	Ar << NumBytes;

	TArray<BYTE> SerializedData;
	SerializedData.Empty( NumBytes );
	SerializedData.Add( NumBytes );
	Ar.Serialize( SerializedData.GetData(), NumBytes );

	// Serialize the key times.
	Ar << Dst.Times;

	// Serialize mins and ranges.
	Ar << Dst.Mins[0] << Dst.Mins[1] << Dst.Mins[2];
	Ar << Dst.Ranges[0] << Dst.Ranges[1] << Dst.Ranges[2];
}
	
void UAnimSequence::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	//@compatibility:
	if( Ar.Ver() < VER_NATIVE_RAWANIMDATA_SERIALIZATION )
	{
		// Deprecated old serialization. Copy the script serialized array over to the new
		// property. Sadly there is no good way to change from script to native serialization
		// in a straightforward way without introducing a new property.
		RawAnimationData = RawAnimData_DEPRECATED;
		RawAnimData_DEPRECATED.Empty();
	}
	else
	{
		Ar << RawAnimationData;
	}

	if ( Ar.IsLoading() )
	{
		// Serialize the compressed byte stream from the archive to the buffer.
		INT NumBytes;
		Ar << NumBytes;

		TArray<BYTE> SerializedData;
		SerializedData.Empty( NumBytes );
		SerializedData.Add( NumBytes );
		Ar.Serialize( SerializedData.GetData(), NumBytes );

		// Swap the buffer into the byte stream.
		FMemoryReader MemoryReader( SerializedData, TRUE );
		MemoryReader.SetByteSwapping( Ar.ForceByteSwapping() );

		// we must know the proper codecs to use
		AnimationFormat_SetInterfaceLinks(*this);

		// and then use the codecs to byte swap
		check( RotationCodec != NULL );
		((AnimationEncodingFormat*)RotationCodec)->ByteSwapIn(*this, MemoryReader, Ar.Ver());
	}
	else if( Ar.IsSaving() || Ar.IsCountingMemory() )
	{
		// Swap the byte stream into a buffer.
		TArray<BYTE> SerializedData;

		// we must know the proper codecs to use
		AnimationFormat_SetInterfaceLinks(*this);

		// and then use the codecs to byte swap
		check( RotationCodec != NULL );
		((AnimationEncodingFormat*)RotationCodec)->ByteSwapOut(*this, SerializedData, Ar.ForceByteSwapping());

		// Make sure the entire byte stream was serialized.
		check( CompressedByteStream.Num() == SerializedData.Num() );

		// Serialize the buffer to archive.
		INT Num = SerializedData.Num();
		Ar << Num;
		Ar.Serialize( SerializedData.GetData(), SerializedData.Num() );

		// Count compressed data.
		Ar.CountBytes( SerializedData.Num(), SerializedData.Num() );
	}
}

/**
 * Used by various commandlets to purge editor only and platform-specific data from various objects
 * 
 * @param PlatformsToKeep Platforms for which to keep platform-specific data
 * @param bStripLargeEditorData If TRUE, data used in the editor, but large enough to bloat download sizes, will be removed
 */
void UAnimSequence::StripData(UE3::EPlatformType PlatformsToKeep, UBOOL bStripLargeEditorData)
{
	Super::StripData(PlatformsToKeep, bStripLargeEditorData); 

#if WITH_EDITORONLY_DATA
	// if we aren't keeping any non-stripped platforms, we can toss the data
	if (!(PlatformsToKeep & ~UE3::PLATFORM_Stripped))
	{
		// Remove raw animation data.
		for( INT TrackIndex = 0 ; TrackIndex < RawAnimationData.Num() ; ++TrackIndex )
		{
			FRawAnimSequenceTrack& RawTrack = RawAnimationData(TrackIndex);
			RawTrack.PosKeys.Empty();
			RawTrack.RotKeys.Empty();
		}
		RawAnimationData.Empty();

		// Remove any key-reduced data.
		TranslationData.Empty();
		RotationData.Empty();

		// Remove raw animation data.
		for(INT TrackIndex = 0; TrackIndex < AdditiveBasePose.Num(); ++TrackIndex)
		{
			FRawAnimSequenceTrack& BasePoseTrack = AdditiveBasePose(TrackIndex);
			BasePoseTrack.PosKeys.Empty();
			BasePoseTrack.RotKeys.Empty();
		}
		AdditiveBasePose.Empty();
	}
#endif // WITH_EDITORONLY_DATA
}

void UAnimSequence::PreSave()
{
#if WITH_EDITORONLY_DATA
	// UAnimSequence::CompressionScheme is editoronly, but animations could be compressed during cook
	// via PostLoad; so, clear the reference before saving in the cooker,
	if ( GIsCooking )
	{
		CompressionScheme = NULL;
	}
#endif // WITH_EDITORONLY_DATA
}

void UAnimSequence::PostLoad()
{
	UBOOL bMarkDirty = FALSE;
	Super::PostLoad();

	// Convert to new base pose for additive animations if needed
	if( bIsAdditive && GetLinkerVersion() < VER_NEW_BASE_POSE_ADDITIVE_ANIM_FORMAT )
	{
		if( AdditiveRefPose_DEPRECATED.Num() > 0 && AdditiveBasePose.Num() == 0 )
		{
			AdditiveBasePose.AddZeroed( AdditiveRefPose_DEPRECATED.Num() );
			for(INT TrackIndex = 0; TrackIndex < AdditiveBasePose.Num(); ++TrackIndex)
			{
				FRawAnimSequenceTrack& BasePoseTrack = AdditiveBasePose(TrackIndex);
				BasePoseTrack.PosKeys.AddItem( AdditiveRefPose_DEPRECATED(TrackIndex).GetTranslation() );
				BasePoseTrack.RotKeys.AddItem( AdditiveRefPose_DEPRECATED(TrackIndex).GetRotation() );
			}

			AdditiveRefPose_DEPRECATED.Empty();
			bMarkDirty = TRUE;
		}
	}

	// Fix bad additive base pose data
	if( bIsAdditive && GIsEditor && GetLinkerVersion() < VER_FIXED_BAD_ADDITIVE_DATA )
	{
		UAnimSet* AnimSet = GetAnimSet();

		// Make sure AdditiveBasePose matches RawAnimationData length.
		if( AdditiveBasePose.Num() != RawAnimationData.Num() )
		{
			const INT NumAdditiveBasePose = AdditiveBasePose.Num();
			const INT NumRawAnimationData = RawAnimationData.Num();
			debugf(TEXT("Detected bad AdditiveBasePose Length (%d vs %d) for animation %s in AnimSet %s"), AdditiveBasePose.Num(), RawAnimationData.Num(), *SequenceName.ToString(), *AnimSet->GetFName().ToString());
			if( NumAdditiveBasePose > NumRawAnimationData )
			{
				AdditiveBasePose.Remove(NumRawAnimationData, NumAdditiveBasePose - NumRawAnimationData);
				AdditiveBasePose.Shrink();
				check( AdditiveBasePose.Num() == RawAnimationData.Num() );
			}
			else
			{
				FRawAnimSequenceTrack BasePoseTrack;
				BasePoseTrack.PosKeys.AddItem( FVector::ZeroVector );
				BasePoseTrack.RotKeys.AddItem( FQuat::Identity );

				for(INT i=0; i<(NumRawAnimationData-NumAdditiveBasePose); i++)
				{
					AdditiveBasePose.AddItem(BasePoseTrack);
				}
				check( AdditiveBasePose.Num() == RawAnimationData.Num() );
			}

			bMarkDirty = TRUE;
		}
	}

	// If RAW animation data exists, and needs to be recompressed, do so.
	if( GIsEditor && GetLinkerVersion() < VER_FIXED_MALFORMED_RAW_ANIM_DATA && RawAnimationData.Num() > 0 )
	{
		// Recompress Raw Animation data w/ lossless compression.
		// If some keys have been removed, then recompress animsequence with its original compression algorithm
		CompressRawAnimData();
	}

	// Convert AnimSequences BoneControlModifiers to AnimMetadata system
	if( GetLinkerVersion() < VER_ADDED_ANIM_METADATA_FIXED_QUATERROR && BoneControlModifiers_DEPRECATED.Num() > 0 )
	{
		for(INT ModifierIndex=0; ModifierIndex<BoneControlModifiers_DEPRECATED.Num(); ModifierIndex++)
		{
			FSkelControlModifier& Modifier = BoneControlModifiers_DEPRECATED(ModifierIndex);
			if( Modifier.Modifiers.Num() > 0 )
			{
				UAnimMetaData_SkelControlKeyFrame* MetadataObject = ConstructObject<UAnimMetaData_SkelControlKeyFrame>(UAnimMetaData_SkelControlKeyFrame::StaticClass(), this);
				this->MetaData.AddItem(MetadataObject);

				MetadataObject->SkelControlNameList.AddItem(Modifier.SkelControlName);
				MetadataObject->KeyFrames = Modifier.Modifiers;

				bMarkDirty = TRUE;
				warnf( TEXT("Converted BoneControlModifier to AnimMetaData_SkelControlKeyFrame (%s) for sequence %s (%s)"), *Modifier.SkelControlName.ToString(), *SequenceName.ToString(), (GetOuter() ? *GetOuter()->GetFullName() : *GetFullName()) );
			}
		}
	}

	// Fix up 'Bake and Prune' animations where their num frames doesn't match NumKeys.
#if !CONSOLE && !FINAL_RELEASE
	if( bIsAdditive && GetLinkerVersion() < VER_FIX_BAKEANDPRUNE_NUMFRAMES )
	{
		for(INT i=0; i<RawAnimationData.Num(); i++)
		{
			const INT NumPosKeys = RawAnimationData(i).PosKeys.Num();
			const INT NumRotKeys = RawAnimationData(i).RotKeys.Num();

			if( NumPosKeys > 1 || NumRotKeys > 1 )
			{
				if( (NumPosKeys != 1 && NumPosKeys != NumFrames) || (NumRotKeys != 1 && NumRotKeys != NumFrames) )
				{
					UPackage* AnimSeqPackage = GetOutermost();
					warnf(TEXT("Found animation (%s, %s, %s) where NumFrames doesn't match NumKeys!! NumFrames: %i, NumPosKeys: %i, NumRotKeys: %i"), 
						*SequenceName.ToString(), *GetAnimSet()->GetFName().ToString(), *AnimSeqPackage->GetFName().ToString(), NumFrames, NumPosKeys, NumRotKeys);

					// If saved into a level, then we know it was baked and pruned, so we just fix up NumFrames in that case
					if( AnimSeqPackage->ContainsMap() )
					{
						NumFrames = (NumPosKeys >= NumRotKeys) ? NumPosKeys : NumRotKeys;
						warnf(TEXT("Was detected as a Bake and Prune error, fixing up! NumFrames: %d"), NumFrames);
						bMarkDirty = TRUE;
					}
				}
				break;
			}
		}
	}
#endif

	// Fix up any bad notifies...
	for (INT NotifyIdx = 0; NotifyIdx < Notifies.Num(); NotifyIdx++)
	{
		FAnimNotifyEvent& CurrNotify = Notifies(NotifyIdx);
		if (CurrNotify.Notify != NULL)
		{
			if (CurrNotify.Notify->GetOuter() != this)
			{
				warnf(NAME_Error, TEXT("*** Found bad AnimNotify in %s : %s"), 
					*(GetPathName()), *(CurrNotify.Notify->GetFullName()));
				CurrNotify.Notify->Rename(NULL, this, REN_ForceNoResetLoaders|REN_DoNotDirty);
			}
		}
	}

	// Ensure notifies are sorted.
	SortNotifies();

	// Removing bad animations which contain no data from sets.
	if( GetLinkerVersion() < VER_REMOVE_BAD_ANIMSEQ 
		&& RawAnimationData.Num() == 0 
		&& CompressedTrackOffsets.Num() == 0 )
	{
#if !CONSOLE
		warnf( TEXT("Removing bad AnimSequence (%s) from %s."), *SequenceName.ToString(), (GetOuter() ? *GetOuter()->GetFullName() : *GetFullName()) );
		UAnimSet* ParentSet = GetAnimSet();
		check( ParentSet );

		// Mark package dirty for resave
		bMarkDirty = TRUE;
#endif
	}
	// No animation data is found. Warn - this should check before we check CompressedTrackOffsets size
	// Otherwise, we'll see empty data set crashing game due to no CompressedTrackOffsets
	// You can't check RawAnimationData size since it gets removed during cooking
	else if ( NumFrames == 0 )
	{
		warnf( TEXT("No animation data exists for sequence %s (%s)"), *SequenceName.ToString(), (GetOuter() ? *GetOuter()->GetFullName() : *GetFullName()) );
#if !CONSOLE
		if( GIsEditor )
		{
			warnf( TEXT("Removing bad AnimSequence (%s) from %s."), *SequenceName.ToString(), (GetOuter() ? *GetOuter()->GetFullName() : *GetFullName()) );
			UAnimSet* ParentSet = GetAnimSet();
			ParentSet->RemoveAnimSequenceFromAnimSet(this);

			// Mark package dirty for resave
			bMarkDirty = TRUE;
		}
#endif
	}
	// Raw data exists, but missing compress animation data
	else if( CompressedTrackOffsets.Num() == 0 )
	{
#if CONSOLE
		// Never compress on consoles.
		appErrorf( TEXT("No animation compression exists for sequence %s (%s)"), *SequenceName.ToString(), (GetOuter() ? *GetOuter()->GetFullName() : *GetFullName()) );
#else
		warnf( TEXT("No animation compression exists for sequence %s (%s)"), *SequenceName.ToString(), (GetOuter() ? *GetOuter()->GetFullName() : *GetFullName()) );
		// No animation compression, recompress using default settings.
		FAnimationUtils::CompressAnimSequence(this, NULL, FALSE, FALSE);
#endif // CONSOLE
	}

	static UBOOL ForcedRecompressionSetting = FAnimationUtils::GetForcedRecompressionSetting();

	// Recompress the animation if it was encoded with an old package set
	// or we are being forced to do so
	if (EncodingPkgVersion != CURRENT_ANIMATION_ENCODING_PACKAGE_VERSION ||
		ForcedRecompressionSetting)
	{
#if CONSOLE
		if (EncodingPkgVersion != CURRENT_ANIMATION_ENCODING_PACKAGE_VERSION)
		{
			// Never compress on consoles.
			appErrorf( TEXT("Animation compression method out of date for sequence %s"), *SequenceName.ToString() );
			CompressedTrackOffsets.Empty(0);
			CompressedByteStream.Empty(0);
		}
#else
		FAnimationUtils::CompressAnimSequence(this, NULL, TRUE, FALSE);
#endif // CONSOLE
	}

	// If we're in the game and compressed animation data exists, whack the raw data.
	if( GIsGame && !GIsEditor )
	{
		if( RawAnimationData.Num() > 0  && CompressedTrackOffsets.Num() > 0 )
		{
#if CONSOLE
			// Don't do this on consoles; raw animation data should have been stripped during cook!
			appErrorf( TEXT("Cooker did not strip raw animation from sequence %s"), *SequenceName.ToString() );
#else
			// Remove raw animation data.
			for ( INT TrackIndex = 0 ; TrackIndex < RawAnimationData.Num() ; ++TrackIndex )
			{
				FRawAnimSequenceTrack& RawTrack = RawAnimationData(TrackIndex);
				RawTrack.PosKeys.Empty();
				RawTrack.RotKeys.Empty();
			}
			
			RawAnimationData.Empty();
#endif // CONSOLE
		}

		// Remove raw animation data for additive base pose.
		if( AdditiveBasePose.Num() > 0 )
		{
			for(INT TrackIndex = 0; TrackIndex < AdditiveBasePose.Num(); ++TrackIndex)
			{
				FRawAnimSequenceTrack& BasePoseTrack = AdditiveBasePose(TrackIndex);
				BasePoseTrack.PosKeys.Empty();
				BasePoseTrack.RotKeys.Empty();
			}
			AdditiveBasePose.Empty();
		}
	}

#if WITH_EDITORONLY_DATA
	// swap out the deprecated revert to raw compression scheme with a least destructive compression scheme
	if (GIsEditor && GetAnimSet() && CompressionScheme && CompressionScheme->IsA(UDEPRECATED_AnimationCompressionAlgorithm_RevertToRaw::StaticClass()))
	{
		UAnimSet* AnimSet = GetAnimSet();
		warnf(TEXT("AnimSequence %s (%s) uses the deprecated revert to RAW compression scheme. Using least destructive compression scheme instead"), *GetName(), *AnimSet->GetFullName());
		USkeletalMesh* DefaultSkeletalMesh = LoadObject<USkeletalMesh>(NULL, *AnimSet->BestRatioSkelMeshName.ToString(), NULL, LOAD_None, NULL);
		UAnimationCompressionAlgorithm* NewAlgorithm = ConstructObject<UAnimationCompressionAlgorithm>( UAnimationCompressionAlgorithm_LeastDestructive::StaticClass() );
		NewAlgorithm->Reduce(this, DefaultSkeletalMesh, FALSE);
	}

	// assume that the sequence was last compressed w/ the AnimSets current bAnimRotationOnly value
	if (GIsEditor && GetAnimSet() && (GetLinkerVersion() < VER_ANIM_SEQ_TRANSLATION_STATE))
	{
		bWasCompressedWithoutTranslations = GetAnimSet()->bAnimRotationOnly;
	}
#endif // WITH_EDITORONLY_DATA

	// setup the Codec interfaces
	AnimationFormat_SetInterfaceLinks(*this);

	// verify if it has valid curve keys. If not remove it. We don't have to save it. 
	for (INT I=0; I<CurveData.Num(); ++I)
	{
		// if not valid curve track remove this
		if (CurveData(I).IsValidCurveTrack() == FALSE)
		{
			// remove the item, no reason to have it
			CurveData.Remove(I);
			bMarkDirty = TRUE;
			--I;
		}
		else
		{
			bMarkDirty = CurveData(I).CompressCurveWeights() || bMarkDirty;
		}
	}

	if( bMarkDirty && (GIsRunning || GIsUCC) )
	{
		MarkPackageDirty();
	}

	if( GIsGame && !GIsEditor )
	{
		// this probably will not show newly created animations in PIE but will show them in the game once they have been saved off
		INC_DWORD_STAT_BY( STAT_AnimationMemory, GetResourceSize() );
	}
}

void UAnimSequence::BeginDestroy()
{
	Super::BeginDestroy();

	// clear any active codec links
	RotationCodec = NULL;
	TranslationCodec = NULL;

	if( GIsGame && !GIsEditor )
	{
		DEC_DWORD_STAT_BY( STAT_AnimationMemory, GetResourceSize() );
	}
}

void UAnimSequence::FixUpBadAnimNotifiers()
{
	// iterate over all animnotifiers
	// if any animnotifier outer != current animsequence
	// then add to map
	for (INT I=0; I<Notifies.Num(); ++I)
	{
		if (Notifies(I).Notify)
		{
			if ( Notifies(I).Notify->GetOuter()!=this )
			{
				// fix animnotifiers
				Notifies(I).Notify = CastChecked<UAnimNotify>( UObject::StaticConstructObject(Notifies(I).Notify->GetClass(), this,NAME_None,0,Notifies(I).Notify) );
				MarkPackageDirty();
			}
			if (Notifies(I).Notify->GetArchetype()!=Notifies(I).Notify->GetClass()->GetDefaultObject())
			{
				Notifies(I).Notify->SetArchetype(Notifies(I).Notify->GetClass()->GetDefaultObject());
				MarkPackageDirty();
			}
		}
	}
}

void UAnimSequence::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if(!IsTemplate())
	{
		FixUpBadAnimNotifiers();
		// Make sure package is marked dirty when doing stuff like adding/removing notifies
		MarkPackageDirty();
	}
}

// @todo DB: Optimize!
template<typename TimeArray>
static INT FindKeyIndex(FLOAT Time, const TimeArray& Times)
{
	INT FoundIndex = 0;
	for ( INT Index = 0 ; Index < Times.Num() ; ++Index )
	{
		const FLOAT KeyTime = Times(Index);
		if ( Time >= KeyTime )
		{
			FoundIndex = Index;
		}
		else
		{
			break;
		}
	}
	return FoundIndex;
}



/**
 * Populates the key reduced arrays from raw animation data.
 */
void UAnimSequence::SeparateRawDataToTracks(const TArray<FRawAnimSequenceTrack>& RawAnimData,
											FLOAT SequenceLength,
											TArray<FTranslationTrack>& OutTranslationData,
											TArray<FRotationTrack>& OutRotationData)
{
	// functionality moved to UAnimationCompressionAlgorithm for general use by Codecs
}

/**
* Interpolate curve weights of the Time in this sequence if curve data exists
*
* @param	Time			Time on track to interpolate to.
* @param	bLooping		TRUE if the animation is looping.
* @param	CurveKeys		Add the curve keys if exists
*/
void UAnimSequence::GetCurveData(FLOAT Time, UBOOL bLooping, FCurveKeyArray& CurveKeys) const
{
	// this code is almost replica of GetBoneAtom to get interpolated time to keep consistency between anim and curve
	if ( CurveData.Num() > 0 )
	{
		// This assumes that all keys are equally spaced (ie. won't work if we have dropped unimportant frames etc).
		// by default, for looping animation, the last frame has a duration, and interpolates back to the first one.
		const INT NumKeys = bLooping ? NumFrames : NumFrames - 1;
		const FLOAT KeyPos = ((FLOAT)NumKeys * Time) / SequenceLength;

		// Find the integer part (ensuring within range) and that gives us the 'starting' key index.
		const INT KeyIndex1 = Clamp<INT>( appFloor(KeyPos), 0, NumFrames-1 );  // @todo should be changed to appTrunc

		// The alpha (fractional part) is then just the remainder.
		const FLOAT Alpha = (KeyPos - (FLOAT)KeyIndex1);

		INT KeyIndex2 = KeyIndex1 + 1;

		// If we have gone over the end, do different things in case of looping
		if( KeyIndex2 == NumFrames )
		{
			// If looping, interpolate between last and first frame
			if( bLooping )
			{
				KeyIndex2 = 0;
			}
			// If not looping - hold the last frame.
			else
			{
				KeyIndex2 = KeyIndex1;
			}
		}

		const INT PosKeyIndex1 = ::Min(KeyIndex1, NumFrames-1);
		const INT PosKeyIndex2 = ::Min(KeyIndex2, NumFrames-1);

		INT StartIdx=CurveKeys.Num();
		INT EndIdx=StartIdx + CurveData.Num();

		CurveKeys.AddZeroed(CurveData.Num());

		// go through every curve
		// current I don't cull here - even when weight is 0, it all adds up.
		// the reason is that I purge after collecting all to keep the consistency 
		for (INT I=StartIdx; I<EndIdx; ++I)
		{
			if ( NumFrames == CurveData(I-StartIdx).CurveWeights.Num() )
			{
				CurveKeys(I).CurveName = CurveData(I-StartIdx).CurveName;
				CurveKeys(I).Weight = CurveData(I-StartIdx).CurveWeights(PosKeyIndex1) + (CurveData(I-StartIdx).CurveWeights(PosKeyIndex2)-CurveData(I-StartIdx).CurveWeights(PosKeyIndex1))*(Alpha);
			}
			// if only 1 key, that means there is only one value
			else if ( 1 == CurveData(I-StartIdx).CurveWeights.Num() )
			{
				CurveKeys(I).CurveName = CurveData(I-StartIdx).CurveName;
				CurveKeys(I).Weight = CurveData(I-StartIdx).CurveWeights(0);
			}
			else
			{
				checkMsg( 1, *FString::Printf(TEXT("Curve Data is corrupted : [Sequence (%s): Curve Name (%s)]"), *SequenceName.GetNameString(), *CurveData(I-StartIdx).CurveName.GetNameString() ) );
			}
//			debugf(TEXT("SequenceName: %s Weight is (%s: %0.5f) for Time %0.2f"), *SequenceName.GetNameString(), *CurveKeys(I).CurveName.GetNameString(), CurveKeys(I).Weight, Time);
		}
	}
}

/**
 * Interpolate keyframes in this sequence to find the bone transform (relative to parent).
 * 
 * @param	OutAtom			[out] Output bone transform.
 * @param	TrackIndex		Index of track to interpolate.
 * @param	Time			Time on track to interpolate to.
 * @param	bLooping		TRUE if the animation is looping.
 * @param	bUseRawData		If TRUE, use raw animation data instead of compressed data.
 * @param	CurveKeys		GetCurveKeys if exists
 */
void UAnimSequence::GetBoneAtom(FBoneAtom& OutAtom, INT TrackIndex, FLOAT Time, UBOOL bLooping, UBOOL bUseRawData, FCurveKeyArray* CurveKeys) const
{
	// If the caller didn't request that raw animation data be used . . .
	if ( !bUseRawData )
	{
		if ( CompressedTrackOffsets.Num() > 0 )
		{
			AnimationFormat_GetBoneAtom( OutAtom, *this, TrackIndex, Time, bLooping );
			if ( CurveKeys != NULL && CurveData.Num() >  0 )
			{
				GetCurveData(Time, bLooping, *CurveKeys);
			}

			return;
		}
	}

	//const FScopedTimer Timer( TEXT("Raw") );
	OutAtom.SetScale(1.f);

	// Bail out if the animation data doesn't exists (e.g. was stripped by the cooker).
	if ( RawAnimationData.Num() == 0 )
	{
		debugf( NAME_DevAnim, TEXT("UAnimSequence::GetBoneAtom : No anim data in AnimSequence!") );
		OutAtom.SetIdentity();
		return;
	}

	const FRawAnimSequenceTrack& RawTrack = RawAnimationData(TrackIndex);

	// Bail out (with rather wacky data) if data is empty for some reason.
	if( RawTrack.PosKeys.Num() == 0 || RawTrack.RotKeys.Num() == 0 )
	{
		debugf( NAME_DevAnim, TEXT("UAnimSequence::GetBoneAtom : No anim data in AnimSequence!") );
		OutAtom.SetIdentity();
		return;
	}

   	// Check for 1-frame, before-first-frame and after-last-frame cases.
	if( Time <= 0.f || NumFrames == 1 )
	{
		OutAtom.SetTranslation(RawTrack.PosKeys(0));
		OutAtom.SetRotation(RawTrack.RotKeys(0));

		if( CurveKeys != NULL && CurveData.Num() >  0 )
		{
			GetCurveData(0.0f, FALSE, *CurveKeys);
		}
		return;
	}

	const INT LastIndex		= NumFrames - 1;
	const INT LastPosIndex	= ::Min(LastIndex, RawTrack.PosKeys.Num()-1);
	const INT LastRotIndex	= ::Min(LastIndex, RawTrack.RotKeys.Num()-1);
	if( Time >= SequenceLength )
	{
		// If we're not looping, key n-1 is the final key.
		// If we're looping, key 0 is the final key.
		OutAtom.SetTranslation( RawTrack.PosKeys( bLooping ? 0 : LastPosIndex ) );
		OutAtom.SetRotation( RawTrack.RotKeys( bLooping ? 0 : LastRotIndex ) );
		return;
	}

	// This assumes that all keys are equally spaced (ie. won't work if we have dropped unimportant frames etc).
	// by default, for looping animation, the last frame has a duration, and interpolates back to the first one.
	const INT NumKeys = bLooping ? NumFrames : NumFrames - 1;
	const FLOAT KeyPos = ((FLOAT)NumKeys * Time) / SequenceLength;

// 	debugf(TEXT(" *  *  *  GetBoneAtom. Time: %f, KeyPos: %f"), Time, KeyPos);

	// Find the integer part (ensuring within range) and that gives us the 'starting' key index.
	const INT KeyIndex1 = Clamp<INT>( appFloor(KeyPos), 0, NumFrames-1 );  // @todo should be changed to appTrunc

	// The alpha (fractional part) is then just the remainder.
	const FLOAT Alpha = KeyPos - (FLOAT)KeyIndex1;

	INT KeyIndex2 = KeyIndex1 + 1;

	// If we have gone over the end, do different things in case of looping
	if( KeyIndex2 == NumFrames )
	{
		// If looping, interpolate between last and first frame
		if( bLooping )
		{
			KeyIndex2 = 0;
		}
		// If not looping - hold the last frame.
		else
		{
			KeyIndex2 = KeyIndex1;
		}
	}

	const INT PosKeyIndex1 = ::Min(KeyIndex1, RawTrack.PosKeys.Num()-1);
	const INT RotKeyIndex1 = ::Min(KeyIndex1, RawTrack.RotKeys.Num()-1);
	const INT PosKeyIndex2 = ::Min(KeyIndex2, RawTrack.PosKeys.Num()-1);
	const INT RotKeyIndex2 = ::Min(KeyIndex2, RawTrack.RotKeys.Num()-1);

// 	debugf(TEXT(" *  *  *  Position. PosKeyIndex1: %3d, PosKeyIndex2: %3d, Alpha: %f"), PosKeyIndex1, PosKeyIndex2, Alpha);
// 	debugf(TEXT(" *  *  *  Rotation. RotKeyIndex1: %3d, RotKeyIndex2: %3d, Alpha: %f"), RotKeyIndex1, RotKeyIndex2, Alpha);
	OutAtom.SetTranslation( Lerp(RawTrack.PosKeys(PosKeyIndex1), RawTrack.PosKeys(PosKeyIndex2), Alpha) );

#if !USE_SLERP
	// Fast linear quaternion interpolation.
	// To ensure the 'shortest route', we make sure the dot product between the two keys is positive.
	if( (RawTrack.RotKeys(RotKeyIndex1) | RawTrack.RotKeys(RotKeyIndex2)) < 0.f )
	{
		// To clarify the code here: a slight optimization of inverting the parametric variable as opposed to the quaternion.
		OutAtom.SetRotation(  (RawTrack.RotKeys(RotKeyIndex1) * (1.f-Alpha)) + (RawTrack.RotKeys(RotKeyIndex2) * -Alpha) );
	}
	else
	{
		OutAtom.SetRotation(  (RawTrack.RotKeys(RotKeyIndex1) * (1.f-Alpha)) + (RawTrack.RotKeys(RotKeyIndex2) * Alpha) );
	}
#else
	OutAtom.SetRotation( SlerpQuat( RawTrack.RotKeys(RotKeyIndex1), RawTrack.RotKeys(RotKeyIndex2), Alpha ) );
#endif
	OutAtom.NormalizeRotation();
	
	// get curve keys if any
	if ( CurveKeys != NULL && CurveData.Num() >  0 )
	{
		GetCurveData(Time, bLooping, *CurveKeys);
	}
}

/**
 * Interpolate keyframes in this sequence to find the bone transform (relative to parent).
 * This returns the base pose used to create the additive animation.
 *
 * @param	OutAtom			[out] Output bone transform.
 * @param	TrackIndex		Index of track to interpolate.
 * @param	Time			Time on track to interpolate to.
 * @param	bLooping		TRUE if the animation is looping.
 */
void UAnimSequence::GetAdditiveBasePoseBoneAtom(FBoneAtom& OutAtom, INT TrackIndex, FLOAT Time, UBOOL bLooping) const
{
	// Make sure that if we call this, the animation is additive.
	if( !bIsAdditive )
	{
		OutAtom.SetIdentity();
		return;
	}

	OutAtom.SetScale(1.f);

	// Bail out if the animation data doesn't exists (e.g. was stripped by the cooker).
	if( AdditiveBasePose.Num() == 0 )
	{
		debugf( TEXT("UAnimSequence::GetAdditiveBasePoseBoneAtom : No anim data in AnimSequence!") );
		OutAtom.SetIdentity();
		return;
	}

	const FRawAnimSequenceTrack& RawTrack = RawAnimationData(TrackIndex);
	const FRawAnimSequenceTrack& BasePoseTrack = AdditiveBasePose(TrackIndex);

	// Bail out (with rather wacky data) if data is empty for some reason.
	if( BasePoseTrack.PosKeys.Num() == 0 ||
		BasePoseTrack.RotKeys.Num() == 0 )
	{
		debugf( TEXT("UAnimSequence::GetAdditiveBasePoseBoneAtom : No anim data in AnimSequence!") );
		OutAtom.SetIdentity();
		return;
	}

   	// Check for 1-frame, before-first-frame and after-last-frame cases.
	if( Time <= 0.f || NumFrames == 1 )
	{
		OutAtom.SetRotation( BasePoseTrack.RotKeys(0) );
		OutAtom.SetTranslation( BasePoseTrack.PosKeys(0) );
		return;
	}

	const INT LastIndex		= NumFrames - 1;
	const INT LastPosIndex	= ::Min(LastIndex, BasePoseTrack.PosKeys.Num()-1);
	const INT LastRotIndex	= ::Min(LastIndex, BasePoseTrack.RotKeys.Num()-1);
	if( Time >= SequenceLength )
	{
		// If we're not looping, key n-1 is the final key.
		// If we're looping, key 0 is the final key.
		OutAtom.SetRotation( BasePoseTrack.RotKeys( bLooping ? 0 : LastRotIndex ) );
		OutAtom.SetTranslation( BasePoseTrack.PosKeys( bLooping ? 0 : LastPosIndex ) );
		return;
	}

	// This assumes that all keys are equally spaced (ie. won't work if we have dropped unimportant frames etc).
	// by default, for looping animation, the last frame has a duration, and interpolates back to the first one.
	const INT NumKeys = bLooping ? NumFrames : NumFrames - 1;
	const FLOAT KeyPos = ((FLOAT)NumKeys * Time) / SequenceLength;

	// Find the integer part (ensuring within range) and that gives us the 'starting' key index.
	const INT KeyIndex1 = Clamp<INT>( appFloor(KeyPos), 0, NumFrames-1 );  // @todo should be changed to appTrunc

	// The alpha (fractional part) is then just the remainder.
	const FLOAT Alpha = KeyPos - (FLOAT)KeyIndex1;

	INT KeyIndex2 = KeyIndex1 + 1;

	// If we have gone over the end, do different things in case of looping
	if( KeyIndex2 == NumFrames )
	{
		// If looping, interpolate between last and first frame
		if( bLooping )
		{
			KeyIndex2 = 0;
		}
		// If not looping - hold the last frame.
		else
		{
			KeyIndex2 = KeyIndex1;
		}
	}

	const INT PosKeyIndex1 = ::Min(KeyIndex1, BasePoseTrack.PosKeys.Num()-1);
	const INT RotKeyIndex1 = ::Min(KeyIndex1, BasePoseTrack.RotKeys.Num()-1);
	const INT PosKeyIndex2 = ::Min(KeyIndex2, BasePoseTrack.PosKeys.Num()-1);
	const INT RotKeyIndex2 = ::Min(KeyIndex2, BasePoseTrack.RotKeys.Num()-1);

	OutAtom.SetTranslation(Lerp(BasePoseTrack.PosKeys(PosKeyIndex1), BasePoseTrack.PosKeys(PosKeyIndex2), Alpha));

#if !USE_SLERP
	// Fast linear quaternion interpolation.
	// To ensure the 'shortest route', we make sure the dot product between the two keys is positive.
	if( (BasePoseTrack.RotKeys(RotKeyIndex1) | BasePoseTrack.RotKeys(RotKeyIndex2)) < 0.f )
	{
		// To clarify the code here: a slight optimization of inverting the parametric variable as opposed to the quaternion.
		OutAtom.SetRotation( (BasePoseTrack.RotKeys(RotKeyIndex1) * (1.f-Alpha)) + (BasePoseTrack.RotKeys(RotKeyIndex2) * -Alpha) );
	}
	else
	{
		OutAtom.SetRotation( (BasePoseTrack.RotKeys(RotKeyIndex1) * (1.f-Alpha)) + (BasePoseTrack.RotKeys(RotKeyIndex2) * Alpha) );
	}
#else
	OutAtom.SetRotation( SlerpQuat( BasePoseTrack.RotKeys(RotKeyIndex1), BasePoseTrack.RotKeys(RotKeyIndex2), Alpha ) );
#endif
	OutAtom.NormalizeRotation();
}

IMPLEMENT_COMPARE_CONSTREF( FAnimNotifyEvent, UnSkeletalAnim, 
{
	if		(A.Time > B.Time)	return 1;
	else if	(A.Time < B.Time)	return -1;
	else						return 0;
} 
)

/**
 * Sort the Notifies array by time, earliest first.
 */
void UAnimSequence::SortNotifies()
{
	Sort<USE_COMPARE_CONSTREF(FAnimNotifyEvent,UnSkeletalAnim)>(&Notifies(0),Notifies.Num());
}

/**
 * @return		A reference to the AnimSet this sequence belongs to.
 */
UAnimSet* UAnimSequence::GetAnimSet() const
{
	return CastChecked<UAnimSet>( GetOuter() );
}

/** Utility function to crop data from a RawAnimSequenceTrack */
static INT CropRawTrack(FRawAnimSequenceTrack& RawTrack, INT StartKey, INT NumKeys, INT TotalNumOfFrames)
{
	check(RawTrack.PosKeys.Num() == 1 || RawTrack.PosKeys.Num() == TotalNumOfFrames);
	check(RawTrack.RotKeys.Num() == 1 || RawTrack.RotKeys.Num() == TotalNumOfFrames);

	if( RawTrack.PosKeys.Num() > 1 )
	{
		RawTrack.PosKeys.Remove(StartKey, NumKeys);
		check(RawTrack.PosKeys.Num() > 0);
		RawTrack.PosKeys.Shrink();
	}

	if( RawTrack.RotKeys.Num() > 1 )
	{
		RawTrack.RotKeys.Remove(StartKey, NumKeys);
		check(RawTrack.RotKeys.Num() > 0);
		RawTrack.RotKeys.Shrink();
	}

	// Update NumFrames below to reflect actual number of keys.
	return Max<INT>( RawTrack.PosKeys.Num(), RawTrack.RotKeys.Num() );
}

/**
 * Crops the raw anim data either from Start to CurrentTime or CurrentTime to End depending on 
 * value of bFromStart.  Can't be called against cooked data.
 * @Note: Animation must be recompressed after this. As this only affects Raw data.
 *
 * @param	CurrentTime		marker for cropping (either beginning or end)
 * @param	bFromStart		whether marker is begin or end marker
 * @return					TRUE if the operation was successful.
 */
UBOOL UAnimSequence::CropRawAnimData( FLOAT CurrentTime, UBOOL bFromStart )
{
	if (GIsCooking)
	{
		if (HasAnyFlags(RF_MarkedByCooker))
		{
			return FALSE;
		}
	}
	else
	{
		// Can't crop cooked animations.
		const UPackage* Package = GetOutermost();
		if( Package->PackageFlags & PKG_Cooked )
		{
			return FALSE;
		}
	}

	// Length of one frame.
	FLOAT const FrameTime = SequenceLength / ((FLOAT)NumFrames);
	// Save Total Number of Frames before crop
	INT TotalNumOfFrames = NumFrames;

	// if current frame is 1, do not try crop. There is nothing to crop
	if ( NumFrames <= 1 )
	{
		return FALSE;
	}
	
	// If you're end or beginning, you can't cut all nor nothing. 
	// Avoiding ambiguous situation what exactly we would like to cut 
	// Below it clamps range to 1, TotalNumOfFrames-1
	// causing if you were in below position, it will still crop 1 frame. 
	// To be clearer, it seems better if we reject those inputs. 
	// If you're a bit before/after, we assume that you'd like to crop
	if ( CurrentTime == 0.f || CurrentTime == SequenceLength )
	{
		return FALSE;
	}

	// Find the right key to cut at.
	// This assumes that all keys are equally spaced (ie. won't work if we have dropped unimportant frames etc).
	// The reason I'm changing to TotalNumOfFrames is CT/SL = KeyIndexWithFraction/TotalNumOfFrames
	// To play TotalNumOfFrames, it takes SequenceLength. Each key will take SequenceLength/TotalNumOfFrames
	FLOAT const KeyIndexWithFraction = (CurrentTime * (FLOAT)(TotalNumOfFrames)) / SequenceLength;
	INT KeyIndex = bFromStart ? appFloor(KeyIndexWithFraction) : appCeil(KeyIndexWithFraction);
	// Ensure KeyIndex is in range.
	KeyIndex = Clamp<INT>(KeyIndex, 1, TotalNumOfFrames-1); 
	// determine which keys need to be removed.
	INT const StartKey = bFromStart ? 0 : KeyIndex;
	INT const NumKeys = bFromStart ? KeyIndex : TotalNumOfFrames - KeyIndex ;

	// Recalculate NumFrames
	NumFrames = TotalNumOfFrames - NumKeys;

	debugf(TEXT("UAnimSequence::CropRawAnimData %s - CurrentTime: %f, bFromStart: %d, TotalNumOfFrames: %d, KeyIndex: %d, StartKey: %d, NumKeys: %d"), *SequenceName.ToString(), CurrentTime, bFromStart, TotalNumOfFrames, KeyIndex, StartKey, NumKeys);

	// Iterate over tracks removing keys from each one.
	for(INT i=0; i<RawAnimationData.Num(); i++)
	{
		// Update NumFrames below to reflect actual number of keys while we crop the anim data
		CropRawTrack(RawAnimationData(i), StartKey, NumKeys, TotalNumOfFrames);

		// Do the same with additive data.
		if( bIsAdditive )
		{
			CropRawTrack(AdditiveBasePose(i), StartKey, NumKeys, TotalNumOfFrames);
		}
	}

	// Double check that everything is fine
	for(INT i=0; i<RawAnimationData.Num(); i++)
	{
		FRawAnimSequenceTrack& RawTrack = RawAnimationData(i);
		check(RawTrack.PosKeys.Num() == 1 || RawTrack.PosKeys.Num() == NumFrames);
		check(RawTrack.RotKeys.Num() == 1 || RawTrack.RotKeys.Num() == NumFrames);

		// Do the same with additive data.
		if( bIsAdditive )
		{
			FRawAnimSequenceTrack& BasePoseRawTrack = AdditiveBasePose(i);
			check(BasePoseRawTrack.PosKeys.Num() == 1 || BasePoseRawTrack.PosKeys.Num() == NumFrames);
			check(BasePoseRawTrack.RotKeys.Num() == 1 || BasePoseRawTrack.RotKeys.Num() == NumFrames);
		}
	}

	// Crop curve data 
	for( INT CurveTrackIdx = 0; CurveTrackIdx < CurveData.Num(); ++CurveTrackIdx )
	{
		if( CurveData(CurveTrackIdx).CurveWeights.Num() > 1 )
		{
			CurveData(CurveTrackIdx).CurveWeights.Remove(StartKey, NumKeys);
			CurveData(CurveTrackIdx).CompressCurveWeights();

			check(CurveData(CurveTrackIdx).CurveWeights.Num() == 1 || CurveData(CurveTrackIdx).CurveWeights.Num() == NumFrames );
		}
	}

	// Update sequence length to match new number of frames.
	SequenceLength = (FLOAT)NumFrames * FrameTime;

	debugf(TEXT("\tSequenceLength: %f, NumFrames: %d"), SequenceLength, NumFrames);

	MarkPackageDirty();
	return TRUE;
}

/** Utility function to losslessly compress a FRawAnimSequenceTrack */
UBOOL UAnimSequence::CompressRawAnimSequenceTrack(FRawAnimSequenceTrack& RawTrack, FLOAT MaxPosDiff, FLOAT MaxAngleDiff)
{
	UBOOL bRemovedKeys = FALSE;

	// First part is to make sure we have valid input
	UBOOL const bPosTrackIsValid = (RawTrack.PosKeys.Num() == 1 || RawTrack.PosKeys.Num() == NumFrames);
	if( !bPosTrackIsValid )
	{
		warnf(TEXT("Found non valid position track for %s, %d frames, instead of %d. Chopping!"), *SequenceName.ToString(), RawTrack.PosKeys.Num(), NumFrames);
		bRemovedKeys = TRUE;
		RawTrack.PosKeys.Remove(1, RawTrack.PosKeys.Num()- 1);
		RawTrack.PosKeys.Shrink();
		check( RawTrack.PosKeys.Num() == 1);
	}

	UBOOL const bRotTrackIsValid = (RawTrack.RotKeys.Num() == 1 || RawTrack.RotKeys.Num() == NumFrames);
	if( !bRotTrackIsValid )
	{
		warnf(TEXT("Found non valid rotation track for %s, %d frames, instead of %d. Chopping!"), *SequenceName.ToString(), RawTrack.RotKeys.Num(), NumFrames);
		bRemovedKeys = TRUE;
		RawTrack.RotKeys.Remove(1, RawTrack.RotKeys.Num()- 1);
		RawTrack.RotKeys.Shrink();
		check( RawTrack.RotKeys.Num() == 1);
	}

	// Second part is actual compression.

	// Check variation of position keys
	if( (RawTrack.PosKeys.Num() > 1) && (MaxPosDiff >= 0.0f) )
	{
		FVector FirstPos = RawTrack.PosKeys(0);
		UBOOL bFramesIdentical = TRUE;
		for(INT j=1; j<RawTrack.PosKeys.Num() && bFramesIdentical; j++)
		{
			if( (FirstPos - RawTrack.PosKeys(j)).Size() > MaxPosDiff )
			{
				bFramesIdentical = FALSE;
			}
		}

		// If all keys are the same, remove all but first frame
		if( bFramesIdentical )
		{
			bRemovedKeys = TRUE;
			RawTrack.PosKeys.Remove(1, RawTrack.PosKeys.Num()- 1);
			RawTrack.PosKeys.Shrink();
			check( RawTrack.PosKeys.Num() == 1);
		}
	}

	// Check variation of rotational keys
	if( (RawTrack.RotKeys.Num() > 1) && (MaxAngleDiff >= 0.0f) )
	{
		FQuat FirstRot = RawTrack.RotKeys(0);
		UBOOL bFramesIdentical = TRUE;
		for(INT j=1; j<RawTrack.RotKeys.Num() && bFramesIdentical; j++)
		{
			if( FQuatError(FirstRot, RawTrack.RotKeys(j)) > MaxAngleDiff )
			{
				bFramesIdentical = FALSE;
			}
		}

		// If all keys are the same, remove all but first frame
		if( bFramesIdentical )
		{
			bRemovedKeys = TRUE;
			RawTrack.RotKeys.Remove(1, RawTrack.RotKeys.Num()- 1);
			RawTrack.RotKeys.Shrink();
			check( RawTrack.RotKeys.Num() == 1);
		}			
	}

	return bRemovedKeys;
}

/**
 * Removes trivial frames -- frames of tracks when position or orientation is constant
 * over the entire animation -- from the raw animation data.  If both position and rotation
 * go down to a single frame, the time is stripped out as well.
 */
UBOOL UAnimSequence::CompressRawAnimData(float MaxPosDiff, float MaxAngleDiff)
{
	// Only bother doing anything if we have some keys!
	if( NumFrames == 1 )
	{
		return FALSE;
	}

	UBOOL bRemovedKeys = FALSE;
	// Raw animation data
	for(INT i=0; i<RawAnimationData.Num(); i++)
	{
		bRemovedKeys = CompressRawAnimSequenceTrack( RawAnimationData(i), MaxPosDiff, MaxAngleDiff ) || bRemovedKeys;
	}

	// Additive base pose
	if( bIsAdditive )
	{
		for(INT i=0; i<AdditiveBasePose.Num(); i++)
		{
			bRemovedKeys = CompressRawAnimSequenceTrack( AdditiveBasePose(i), MaxPosDiff, MaxAngleDiff ) || bRemovedKeys;
		}
	}

	return bRemovedKeys;
}

/**
 * Removes trivial frames -- frames of tracks when position or orientation is constant
 * over the entire animation -- from the raw animation data.  If both position and rotation
 * go down to a single frame, the time is stripped out as well.
 */
UBOOL UAnimSequence::CompressRawAnimData()
{
	const FLOAT MaxPosDiff = 0.0001f;
	const FLOAT MaxAngleDiff = 0.0003f;
	return CompressRawAnimData(MaxPosDiff, MaxAngleDiff);
}

/** Clears any data in the AnimSequence, so it can be recycled when importing a new animation with same name over it. */
void UAnimSequence::RecycleAnimSequence()
{
#if WITH_EDITORONLY_DATA
	// Clear RawAnimData
	RawAnimationData.Empty();

	// Clear additive animation information
	bIsAdditive = FALSE;

	// Remove raw animation data.
	for(INT TrackIndex = 0; TrackIndex < AdditiveBasePose.Num(); ++TrackIndex)
	{
		FRawAnimSequenceTrack& BasePoseTrack = AdditiveBasePose(TrackIndex);
		BasePoseTrack.PosKeys.Empty();
		BasePoseTrack.RotKeys.Empty();
	}
	AdditiveBasePose.Empty();

	// Clear additive animation references, and remove ourselves from the animations referencing us.
	ClearAdditiveAnimReferences();
#endif // WITH_EDITORONLY_DATA
}

/** 
 * Clear references to additive animations.
 * This is the following arrays: AdditiveBasePoseAnimSeq, AdditiveTargetPoseAnimSeq and RelatedAdditiveAnimSeqs.
 * Handles dependencies, and removes us properly.
 */
void UAnimSequence::ClearAdditiveAnimReferences()
{
#if WITH_EDITORONLY_DATA
	// Go through our references for base poses.
	for(INT i=0; i<AdditiveBasePoseAnimSeq.Num(); i++)
	{
		UAnimSequence* AnimSeq = AdditiveBasePoseAnimSeq(i);
		if( AnimSeq )
		{
			if( AnimSeq->RelatedAdditiveAnimSeqs.RemoveItem( this ) != INDEX_NONE && GIsEditor )
			{
				AnimSeq->MarkPackageDirty();
			}
		}
	}
	AdditiveBasePoseAnimSeq.Empty();

	// Go through our references for target poses.
	for(INT i=0; i<AdditiveTargetPoseAnimSeq.Num(); i++)
	{
		UAnimSequence* AnimSeq = AdditiveTargetPoseAnimSeq(i);
		if( AnimSeq )
		{
			if( AnimSeq->RelatedAdditiveAnimSeqs.RemoveItem( this ) != INDEX_NONE && GIsEditor )
			{
				AnimSeq->MarkPackageDirty();
			}
		}
	}
	AdditiveTargetPoseAnimSeq.Empty();

	// Go through our additive animation references
	for(INT i=0; i<RelatedAdditiveAnimSeqs.Num(); i++)
	{
		UAnimSequence* AnimSeq = RelatedAdditiveAnimSeqs(i);
		if( AnimSeq )
		{
			if( AnimSeq->AdditiveBasePoseAnimSeq.RemoveItem( this ) != INDEX_NONE && GIsEditor )
			{
				AnimSeq->MarkPackageDirty();
			}
			if( AnimSeq->AdditiveTargetPoseAnimSeq.RemoveItem( this ) != INDEX_NONE && GIsEditor )
			{
				AnimSeq->MarkPackageDirty();
			}
		}
	}
	RelatedAdditiveAnimSeqs.Empty();

	if( GIsEditor )
	{
		MarkPackageDirty();
	}
#endif // WITH_EDITORONLY_DATA
}

/** 
 * Utility function to copy all UAnimSequence properties from Source to Destination.
 * Does not copy however RawAnimData, CompressedAnimData and AdditiveBasePose.
 */
UBOOL UAnimSequence::CopyAnimSequenceProperties(UAnimSequence* SourceAnimSeq, UAnimSequence* DestAnimSeq, UBOOL bSkipCopyingNotifies)
{
#if WITH_EDITORONLY_DATA
	// Copy parameters
	DestAnimSeq->SequenceName				= SourceAnimSeq->SequenceName;
	DestAnimSeq->SequenceLength				= SourceAnimSeq->SequenceLength;
	DestAnimSeq->NumFrames					= SourceAnimSeq->NumFrames;
	DestAnimSeq->RateScale					= SourceAnimSeq->RateScale;
	DestAnimSeq->bNoLoopingInterpolation	= SourceAnimSeq->bNoLoopingInterpolation;
	DestAnimSeq->bIsAdditive				= SourceAnimSeq->bIsAdditive;
	DestAnimSeq->AdditiveRefName			= SourceAnimSeq->AdditiveRefName;
	DestAnimSeq->bDoNotOverrideCompression	= SourceAnimSeq->bDoNotOverrideCompression;

	// Copy Compression Settings
	DestAnimSeq->CompressionScheme				= static_cast<UAnimationCompressionAlgorithm*>( StaticDuplicateObject( SourceAnimSeq->CompressionScheme, SourceAnimSeq->CompressionScheme, DestAnimSeq, TEXT("None"), ~RF_RootSet ) );
	DestAnimSeq->TranslationCompressionFormat	= SourceAnimSeq->TranslationCompressionFormat;
	DestAnimSeq->RotationCompressionFormat		= SourceAnimSeq->RotationCompressionFormat;

	// Transfer additive animation references

	// Transfer RelatedAdditiveAnimSeqs array
	// if we're in the same package. Otherwise it will create referenced to external package
	// since now we allow moving/copying between packages, we can't copy the additive 
	// if we're not in the same package
	if (DestAnimSeq->GetOutermost()==SourceAnimSeq->GetOutermost())
	{
		DestAnimSeq->RelatedAdditiveAnimSeqs = SourceAnimSeq->RelatedAdditiveAnimSeqs;

		// This is additive animations we have built. We are either a base pose or target pose.
		// Since we created a copy of Source, also add Dest as either another base or target pose for those additive animations.
		for(INT i=0; i<SourceAnimSeq->RelatedAdditiveAnimSeqs.Num(); i++)
		{
			UAnimSequence* AdditiveAnimSeq = SourceAnimSeq->RelatedAdditiveAnimSeqs(i);
			if( AdditiveAnimSeq )
			{
				if( AdditiveAnimSeq->AdditiveBasePoseAnimSeq.FindItemIndex(SourceAnimSeq) != INDEX_NONE  )
				{
					// If Source was used as a Base pose for AdditiveAnimSeq, then also add Dest as well
					AdditiveAnimSeq->AdditiveBasePoseAnimSeq.AddUniqueItem( DestAnimSeq );
					AdditiveAnimSeq->MarkPackageDirty();
				}
				if( AdditiveAnimSeq->AdditiveTargetPoseAnimSeq.FindItemIndex(SourceAnimSeq) != INDEX_NONE )
				{
					// If Source was used as a Target pose for AdditiveAnimSeq, then also add Dest as well
					AdditiveAnimSeq->AdditiveTargetPoseAnimSeq.AddUniqueItem( DestAnimSeq );
					AdditiveAnimSeq->MarkPackageDirty();
				}
			}
		}

		// Transfer AdditiveBasePoseAnimSeq array 
		DestAnimSeq->AdditiveBasePoseAnimSeq = SourceAnimSeq->AdditiveBasePoseAnimSeq;
		
		// If we're an additive animation, this is a list of the base pose animations used to build us.
		// Add us to them, so they know they're built us.
		for(INT i=0; i<SourceAnimSeq->AdditiveBasePoseAnimSeq.Num(); i++)
		{
			UAnimSequence* BaseAnimSeq = SourceAnimSeq->AdditiveBasePoseAnimSeq(i);
			if( BaseAnimSeq )
			{
				BaseAnimSeq->RelatedAdditiveAnimSeqs.AddUniqueItem( DestAnimSeq );
			}
		}

		// Transfer AdditiveTargetPoseAnimSeq array 
		DestAnimSeq->AdditiveTargetPoseAnimSeq = SourceAnimSeq->AdditiveTargetPoseAnimSeq;
		// If we're an additive animation, this is a list of the target pose animations used to build us.
		// Add us to them, so they know they're built us.
		for(INT i=0; i<SourceAnimSeq->AdditiveTargetPoseAnimSeq.Num(); i++)
		{
			UAnimSequence* BaseAnimSeq = SourceAnimSeq->AdditiveTargetPoseAnimSeq(i);
			if( BaseAnimSeq )
			{
				BaseAnimSeq->RelatedAdditiveAnimSeqs.AddUniqueItem( DestAnimSeq );
			}
		}
	}
	else
	{
		debugf(TEXT("Copying properties of additive animation from [%s] to [%s] failed since they're in different packages."), *SourceAnimSeq->GetFullName(), *DestAnimSeq->GetFullName());
	}


	if( !bSkipCopyingNotifies )
	{
		// Copy Metadata information
		CopyMetadata(SourceAnimSeq, DestAnimSeq);
		CopyNotifies(SourceAnimSeq, DestAnimSeq);
	}

	DestAnimSeq->MarkPackageDirty();

	// Copy Curve Data
	DestAnimSeq->CurveData = SourceAnimSeq->CurveData;
#endif // WITH_EDITORONLY_DATA

	return TRUE;
}

void UAnimSequence::CopyMetadata(UAnimSequence* SourceAnimSeq, UAnimSequence* DestAnimSeq)
{
	// Don't need to do anything if they're the same!!
	if( SourceAnimSeq == DestAnimSeq )
	{
		return;
	}

	// If the destination sequence contains any metadata, ask the user if they'd like
	// to delete the existing metadata before copying over from the source sequence.
	if( DestAnimSeq->MetaData.Num() > 0 )
	{
		const UBOOL bDeleteExistingMetadata = appMsgf( AMT_YesNo, LocalizeSecure(LocalizeUnrealEd("DestSeqAlreadyContainsMetadataMergeQ"), DestAnimSeq->MetaData.Num()) );
		if( bDeleteExistingMetadata )
		{
			DestAnimSeq->MetaData.Empty();
			DestAnimSeq->MarkPackageDirty();
		}
	}

	// Do the copy.
	TArray<INT> NewMetadataIndices;

	for(INT MetadataIndex=0; MetadataIndex<SourceAnimSeq->MetaData.Num(); ++MetadataIndex)
	{
		INT NewMetadataIndex = DestAnimSeq->MetaData.AddZeroed();

		// Copy the notify itself, and point the new one at it.
		if( SourceAnimSeq->MetaData(MetadataIndex) )
		{
			FObjectDuplicationParameters DupParams( SourceAnimSeq->MetaData(MetadataIndex), DestAnimSeq );
			DestAnimSeq->MetaData(NewMetadataIndex) = CastChecked<UAnimMetaData>( UObject::StaticDuplicateObjectEx(DupParams) );
		}
		else
		{
			DestAnimSeq->MetaData(NewMetadataIndex) = NULL;
		}
	}

	// Make sure editor knows we've changed something.
	DestAnimSeq->MarkPackageDirty();
}

/** 
 * Copy AnimNotifies from one UAnimSequence to another.
 */
UBOOL UAnimSequence::CopyNotifies(UAnimSequence* SourceAnimSeq, UAnimSequence* DestAnimSeq)
{
#if WITH_EDITORONLY_DATA
	// Abort if source == destination.
	if( SourceAnimSeq == DestAnimSeq )
	{
		return TRUE;
	}

	// If the destination sequence is shorter than the source sequence, we'll be dropping notifies that
	// occur at later times than the dest sequence is long.  Give the user a chance to abort if we
	// find any notifies that won't be copied over.
	if( DestAnimSeq->SequenceLength < SourceAnimSeq->SequenceLength )
	{
		for(INT NotifyIndex=0; NotifyIndex<SourceAnimSeq->Notifies.Num(); ++NotifyIndex)
		{
			// If a notify is found which occurs off the end of the destination sequence, prompt the user to continue.
			const FAnimNotifyEvent& SrcNotifyEvent = SourceAnimSeq->Notifies(NotifyIndex);
			if( SrcNotifyEvent.Time > DestAnimSeq->SequenceLength )
			{
				const UBOOL bProceed = appMsgf( AMT_YesNo, *LocalizeUnrealEd("SomeNotifiesWillNotBeCopiedQ") );
				if( !bProceed )
				{
					return FALSE;
				}
				else
				{
					break;
				}
			}
		}
	}

	// If the destination sequence contains any notifies, ask the user if they'd like
	// to delete the existing notifies before copying over from the source sequence.
	if( DestAnimSeq->Notifies.Num() > 0 )
	{
		const UBOOL bDeleteExistingNotifies = appMsgf( AMT_YesNo, LocalizeSecure(LocalizeUnrealEd("DestSeqAlreadyContainsNotifiesMergeQ"), DestAnimSeq->Notifies.Num()) );
		if( bDeleteExistingNotifies )
		{
			DestAnimSeq->Notifies.Empty();
			DestAnimSeq->MarkPackageDirty();
		}
	}

	// Do the copy.
	TArray<INT> NewNotifyIndices;
	INT NumNotifiesThatWereNotCopied = 0;

	for(INT NotifyIndex=0; NotifyIndex<SourceAnimSeq->Notifies.Num(); ++NotifyIndex)
	{
		const FAnimNotifyEvent& SrcNotifyEvent = SourceAnimSeq->Notifies(NotifyIndex);

		// Skip notifies which occur at times later than the destination sequence is long.
		if( SrcNotifyEvent.Time > DestAnimSeq->SequenceLength )
		{
			continue;
		}

		// Do a linear-search through existing notifies to determine where
		// to insert the new notify.
		INT NewNotifyIndex = 0;
		while( NewNotifyIndex < DestAnimSeq->Notifies.Num()
			&& DestAnimSeq->Notifies(NewNotifyIndex).Time <= SrcNotifyEvent.Time )
		{
			++NewNotifyIndex;
		}

		// Track the location of the new notify.
		NewNotifyIndices.AddItem(NewNotifyIndex);

		// Create a new empty on in the array.
		DestAnimSeq->Notifies.InsertZeroed(NewNotifyIndex);

		// Copy time and comment.
		DestAnimSeq->Notifies(NewNotifyIndex).Time = SrcNotifyEvent.Time;
		DestAnimSeq->Notifies(NewNotifyIndex).Comment = SrcNotifyEvent.Comment;
		DestAnimSeq->Notifies(NewNotifyIndex).Duration = SrcNotifyEvent.Duration;

		// Copy the notify itself, and point the new one at it.
		if( SrcNotifyEvent.Notify )
		{
			DestAnimSeq->Notifies(NewNotifyIndex).Notify = static_cast<UAnimNotify*>( StaticDuplicateObject(SrcNotifyEvent.Notify, SrcNotifyEvent.Notify, DestAnimSeq, TEXT("None"), ~RF_RootSet ) );
			// Make sure Archetype points to default object. For some reason we have bad references in existing content...
			DestAnimSeq->Notifies(NewNotifyIndex).Notify->SetArchetype(DestAnimSeq->Notifies(NewNotifyIndex).Notify->GetClass()->GetDefaultObject());
		}
		else
		{
			DestAnimSeq->Notifies(NewNotifyIndex).Notify = NULL;
		}

		// Make sure editor knows we've changed something.
		DestAnimSeq->MarkPackageDirty();
	}

	// Inform the user if some notifies weren't copied.
	if( SourceAnimSeq->Notifies.Num() > NewNotifyIndices.Num() )
	{
		appMsgf( AMT_OK, LocalizeSecure(LocalizeUnrealEd("SomeNotifiesWereNotCopiedF"), SourceAnimSeq->Notifies.Num() - NewNotifyIndices.Num()) );
	}
#endif // WITH_EDITORONLY_DATA

	return TRUE;
}

FLOAT UAnimSequence::GetNotifyTimeByClass( UClass* NotifyClass, FLOAT PlayRate, FLOAT StartPosition, UAnimNotify** out_Notify, FLOAT* out_Duration )
{
	if( PlayRate <= 0.f )
	{
		PlayRate = 1.f;
	}
	for( INT i = 0; i < Notifies.Num(); i++ )
	{
		UClass* C = (Notifies(i).Notify != NULL) ? Notifies(i).Notify->GetClass() : NULL;
		FLOAT NotifyTime = Notifies(i).Time / PlayRate;
		if( C != NULL && C->IsChildOf( NotifyClass ) && NotifyTime > StartPosition )
		{
			if( out_Notify != NULL )
			{
				*out_Notify = Notifies(i).Notify;
			}
			if( out_Duration != NULL )
			{
				*out_Duration = Notifies(i).Duration;
			}

			return NotifyTime;
		}
	}

	return -1.f;
}

/*-----------------------------------------------------------------------------
	UAnimSet
-----------------------------------------------------------------------------*/

void FAnimSetMeshLinkup::BuildLinkup(USkeletalMesh* InSkelMesh, UAnimSet* InAnimSet)
{
	INT const NumBones = InSkelMesh->RefSkeleton.Num();

	// Bone to Track mapping.
	BoneToTrackTable.Empty(NumBones);
	BoneToTrackTable.Add(NumBones);

	// For each bone in skeletal mesh, find which track to pull from in the AnimSet.
	for(INT i=0; i<NumBones; i++)
	{
		FName const BoneName = InSkelMesh->RefSkeleton(i).Name;

		// FindTrackWithName will return INDEX_NONE if no track exists.
		BoneToTrackTable(i) = InAnimSet->FindTrackWithName(BoneName);
	}

	// Check here if we've properly cached those arrays.
	if( InAnimSet->BoneUseAnimTranslation.Num() != InAnimSet->TrackBoneNames.Num() )
	{
		INT const NumTracks = InAnimSet->TrackBoneNames.Num();

		InAnimSet->BoneUseAnimTranslation.Empty(NumTracks);
		InAnimSet->BoneUseAnimTranslation.Add(NumTracks);

		InAnimSet->ForceUseMeshTranslation.Empty(NumTracks);
		InAnimSet->ForceUseMeshTranslation.Add(NumTracks);

		for(INT TrackIndex = 0; TrackIndex<NumTracks; TrackIndex++)
		{
			FName const TrackBoneName = InAnimSet->TrackBoneNames(TrackIndex);

			// Cache whether to use the translation from this bone or from ref pose.
			InAnimSet->BoneUseAnimTranslation(TrackIndex) = InAnimSet->UseTranslationBoneNames.ContainsItem(TrackBoneName);
			InAnimSet->ForceUseMeshTranslation(TrackIndex) = InAnimSet->ForceMeshTranslationBoneNames.ContainsItem(TrackBoneName);
		}
	}

#if !FINAL_RELEASE
	TArray<UBOOL> TrackUsed;
	TrackUsed.AddZeroed(InAnimSet->TrackBoneNames.Num());
	const INT AnimLinkupIndex = InAnimSet->GetMeshLinkupIndex( InSkelMesh );
	const FAnimSetMeshLinkup& AnimLinkup = InAnimSet->LinkupCache( AnimLinkupIndex );
	for(INT BoneIndex=0; BoneIndex<NumBones; BoneIndex++)
	{
		const INT TrackIndex = AnimLinkup.BoneToTrackTable(BoneIndex);

		if( TrackIndex == INDEX_NONE )
		{
			continue;
		}

		if( TrackUsed(TrackIndex) )
		{
			warnf(TEXT("%s has multiple bones sharing the same track index!!!"), *InAnimSet->GetFullName());	
			for(INT DupeBoneIndex=0; DupeBoneIndex<NumBones; DupeBoneIndex++)
			{
				const INT DupeTrackIndex = AnimLinkup.BoneToTrackTable(DupeBoneIndex);
				if( DupeTrackIndex == TrackIndex )
				{
					warnf(TEXT(" BoneIndex: %i, BoneName: %s, TrackIndex: %i, TrackBoneName: %s"), DupeBoneIndex, *InSkelMesh->RefSkeleton(DupeBoneIndex).Name.ToString(), DupeTrackIndex, *InAnimSet->TrackBoneNames(DupeTrackIndex).ToString());	
				}
			}
		}

		TrackUsed(TrackIndex) = TRUE;
	}
#endif
}


// Global flag to trace animation usage
// Currently gets activated with only command "-TRACEANIMUSAGE"
extern UBOOL GShouldTraceAnimationUsage;

void UAnimSet::PreSave()
{
	// Will need to check if bad outer still happens or not
	// We could not repro this, but now we're going to monitor to see if this happens.
	for (INT J=0; J<Sequences.Num(); ++J)
	{
		UAnimSequence * AnimSeq = Sequences(J);
		AnimSeq->FixUpBadAnimNotifiers();
	}

	Super::PreSave();
}

void UAnimSet::PostLoad()
{
	Super::PostLoad();

	// Populate sequence cache.
	for( INT i=0; i<Sequences.Num(); i++ )
	{
		UAnimSequence* AnimSequence = Sequences(i);
		if( AnimSequence )
		{
			SequenceCache.Set( AnimSequence->SequenceName, i ); 
		}
	}
	
	// Make sure that AnimSets (and sequences) within level packages are not marked as standalone.
	if(GetOutermost()->ContainsMap() && HasAnyFlags(RF_Standalone))
	{
		ClearFlags(RF_Standalone);

		for(INT i=0; i<Sequences.Num(); i++)
		{
			UAnimSequence* Seq = Sequences(i);
			if(Seq)
			{
				Seq->ClearFlags(RF_Standalone);
			}
		}
	}

	// If we're tracing animation usage, start
	if ( GShouldTraceAnimationUsage )
	{
		TraceAnimationUsage();
	}
}	

void UAnimSet::BeginDestroy()
{
	if ( GShouldTraceAnimationUsage )
	{
		RecordAnimationUsage();
	}

	Super::BeginDestroy();
}

/**
 * See if we can play sequences from this AnimSet on the provided SkeletalMesh.
 * Returns true if there is a bone in SkelMesh for every track in the AnimSet,
 * or there is a track of animation for every bone of the SkelMesh.
 * 
 * @param	SkelMesh	SkeletalMesh to compare the AnimSet against.
 * @return				TRUE if animation set can play on supplied SkeletalMesh, FALSE if not.
 */
UBOOL UAnimSet::CanPlayOnSkeletalMesh(USkeletalMesh* SkelMesh) const
{
	// Temporarily allow any animation to play on any AnimSet. 
	// We need a looser metric for matching animation to skeletons. Some 'overlap bone count'?
#if 0
	// This is broken and needs to be looked into.
	// we require at least 10% of tracks matched by skeletal mesh.
	return GetSkeletalMeshMatchRatio(SkelMesh) > 0.1f;
#else
	return TRUE;
#endif
}

FLOAT UAnimSet::GetSkeletalMeshMatchRatio(USkeletalMesh* SkelMesh) const
{
	// First see if there is a bone for all tracks
	INT TracksMatched = 0;
	for(INT i=0; i<TrackBoneNames.Num() ; i++)
	{
		const INT BoneIndex = SkelMesh->MatchRefBone( TrackBoneNames(i) );
		if( BoneIndex != INDEX_NONE )
		{
			++TracksMatched;
		}
	}

	// If we can't match any bones, then this is definitely not compatible.
	if( TrackBoneNames.Num() == 0 || TracksMatched == 0 )
	{
		return 0.f;
	}

	// return how many of the animation tracks were matched by that mesh
	return (FLOAT)TracksMatched / FLOAT(TrackBoneNames.Num());
}

/**
 * Returns the AnimSequence with the specified name in this set.
 * 
 * @param		SequenceName	Name of sequence to find.
 * @return						Pointer to AnimSequence with desired name, or NULL if sequence was not found.
 */
UAnimSequence* UAnimSet::FindAnimSequence(FName SequenceName)
{
	UAnimSequence* AnimSequence = NULL;

	if( SequenceName != NAME_None )
	{
		const INT* IndexPtr = SequenceCache.Find( SequenceName );
		if( IndexPtr )
		{
			// Check for mismatch and clear entire cache if that happens.
			AnimSequence = Sequences(Min(*IndexPtr,Sequences.Num()-1));
			if( AnimSequence->SequenceName != SequenceName )
			{
				check(GIsEditor);
				AnimSequence = NULL;
				SequenceCache.Empty();
			}
		}		
		// Modifications in the editor might have invalidated the cache or there are new entries.
		if( GIsEditor && AnimSequence == NULL )
		{
			for(INT i=0; i<Sequences.Num(); i++)
			{
				if( Sequences(i)->SequenceName == SequenceName )
				{				
					AnimSequence = Sequences(i);
					// Populate cache on demand with new entries.
					SequenceCache.Set( SequenceName, i );
					break;
				}
			}
		}
	}

	return AnimSequence;
}

/**
 * Find a mesh linkup table (mapping of sequence tracks to bone indices) for a particular SkeletalMesh
 * If one does not already exist, create it now.
 *
 * @param InSkelMesh SkeletalMesh to look for linkup with.
 *
 * @return Index of Linkup between mesh and animation set.
 */
INT UAnimSet::GetMeshLinkupIndex(USkeletalMesh* InSkelMesh)
{
	// First, see if we have a cached link-up between this animation and the given skeletal mesh.
	check(InSkelMesh);

	// Get SkeletalMesh path name
	FName SkelMeshName = FName( *InSkelMesh->GetPathName() );

	// See if we have already cached this Skeletal Mesh.
	const INT* IndexPtr = SkelMesh2LinkupCache.Find( SkelMeshName );

	// If not found, create a new entry
	if( IndexPtr == NULL )
	{
		// No linkup found - so create one here and add to cache.
		const INT NewLinkupIndex = LinkupCache.AddZeroed();

		// Add it to our cache
		SkelMesh2LinkupCache.Set( SkelMeshName, NewLinkupIndex );
		
		// Fill it up
		FAnimSetMeshLinkup* NewLinkup = &LinkupCache(NewLinkupIndex);
		NewLinkup->BuildLinkup(InSkelMesh, this);

		return NewLinkupIndex;
	}

	return (*IndexPtr);
}

/**
 * Returns the size of the object/ resource for display to artists/ LDs in the Editor.
 *
 * @return size of resource as to be displayed to artists/ LDs in the Editor.
 */
INT UAnimSet::GetResourceSize()
{
	if (GExclusiveResourceSizeMode)
	{
		// This object only references others, it doesn't have any real resource bytes
		return 0;
	}
	else
	{
		FArchiveCountMem CountBytesSize( this );
		INT ResourceSize = CountBytesSize.GetNum();
		for( INT i=0; i<Sequences.Num(); i++ )
		{
			UAnimSequence* AnimSeq = Sequences(i);
			if( AnimSeq )
			{
				ResourceSize += AnimSeq->GetResourceSize();
			}			
		}
		return ResourceSize;
	}
}

/**
 * Clears all sequences and resets the TrackBoneNames table.
 */
void UAnimSet::ResetAnimSet()
{
#if WITH_EDITORONLY_DATA
	// Make sure we handle AnimSequence references properly before emptying the array.
	for(INT i=0; i<Sequences.Num(); i++)
	{	
		UAnimSequence* AnimSeq = Sequences(i);
		if( AnimSeq )
		{
			AnimSeq->RecycleAnimSequence();
		}
	}
	Sequences.Empty();
	SequenceCache.Empty();
	TrackBoneNames.Empty();
	LinkupCache.Empty();
	SkelMesh2LinkupCache.Empty();

	// We need to re-init any skeleltal mesh components now, because they might still have references to linkups in this set.
	for(TObjectIterator<USkeletalMeshComponent> It;It;++It)
	{
		USkeletalMeshComponent* SkelComp = *It;
		if(!SkelComp->IsPendingKill() && !SkelComp->IsTemplate())
		{
			SkelComp->InitAnimTree();
		}
	}
#endif // WITH_EDITORONLY_DATA
}

/** 
 * Properly remove an AnimSequence from an AnimSet, and updates references it might have.
 * @return TRUE if AnimSequence was properly removed, FALSE if it wasn't found.
 */
UBOOL UAnimSet::RemoveAnimSequenceFromAnimSet(UAnimSequence* AnimSeq)
{
#if WITH_EDITORONLY_DATA
	INT SequenceIndex = Sequences.FindItemIndex(AnimSeq);
	if( SequenceIndex != INDEX_NONE )
	{
		// Handle reference clean up properly
		AnimSeq->RecycleAnimSequence();
		SequenceCache.Remove(AnimSeq->SequenceName);
		// Remove from array
		Sequences.Remove(SequenceIndex, 1);
		if( GIsEditor )
		{
			MarkPackageDirty();
		}
		return TRUE;
	}
#endif // WITH_EDITORONLY_DATA

	return FALSE;
}


/** Util that find all AnimSets and flushes their LinkupCache, then calls InitAnimTree on all SkeletalMeshComponents. */
void UAnimSet::ClearAllAnimSetLinkupCaches()
{
	DOUBLE Start = appSeconds();

	TArray<UAnimSet*> AnimSets;
	TArray<USkeletalMeshComponent*> SkelComps;
	// Find all AnimSets and SkeletalMeshComponents (just do one iterator)
	for(TObjectIterator<UObject> It;It;++It)
	{
		UAnimSet* AnimSet = Cast<UAnimSet>(*It);
		if(AnimSet && !AnimSet->IsPendingKill() && !AnimSet->IsTemplate())
		{
			AnimSets.AddItem(AnimSet);
		}

		USkeletalMeshComponent* SkelComp = Cast<USkeletalMeshComponent>(*It);
		if(SkelComp && !SkelComp->IsPendingKill() && !SkelComp->IsTemplate())
		{
			SkelComps.AddItem(SkelComp);
		}
	}

	// For all AnimSets, empty their linkup cache
	for(INT i=0; i<AnimSets.Num(); i++)
	{
		AnimSets(i)->LinkupCache.Empty();
		AnimSets(i)->SkelMesh2LinkupCache.Empty();
	}

	// For all SkeletalMeshComponents, force anims to be re-bound
	for(INT i=0; i<SkelComps.Num(); i++)
	{
		SkelComps(i)->UpdateAnimations();
	}

	debugf(NAME_DevAnim, TEXT("ClearAllAnimSetLinkupCaches - Took %3.2fms"), (appSeconds() - Start)*1000.f);
}

/*
 * Animation Tracking System - either unused or used and if used, how much used...
 * Per animation and per animset
 */

// Animation information

typedef struct _FAnimationInfo
{
	FString AnimName;
	FString Tag; // temporary tag before we have animsequence tag
	INT		ResourceSize;
	FLOAT	UseScore; 
	UBOOL	Used; // UseScore == 0 does not mean not used - 

	_FAnimationInfo(const FString& InAnimName, INT InResourceSize)
		:	AnimName(InAnimName),
			ResourceSize(InResourceSize), 
			UseScore(0.f), 
			Used(FALSE){}

	void AddScore(FLOAT InScore)
	{
		UseScore += InScore;
	}

	// if Used == FALSE, then set TRUE
	void SetUsed()
	{
		if (Used == FALSE)
		{
			Used = TRUE;
		}
	}
} FAnimationInfo;

// Per loading trace information : Meaning from loading to unloading
// One animset could have been loaded and unloaded multiple time across levels
typedef struct _FLevelAnimSetUsage
{
	PTRINT	InstanceID;			// address, so that I can update information by
	FString LevelName;			// Level Name that was loaded by
	FLOAT	LoadingTime;		// Loading Time - GWorld TimeSeconds
	FLOAT	UnloadingTime;		// Unloading Time - Gworld Time Seconds
	INT		TotalUnusedResourceSize;
	TArray<const FAnimationInfo*>	UnusedAnimations;	// Unused animation information

	_FLevelAnimSetUsage(const FString& InLevelName, FLOAT InLoadingTime, PTRINT InInstanceID) 
		:	InstanceID(InInstanceID),
		LevelName(InLevelName),
		LoadingTime(InLoadingTime), 
		UnloadingTime(0.f),
		TotalUnusedResourceSize(0)
	{
		UnusedAnimations.Empty();
	}
} FLevelAnimSetUsage;

typedef struct _FAnimSetUsage
{
	FString	AnimSetPath;
	INT		TotalNumOfAnimations;	// Num of total animations in the animset
	INT		TotalNumOfUsedAnimations;
	INT		TotalResourceSize;
	INT		TotalResourceSizeOfUsedAnimations;
	FLOAT	AnimsetUseScore;

	TArray<FLevelAnimSetUsage*>	LevelInformation;
	TArray<FAnimationInfo*>		AnimationList;

	_FAnimSetUsage(const FString & InAnimSetPath, INT InTotalNumAnimations)
		:	AnimSetPath(InAnimSetPath),
			TotalNumOfAnimations(InTotalNumAnimations), 
			TotalNumOfUsedAnimations(0), 
			TotalResourceSize(0),
			TotalResourceSizeOfUsedAnimations(0),
			AnimsetUseScore (0.f){} 
} FAnimSetUsage;

// Global debug variable information
// Should be only used if GShouldTraceAnimationUsage is TRUE
TMap<FString, FAnimationInfo*> GAnimationList; // this is unique animation list
TMap<FString, FAnimSetUsage*> GAnimsetUsageMap;
UBOOL GShouldTraceAnimationUsage = FALSE;
UBOOL GBeingTraceAnimationUsage = TRUE;

// Save last saved folder and delete it once new one is created
// So that if any reason, crashed, it will keep the last saved folder
FString GLastFolderSaved;
FLOAT	GLastOutputTime = 0.f;

/*
* return animation info tag
* This is temporary until we add content tag to animation 
* First they search from sequence name, and if nothing is found, they look for animset
* Most of case, animset includes a lot of key information.
*/
FString GetAnimationTag( UAnimSequence * Sequence )
{
	check (Sequence);

	UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);
	if (GameEngine)
	{
		const TArray<FAnimTag> & AnimTags = GameEngine->AnimTags;

		FString AnimName = Sequence->SequenceName.GetNameString();
		FString AnimSetName = Sequence->GetAnimSet()->GetName();

		for ( INT I=0; I<AnimTags.Num(); ++I )
		{
			for ( INT J=0; J<AnimTags(I).Contains.Num(); ++J )
			{
				// first found, then return
				// 0-end is priority
				if (AnimName.InStr(AnimTags(I).Contains(J), FALSE, TRUE) != INDEX_NONE)
				{
					return AnimTags(I).Tag;
				}
				// if they don't find it from animname, try animset name
				else if (AnimSetName.InStr(AnimTags(I).Contains(J), FALSE, TRUE) != INDEX_NONE)
				{
					return AnimTags(I).Tag;
				}
			}
		}
	}

	// default
	return TEXT("NONE");
}

/*
 * Get FAnimationInfo from Sequence
 * If not found, create one.
 */
FAnimationInfo* GetAnimationInfo( UAnimSequence * Sequence )
{
	check (GShouldTraceAnimationUsage);

	if ( Sequence )
	{
		FAnimationInfo ** Found = GAnimationList.Find(Sequence->GetPathName());
		if ( Found == NULL )
		{
			FAnimationInfo * NewInfo = new FAnimationInfo(Sequence->SequenceName.GetNameString(), Sequence->GetResourceSize());
			GAnimationList.Set(Sequence->GetPathName(), NewInfo);
			NewInfo->Tag = GetAnimationTag(Sequence);
			return NewInfo;
		}

		return *Found;
	}

	return NULL;
}

/*
* Get FAnimSetUsage from AnimSet
* If not found, create one.
*/
FAnimSetUsage* GetAnimSetUsage( UAnimSet * AnimSet )
{
	check (GShouldTraceAnimationUsage);

	if ( AnimSet )
	{
		// Get AnimSetUsage that has same path name with this
		FAnimSetUsage ** Found = GAnimsetUsageMap.Find(AnimSet->GetPathName());
		if ( Found == NULL )
		{
			FAnimSetUsage * NewUsage = new FAnimSetUsage(AnimSet->GetPathName(), AnimSet->Sequences.Num());
			// Enter all animsequence for this animset
			for ( INT I=0; I<AnimSet->Sequences.Num(); ++I )
			{
				NewUsage->AnimationList.AddItem(GetAnimationInfo(AnimSet->Sequences(I)));
			}

			// add to map
			GAnimsetUsageMap.Set(AnimSet->GetPathName(), NewUsage);

			return NewUsage;
		}

		return *Found;
	}

	return NULL;
}

/**
* Add animation usage information to TMap
*/
void UAnimSet::TraceAnimationUsage()
{
	check (GShouldTraceAnimationUsage);

	if (GBeingTraceAnimationUsage == FALSE)
	{
		return;
	}

	FAnimSetUsage * Current = GetAnimSetUsage( this );

	check (Current);

	if ( GWorld )
	{
		if ( GWorld->PersistentLevel )
		{
			Current->LevelInformation.AddItem(new FLevelAnimSetUsage(GWorld->PersistentLevel->GetPathName(), GWorld->GetTimeSeconds(), (PTRINT)this));
		}
		else 
		{
			Current->LevelInformation.AddItem(new FLevelAnimSetUsage(TEXT("No Persistent Level"), GWorld->GetTimeSeconds(), (PTRINT)this));
		}
	}
	else
	{
		Current->LevelInformation.AddItem(new FLevelAnimSetUsage(TEXT("No Persistent Level"), 0.0f, (PTRINT)this));
	}
}

/**
* Record Animation Usage
*/
void UAnimSet::RecordAnimationUsage()
{
	check (GShouldTraceAnimationUsage);

	if (GBeingTraceAnimationUsage == FALSE)
	{
		return;
	}

	FAnimSetUsage* Current = GetAnimSetUsage( this );

	// there is chance it might not be there when exiting
	check (Current);

	// find current animset ones
	for( INT I=0; I<Current->LevelInformation.Num(); ++I )
	{
		// find this instance
		if (Current->LevelInformation(I)->InstanceID == (PTRINT)this)
		{
			// get the level data
			FLevelAnimSetUsage* SrcUsage = Current->LevelInformation(I);
			if (SrcUsage)
			{
				SrcUsage->UnloadingTime = (GWorld)?GWorld->GetTimeSeconds():0.0f;
				// clear
				SrcUsage->UnusedAnimations.Empty();
				SrcUsage->TotalUnusedResourceSize = 0;

				for (INT J=0; J<Sequences.Num(); ++J)
				{
					FAnimationInfo * Found = GetAnimationInfo(Sequences(J));

					// add score, and clear sequence score to avoid multiple additions
					Found->AddScore(Sequences(J)->UseScore);
					Sequences(J)->UseScore = 0;

					// if haven't been used, add item
					if (Sequences(J)->bHasBeenUsed == FALSE)
					{
						SrcUsage->UnusedAnimations.AddItem(Found);
						SrcUsage->TotalUnusedResourceSize+=Found->ResourceSize;
					}
					else
					{
						// Mark used
						Found->SetUsed();
					}
				}
			}

			break;
		}
	}
}

void UAnimSet::CleanUpAnimationUsage()
{
	// free all allocated memory
	for (TMap<FString, FAnimationInfo*>::TIterator Iter(GAnimationList); Iter; ++Iter) // this is unique animation list
	{
		delete Iter.Value();
	}

	GAnimationList.Empty();

	for (TMap<FString, FAnimSetUsage*>::TIterator Iter(GAnimsetUsageMap); Iter; ++Iter) 
	{
		FAnimSetUsage * AnimSetUsage = Iter.Value();

		for ( INT I=0; I<AnimSetUsage->LevelInformation.Num(); ++I )
		{
			delete AnimSetUsage->LevelInformation(I);
		}

		delete AnimSetUsage;
	}

	GAnimsetUsageMap.Empty();

	// We'd like to avoid more allocation after animation usage is cleared, (Right now in PreExit)
	// Right now this is only per game session. If you'd like to make it
	// multiple session per game, then make sure you turn this flag on when start capturing again
	GBeingTraceAnimationUsage = FALSE;
}

/** 
 *  Output if time is up
 */

void UAnimSet::TickAnimationUsage()
{
	if ( GWorld )
	{
		// if more than 10 mins, record...
		if (GWorld->GetTimeSeconds() - GLastOutputTime > 600)
		{
			OutputAnimationUsage();
			GLastOutputTime = GWorld->GetTimeSeconds();
		}
	}

}
/**
* Output Animation Usage
*/ 
void UAnimSet::OutputAnimationUsage()
{
	// if the animset hasn't been closed yet, go over and close it up.
	if ( GShouldTraceAnimationUsage && GAnimsetUsageMap.Num()>0 )
	{
		// go through recording since I'm about to output
		for(TObjectIterator<UAnimSet> It;It;++It)
		{
			UAnimSet * Current = *It;
			Current->RecordAnimationUsage();
		}

		const FString CurrentTime = appSystemTimeString();

		/// create base folder
		GFileManager->MakeDirectory( *FString(appGameLogDir() + TEXT("AnimationUsage")) );

		// create current folder
		const FString CSVDirectory = appGameLogDir() + TEXT("AnimationUsage") + PATH_SEPARATOR + FString::Printf( TEXT("%s-%d-%s"), GGameName, GetChangeListNumberForPerfTesting(), *CurrentTime ) + PATH_SEPARATOR;
		GFileManager->MakeDirectory( *CSVDirectory );

		// fill up empty data first before output
		for ( TMap<FString, FAnimSetUsage*>::TConstIterator Iter(GAnimsetUsageMap); Iter; ++Iter )
		{
			FAnimSetUsage * AnimSet = Iter.Value();
			AnimSet->AnimsetUseScore = 0.f;
			AnimSet->TotalNumOfUsedAnimations = 0;
			AnimSet->TotalResourceSizeOfUsedAnimations = 0;
			AnimSet->TotalResourceSize = 0;

			for (INT I=0; I<AnimSet->AnimationList.Num(); ++I)
			{
				// get total use score to get idea 
				AnimSet->AnimsetUseScore += AnimSet->AnimationList(I)->UseScore;
				AnimSet->TotalResourceSize += AnimSet->AnimationList(I)->ResourceSize;

				if (AnimSet->AnimationList(I)->Used)
				{
					AnimSet->TotalNumOfUsedAnimations++;
					AnimSet->TotalResourceSizeOfUsedAnimations += AnimSet->AnimationList(I)->ResourceSize;
				}
			}
		}

		// go through two phase
		// first phase, just output unused animations
		for ( TMap<FString, FAnimSetUsage*>::TConstIterator Iter(GAnimsetUsageMap); Iter; ++Iter )
		{
			FAnimSetUsage * AnimSet = Iter.Value();
			FArchive*	CSVFile	= NULL;
			const FString CSVFilename	= FString::Printf(TEXT("%s%s%s.csv"), *CSVDirectory, PATH_SEPARATOR, *AnimSet->AnimSetPath);

			// find current animset ones
			// write the header first
			// go through all level info and write it up first
			for( INT I=0; I<AnimSet->LevelInformation.Num(); ++I )
			{
				FLevelAnimSetUsage* Usage = AnimSet->LevelInformation(I);

				// No need to serialize if no unused animations, so defer the serialization until it's found
				if (Usage->UnusedAnimations.Num() > 0 )			
				{
					FString Row;

					// if that hasn't been created, create one for this animset
					if ( CSVFile==NULL )
					{
						CSVFile	= GFileManager->CreateFileWriter( *CSVFilename );

						Row = TEXT("AnimSet,LevelName,LoadingTime,UnloadingTime,Total Number of Animation,Unused Percentage, Unused Resource Percentage,Unused Animations,Unused Resource Size") LINE_TERMINATOR;
						CSVFile->Serialize( TCHAR_TO_ANSI( *Row ), Row.Len() );
					}

					Row = FString::Printf(TEXT("%s,%s,%0.2f,%0.2f,%d,%d,%d,%d,%d%s"), *AnimSet->AnimSetPath,*Usage->LevelName,Usage->LoadingTime,Usage->UnloadingTime,
						AnimSet->TotalNumOfAnimations, (INT)(100*(Usage->UnusedAnimations.Num())/AnimSet->TotalNumOfAnimations), (INT)(100*(Usage->TotalUnusedResourceSize)/AnimSet->TotalResourceSize),
						Usage->UnusedAnimations.Num(), Usage->TotalUnusedResourceSize, LINE_TERMINATOR);

					CSVFile->Serialize( TCHAR_TO_ANSI( *Row ), Row.Len() );

					for (INT J=0; J<Usage->UnusedAnimations.Num(); ++J)
					{
						Row = FString::Printf(TEXT(",,,,,,,%s,%d%s"), *Usage->UnusedAnimations(J)->AnimName, Usage->UnusedAnimations(J)->ResourceSize, LINE_TERMINATOR);
						CSVFile->Serialize( TCHAR_TO_ANSI( *Row ), Row.Len() );
					}
				}
			}

			// if file has been created for this animset, close it now. 
			if ( CSVFile )
			{
				CSVFile->Close();
				delete CSVFile;
				CSVFile	= NULL;
			}
		}

		// once that's done, create summary in the second phase
		// Set AnimSetUsage UseScore
		FString CSVFilename	= FString::Printf(TEXT("%s%sAnimSetUsageSummary.csv"), *CSVDirectory, PATH_SEPARATOR);
		FArchive*	CSVFile = GFileManager->CreateFileWriter( *CSVFilename );
		FString Row = TEXT("AnimSet,Number of Loadings,Total Number of Animation,Total Usage Score,Used Percentage,Unused Resource Percentage,Unused Resource Size,Total Resource Size") LINE_TERMINATOR;

		CSVFile->Serialize( TCHAR_TO_ANSI( *Row ), Row.Len() );

		for ( TMap<FString, FAnimSetUsage*>::TConstIterator Iter(GAnimsetUsageMap); Iter; ++Iter )
		{
			FAnimSetUsage * AnimSet = Iter.Value();

			Row = FString::Printf(TEXT("%s,%d,%d,%0.2f,%d,%d,%d,%d%s"), *AnimSet->AnimSetPath,  AnimSet->LevelInformation.Num(),  
				AnimSet->TotalNumOfAnimations, AnimSet->AnimsetUseScore, (AnimSet->TotalNumOfUsedAnimations>0)?(INT)(100*AnimSet->TotalNumOfUsedAnimations/AnimSet->TotalNumOfAnimations):0, 
				(AnimSet->TotalResourceSizeOfUsedAnimations>0)?(INT)((100*(AnimSet->TotalResourceSize-AnimSet->TotalResourceSizeOfUsedAnimations))/AnimSet->TotalResourceSize):0,
				(AnimSet->TotalResourceSize-AnimSet->TotalResourceSizeOfUsedAnimations), AnimSet->TotalResourceSize, LINE_TERMINATOR);

			CSVFile->Serialize( TCHAR_TO_ANSI( *Row ), Row.Len() );
		}

		CSVFile->Close();
		delete CSVFile;

		CSVFilename	= FString::Printf(TEXT("%s%sAnimSequenceUsageSummary.csv"), *CSVDirectory, PATH_SEPARATOR);
		CSVFile = GFileManager->CreateFileWriter( *CSVFilename );
		Row = TEXT("AnimSet,Animation,Tag,Score,Resource Size,Not Used") LINE_TERMINATOR;

		CSVFile->Serialize( TCHAR_TO_ANSI( *Row ), Row.Len() );

		for ( TMap<FString, FAnimSetUsage*>::TConstIterator Iter(GAnimsetUsageMap); Iter; ++Iter )
		{
			FAnimSetUsage * AnimSet = Iter.Value();

			for (INT I=0; I<AnimSet->AnimationList.Num(); ++I)
			{
				Row = FString::Printf(TEXT("%s,%s,%s,%0.2f,%d,%d%s"), *AnimSet->AnimSetPath,  *AnimSet->AnimationList(I)->AnimName,  *AnimSet->AnimationList(I)->Tag, 
					AnimSet->AnimationList(I)->UseScore, AnimSet->AnimationList(I)->ResourceSize, (AnimSet->AnimationList(I)->Used)?0:1, LINE_TERMINATOR);

				CSVFile->Serialize( TCHAR_TO_ANSI( *Row ), Row.Len() );
			}
		}

		CSVFile->Close();
		delete CSVFile;

		// Now delete previous saved folder and update LastSavedFolder
		// Before moving forward, delete previous folder
		if ( GLastFolderSaved!=TEXT(""))
		{
			GFileManager->DeleteDirectory(*GLastFolderSaved, TRUE, TRUE);
		}

		GLastFolderSaved = CSVDirectory;
	}
}
/*-----------------------------------------------------------------------------
	AnimNotify subclasses
-----------------------------------------------------------------------------*/

//
// UAnimNotify_Sound
//
void UAnimNotify_Sound::Notify( UAnimNodeSequence* NodeSeq )
{
	USkeletalMeshComponent* SkelComp = NodeSeq->SkelComponent;
	check( SkelComp );

	AActor* Owner = SkelComp->GetOwner();
	const UBOOL bIsOwnerHidden = Owner != NULL && Owner->bHidden;

	if( !bIgnoreIfActorHidden || !bIsOwnerHidden )
	{
		if( ( PercentToPlay >= 1.0f ) || ( appFrand() < PercentToPlay ) )
		{
			UAudioComponent* AudioComponent = UAudioDevice::CreateComponent( SoundCue, SkelComp->GetScene(), Owner, 0 );
			if( AudioComponent )
			{
				if( BoneName != NAME_None )
				{
					AudioComponent->bUseOwnerLocation	= 0;
					AudioComponent->Location			= SkelComp->GetBoneLocation( BoneName );
				}
				else if( !(bFollowActor && Owner) )
				{	
					AudioComponent->bUseOwnerLocation	= 0;
					AudioComponent->Location			= SkelComp->LocalToWorld.GetOrigin();
				}

				AudioComponent->VolumeMultiplier		= VolumeMultiplier;
				AudioComponent->PitchMultiplier			= PitchMultiplier;
				AudioComponent->bAllowSpatialization	&= GIsGame;
				AudioComponent->bIsUISound				= !GIsGame;
				AudioComponent->bAutoDestroy			= 1;
				AudioComponent->SubtitlePriority		= SUBTITLE_PRIORITY_ANIMNOTIFY;
				AudioComponent->Play();
			}
		}
	}
}
IMPLEMENT_CLASS(UAnimNotify_Sound);

IMPLEMENT_CLASS(UAnimNotify_PawnMaterialParam);

//
// UAnimNotify_Script
//

struct FAnimNotifierHandler_Parms
{
	FLOAT	CurrentTime;
	FLOAT	TotalDuration;
	FAnimNotifierHandler_Parms(EEventParm)
	{
	}
};

void FindAndCallFunctionOnActor(AActor *Owner, FName NotifyName, FLOAT CurrentTime=0.f, FLOAT TotalDuration=0.f)
{
	if( Owner && NotifyName != NAME_None )
	{
		if( !GWorld->HasBegunPlay() )
		{
			//shhhhh... debugf( NAME_Log, TEXT("Editor: skipping AnimNotify_Script %s"), *NotifyName.ToString() );
		}
		else
		{
			//warnf( TEXT("UAnimNotify_Script: %s %s %s"), *NodeSeq->SkelComponent->GetDetailedInfo(), *NotifyName.ToString(), *Owner->GetName() );
			UFunction* Function = Owner->FindFunction( NotifyName );
			if( Function )
			{
				if( Function->FunctionFlags & FUNC_Delegate )
				{
					UDelegateProperty* DelegateProperty = FindField<UDelegateProperty>( Owner->GetClass(), *FString::Printf(TEXT("__%s__Delegate"),*NotifyName.ToString()) );
					FScriptDelegate* ScriptDelegate = (FScriptDelegate*)((BYTE*)Owner + DelegateProperty->Offset);
                    Owner->ProcessDelegate( NotifyName, ScriptDelegate, NULL );
				}
				else 
				{
					// if parameter is none, add event
					if ( Function->NumParms == 0 )
					{
						Owner->ProcessEvent( Function, NULL );								
					}
					// if parameter exists and are floats, send parameter
					else if ( Function->NumParms == 2 &&  // num parameter is 1
						Cast<UFloatProperty>(Function->PropertyLink) != NULL &&  // make sure that's animnotifi)
						Cast<UFloatProperty>(Function->PropertyLink->PropertyLinkNext) != NULL)
					{
						FAnimNotifierHandler_Parms Parms(EC_EventParm);
						Parms.CurrentTime = CurrentTime;
						Parms.TotalDuration = TotalDuration;
						Owner->ProcessEvent( Function, &Parms );								
					}
					else
					{
						// Actor has event, but with different parameters. Print warning
						debugf(NAME_Warning, TEXT("Actor %s has a anim notifier named %s, but the parameter number does not match or not of the correct type (should have one or zero and if one, it should be AnimNotify_Script)"), *Owner->GetName(), *NotifyName.ToString());
					}
				}
			}
			else
			{
				debugf(NAME_Warning,TEXT("Failed to find notify %s on %s (%s)"),*NotifyName.ToString(),*Owner->GetName(),*Owner->GetDetailedInfo());
			}
		}
	}
}

void UAnimNotify_Script::Notify( UAnimNodeSequence* NodeSeq )
{
	FindAndCallFunctionOnActor(NodeSeq->SkelComponent->GetOwner(),NotifyName);
	//debugf(TEXT("%.4f %s begin"),GWorld->GetTimeSeconds(),*NotifyName.ToString());
}

void UAnimNotify_Script::NotifyTick( UAnimNodeSequence* NodeSeq, FLOAT AnimCurrentTime, FLOAT AnimTimeStep, FLOAT InTotalDuration )
{
	FindAndCallFunctionOnActor(NodeSeq->SkelComponent->GetOwner(),NotifyTickName, AnimCurrentTime, InTotalDuration);
	//debugf(TEXT("%.4f %s %.3f"),GWorld->GetTimeSeconds(),*NotifyTickName.ToString(),AnimTimeStep);
}

void UAnimNotify_Script::NotifyEnd( UAnimNodeSequence* NodeSeq, FLOAT AnimCurrentTime )
{
	FindAndCallFunctionOnActor(NodeSeq->SkelComponent->GetOwner(),NotifyEndName);
	//debugf(TEXT("%.4f %s end"),GWorld->GetTimeSeconds(),*NotifyEndName.ToString());
}
IMPLEMENT_CLASS(UAnimNotify_Script);

//
// UAnimNotify_Scripted
//
void UAnimNotify_Scripted::Notify( UAnimNodeSequence* NodeSeq )
{
	AActor* Owner = NodeSeq->SkelComponent->GetOwner();
	if( Owner )
	{
		if( !GWorld->HasBegunPlay() )
		{
			debugf( NAME_Log, TEXT("Editor: skipping AnimNotify_Scripted %s"), *GetName() );
		}
		else
		{
			eventNotify( Owner, NodeSeq );
		}
	}
}

void UAnimNotify_Scripted::NotifyEnd( class UAnimNodeSequence* NodeSeq, FLOAT AnimCurrentTime )
{
	AActor* Owner = NodeSeq->SkelComponent->GetOwner();
	if( Owner )
	{
		if( !GWorld->HasBegunPlay() )
		{
			debugf( NAME_Log, TEXT("Editor: skipping AnimNotify_Scripted %s"), *GetName() );
		}
		else
		{
			eventNotifyEnd( Owner, NodeSeq );
		}
	}
}
IMPLEMENT_CLASS(UAnimNotify_Scripted);

//
// UAnimNotify_Kismet
//
void UAnimNotify_Kismet::Notify( UAnimNodeSequence* NodeSeq )
{
	AActor* Owner = NodeSeq->SkelComponent->GetOwner();
	if( Owner && NotifyName != NAME_None )
	{
		if( !GWorld->HasBegunPlay() )
		{
			debugf( NAME_Log, TEXT("Editor: skipping AnimNotify_Kismet %s"), *NotifyName.ToString() );
		}
		else
		{
			USeqEvent_AnimNotify* Evt;
			for( INT EventIdx = 0; EventIdx < Owner->GeneratedEvents.Num(); ++EventIdx )
			{
				Evt = Cast<USeqEvent_AnimNotify>(Owner->GeneratedEvents(EventIdx));
				if( Evt && NotifyName == Evt->NotifyName )
				{
					Evt->CheckActivate(Owner, Owner, FALSE);
				}
			}
		}
	}

}
IMPLEMENT_CLASS(UAnimNotify_Kismet);

//
// UAnimNotify_Footstep
//
void UAnimNotify_Footstep::Notify( UAnimNodeSequence* NodeSeq )
{
	AActor* Owner = (NodeSeq && NodeSeq->SkelComponent) ? NodeSeq->SkelComponent->GetOwner() : NULL;

	if( !Owner )
	{
		// This should not be the case in the game, so generate a warning.
		if( GWorld->HasBegunPlay() )
		{
			debugf(TEXT("FOOTSTEP no owner"));
		}
	}
	else
	{
		//debugf(TEXT("FOOTSTEP for %s"),*Owner->GetName());

		// Act on footstep...  FootDown signifies which paw hits earth 0=left, 1=right, 2=left-hindleg etc.
		if( Owner->GetAPawn() )
		{
			Owner->GetAPawn()->eventPlayFootStepSound(FootDown);
		}
	}
}
IMPLEMENT_CLASS(UAnimNotify_Footstep);


//
// AnimNotify_CameraEffect
//
void UAnimNotify_CameraEffect::Notify( UAnimNodeSequence* NodeSeq )
{
	AActor* Owner = NodeSeq->SkelComponent->GetOwner();
	if( Owner )
	{
		if( !GWorld->HasBegunPlay() )
		{
			debugf( NAME_Log, TEXT("Editor: skipping AnimNotify_CameraEffect %s"), *GetName() );
		}
		else
		{
			if( ( Owner->GetAPawn() != NULL )
				&& ( Cast<APlayerController>(Owner->GetAPawn()->Controller) != NULL )
				)
			{
				Cast<APlayerController>(Owner->GetAPawn()->Controller)->eventClientSpawnCameraLensEffect( CameraLensEffect );
			}
		}
	}
}
IMPLEMENT_CLASS(UAnimNotify_CameraEffect);


//
// AnimNotify_PlayParticleEffect
//
void UAnimNotify_PlayParticleEffect::Notify( UAnimNodeSequence* NodeSeq )
{
	// Don't bother trying if the template is null...
	if( PSTemplate == NULL || DEDICATED_SERVER )
	{
#if DEDICATED_SERVER
		debugf(TEXT("AnimNotify::PlayParticleEffect Anim:%s Template: %s"), *NodeSeq->AnimSeqName.ToString(), PSTemplate ? *PSTemplate->GetPathName() : TEXT("None") );
#endif
		return;
	}

	AActor* Owner = NodeSeq->SkelComponent->GetOwner();
	
	// Skip if Owner is hidden.
	if( bSkipIfOwnerIsHidden 
		&& ((Owner && Owner->bHidden) || NodeSeq->SkelComponent->HiddenGame) )
	{
		return;
	}

	UBOOL bPlayedEffect = FALSE;
	// try owner first
	if( Owner )
	{
		if( Owner->bHidden )
		{
			bSkipIfOwnerIsHidden = TRUE;
		}

		if( !GWorld->HasBegunPlay() )
		{
			debugf( NAME_Log, TEXT("Editor: skipping AnimNotify_PlayParticleEffect %s"), *GetName() );
		}
		else
		{
			bPlayedEffect = Owner->eventPlayParticleEffect( this );
		}
	}
	
	// If we are showing in the editor, or play has begun and there is no owner
	if (bPlayedEffect == FALSE)
	{
		// Play it on the skeletal mesh
		NodeSeq->SkelComponent->eventPlayParticleEffect( this );
	}
}
IMPLEMENT_CLASS(UAnimNotify_PlayParticleEffect);

//
// AnimNotify_SetMaxDistanceScale
//
void UAnimNotify_ClothingMaxDistanceScale::Notify( UAnimNodeSequence* NodeSeq )
{
	Super::Notify(NodeSeq);
	FLOAT PlayRate = NodeSeq->GetGlobalPlayRate();
	FLOAT ScaledDuration = (PlayRate > 0) ? (1.0f / PlayRate) * Duration : 0.0f;
	NodeSeq->SkelComponent->SetApexClothingMaxDistanceScale(StartScale, EndScale, static_cast<EMaxDistanceScaleMode>(ScaleMode), ScaledDuration);
}

void UAnimNotify_ClothingMaxDistanceScale::NotifyEnd( class UAnimNodeSequence* NodeSeq, FLOAT AnimCurrentTime )
{
	Super::NotifyEnd(NodeSeq, AnimCurrentTime);
	NodeSeq->SkelComponent->SetApexClothingMaxDistanceScale(EndScale, EndScale, static_cast<EMaxDistanceScaleMode>(ScaleMode), 0.0f);
}

void UAnimNotify_ClothingMaxDistanceScale::AnimNotifyEventChanged(class UAnimNodeSequence* NodeSeq, FAnimNotifyEvent* OwnerEvent)
{
	Super::AnimNotifyEventChanged(NodeSeq, OwnerEvent);
	Duration = OwnerEvent->Duration;
}
IMPLEMENT_CLASS(UAnimNotify_ClothingMaxDistanceScale);

//
// AnimNotify_Rumble
//
void UAnimNotify_Rumble::Notify( UAnimNodeSequence* NodeSeq )
{
	AActor* Owner = NodeSeq->SkelComponent->GetOwner();
	if( Owner )
	{
		if( !GWorld->HasBegunPlay() )
		{
			debugf( NAME_Log, TEXT("Editor: skipping AnimNotify_Rumble %s"), *GetName() );
		}
		else
		{
			if( bCheckForBasedPlayer || EffectRadius > 0.0 )
			{
				for( INT PlayerIndex = 0; PlayerIndex < GEngine->GamePlayers.Num(); PlayerIndex++ )
				{
					if( GEngine->GamePlayers(PlayerIndex) )
					{
						if( !GEngine->GamePlayers(PlayerIndex)->Actor || !GEngine->GamePlayers(PlayerIndex)->Actor->Pawn )
							continue;
						else
						{
							if( bCheckForBasedPlayer )
							{
								if( GEngine->GamePlayers(PlayerIndex)->Actor->Pawn->IsBasedOn(Owner) )
								{
									Owner = GEngine->GamePlayers(PlayerIndex)->Actor->Pawn;
									break;
								}
							}
							else
							{
								FLOAT fdistSq = (Owner->Location - GEngine->GamePlayers(PlayerIndex)->Actor->Pawn->Location).SizeSquared();
								if(fdistSq <= EffectRadius * EffectRadius)
								{
									//found the player and he is based on us
									Owner = GEngine->GamePlayers(PlayerIndex)->Actor->Pawn;
									break;
								}
							}
						}
					}
				}
			}

			if( ( Owner->GetAPawn() != NULL )
				&& ( Cast<APlayerController>(Owner->GetAPawn()->Controller) != NULL )
				)
			{
				Cast<APlayerController>(Owner->GetAPawn()->Controller)->eventPlayRumble( this );
			}
		}
	}
}
IMPLEMENT_CLASS(UAnimNotify_Rumble);
IMPLEMENT_CLASS(UWaveFormBase);

void UAnimNotify_ViewShake::PostLoad()
{
	// upgrade old format data to the new format
	if ( (!RotAmplitude.IsZero() || !LocAmplitude.IsZero() || FOVAmplitude != 0.f) && (ShakeParams == NULL) )
	{
		// need to upgrade
		ShakeParams = Cast<UCameraShake>(StaticConstructObject(UCameraShake::StaticClass(), this));
		if (ShakeParams)
		{
			// copy data to new format
			ShakeParams->OscillationDuration = Duration;

			ShakeParams->RotOscillation.Pitch.Amplitude = RotAmplitude.X;
			ShakeParams->RotOscillation.Pitch.Frequency = RotFrequency.X;
			ShakeParams->RotOscillation.Yaw.Amplitude = RotAmplitude.Y;
			ShakeParams->RotOscillation.Yaw.Frequency = RotFrequency.Y;
			ShakeParams->RotOscillation.Roll.Amplitude = RotAmplitude.Z;
			ShakeParams->RotOscillation.Roll.Frequency = RotFrequency.Z;

			ShakeParams->LocOscillation.X.Amplitude = LocAmplitude.X;
			ShakeParams->LocOscillation.X.Frequency = LocFrequency.X;
			ShakeParams->LocOscillation.Y.Amplitude = LocAmplitude.Y;
			ShakeParams->LocOscillation.Y.Frequency = LocFrequency.Y;
			ShakeParams->LocOscillation.Z.Amplitude = LocAmplitude.Z;
			ShakeParams->LocOscillation.Z.Frequency = LocFrequency.Z;

			ShakeParams->FOVOscillation.Amplitude = FOVAmplitude;
			ShakeParams->FOVOscillation.Frequency = FOVFrequency;
		}

		// zero out old data so we don't upgrade again
		RotAmplitude = FVector::ZeroVector;
		RotFrequency = FVector::ZeroVector;
		LocAmplitude = FVector::ZeroVector;
		LocFrequency = FVector::ZeroVector;
		FOVAmplitude = 0.f;
		FOVFrequency = 0.f;

		MarkPackageDirty(TRUE);
	}

	Super::PostLoad();
}
IMPLEMENT_CLASS(UAnimNotify_ViewShake);

INT UAnimNotify_Trails::GetNumSteps(INT InLastTrailIndex) const
{
	if ((CurrentTime < 0.0f) || (InLastTrailIndex == -1))
	{
		return 0;
	}

	FLOAT CurrEndTime = CurrentTime + TimeStep;
	INT NumSteps = 0;
	if ((InLastTrailIndex + 1) < TrailSampledData.Num())
	{
		const FTrailSample& CheckStartAnimSamplePoint = TrailSampledData(InLastTrailIndex);
		FLOAT CurrSampledTime = LastStartTime + CheckStartAnimSamplePoint.RelativeTime;
		while (CurrEndTime >= CurrSampledTime)
		{
			if ((InLastTrailIndex + NumSteps + 1) < TrailSampledData.Num())
			{
				NumSteps++;
				const FTrailSample& CheckAnimSamplePoint = TrailSampledData(InLastTrailIndex + NumSteps);
				CurrSampledTime = LastStartTime + CheckAnimSamplePoint.RelativeTime;
			}
			else
			{
				// We have hit the end of available sample data... but not worthy of warning.
				break;
			}
		}
	}

	return NumSteps;
}

// UObject interface.
void UAnimNotify_Trails::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UBOOL bResampleAnimation = FALSE;
	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if (PropertyThatChanged)
	{
		if (PropertyThatChanged->GetName() == TEXT("EndTime"))
		{
			bResampleAnimation = TRUE;
		}
		else if (PropertyThatChanged->GetName() == TEXT("SamplesPerSecond"))
		{
			// By default, make 200 FPS the max sample rate.
			// Allow the INI setting to override this.
			FLOAT MaxSampleRate = 200.0f;
			GConfig->GetFloat(TEXT("AnimNotify"), TEXT("Trail_MaxSampleRate"), MaxSampleRate, GEngineIni);
			SamplesPerSecond = Clamp<FLOAT>(SamplesPerSecond, 0.01f, MaxSampleRate);
			bResampleAnimation = TRUE;
		}
		else if (PropertyThatChanged->GetName() == TEXT("FirstEdgeSocketName"))
		{
			bResampleAnimation = TRUE;
		}
		else if (PropertyThatChanged->GetName() == TEXT("SecondEdgeSocketName"))
		{
			bResampleAnimation = TRUE;
		}
		else if (PropertyThatChanged->GetName() == TEXT("ControlPointSocketName"))
		{
			bResampleAnimation = TRUE;
		}
	}

	if (bResampleAnimation == TRUE)
	{
		//@todo. How to get the anim sequence now????
	}
}

void UAnimNotify_Trails::PostLoad()
{
	Super::PostLoad();
	if (GetLinkerVersion() < VER_ANIMNOTIFY_TRAIL_SAMPLEFRAMERATE)
	{
		SamplesPerSecond = 1.0f / SampleTimeStep_DEPRECATED;
	}

	if ((GetLinkerVersion() < VER_ANIMNOTIFY_TRAILS_REMOVED_VELOCITY) && (IsTemplate() == FALSE))
	{
		// Copy the results from the old to the new
		TrailSampledData.Empty(TrailSampleData_DEPRECATED.Num());
		TrailSampledData.AddZeroed(TrailSampleData_DEPRECATED.Num());
		for (INT CopyIdx = 0; CopyIdx < TrailSampleData_DEPRECATED.Num(); CopyIdx++)
		{
			FTrailSamplePoint& SrcSample = TrailSampleData_DEPRECATED(CopyIdx);
			FTrailSample& DestSample = TrailSampledData(CopyIdx);

			DestSample.RelativeTime = SrcSample.RelativeTime;
			DestSample.FirstEdgeSample = SrcSample.FirstEdgeSample.Position;
			DestSample.SecondEdgeSample = SrcSample.SecondEdgeSample.Position;
			DestSample.ControlPointSample = SrcSample.ControlPointSample.Position;
		}
		TrailSampleData_DEPRECATED.Empty();
	}
}

AActor* UAnimNotify_Trails::GetNotifyActor(class UAnimNodeSequence* NodeSeq)
{
	check(NodeSeq);
	return (NodeSeq->SkelComponent ? NodeSeq->SkelComponent->GetOwner() : NULL);
}

// AnimNotify interface.
void UAnimNotify_Trails::Notify(class UAnimNodeSequence* NodeSeq)
{
	Super::Notify(NodeSeq);

	AnimNodeSeq = NodeSeq;
	CurrentTime = LastStartTime;
	TimeStep = 0.0f;

	HandleNotify(NodeSeq, TrailNotifyType_Start);
}

void UAnimNotify_Trails::NotifyTick(class UAnimNodeSequence* NodeSeq, FLOAT AnimCurrentTime, FLOAT AnimTimeStep, FLOAT InTotalDuration)
{
	Super::NotifyTick(NodeSeq, AnimCurrentTime, AnimTimeStep, InTotalDuration);

	AnimNodeSeq = NodeSeq;
	CurrentTime = AnimCurrentTime;
	TimeStep = AnimTimeStep;

	HandleNotify(NodeSeq, TrailNotifyType_Tick);
}

void UAnimNotify_Trails::NotifyEnd(class UAnimNodeSequence* NodeSeq, FLOAT AnimCurrentTime)
{
	Super::NotifyEnd(NodeSeq, AnimCurrentTime);

	AnimNodeSeq = NodeSeq;
	TimeStep = CurrentTime - EndTime;
	CurrentTime = AnimCurrentTime;

	HandleNotify(NodeSeq, TrailNotifyType_End);
}

/**
 *	Handle the various notifies. This should only be called internally!
 *
 *	@param	InNodeSeq		The anim node sequence triggering the notify
 *	@param	InNotifyType	The type of notify that is being handled
 */
void UAnimNotify_Trails::HandleNotify(class UAnimNodeSequence* InNodeSeq, ETrailNotifyType InNotifyType)
{
	check((InNotifyType == TrailNotifyType_Start) || (InNotifyType == TrailNotifyType_Tick) || (InNotifyType == TrailNotifyType_End));

	AActor* Owner = GetNotifyActor(InNodeSeq);
	if (Owner)
	{
		if (GWorld->HasBegunPlay())
		{
			switch (InNotifyType)
			{
			case TrailNotifyType_Start:
				Owner->eventTrailsNotify(this);
				break;
			case TrailNotifyType_Tick:
				Owner->eventTrailsNotifyTick(this);
				break;
			case TrailNotifyType_End:
				Owner->eventTrailsNotifyEnd(this);
				break;
			}
		}
	}

	const UBOOL bShowInEditor = GIsEditor && bPreview && !GIsGame /*&& bAttach && ((SocketName != NAME_None) || (BoneName != NAME_None))*/;
	if (GWorld->HasBegunPlay() || bShowInEditor)
	{
		// Save off the 'default' particle system template
		UParticleSystem* SavedPSTemplate = PSTemplate;

		// Find for editor preview
		if(GIsEditor && !GIsGame && !bPreviewForceExplicit && GetOuter() && GetOuter()->GetOuter())
		{
			// Try to find an archetype matched with this animset so we can preview the right effect
			APawn* PreviewArchetype = NULL;
			UAnimSet* AnimSet = Cast<UAnimSet>(GetOuter()->GetOuter());
			if( AnimSet )
			{
				FString PreviewPawnName = GConfig->GetStr(TEXT("AnimNotify_FX_Preview"), *AnimSet->PreviewSkelMeshName.ToString(), GEditorIni);
				if( PreviewPawnName.Len() > 0 )
				{
					PreviewArchetype = LoadObject<APawn>(NULL, *PreviewPawnName, NULL, LOAD_None, NULL);
					if(!PreviewArchetype)
					{
						debugf(TEXT("UAnimNotify_Trails preview pawn archetype doesn't exist! %s"), *PreviewPawnName);
						PSTemplate = NULL;
					}
					else
					{
						PSTemplate = PreviewArchetype->GetAnimTrailParticleSystem(this);
						//if empty, go back to default
						if(PSTemplate == NULL)
						{
							PSTemplate = SavedPSTemplate;
						}
					}
				}
			}
		}
		else
		{
			// Query the owner (if there is one) for the particle system to use
			PSTemplate = (Owner != NULL) ? Owner->GetAnimTrailParticleSystem(this) : SavedPSTemplate;
		}

		if (PSTemplate != NULL)
		{
			// Skip if Owner is hidden.
			if (!(bSkipIfOwnerIsHidden && ((Owner && Owner->bHidden) || InNodeSeq->SkelComponent->HiddenGame)))
			{
				FMatrix SavedLocalToWorld(FMatrix::Identity);
				if (bShowInEditor == TRUE)
				{
					SavedLocalToWorld = InNodeSeq->SkelComponent->LocalToWorld;
					FTranslationMatrix TransMat(InNodeSeq->SkelComponent->LocalToWorld.TransformNormal(InNodeSeq->SkelComponent->RootBoneTranslation));
					InNodeSeq->SkelComponent->LocalToWorld *= TransMat;
				}

				UParticleSystemComponent* PSysComp = GetPSysComponent(InNodeSeq);
				if ((PSysComp == NULL) && (InNotifyType == TrailNotifyType_Start))
				{
					PSysComp = ConstructObject<UParticleSystemComponent>(UParticleSystemComponent::StaticClass(), InNodeSeq->SkelComponent);
					InNodeSeq->SkelComponent->AttachComponentToSocket(PSysComp, ControlPointSocketName);
					PSysComp->SetTemplate(PSTemplate);
					PSysComp->SetTickGroup(TG_PostUpdateWork);
					// @todo ib2merge: Some way to clean this up? -- support for using the same template with multiple trail instances
					// PSysComp->InstanceParameters.AddZeroed(1);
					// PSysComp->InstanceParameters(0).Name = ControlPointSocketName;
				}

				if (PSysComp)
				{
					switch (InNotifyType)
					{
					case TrailNotifyType_Start:
						PSysComp->ActivateSystem(TRUE);
						PSysComp->TrailsNotify(this);
						break;
					case TrailNotifyType_Tick:
						PSysComp->TrailsNotifyTick(this);
						break;
					case TrailNotifyType_End:
						PSysComp->TrailsNotifyEnd(this);
						break;
					}
				}

				if (bShowInEditor == TRUE)
				{
					InNodeSeq->SkelComponent->LocalToWorld = SavedLocalToWorld;
				}
			}
		}

		// Restore the potentially overridden particle system template...
		PSTemplate = SavedPSTemplate;
	}

	// Clear the reference to the anim node seq
	AnimNodeSeq = NULL;
}

/** 
 *	Find the ParticleSystemComponent used by this anim notify.
 *
 *	@param	NodeSeq						The AnimNodeSequence this notify is associated with.
 *
 *	@return	UParticleSystemComponent	The particle system component
 */
UParticleSystemComponent* UAnimNotify_Trails::GetPSysComponent(class UAnimNodeSequence* NodeSeq)
{
	if (NodeSeq && NodeSeq->SkelComponent)
	{
		for (INT AttachmentIdx = 0; AttachmentIdx < NodeSeq->SkelComponent->Attachments.Num(); AttachmentIdx++)
		{
			UParticleSystemComponent* PSysComp = Cast<UParticleSystemComponent>(NodeSeq->SkelComponent->Attachments(AttachmentIdx).Component);
			if (PSysComp)
			{
				if (PSysComp->Template == PSTemplate)
				{
					// @todo ib2merge: Some way to clean this up? -- support for using the same template with multiple trail instances
					// if(PSysComp->InstanceParameters.Num() < 1 || PSysComp->InstanceParameters(0).Name == ControlPointSocketName)
					return PSysComp;
				}
			}
		}
	}

	return NULL;
}

/**
 *	Called by the AnimSet viewer when the 'parent' FAnimNotifyEvent is edited.
 *
 *	@param	NodeSeq			The AnimNodeSequence this notify is associated with.
 *	@param	OwnerEvent		The FAnimNotifyEvent that 'owns' this AnimNotify.
 */
void UAnimNotify_Trails::AnimNotifyEventChanged(class UAnimNodeSequence* NodeSeq, FAnimNotifyEvent* OwnerEvent)
{
	Super::AnimNotifyEventChanged(NodeSeq, OwnerEvent);
	UBOOL bRecalculateAnimationData = FALSE;
	if (OwnerEvent->Time != LastStartTime)
	{
		LastStartTime = OwnerEvent->Time;
		bRecalculateAnimationData = TRUE;
	}
	if (OwnerEvent->Duration != (LastStartTime - EndTime))
	{
		EndTime = LastStartTime + OwnerEvent->Duration;
		bRecalculateAnimationData = TRUE;
	}
	if (TrailSampledData.Num() == 0)
	{
		bRecalculateAnimationData = TRUE;
	}

	if (bRecalculateAnimationData == TRUE)
	{
		StoreAnimationData(NodeSeq);
	}
}

/** Store the animation data for the current settings. Editor-only. */
void UAnimNotify_Trails::StoreAnimationData(class UAnimNodeSequence* NodeSeq)
{
	if (IsSetupValid(NodeSeq) == FALSE)
	{
		return;
	}

	// Sample the animation starting at LastStartTime until EndTime using SampleTimeStep
	// Retrieve each sockets position and velocity at each time step
	FLOAT SampleTimeStep = 1.0f / SamplesPerSecond;
//#define DEBUG_ANIMNOTIFY_TRAILS
#ifdef DEBUG_ANIMNOTIFY_TRAILS
	debugf(TEXT("UAnimNotify_Trails Sampling %s"), *(NodeSeq->GetName()));
	debugf(TEXT("\tStartTime %8.6f, EndTime %8.6f, TimeStep = %8.6f"), LastStartTime, EndTime, SampleTimeStep);
#endif	//DEBUG_ANIMNOTIFY_TRAILS

	if ((FirstEdgeSocketName == NAME_None) || (SecondEdgeSocketName == NAME_None) || (ControlPointSocketName == NAME_None))
	{
		warnf(TEXT("Unable to sample animation data... Missing require EdgeSocket names!"));
		return;
	}

	FLOAT TimeDiff = EndTime - LastStartTime;
	if (TimeDiff <= 0.0f)
	{
		warnf(TEXT("Unable to sample animation data... Invalid time range!"));
		return;
	}

#if WITH_EDITORONLY_DATA
	//Make sure we're sampling from the mesh we want want to sample from...
	if (!AssociateSkeletalMeshWithAnimTrailData(NodeSeq))
	{
		warnf(TEXT("Skipping sampling of animation data, a different Skeletal Mesh is to be used"));
		return;	
	}
#endif

	INT NumberOfSteps = appTrunc(TimeDiff / SampleTimeStep) + 2;
	TrailSampledData.Empty(NumberOfSteps);
	TrailSampledData.AddZeroed(NumberOfSteps);

	FMatrix BoneMatrix = NodeSeq->SkelComponent->GetBoneMatrix(0);
	FMatrix BoneMatrixInv = BoneMatrix.InverseSafe();
	FVector FirstPreLocation;
	FVector SecondPreLocation;
	FVector ControlPreLocation;
	FLOAT PreStep = LastStartTime - SampleTimeStep;
	if (PreStep >= 0.0f)
	{
		NodeSeq->SetPosition(PreStep, FALSE);
	}
	else
	{
		NodeSeq->SetPosition(0.0f, FALSE);
	}
	NodeSeq->SkelComponent->ForceSkelUpdate();
	NodeSeq->SkelComponent->GetSocketWorldLocationAndRotation(FirstEdgeSocketName, FirstPreLocation, NULL, 0);
	NodeSeq->SkelComponent->GetSocketWorldLocationAndRotation(SecondEdgeSocketName, SecondPreLocation, NULL, 0);
	NodeSeq->SkelComponent->GetSocketWorldLocationAndRotation(ControlPointSocketName, ControlPreLocation, NULL, 0);

	FirstPreLocation = BoneMatrixInv.TransformFVector(FirstPreLocation);
	SecondPreLocation = BoneMatrixInv.TransformFVector(SecondPreLocation);
	ControlPreLocation = BoneMatrixInv.TransformFVector(ControlPreLocation);

	INT StoreCount = 0;
	FLOAT CurrTime = 0.0f;
	// Force it to sample exactly at the EndTime as well!
	for (FLOAT TimeStep = LastStartTime; TimeStep < (EndTime + SampleTimeStep); TimeStep += SampleTimeStep)
	{
		if (TimeStep > EndTime)
		{
			TimeStep = EndTime;
		}
		NodeSeq->SetPosition(TimeStep, FALSE);
		NodeSeq->SkelComponent->ForceSkelUpdate();

		BoneMatrix = NodeSeq->SkelComponent->GetBoneMatrix(0);
		BoneMatrixInv = BoneMatrix.InverseSafe();

		FVector FirstLocation;
		FVector SecondLocation;
		FVector ControlLocation;

		NodeSeq->SkelComponent->GetSocketWorldLocationAndRotation(FirstEdgeSocketName, FirstLocation, NULL, 0);
		NodeSeq->SkelComponent->GetSocketWorldLocationAndRotation(SecondEdgeSocketName, SecondLocation, NULL, 0);
		NodeSeq->SkelComponent->GetSocketWorldLocationAndRotation(ControlPointSocketName, ControlLocation, NULL, 0);

		FirstLocation = BoneMatrixInv.TransformFVector(FirstLocation);
		SecondLocation = BoneMatrixInv.TransformFVector(SecondLocation);
		ControlLocation = BoneMatrixInv.TransformFVector(ControlLocation);

		// Store off the location
		if (StoreCount >= TrailSampledData.Num())
		{
			warnf(TEXT("SampleAnimationData: Stepping more than calculated! StoreCount = %3d, ArrayCount = %3d, TimeStep = %15.10f, StartTime = %15.10f, EndTime = %15.10f, SampleTimeStep = %15.10f"),
				StoreCount, TrailSampledData.Num(), TimeStep, LastStartTime, EndTime, SampleTimeStep);
			TrailSampledData.AddZeroed(1);
		}
		FTrailSample& SamplePoint = TrailSampledData(StoreCount);
		SamplePoint.RelativeTime = CurrTime;
		SamplePoint.FirstEdgeSample = FirstLocation;
		SamplePoint.SecondEdgeSample = SecondLocation;
		SamplePoint.ControlPointSample = ControlLocation;

		StoreCount++;
		CurrTime += SampleTimeStep;
	}
	//check(StoreCount <= NumberOfSteps);

#ifdef DEBUG_ANIMNOTIFY_TRAILS
	for (INT DumpIdx = 0; DumpIdx < TrailSampledData.Num(); DumpIdx++)
	{
		FTrailSample* CurrSamplePoint = &(TrailSampledData(DumpIdx));
		debugf(TEXT("\t%8.6f: First   %8.6f,%8.6f,%8.6f"), 
			CurrSamplePoint->RelativeTime, 
			CurrSamplePoint->FirstEdgeSample.X, CurrSamplePoint->FirstEdgeSample.Y, CurrSamplePoint->FirstEdgeSample.Z);
		debugf(TEXT("\t%8.6f: Second  %8.6f,%8.6f,%8.6f"), 
			CurrSamplePoint->RelativeTime, 
			CurrSamplePoint->SecondEdgeSample.X, CurrSamplePoint->SecondEdgeSample.Y, CurrSamplePoint->SecondEdgeSample.Z);
		debugf(TEXT("\t%8.6f: Control %8.6f,%8.6f,%8.6f"), 
			CurrSamplePoint->RelativeTime, 
			CurrSamplePoint->ControlPointSample.X, CurrSamplePoint->ControlPointSample.Y, CurrSamplePoint->ControlPointSample.Z);
	}
#endif	//DEBUG_ANIMNOTIFY_TRAILS

	bResampleRequired = FALSE;
}

UBOOL UAnimNotify_Trails::AssociateSkeletalMeshWithAnimTrailData(class UAnimNodeSequence* NodeSeq)
{
	UBOOL bAssociateMesh = FALSE;
#if WITH_EDITORONLY_DATA
	USkeletalMesh* SelectedMesh = NodeSeq->SkelComponent->SkeletalMesh;
	if (SelectedMesh)
	{
		if (SampledSkeletalMesh == NULL || SelectedMesh == SampledSkeletalMesh)
		{
			//associate the mesh by default
			bAssociateMesh = TRUE;
		}
		else if (SelectedMesh != SampledSkeletalMesh)
		{
			//if the associated mesh differs from the current mesh, 
			//make sure the user wants to use this mesh for sampling
			FString Output = FString::Printf(
				LocalizeSecure( LocalizeUnrealEd("AnimSetSampleNewMesh"), 
					*NodeSeq->AnimSeqName.ToString(), 
					*SelectedMesh->GetName(), 
					*SampledSkeletalMesh->GetName() 
				));
			bAssociateMesh = appMsgf(AMT_YesNo, *Output);
		}
	}

	if (bAssociateMesh)
	{
		SampledSkeletalMesh = SelectedMesh;
		PreEditChange(NULL);
		PostEditChange();
		MarkPackageDirty();
		GCallbackEvent->Send(CALLBACK_RefreshEditor);
		GCallbackEvent->Send(CALLBACK_RedrawAllViewports);
	}	

#endif
	
	return bAssociateMesh;
}

/** Verify the notify is setup correctly for sampling animation data. Editor-only. */
UBOOL UAnimNotify_Trails::IsSetupValid(class UAnimNodeSequence* NodeSeq)
{
	UBOOL bBadSampleState = FALSE;

	FString ErrorMessage;

	// Check for valid mesh and sockets...
	if (NodeSeq != NULL)
	{
		if ((NodeSeq->SkelComponent == NULL) || (NodeSeq->SkelComponent->SkeletalMesh == NULL))
		{
			bBadSampleState = TRUE;
			ErrorMessage = LocalizeUnrealEd("InvalidSkeletalMesh");
		}
		else
		{
			USkeletalMesh* SkelMesh = NodeSeq->SkelComponent->SkeletalMesh;
			UAnimSequence* AnimSeq = NodeSeq->AnimSeq;
			if (AnimSeq == NULL)
			{
				bBadSampleState = TRUE;
				ErrorMessage = LocalizeUnrealEd("InvalidAnimSequence");
			}
			else
			{
				// Check for sockets...
				if ((FirstEdgeSocketName == NAME_None) ||
					(SecondEdgeSocketName == NAME_None) ||
					(ControlPointSocketName == NAME_None))
				{
					bBadSampleState = TRUE;
					ErrorMessage = LocalizeUnrealEd("AnimNotify_Trails_MissingSocketNames");
				}
				else
				{
					if (SkelMesh->FindSocket(FirstEdgeSocketName) == NULL)
					{
						bBadSampleState = TRUE;
						ErrorMessage = FString::Printf(LocalizeSecure(LocalizeUnrealEd("AnimNotify_Trails_MissingSocketOnSkelMesh"), *(FirstEdgeSocketName.ToString())));
					}
					else if (SkelMesh->FindSocket(SecondEdgeSocketName) == NULL)
					{
						bBadSampleState = TRUE;
						ErrorMessage = FString::Printf(LocalizeSecure(LocalizeUnrealEd("AnimNotify_Trails_MissingSocketOnSkelMesh"), *(SecondEdgeSocketName.ToString())));
					}
					else if (SkelMesh->FindSocket(ControlPointSocketName) == NULL)
					{
						bBadSampleState = TRUE;
						ErrorMessage = FString::Printf(LocalizeSecure(LocalizeUnrealEd("AnimNotify_Trails_MissingSocketOnSkelMesh"), *(ControlPointSocketName.ToString())));
					}
				}
			}
		}
	}

	if (bBadSampleState == TRUE)
	{
		FString DisplayError = LocalizeUnrealEd("AnimNotify_Trails_SkippingError");
		DisplayError += TEXT("\n");
		DisplayError += ErrorMessage;
		appMsgf(AMT_OK, *DisplayError);
	}

	return !bBadSampleState;
}

IMPLEMENT_CLASS(UAnimNotify_Trails);

////////////

void UAnimNotify_ForceField::Notify( class UAnimNodeSequence* NodeSeq )
{
	if( ForceFieldComponent == NULL )
	{
		return;
	}
	USkeletalMeshComponent* SkeletalMeshComponent = NodeSeq->SkelComponent;
	UBOOL 	bValidSocket, bValidBone;
	// See if the bone name refers to an existing socket on the skeletal mesh.
	bValidSocket	= (SkeletalMeshComponent->SkeletalMesh->FindSocket(SocketName) != NULL);
	bValidBone		= (SkeletalMeshComponent->MatchRefBone(BoneName) != INDEX_NONE);
	if( SkeletalMeshComponent )
	{
		if(bAttach)
		{
			if (!(bValidBone || bValidSocket))
			{
				// Nothing to Attach to
				return;
			}

			ASkeletalMeshActor* Owner = Cast<ASkeletalMeshActor>(SkeletalMeshComponent->GetOwner());
			// Attach Component to SkeletalMeshActor
			if ( Owner )
			{
				Owner->eventCreateForceField(this);
			}
			// Attach Component to SkeletalMeshComponent
			else
			{
				SkeletalMeshComponent->eventCreateForceField(this);
			}
		}
		else
		{
			// Find Spawning Location. First using the socket, bone, the Actor that the SkeletalMeshComponent is attached to, SkeletalMeshComponent. 
			FVector Location;
			FRotator Rotation(0, 0, 1);
			AActor* Owner = SkeletalMeshComponent->GetOwner();
			if (bValidSocket)
			{
				SkeletalMeshComponent->GetSocketWorldLocationAndRotation(SocketName, Location, &Rotation);
			}
			else if (bValidBone)
			{
				Location = SkeletalMeshComponent->GetBoneLocation(BoneName);
			}
			else if (Owner)
			{
				Location = Owner->Location;
				Rotation = Owner->Rotation;
			}
			else
			{
				Location = SkeletalMeshComponent->Translation;
				Rotation = SkeletalMeshComponent->Rotation;
			}

			// Spawn Actor and Add the ForceField Component to it
			ANxForceFieldSpawnable* SpawnedActor = Cast<ANxForceFieldSpawnable>(GWorld->SpawnActor(ANxForceFieldSpawnable::StaticClass(), NAME_None, Location, Rotation ));
			if (SpawnedActor)
			{
				SpawnedActor->ForceFieldComponent = Cast<UNxForceFieldComponent>(
					UObject::StaticDuplicateObject(
					ForceFieldComponent,
					ForceFieldComponent,
					SpawnedActor,
					TEXT("None")
					)
					);
				SpawnedActor->Components.AddItem(SpawnedActor->ForceFieldComponent);
				SpawnedActor->ForceFieldComponent->DoInitRBPhys();
			}
		}

	}
}

IMPLEMENT_CLASS(UAnimNotify_ForceField);

#if !FINAL_RELEASE

void GatherAnimSequenceStats(FOutputDevice& Ar)
{
	INT AnimationKeyFormatNum[AKF_MAX];
	INT TranslationCompressionFormatNum[ACF_MAX];
	INT RotationCompressionFormatNum[ACF_MAX];
	appMemzero( AnimationKeyFormatNum, AKF_MAX * sizeof(INT) );
	appMemzero( TranslationCompressionFormatNum, ACF_MAX * sizeof(INT) );
	appMemzero( RotationCompressionFormatNum, ACF_MAX * sizeof(INT) );

	Ar.Logf( TEXT(" %60s, Frames,NTT,NRT, NT1,NR1, TotTrnKys,TotRotKys,Codec,ResBytes"), TEXT("Sequence Name") );
	INT GlobalNumTransTracks = 0;
	INT GlobalNumRotTracks = 0;
	INT GlobalNumTransTracksWithOneKey = 0;
	INT GlobalNumRotTracksWithOneKey = 0;
	INT GlobalApproxCompressedSize = 0;
	INT GlobalApproxKeyDataSize = 0;
	INT GlobalNumTransKeys = 0;
	INT GlobalNumRotKeys = 0;

	for( TObjectIterator<UAnimSequence> It; It; ++It )
	{
		UAnimSequence* Seq = *It;

		INT NumTransTracks = 0;
		INT NumRotTracks = 0;
		INT TotalNumTransKeys = 0;
		INT TotalNumRotKeys = 0;
		FLOAT TranslationKeySize = 0.0f;
		FLOAT RotationKeySize = 0.0f;
		INT OverheadSize = 0;
		INT NumTransTracksWithOneKey = 0;
		INT NumRotTracksWithOneKey = 0;

		AnimationFormat_GetStats(
			Seq, 
			NumTransTracks,
			NumRotTracks,
			TotalNumTransKeys,
			TotalNumRotKeys,
			TranslationKeySize,
			RotationKeySize,
			OverheadSize,
			NumTransTracksWithOneKey,
			NumRotTracksWithOneKey);

		GlobalNumTransTracks += NumTransTracks;
		GlobalNumRotTracks += NumRotTracks;
		GlobalNumTransTracksWithOneKey += NumTransTracksWithOneKey;
		GlobalNumRotTracksWithOneKey += NumRotTracksWithOneKey;

		GlobalApproxCompressedSize += Seq->GetApproxCompressedSize();
		GlobalApproxKeyDataSize += (INT)((TotalNumTransKeys * TranslationKeySize) + (TotalNumRotKeys * RotationKeySize));

		GlobalNumTransKeys += TotalNumTransKeys;
		GlobalNumRotKeys += TotalNumRotKeys;

		Ar.Logf(TEXT(" %60s, %3i, %3i,%3i, %3i,%3i, %10i,%10i, %s, %i"),
			*Seq->SequenceName.ToString(),
			Seq->NumFrames,
			NumTransTracks, NumRotTracks,
			NumTransTracksWithOneKey, NumRotTracksWithOneKey,
			TotalNumTransKeys, TotalNumRotKeys,
			*FAnimationUtils::GetAnimationKeyFormatString(static_cast<AnimationKeyFormat>(Seq->KeyEncodingFormat)),
			Seq->GetResourceSize() );
	}
	Ar.Logf( TEXT("======================================================================") );
	Ar.Logf( TEXT("Total Num Tracks: %i trans, %i rot, %i trans1, %i rot1"), GlobalNumTransTracks, GlobalNumRotTracks, GlobalNumTransTracksWithOneKey, GlobalNumRotTracksWithOneKey );
	Ar.Logf( TEXT("Total Num Keys: %i trans, %i rot"), GlobalNumTransKeys, GlobalNumRotKeys );

	Ar.Logf( TEXT("Approx Compressed Memory: %i bytes"), GlobalApproxCompressedSize);
	Ar.Logf( TEXT("Approx Key Data Memory: %i bytes"), GlobalApproxKeyDataSize);
}

#endif

/************************************************************************************
 * UAnimMetaData
 ***********************************************************************************/

void UAnimMetaData::AnimSet(UAnimNodeSequence* SeqNode)
{
}

void UAnimMetaData::AnimUnSet(UAnimNodeSequence* SeqNode)
{
}

void UAnimMetaData::TickMetaData(UAnimNodeSequence* SeqNode)
{
}

/************************************************************************************
 * UAnimMetaData_SkelControl
 ***********************************************************************************/

void UAnimMetaData_SkelControl::PostLoad()
{
	Super::PostLoad();
	// Fixup switch from single SkelControlName to Array of names.
	UBOOL bMarkDirty = FALSE;
	if( GetLinkerVersion() < VER_SKELCONTROL_ANIMMETADATA_LIST )
	{
		SkelControlNameList.AddItem(SkelControlName_DEPRECATED);
		bMarkDirty = TRUE;
	}

	if( bMarkDirty && (GIsRunning || GIsUCC) )
	{
		MarkPackageDirty();
	}
}

void UAnimMetaData_SkelControl::AnimSet(UAnimNodeSequence* SeqNode)
{
	Super::AnimSet(SeqNode);

	for(INT NameIdx=0; NameIdx<SkelControlNameList.Num(); NameIdx++)
	{
		if( SkelControlNameList(NameIdx) != NAME_None )
		{
			// Find SkelControl, and add us to the list of cached AnimNodeSequences
			USkelControlBase* SkelControl = SeqNode->SkelComponent->FindSkelControl( SkelControlNameList(NameIdx) );
			if( SkelControl )
			{
				SeqNode->MetaDataSkelControlList.AddUniqueItem(SkelControl);
			}
		}
	}
}

void UAnimMetaData_SkelControl::AnimUnSet(UAnimNodeSequence* SeqNode)
{
	Super::AnimUnSet(SeqNode);

	if ( SeqNode->SkelComponent )
	{
		for(INT NameIdx=0; NameIdx<SkelControlNameList.Num(); NameIdx++)
		{
			if( SkelControlNameList(NameIdx) != NAME_None )
			{
				// Find SkelControl, and remove us from the list of cached AnimNodeSequences
				USkelControlBase* SkelControl = SeqNode->SkelComponent->FindSkelControl( SkelControlNameList(NameIdx) );
				if( SkelControl )
				{
					SeqNode->MetaDataSkelControlList.RemoveItem(SkelControl);
				}
			}
		}
	}
}

void UAnimMetaData_SkelControl::TickMetaData(UAnimNodeSequence* SeqNode)
{
	for(INT SkelControlIdx=0; SkelControlIdx<SeqNode->MetaDataSkelControlList.Num(); SkelControlIdx++)
	{
		USkelControlBase* SkelControl = SeqNode->MetaDataSkelControlList(SkelControlIdx);
		if( ShouldCallSkelControlTick(SkelControl, SeqNode) )
		{
			// Reset weights once  per frame.
			if( SkelControl->AnimMetaDataUpdateTag != SeqNode->NodeTickTag )
			{
				SkelControl->AnimMetaDataUpdateTag = SeqNode->NodeTickTag;
				SkelControl->AnimMetadataWeight = 0.f;
			}

			SkelControlTick(SkelControl, SeqNode);
		}
	}
}

/** Only call the tick function for the right SkelControl. */
UBOOL UAnimMetaData_SkelControl::ShouldCallSkelControlTick(USkelControlBase* SkelControl, UAnimNodeSequence* SeqNode)
{
	if( !bFullControlOverController || SkelControl->bControlledByAnimMetada )
	{
		for(INT NameIdx=0; NameIdx<SkelControlNameList.Num(); NameIdx++)
		{
			if( SkelControlNameList(NameIdx) == SkelControl->ControlName )
			{
				return TRUE;
			}
		}
	}
	return FALSE;
}

/** Increase AnimMetaDataWeight by our AnimNodeSequence's total weight contribution in the Tree. */
void UAnimMetaData_SkelControl::SkelControlTick(USkelControlBase* SkelControl, UAnimNodeSequence* SeqNode)
{
	if( bFullControlOverController )
	{
		// Add up this node's total weight contribution to the tree.
		// No need to clamp AnimMetadataWeight, that is done for us in the SkelControl.
		SkelControl->AnimMetadataWeight = ::Min(SkelControl->AnimMetadataWeight + SeqNode->NodeTotalWeight, 1.f);
	}
}

/************************************************************************************
 * UAnimMetaData_SkelControlKeyFrame
 ***********************************************************************************/

/** Increase AnimMetaDataWeight by our AnimNodeSequence's total weight contribution in the Tree multiplied by the key framed weight, user setup. */
void UAnimMetaData_SkelControlKeyFrame::SkelControlTick(USkelControlBase* SkelControl, UAnimNodeSequence* SeqNode)
{
	// Set default start time
	FLOAT StartTime = 0.0f, EndTime = -1.f;
	FLOAT StartStrength = 0.f, EndStrength = 0.f;

	// need to find where Current Time fits
	for (INT KeyIndex=0; KeyIndex<KeyFrames.Num(); KeyIndex++)
	{
		FTimeModifier& Key = KeyFrames(KeyIndex);
		if( Key.Time > SeqNode->CurrentTime )
		{
			EndTime = Key.Time;		
			EndStrength = Key.TargetStrength;
			break;
		}
		else
		{
			StartTime = Key.Time;
			StartStrength = Key.TargetStrength;
		}
	}

	// End time isn't set because current time is after last index
	// Set with total length, and start strength; 
	if( EndTime < 0.f )
	{
		EndTime = SeqNode->AnimSeq->SequenceLength;
		EndStrength = StartStrength;
	}

	// linear blend
	FLOAT const KeyFrameWeight = StartStrength + ((SeqNode->CurrentTime-StartTime)/(EndTime-StartTime))*(EndStrength-StartStrength);
	if( bFullControlOverController )
	{
		// If we take full control of the node, then we modulate by node's weight (for blend transitions)
		// and work on AnimMetadataWeight.
		SkelControl->AnimMetadataWeight = ::Min(SkelControl->AnimMetadataWeight + SeqNode->NodeTotalWeight * KeyFrameWeight, 1.f);
	}
	else
	{
		// Otherwise, we just overwrite the control's strength with our weight.
		// It's a simple way to override and set the weight of the bone controller.
		SkelControl->ControlStrength = KeyFrameWeight;
	}
}

/**
 *************************************************
 * HeadTrackingComponent for skeletalmeshcomponent
 *************************************************
 * This is to collect actors/properly find best candidate to look at
 * Used by Matinee/Kismet by LDs set up
 * Can be used for any kind of skeletalmeshcomponet as far as the animtree contains correct look at skelcontrol
 * When you attach this class, make sure you don't have any other HeadTrackingComponent 
 * That will create conflict. It will warn if it already has headtrackingcomponent
 */

/** Enable/Disable HeadTracking **/
void UHeadTrackingComponent::EnableHeadTracking(UBOOL bEnable)
{
	if ( bEnable )
	{
		// need to clear this up
		for( TMap<class AActor*,struct FActorToLookAt*>::TIterator It(CurrentActorMap); It; ++It )
		{
			FActorToLookAt * ActorToLookAt = It.Value();
			delete It.Value();
		}

		CurrentActorMap.Empty();

		TrackControls.Empty();
		// find new track controls
		RefreshTrackControls();
	}
	else
	{
		// need to clear this up
		for( TMap<class AActor*,struct FActorToLookAt*>::TIterator It(CurrentActorMap); It; ++It )
		{
			FActorToLookAt * ActorToLookAt = It.Value();
			delete It.Value();
		}

		CurrentActorMap.Empty();

		for (INT I=0; I<TrackControls.Num(); ++I)
		{
			if (TrackControls(I) != NULL)
			{
				TrackControls(I)->SetSkelControlStrength(0.f, 0.25f);
			}
		}

		TrackControls.Empty();
	}
}

/**
 * Attaches the component to a ParentToWorld transform, owner and scene.
 * Requires IsValidComponent() == true.
 */
void UHeadTrackingComponent::Attach()
{	
	Super::Attach();

	// if my owner has another headtrackingcomponent, warn it
	AActor * MyOwner = GetOwner();
	if ( MyOwner )
	{
		// only if actorcomponent
		for (INT ComponentIndex = 0; ComponentIndex < MyOwner->Components.Num(); ComponentIndex++)
		{
			// if this isn't me
			if (MyOwner->Components(ComponentIndex) != this)
			{
				UClass * ComponentClass = MyOwner->Components(ComponentIndex)->GetClass();
				if ( ComponentClass->IsChildOf(UHeadTrackingComponent::StaticClass()) )
				{
					// another head tracking component is already attached
					debugf(TEXT("%s already has HeadTracking Component. Adding multiple headtracking components won't work."), *MyOwner->GetName());
				}
			}
		}
	}
}

/**
 * Detaches the component from the scene it is in.
 * Requires bAttached == true
 *
 * @param bWillReattach TRUE is passed if Attach will be called immediately afterwards.  This can be used to
 *                      preserve state between reattachments.
 */
void UHeadTrackingComponent::Detach( UBOOL bWillReattach )
{
	// disable head track control
	// unless it's getting reattached
	if (!bWillReattach)
	{
		EnableHeadTracking(FALSE);
	}

	Super::Detach(bWillReattach);
}

/**
 * Updates time dependent state for this component.
 * Requires bAttached == true.
 * @param DeltaTime - The time since the last tick.
 */
void UHeadTrackingComponent::Tick(FLOAT DeltaTime)
{
	UpdateHeadTracking(DeltaTime);
	Super::Tick(DeltaTime);
}

#define DEBUG_HEADTRACKING 0

/** Get Pawn from the given Actor **/
APawn * GetPawn(AActor *Actor)
{
	if (Actor)
	{
		APawn * Pawn = Actor->GetAPawn();
		if (!Pawn)
		{
			// I can't do playercontroller since UT uses UTPlayerController to animate camera
			// For now we only support AI
			if (Actor->IsA(AController::StaticClass()))
			{
				Pawn = CastChecked<AController>(Actor)->Pawn;
			}
		}

		return Pawn;
	}

	return NULL;
}

/** Get SkeletalMeshComp from the given Actor **/
USkeletalMeshComponent * GetSkeletalMeshComp( AActor * Actor )
{
	USkeletalMeshComponent * SkeletalMeshComp=NULL;
	APawn * Pawn = GetPawn(Actor);

	if (Pawn)
	{
		SkeletalMeshComp = Pawn->Mesh;
	}
	else
	{
		// FIXME: should I change to SkeletalMeshActor?
		ASkeletalMeshActorMAT * MATActor = Cast<ASkeletalMeshActorMAT>(Actor);

		if (MATActor)
		{
			SkeletalMeshComp = MATActor->SkeletalMeshComponent;
		}
		else
		{
			debugf(TEXT("Unknown actor for head tracking track. Only support AI or SkeletalMeshActorMAT"));
		}
	}

	return  SkeletalMeshComp;
}

/** 
 * Update Acotr Map
 */
INT UHeadTrackingComponent::UpdateActorMap(FLOAT CurrentTime)
{
	// Remove any pending kill actors from list, so they will get properly GCd
	for( TMap<class AActor*,struct FActorToLookAt*>::TIterator It(CurrentActorMap); It; ++It )
	{
		FActorToLookAt * ActorToLookAt = It.Value();
		// Actor can never be NULL, as the map is exposed to GC
		if ( ActorToLookAt->Actor->ActorIsPendingKill())
		{
			delete It.Value();
			It.RemoveCurrent();
		}
	}

	if ( TrackControls.Num() > 0  && SkeletalMeshComp && SkeletalMeshComp->GetOwner() )
	{
		// find where is my position/rotation
		AActor * Owner = SkeletalMeshComp->GetOwner();
		RootMeshLocation = Owner->Location;
		RootMeshRotation = Owner->Rotation;
		// we consider first one as base, and will calculate based on first one to target
		USkelControlLookAt * LookAtControl = TrackControls(0);
		if ( LookAtControl && LookAtControl->ControlBoneIndex!=INDEX_NONE )
		{
			// change MeshLocation to be that bone index
			FBoneAtom MeshRootBA = SkeletalMeshComp->GetBoneAtom(LookAtControl->ControlBoneIndex);
			RootMeshLocation = MeshRootBA.GetOrigin();
			if (SkeletalMeshComp->SkeletalMesh)
			{
				// apply local mesh transform to the baselookdir
				FMatrix RotMatrix = FRotationMatrix(SkeletalMeshComp->SkeletalMesh->RotOrigin);
				RootMeshRotation = RotMatrix.TransformNormal(LookAtControl->BaseLookDir).Rotation();
			}
			else
			{
				RootMeshRotation = LookAtControl->BaseLookDir.Rotation();
			}
		}

		// find actors around me
		FMemMark Mark( GMainThreadMemStack );
		FCheckResult* Link=GWorld->Hash->ActorRadiusCheck( GMainThreadMemStack, RootMeshLocation, LookAtActorRadius, TRACE_Actors );
		TArray<AActor *> ActorList;
		while ( Link )
		{
			if( Link->Actor &&
				Link->Actor->bCollideActors && 
				!Link->Actor->bDeleteMe )
			{
				// go through actor list to see if I have it. 
				for ( INT ActorID =0; ActorID < ActorClassesToLookAt.Num(); ++ActorID )
				{
					if (Link->Actor->IsA( ActorClassesToLookAt(ActorID) ))
					{
						ActorList.AddUniqueItem(Link->Actor);
						break;
					}
				}
			
				Link = Link->GetNext();
			}
		}

		// add new items to the map
		for ( INT ActorID = 0; ActorID < ActorList.Num(); ++ActorID )
		{
			FActorToLookAt* ActorToLookAt = NULL;

			// if it's not in the list yet add
			if ( CurrentActorMap.HasKey(ActorList(ActorID)) == FALSE )
			{
				ActorToLookAt = new FActorToLookAt;
				ActorToLookAt->Actor = ActorList(ActorID);
				ActorToLookAt->EnteredTime = CurrentTime;
				ActorToLookAt->CurrentlyBeingLookedAt = FALSE;
				ActorToLookAt->LastKnownDistance = 0.f;
				ActorToLookAt->StartTimeBeingLookedAt = 0.f;
				ActorToLookAt->Rating = 0.f;
				CurrentActorMap.Set(ActorToLookAt->Actor, ActorToLookAt);
			}
		}

		return CurrentActorMap.Num();
	}

	return 0;
}
/**
 * Find Best Candidate from the current listing
*/
FActorToLookAt * UHeadTrackingComponent::FindBestCandidate(FLOAT CurrentTime)
{
	// now run ratings
	FLOAT  LookAtActorRadiusSq =  LookAtActorRadius * LookAtActorRadius;
	FActorToLookAt * BestCandidate = NULL;
	FLOAT	BestRating = -99999.f;

	// now update their information
	for( TMap<class AActor*,struct FActorToLookAt*>::TIterator It(CurrentActorMap); It; ++It )
	{
		FActorToLookAt * ActorToLookAt = It.Value();
		ActorToLookAt->LastKnownDistance = (RootMeshLocation-ActorToLookAt->Actor->Location).SizeSquared();
		// outside of raius, do not care, delete them
		if (ActorToLookAt->LastKnownDistance > LookAtActorRadiusSq)
		{
			delete It.Value();
			It.RemoveCurrent();
		}
		else
		{
			// update rating
			// if closer, higher rating - 1 for distance, 1 for recently entered
			FLOAT DistanceRating = 1 - ActorToLookAt->LastKnownDistance/LookAtActorRadiusSq;
			// clamp time rating. Otherwise, you're never going to get second chance
			FLOAT TimeRating = Max(-1.f, (MaxInterestTime - (CurrentTime-ActorToLookAt->EnteredTime))/MaxInterestTime);
			FLOAT LookAtRating = 0.f;
			FLOAT LookAtTime = MinLookAtTime + appFrand()*(MaxLookAtTime-MinLookAtTime);

			if (ActorToLookAt->CurrentlyBeingLookedAt)
			{
				// if less than 1 second, give boost, don't like to switch every time
				LookAtRating = (LookAtTime - (CurrentTime-ActorToLookAt->StartTimeBeingLookedAt))/LookAtTime;
			}
			else if (CurrentTime-ActorToLookAt->StartTimeBeingLookedAt < LookAtTime*2.f)
			{
				// if he has been looked at before, 
				LookAtRating = (LookAtTime - (CurrentTime-ActorToLookAt->StartTimeBeingLookedAt))/LookAtTime;
			}
			else
			{
				// first time? Give boost
				LookAtRating = 0.8f;
			}

			// if it's in front of me, have more rating
			FLOAT AngleRating = (ActorToLookAt->Actor->Location-RootMeshLocation).SafeNormal() | RootMeshRotation.Vector();
			// give boost if target is moving. More interesting to see. 
			FLOAT MovingRating = (ActorToLookAt->Actor->Velocity.IsZero())? 0.f : 1.0f;

			ActorToLookAt->Rating = DistanceRating + TimeRating + LookAtRating + AngleRating + MovingRating;
#if DEBUG_HEADTRACKING
			debugf(TEXT("HeadTracking: [%s] Ratings(%0.2f), DistanceRating(%0.2f), TimeRaiting(%0.2f), LookAtRating(%0.2f), AngleRating(%0.2f), Moving Rating(%0.2f), CurrentLookAtTime(%0.2f)"), *ActorToLookAt->Actor->GetName(), ActorToLookAt->Rating, DistanceRating, TimeRating, LookAtRating, AngleRating, MovingRating, LookAtTime );
#endif
			if ( ActorToLookAt->Rating > BestRating && ActorToLookAt->Actor )
			{
				BestRating = ActorToLookAt->Rating;
				BestCandidate = ActorToLookAt;
			}
		}
	}

	return BestCandidate;
}
/**
 *  Update Head Tracking
 */
void UHeadTrackingComponent::UpdateHeadTracking(FLOAT DeltaTime)
{
	FLOAT CurrentTime = GWorld->GetTimeSeconds();

	UpdateActorMap(CurrentTime);

	FActorToLookAt * BestCandidate = FindBestCandidate(CurrentTime);

	if (BestCandidate)
	{
		for (INT I=0; I<TrackControls.Num(); ++I)
		{
			TrackControls(I)->SetSkelControlStrength(1.f, 0.25f);
		}

#if DEBUG_HEADTRACKING
		debugf(TEXT("HeadTracking: Best Candidate [%s] "), *BestCandidate->Actor->GetName());
#endif
		if (BestCandidate->CurrentlyBeingLookedAt==FALSE)
		{
			BestCandidate->StartTimeBeingLookedAt = CurrentTime;
			for (INT I=0; I<TrackControls.Num(); ++I)
			{
				TrackControls(I)->SetLookAtAlpha(1.0f, 0.25f);
			}
		}

		BestCandidate->CurrentlyBeingLookedAt = TRUE;

		FVector TargetLoc = (BestCandidate->Actor->Location);
		if (TargetBoneNames.Num())
		{
			USkeletalMeshComponent * MeshComp = GetSkeletalMeshComp(BestCandidate->Actor);
			if (MeshComp)
			{
				for (INT TargetID=0; TargetID < TargetBoneNames.Num(); ++TargetID)
				{
					INT BoneIdx = MeshComp->MatchRefBone(TargetBoneNames(TargetID));
					if (BoneIdx != INDEX_NONE)
					{
						// found it, get out
						TargetLoc = MeshComp->GetBoneAtom(BoneIdx).GetOrigin();
						break;
					}
				}
			}
		}

		for (INT I=0; I<TrackControls.Num(); ++I)
		{
			TrackControls(I)->DesiredTargetLocation = TargetLoc;
			TrackControls(I)->InterpolateTargetLocation(DeltaTime);
		}

#if DEBUG_HEADTRACKING
		GWorld->GetWorldInfo()->FlushPersistentDebugLines();

		GWorld->GetWorldInfo()->DrawDebugCoordinateSystem(RootMeshLocation, RootMeshRotation, 10, TRUE);
		GWorld->GetWorldInfo()->DrawDebugLine(RootMeshLocation, BestCandidate->Actor->Location, 0, 0, 255, TRUE);
		GWorld->GetWorldInfo()->DrawDebugLine(RootMeshLocation, TrackControls(0)->DesiredTargetLocation, 255, 0, 0, TRUE);
		GWorld->GetWorldInfo()->DrawDebugLine(RootMeshLocation, TrackControls(0)->TargetLocation, 255, 255, 0, TRUE);
#endif
		for( TMap<class AActor*,struct FActorToLookAt*>::TIterator It(CurrentActorMap); It; ++It )
		{
			FActorToLookAt * ActorToLookAt = It.Value();
			if (ActorToLookAt != BestCandidate)
			{
				ActorToLookAt->CurrentlyBeingLookedAt = FALSE;
			}
		}
	}
	else
	{
		// if nothing else turn that off
		if (TrackControls.Num() > 0 )
		{
#if DEBUG_HEADTRACKING
			debugf(TEXT("HeadTracking: Turning it off "));
#endif
			for (INT I=0; I<TrackControls.Num(); ++I)
			{
				TrackControls(I)->SetSkelControlStrength(0.f, 0.25f);
			}
		}
	}
}

/** 
 * Refresh Head Tracking Skel Control List
 */
void UHeadTrackingComponent::RefreshTrackControls()
{
	// if head track controls are not found yet
	if (SkeletalMeshComp && TrackControls.Num() == 0 && TrackControllerName.Num() > 0 )
	{
		if ( SkeletalMeshComp && SkeletalMeshComp->SkeletalMesh && SkeletalMeshComp->Animations && SkeletalMeshComp->Animations->IsA(UAnimTree::StaticClass()) )
		{
			//now look for look at control
			UAnimTree * AnimTree = CastChecked<UAnimTree>(SkeletalMeshComp->Animations);

			if ( AnimTree )
			{
				for (INT I=0; I<TrackControllerName.Num(); ++I)
				{
					USkelControlLookAt* LookAtControl = Cast<USkelControlLookAt>(AnimTree->FindSkelControl(TrackControllerName(I)));
					if (LookAtControl)
					{
						TrackControls.AddItem(LookAtControl);
					}
				}
			}
		}

		if (TrackControls.Num() > 0)
		{
			for (INT I=0; I<TrackControls.Num(); ++I)
			{
				TrackControls(I)->bDisableBeyondLimit = bDisableBeyondLimit;
				// initialize as turn off
				TrackControls(I)->SetSkelControlStrength(0.f, 0.25f);
			}
		}
		else
		{
			debugf(TEXT("Track control not found for mesh [%s]."), *SkeletalMeshComp->SkeletalMesh->GetName() );
		}
	}
}

/** Clear list **/
void UHeadTrackingComponent::BeginDestroy()
{
	// need to clear this up before destoying
	// just in case it hasn't been detached
	for( TMap<class AActor*,struct FActorToLookAt*>::TIterator It(CurrentActorMap); It; ++It )
	{
		FActorToLookAt * ActorToLookAt = It.Value();
		delete It.Value();
	}

	CurrentActorMap.Empty();

	Super::BeginDestroy();
}

/** Make sure CurrentActorMap is referenced */
void UHeadTrackingComponent::AddReferencedObjects( TArray<UObject*>& ObjectArray )
{
	Super::AddReferencedObjects( ObjectArray );

	// Output reference for each actor in the map
	for( TMap<class AActor*,struct FActorToLookAt*>::TIterator It(CurrentActorMap); It; ++It )
	{
		// Key and Value->Actor are the same Actor
		FActorToLookAt * ActorToLookAt = It.Value();
		AddReferencedObject( ObjectArray, ActorToLookAt->Actor );
	}
}
