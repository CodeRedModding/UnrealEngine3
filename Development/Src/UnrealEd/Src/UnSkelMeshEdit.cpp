/*=============================================================================
	UnSkelMeshEdit.cpp: Unreal editor skeletal mesh/anim support
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "EngineAnimClasses.h"
#include "SkelImport.h"
#include "AnimationUtils.h"
#include "DlgCheckBoxList.h"



#if WITH_FBX
#include "UnFbxImporter.h"
#endif // WITH_FBX

IMPLEMENT_COMPARE_CONSTREF( INT, UnSkelMeshEdit, { return (A - B); } )

static UBOOL GetTrackMapAndExtend(UAnimSet* AnimSet, TArray<FName> &RawBoneNames, USkeletalMesh* FillInMesh, TArray<INT> &TrackMap, UBOOL &bAnimSetTrackChanged, TArray<UAnimSequence*> &SequencesRequiringRecompression, UBOOL bSilence)
{
	// Calculate track mapping from target tracks in AnimSet to the source among those we are loading.
	INT NumTracks = RawBoneNames.Num();
	INT bSkipSkeletonConsitencyPatching=-1;	
	if(AnimSet->TrackBoneNames.Num() == 0)
	{
		AnimSet->TrackBoneNames.Add( NumTracks );
		TrackMap.Add( NumTracks );

		for(INT IncomingTrackIndex=0; IncomingTrackIndex<NumTracks; IncomingTrackIndex++)
		{
			// If an empty AnimSet, we create the AnimSet track name table from this PSA file.
			AnimSet->TrackBoneNames(IncomingTrackIndex) = RawBoneNames(IncomingTrackIndex);
			TrackMap(IncomingTrackIndex) = IncomingTrackIndex;
		}
	}
	else
	{
		// Otherwise, ensure right track goes to right place.
		// If we are missing a track, we give a warning and refuse to import into this set.

		TrackMap.Add( AnimSet->TrackBoneNames.Num() );

		// For each track in the AnimSet, find its index in the imported data
		for(INT AnimSetBoneIndex=0; AnimSetBoneIndex<AnimSet->TrackBoneNames.Num(); AnimSetBoneIndex++)
		{
			FName TrackName = AnimSet->TrackBoneNames(AnimSetBoneIndex);

			TrackMap(AnimSetBoneIndex) = INDEX_NONE;

			for(INT IncomingTrackIndex=0; IncomingTrackIndex<NumTracks; IncomingTrackIndex++)
			{
				FName TestName = RawBoneNames(IncomingTrackIndex);
				if(TestName == TrackName)
				{	
					if( TrackMap(AnimSetBoneIndex) != INDEX_NONE )
					{
						debugf( TEXT(" DUPLICATE TRACK IN INCOMING DATA: %s"), *TrackName.ToString() );
					}
					TrackMap(AnimSetBoneIndex) = IncomingTrackIndex;
				}
			}
			if (bSkipSkeletonConsitencyPatching == 0)
			{
				continue;
			}
			// If we failed to find a track we need in the imported data - see if we want to patch it using the skeletal mesh ref pose.
			if(TrackMap(AnimSetBoneIndex) == INDEX_NONE)
			{
				// we need to skip patching of animset with whan we want only to import by existing tracks
				if (bSkipSkeletonConsitencyPatching == -1)
				{
					if ( appMsgf(AMT_YesNo, *LocalizeUnrealEd(TEXT("Error_Skip_AnimsetPatching"))) )
					{
						debugf( TEXT("Patching of animset skipped!") );
						bSkipSkeletonConsitencyPatching =0;
						continue;
					}
					else
					{
						bSkipSkeletonConsitencyPatching =1;
					}					
				}
				UBOOL bDoPatching = appMsgExf(AMT_YesNo, TRUE, bSilence, LocalizeSecure(LocalizeUnrealEd("Error_CouldNotFindTrack"), *TrackName.ToString()));
				
				if( bDoPatching )
				{
					// Check the selected skel mesh has a bone called that. If we can't find it - fail.
					INT PatchBoneIndex = FillInMesh->MatchRefBone(TrackName);
					if(PatchBoneIndex == INDEX_NONE)
					{
						appMsgExf(AMT_OK, TRUE, bSilence, LocalizeSecure(LocalizeUnrealEd("Error_CouldNotFindPatchBone"), *TrackName.ToString()));
						return FALSE;
					}
				}
				else
				{
					return FALSE;
				}
			}
		}
	}
	// we need to skip patching of animset with whan we want only to import by existing tracks
	if (bSkipSkeletonConsitencyPatching == 0)
	{
		return TRUE;
	}

	// Now we see if there are any tracks in the incoming data which do not have a use in the AnimSet. 
	// These will be thrown away unless we extend the AnimSet name table.
	TArray<FName> AnimSetMissingNames;
	for(INT IncomingTrackIndex=0; IncomingTrackIndex<NumTracks; IncomingTrackIndex++)
	{
		if(!TrackMap.ContainsItem(IncomingTrackIndex))
		{
			if (bSkipSkeletonConsitencyPatching == 0)
			{
				continue;
			}
			FName IncomingTrackName = RawBoneNames(IncomingTrackIndex);
			// we need to skip patching of animset with whan we want only to import by existing tracks
			if (bSkipSkeletonConsitencyPatching == -1)
			{
				if ( appMsgf(AMT_YesNo, *LocalizeUnrealEd(TEXT("Error_Skip_AnimsetPatching"))) )
				{
					debugf( TEXT("Patching of animset skipped!") );
					bSkipSkeletonConsitencyPatching =0;
					continue;
				}
				else
				{
					bSkipSkeletonConsitencyPatching =1;
				}					
			}
			UBOOL bDoExtension = appMsgExf(AMT_YesNo, FALSE, bSilence, LocalizeSecure(LocalizeUnrealEd("Error_ExtraInfoInAnimFile"), *IncomingTrackName.ToString()));
			if(bDoExtension)
			{
				INT PatchBoneIndex = FillInMesh->MatchRefBone(IncomingTrackName);

				// If we can't find the extra track in the SkelMesh to create an animation from, warn and fail.
				if(PatchBoneIndex == INDEX_NONE)
				{
					appMsgExf(AMT_OK, TRUE, bSilence, LocalizeSecure(LocalizeUnrealEd("Error_CouldNotFindPatchBone"), *IncomingTrackName.ToString()));
					return FALSE;
				}
				// If we could, add to all animations in the AnimSet, and add to track table.
				else
				{
					AnimSet->TrackBoneNames.AddItem(IncomingTrackName);
					bAnimSetTrackChanged = TRUE;

					// Iterate over all existing sequences in this set and add an extra track to the end.
					for(INT SetAnimIndex=0; SetAnimIndex<AnimSet->Sequences.Num(); SetAnimIndex++)
					{
						UAnimSequence* ExtendSeq = AnimSet->Sequences(SetAnimIndex);

						// Remove any compression on the sequence so that it will be recomputed with the new track.
						if ( ExtendSeq->CompressedTrackOffsets.Num() > 0 )
						{
							ExtendSeq->CompressedTrackOffsets.Empty();
							// Mark the sequence as requiring recompression.
							SequencesRequiringRecompression.AddUniqueItem( ExtendSeq );
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

							// Extend AdditiveBasePose
							const FMeshBone& RefSkelBone = FillInMesh->RefSkeleton(PatchBoneIndex);

							FBoneAtom RefBoneAtom(
								RefSkelBone.BonePos.Orientation,
								RefSkelBone.BonePos.Position);
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
							const FVector RefPosition = FillInMesh->RefSkeleton(PatchBoneIndex).BonePos.Position;
							RawTrack.PosKeys.AddItem(RefPosition);

							FQuat RefOrientation = FillInMesh->RefSkeleton(PatchBoneIndex).BonePos.Orientation;
							// To emulate ActorX-exported animation quat-flipping, we do it here.
							if( PatchBoneIndex > 0 )
							{
								RefOrientation.W *= -1.f;
							}
							RawTrack.RotKeys.AddItem(RefOrientation);
						}
					}

					// So now the new incoming track 'IncomingTrackIndex' maps to the last track in the AnimSet.
					TrackMap.AddItem(IncomingTrackIndex);
				}
			}
		}
	}
	
	return TRUE;
}

static void PostProcessSequence(UAnimSequence* DestSeq, TArray<AdditiveAnimRebuildInfo> &AdditiveAnimRebuildList, UBOOL bSilence = FALSE)
{
	// Lossless compression for Raw data
	UBOOL bMarkDirty = DestSeq->CompressRawAnimData();

	// Apply compression
	FAnimationUtils::CompressAnimSequence(DestSeq, NULL, FALSE, FALSE);

	DestSeq->MarkPackageDirty(bMarkDirty);

	// Rebuild additive animations if necessary
	if( AdditiveAnimRebuildList.Num() > 0 )
	{
		// Show dialog to select which animations should be rebuilt.
		WxGenericDlgCheckBoxList<INT> dlg;
		for ( INT I=0; I<AdditiveAnimRebuildList.Num(); ++I )
		{
			dlg.AddCheck(I, AdditiveAnimRebuildList(I).AdditivePose->SequenceName.ToString(), TRUE);
		}

		// Show and confirm
		FIntPoint WindowSize(300, 500);
		INT Result = wxID_OK;
		
		if (!bSilence)
		{
			Result = dlg.ShowDialog( TEXT("Rebuilt Additive Animations"), TEXT("Automatically Rebuild Additive Animations"), WindowSize, TEXT("AutoRebuildAdditiveAnimations"));
		}

		if( Result == wxID_OK )
		{
			// Delete elements that were unchecked
			TArray<INT> KeysToDelete;
			dlg.GetResults(KeysToDelete, FALSE);
			if( KeysToDelete.Num() > 0 )
			{
				// Make sure indices are sorted in increasing order.
				Sort<USE_COMPARE_CONSTREF(INT, UnSkelMeshEdit)>(&KeysToDelete(0), KeysToDelete.Num());

				// Go through all sequence
				for(INT KeyIndex=KeysToDelete.Num()-1; KeyIndex>=0; KeyIndex--)
				{
					AdditiveAnimRebuildList.Remove( KeysToDelete(KeyIndex), 1 );
				}
			}
			FAnimationUtils::RebuildAdditiveAnimations(AdditiveAnimRebuildList);
		}
	}
}

static void FlushLinkupCache(UAnimSet* AnimSet, TArray<UAnimSequence*> &SequencesRequiringRecompression)
{
	AnimSet->LinkupCache.Empty();
	AnimSet->SkelMesh2LinkupCache.Empty();

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
	for( INT SequenceIndex = 0 ; SequenceIndex < SequencesRequiringRecompression.Num() ; ++SequenceIndex )
	{
		UAnimSequence* AnimSeq = SequencesRequiringRecompression( SequenceIndex );
		FAnimationUtils::CompressAnimSequence(AnimSeq, NULL, FALSE, FALSE);
	}
}

/**
 * Performs any updates and recalculations on notifiers attached to an animset.  This is useful to recalculate data
 * (such as AnimTrail info) in the event that an animation or mesh has been reimported.
 */
void UEditorEngine::UpdateAnimSetNotifiers(UAnimSet* AnimSet, UAnimNodeSequence* NodeSequence)
{
	if (AnimSet == NULL || NodeSequence == NULL)
	{
		return;
	}

	//Save off some the current setting in case we need to manipulate the sequence
	FName SavedAnimSeqName = NodeSequence->AnimSeqName;
	FLOAT SavedAnimPos = NodeSequence->CurrentTime;
	UBOOL bRestoreSeq = FALSE;

	//run through the animation sequences to update the notifiers
	INT NumSeqs = AnimSet->Sequences.Num();
	for (INT SeqIndex = 0; SeqIndex < NumSeqs; SeqIndex++)
	{
		UAnimSequence *AnimSeq = AnimSet->Sequences(SeqIndex);
		INT NumNotifiers = AnimSeq->Notifies.Num();
		for (INT NotifyIndex = 0; NotifyIndex < NumNotifiers; NotifyIndex++)
		{
			FAnimNotifyEvent* NotifierEvent = &(AnimSeq->Notifies(NotifyIndex));
			if (NotifierEvent && NotifierEvent->Notify)
			{
				UAnimNotify *Notifier = NotifierEvent->Notify;
			
				//update anim trail notifiers
				UAnimNotify_Trails *AnimTrailNotifier = Cast<UAnimNotify_Trails>(Notifier);
				if (AnimTrailNotifier)
				{
					NodeSequence->SetAnim(AnimSeq->SequenceName);
					bRestoreSeq = TRUE;
				
					AnimTrailNotifier->LastStartTime = NotifierEvent->Time;
					AnimTrailNotifier->StoreAnimationData(NodeSequence);
				}
				
				//other updates
				//@todo?
			}
		}
	}
	
	//restore the sequence to its previous state if needed
	if (bRestoreSeq)
	{
		NodeSequence->SetAnim(SavedAnimSeqName);
		NodeSequence->SetPosition(SavedAnimPos, FALSE);
		NodeSequence->SkelComponent->ForceSkelUpdate();
	}
}



/**
 * Open a PSA file with the given name, and import each sequence into the supplied AnimSet.
 * This is only possible if each track expected in the AnimSet is found in the target file. If not the case, a warning is given and import is aborted.
 * If AnimSet is empty (ie. TrackBoneNames is empty) then we use this PSA file to form the track names array.
 */
void UEditorEngine::ImportPSAIntoAnimSet( UAnimSet* AnimSet, const TCHAR* Filename, class USkeletalMesh* FillInMesh, UBOOL bSilence )
{
	// Open skeletal animation key file and read header.
	FArchive* AnimFile = GFileManager->CreateFileReader( Filename, 0, GLog );
	if( !AnimFile )
	{
		appMsgExf( AMT_OK, TRUE, bSilence, LocalizeSecure(LocalizeUnrealEd("Error_ErrorOpeningAnimFile"), Filename) );
		return;
	}

	// Read main header
	VChunkHeader ChunkHeader;
	AnimFile->Serialize( &ChunkHeader, sizeof(VChunkHeader) );
	if( AnimFile->IsError() )
	{
		appMsgExf( AMT_OK, TRUE, bSilence, LocalizeSecure(LocalizeUnrealEd("Error_ErrorReadingAnimFile"), Filename) );
		delete AnimFile;
		return;
	}

	// First I need to verify if curve data would exist
	// if version is higher than this, curve data is expected
	UBOOL bCheckCurveData = ChunkHeader.TypeFlag >= 20090127;
	
	/////// Read the bone names - convert into array of FNames.
	AnimFile->Serialize( &ChunkHeader, sizeof(VChunkHeader) );

	INT NumPSATracks = ChunkHeader.DataCount; // Number of tracks of animation. One per bone.

	struct FNamedBoneBinaryNoAlign
	{
		ANSICHAR   Name[64];	// Bone's name
		DWORD      Flags;		// reserved
		INT        NumChildren; //
		INT		   ParentIndex;	// 0/NULL if this is the root bone.  
		VJointPosNoAlign  BonePos;	    //
	};


	/** This is due to keep the alignment working with ActorX format 
	 * FQuat in ActorX isn't 16 bit aligned, so when serialize using sizeof, this does not match up, 
	 */
	TArray<FNamedBoneBinaryNoAlign> RawBoneNamesBinNoAlign;
	RawBoneNamesBinNoAlign.Add(NumPSATracks);
	AnimFile->Serialize( &RawBoneNamesBinNoAlign(0), sizeof(FNamedBoneBinaryNoAlign) * ChunkHeader.DataCount);

	// Now copy over from no align to align
	TArray<FNamedBoneBinary> RawBoneNamesBin;
	RawBoneNamesBin.Add(NumPSATracks);

	for (INT I=0; I<RawBoneNamesBinNoAlign.Num(); ++I)
	{
		//do this before checking for duplicates, so we can have good error strings
		appMemcpy(RawBoneNamesBin(I).Name, RawBoneNamesBinNoAlign(I).Name, sizeof(ANSICHAR)*64);

		//ensure there are no duplicate names
		for(INT J = I+1; J < RawBoneNamesBinNoAlign.Num(); ++J)
		{
			if(appMemcmp( RawBoneNamesBinNoAlign(I).Name, RawBoneNamesBinNoAlign(J).Name, sizeof(ANSICHAR)*64 ) == 0)
			{
				FNamedBoneBinary& Bone = RawBoneNamesBin( I );
				FString BoneName = FSkeletalMeshBinaryImport::FixupBoneName( Bone.Name );
				appMsgExf( AMT_OK, TRUE, bSilence, LocalizeSecure( LocalizeUnrealEd("Error_DuplicateBoneName"), Filename, *BoneName ) );
				return;
			}
		}
		appMemcpy(&RawBoneNamesBin(I).Flags, &RawBoneNamesBinNoAlign(I).Flags, sizeof(DWORD)+sizeof(INT)+sizeof(INT));
		RawBoneNamesBin(I).BonePos.Orientation = FQuat(RawBoneNamesBinNoAlign(I).BonePos.Orientation.X, RawBoneNamesBinNoAlign(I).BonePos.Orientation.Y, RawBoneNamesBinNoAlign(I).BonePos.Orientation.Z, RawBoneNamesBinNoAlign(I).BonePos.Orientation.W);
		appMemcpy(&RawBoneNamesBin(I).BonePos.Position, &RawBoneNamesBinNoAlign(I).BonePos.Position, sizeof(FVector)+sizeof(FLOAT)*4);
	}

	TArray<FName> RawBoneNames;
	RawBoneNames.Add(NumPSATracks);
	for(INT i=0; i<NumPSATracks; i++)
	{
		FNamedBoneBinary& Bone = RawBoneNamesBin( i );
		FString BoneName = FSkeletalMeshBinaryImport::FixupBoneName( Bone.Name );
		RawBoneNames( i ) = FName( *BoneName );
	}

	/////// Read the animation sequence infos
	AnimFile->Serialize( &ChunkHeader, sizeof(VChunkHeader) );

	INT NumPSASequences = ChunkHeader.DataCount;

	TArray<AnimInfoBinary> RawAnimSeqInfo; // Array containing per-sequence information (sequence name, key range etc)
	RawAnimSeqInfo.Add(NumPSASequences);
	AnimFile->Serialize( &RawAnimSeqInfo(0), sizeof(AnimInfoBinary) * ChunkHeader.DataCount);


	/////// Read the raw keyframe data (1 key per
	AnimFile->Serialize( &ChunkHeader, sizeof(VChunkHeader) );

	/** This is due to keep the alignment working with ActorX format 
	* FQuat in ActorX isn't 16 bit aligned, so when serialize using sizeof, this does not match up, 
	*/
	struct VQuatAnimKeyNoAlign
	{
		FVector			Position;           // relative to parent.
		FQuatNoAlign	Orientation;        // relative to parent.
		FLOAT			Time;				// The duration until the next key (end key wraps to first...)
	};

	TArray<VQuatAnimKey> RawAnimKeys;
	RawAnimKeys.Add(ChunkHeader.DataCount);

	TArray<VQuatAnimKeyNoAlign> RawAnimKeysNoAlign;
	RawAnimKeysNoAlign.Add(ChunkHeader.DataCount);

	AnimFile->Serialize( &RawAnimKeysNoAlign(0), sizeof(VQuatAnimKeyNoAlign) * ChunkHeader.DataCount);	
	
	// Now copy over from no align to align
	for (INT I=0; I<RawAnimKeysNoAlign.Num(); ++I)
	{
		appMemcpy(&RawAnimKeys(I).Position, &RawAnimKeysNoAlign(I).Position, sizeof(FVector));
		RawAnimKeys(I).Orientation = FQuat(RawAnimKeysNoAlign(I).Orientation.X, RawAnimKeysNoAlign(I).Orientation.Y, RawAnimKeysNoAlign(I).Orientation.Z, RawAnimKeysNoAlign(I).Orientation.W);
		appMemcpy(&RawAnimKeys(I).Time, &RawAnimKeysNoAlign(I).Time, sizeof(FLOAT));
	}

	/////// Read the raw curve data information
	struct FBlendCurve
	{
		TArray<VBlendCurve> RawCurveTracks;
	};

	TArray<FBlendCurve> BlendCurves;

	if (bCheckCurveData)
	{
		// to read curve, scale data has to be read first
		// this seems legacy data, but I can't remover it - you never know who might be using this
		struct VScaleAnimKey
		{	
			FVector ScaleVector;   // If uniform scaling is required, just use the X component..
			FLOAT   Time;          // disregarded	
		};

		AnimFile->Serialize( &ChunkHeader, sizeof(VChunkHeader) );

		TArray<VScaleAnimKey> RawScaleKeys;
		RawScaleKeys.Add(ChunkHeader.DataCount);

		AnimFile->Serialize( &RawScaleKeys(0), sizeof(VScaleAnimKey) * ChunkHeader.DataCount);	
	
		// now add blend curves
		BlendCurves.AddZeroed(RawAnimSeqInfo.Num());
		
		/// for each animation, grab chunk header and read data and save it to blendcurve
		for (INT I=0; I<RawAnimSeqInfo.Num(); ++I)
		{
			AnimFile->Serialize( &ChunkHeader, sizeof(VChunkHeader) );

			// num of curves are stored in datacount
			// num of keys are the number of frames
			INT NumCurves = ChunkHeader.DataCount;
			INT NumKeys = RawAnimSeqInfo(I).NumRawFrames;
			
			BlendCurves(I).RawCurveTracks.AddZeroed(NumCurves);

			for (INT J=0; J<NumCurves; ++J)
			{
				BlendCurves(I).RawCurveTracks(J).RawWeightKeys.AddZeroed(NumKeys);
				AnimFile->Serialize( BlendCurves(I).RawCurveTracks(J).RawCurveName, 128);	
				AnimFile->Serialize( &(BlendCurves(I).RawCurveTracks(J).RawWeightKeys(0)), sizeof(FLOAT) * NumKeys);	
			}
		}
	}

	// Have now read all information from file - can release handle.
	delete AnimFile;
	AnimFile = NULL;

	// Y-flip quaternions and translations from Max/Maya/etc space into Unreal space.
	for( INT i=0; i<RawAnimKeys.Num(); i++)
	{
		FQuat Bone = RawAnimKeys(i).Orientation;
		Bone.W = - Bone.W;
		Bone.Y = - Bone.Y;
		RawAnimKeys(i).Orientation = Bone;

		FVector Pos = RawAnimKeys(i).Position;
		Pos.Y = - Pos.Y;
		RawAnimKeys(i).Position = Pos;
	}


	// Calculate track mapping from target tracks in AnimSet to the source among those we are loading.
	TArray<INT> TrackMap;
	TArray<UAnimSequence*> SequencesRequiringRecompression;
	// Flag to indicate that the AnimSet has had its TrackBoneNames array changed, and therefore its LinkupCache needs to be flushed.
	UBOOL bAnimSetTrackChanged = FALSE;
	
	if ( !GetTrackMapAndExtend(AnimSet, RawBoneNames, FillInMesh, TrackMap, bAnimSetTrackChanged, SequencesRequiringRecompression, bSilence))
	{
		return;
	}

	// For each sequence in the PSA file, find the UAnimSequence we want to import into.
	UBOOL bAskAboutReplacing = TRUE;
	UBOOL bDoReplace = FALSE;
	for(INT SeqIdx=0; SeqIdx<NumPSASequences; SeqIdx++)
	{
		AnimInfoBinary& PSASeqInfo = RawAnimSeqInfo(SeqIdx);

		// Get sequence name as an FName
		appTrimSpaces( &PSASeqInfo.Name[0] );
		FName SeqName = FName( ANSI_TO_TCHAR(&PSASeqInfo.Name[0]) );

		// See if this sequence already exists.
		UAnimSequence* DestSeq = AnimSet->FindAnimSequence(SeqName);

		// If we're overwriting an animation that had references to an additive, keep that information, so we can update the re-update the additive animations.
		TArray<AdditiveAnimRebuildInfo> AdditiveAnimRebuildList;

		// If not, create new one now.
		if(!DestSeq)
		{
			DestSeq = ConstructObject<UAnimSequence>( UAnimSequence::StaticClass(), AnimSet );
			AnimSet->Sequences.AddItem( DestSeq );
		}
		else
		{
			if (bAskAboutReplacing)
			{
				// if it's different animset, ask if they'd like to replace
				UINT MsgResult = appMsgExf(AMT_YesNoYesAllNoAll, TRUE, bSilence, LocalizeSecure(LocalizeUnrealEd("Prompt_25"), *SeqName.ToString()));
				bDoReplace = MsgResult == ART_Yes || MsgResult == ART_YesAll;
				bAskAboutReplacing = MsgResult == ART_No || MsgResult == ART_Yes;
			}

			if(!bDoReplace)
			{
				continue; // Move on to next sequence...
			}

			// We're overriding an existing animation.
			// If that animation was involved in building an additive animation, get a list of them so we can auto rebuild them with the new data.
			FAnimationUtils::GetAdditiveAnimRebuildList(DestSeq, AdditiveAnimRebuildList);
			DestSeq->RecycleAnimSequence();
		}

		DestSeq->SequenceName = SeqName;
		DestSeq->SequenceLength = PSASeqInfo.TrackTime / PSASeqInfo.AnimRate; // Time of animation if playback speed was 1.0
		DestSeq->NumFrames = PSASeqInfo.NumRawFrames;

		// Make sure data is zeroes
		DestSeq->RawAnimationData.AddZeroed( AnimSet->TrackBoneNames.Num() );
		
		// Clear before importing 
		DestSeq->CurveData.Empty();

		UBOOL bMarkDirty = FALSE;
		// add curve data if found
		if (BlendCurves.Num() > 0)
		{
			for (INT I=0; I<BlendCurves(SeqIdx).RawCurveTracks.Num(); ++I)
			{
				FCurveTrack Curve;
				appMemzero(&Curve, sizeof(FCurveTrack));
				Curve.CurveName = FName(ANSI_TO_TCHAR(BlendCurves(SeqIdx).RawCurveTracks(I).RawCurveName));
				Curve.CurveWeights.Append(BlendCurves(SeqIdx).RawCurveTracks(I).RawWeightKeys);

				// adds it if it has valid curve track
				if (Curve.IsValidCurveTrack())
				{
					bMarkDirty |= Curve.CompressCurveWeights();
					DestSeq->CurveData.AddItem(Curve);
					
				}
				
			}
		}

		DestSeq->MarkPackageDirty(bMarkDirty);

		// Structure of data is this:
		// RawAnimKeys contains all keys. 
		// Sequence info FirstRawFrame and NumRawFrames indicate full-skel frames (NumPSATracks raw keys). Ie number of elements we need to copy from RawAnimKeys is NumRawFrames * NumPSATracks.

		// Import each track.
		check( AnimSet->TrackBoneNames.Num() == DestSeq->RawAnimationData.Num() );
		for(INT TrackIdx = 0; TrackIdx < AnimSet->TrackBoneNames.Num(); TrackIdx++)
		{
			UBOOL bAnimationKeyVerified = TRUE;
			FRawAnimSequenceTrack& RawTrack = DestSeq->RawAnimationData(TrackIdx);

			// Find the source track for this one in the AnimSet
			INT SourceTrackIdx = TrackMap( TrackIdx );

			// If bone was not found in incoming data, use SkeletalMesh to create the track.
			if(SourceTrackIdx == INDEX_NONE)
			{
				FName TrackName = AnimSet->TrackBoneNames(TrackIdx);
				INT PatchBoneIndex = FillInMesh->MatchRefBone(TrackName);
				check(PatchBoneIndex != INDEX_NONE); // Should have checked for this case above!

				// Create 1-frame animation from the reference pose of the skeletal mesh.
				// This is basically what the compression does, so should be fine.

				FVector RefPosition = FillInMesh->RefSkeleton(PatchBoneIndex).BonePos.Position;
				RawTrack.PosKeys.AddItem(RefPosition);

				FQuat RefOrientation = FillInMesh->RefSkeleton(PatchBoneIndex).BonePos.Orientation;
				// To emulate ActorX-exported animation quat-flipping, we do it here.
				if(PatchBoneIndex > 0)
				{
					RefOrientation.W *= -1.f;
				}
				RawTrack.RotKeys.AddItem(RefOrientation);
			}
			else
			{
				check(SourceTrackIdx >= 0 && SourceTrackIdx < NumPSATracks);

				RawTrack.PosKeys.Add(DestSeq->NumFrames);
				RawTrack.RotKeys.Add(DestSeq->NumFrames);

				for(INT KeyIdx = 0; KeyIdx < DestSeq->NumFrames; KeyIdx++)
				{
					INT SrcKeyIdx = ((PSASeqInfo.FirstRawFrame + KeyIdx) * NumPSATracks) + SourceTrackIdx;
					
					if ( RawAnimKeys.IsValidIndex(SrcKeyIdx) )
					{
						VQuatAnimKey& RawSrcKey = RawAnimKeys(SrcKeyIdx);

						RawTrack.PosKeys(KeyIdx) = RawSrcKey.Position;
						RawTrack.RotKeys(KeyIdx) = RawSrcKey.Orientation;
					}
					else
					{
						FName TrackName = AnimSet->TrackBoneNames(TrackIdx);
						// only say once - it will be very annoying
						if (bAnimationKeyVerified)
						{
							appMsgExf( AMT_OK, TRUE, bSilence, LocalizeSecure(LocalizeUnrealEd("Prompt_40"), *SeqName.ToString(), *TrackName.ToString()) );
							bAnimationKeyVerified = FALSE;
						}

						INT PatchBoneIndex = FillInMesh->MatchRefBone(TrackName);
						check(PatchBoneIndex != INDEX_NONE); // Should have checked for this case above!
						RawTrack.PosKeys(KeyIdx) = FillInMesh->RefSkeleton(PatchBoneIndex).BonePos.Position;
						
						FQuat RefOrientation = FillInMesh->RefSkeleton(PatchBoneIndex).BonePos.Orientation;
						// To emulate ActorX-exported animation quat-flipping, we do it here.
						if(PatchBoneIndex > 0)
						{
							RefOrientation.W *= -1.f;
						}
						RawTrack.RotKeys(KeyIdx) = RefOrientation;
					}
				}
			}
		}

		PostProcessSequence(DestSeq, AdditiveAnimRebuildList, bSilence);
	}
	
	// If we need to, flush the LinkupCache.
	if (bAnimSetTrackChanged)
	{
		FlushLinkupCache(AnimSet, SequencesRequiringRecompression);
	}
}


#if WITH_FBX

void UEditorEngine::ImportFbxANIMIntoAnimSet( UAnimSet* AnimSet, const TCHAR* InFilename, USkeletalMesh* FillInMesh, UBOOL bImportMorphTracks, UBOOL& bHideMissingTrackError )
{
	GWarn->BeginSlowTask( *LocalizeUnrealEd(TEXT("ImportingFbxAnimations")), TRUE );

	UnFbx::CFbxImporter* FbxImporter = UnFbx::CFbxImporter::GetInstance();

	const UBOOL bPrevImportMorph = FbxImporter->ImportOptions->bImportMorph;
	FbxImporter->ImportOptions->bImportMorph = bImportMorphTracks;
	if ( !FbxImporter->ImportFromFile( InFilename ) )
	{
		// Log the error message and fail the import.
		GWarn->Log( NAME_Error, FbxImporter->GetErrorMessage() );
	}
	else
	{
		// Log the import message and import the mesh.
		GWarn->Log( FbxImporter->GetErrorMessage() );

		const FFilename Filename( InFilename );

		// Get Mesh nodes array that bind to the skeleton system, then morph animation is imported.
		TArray<FbxNode*> FBXMeshNodeArray;
		FbxNode* SkeletonRoot = FbxImporter->FindFBXMeshesByBone(FillInMesh, TRUE, FBXMeshNodeArray);

		if (!SkeletonRoot)
		{
			if (!bHideMissingTrackError)
			{
				appMsgf(AMT_OK, LocalizeSecure(LocalizeUnrealEd("Error_CouldNotFindFbxTrack"), *FillInMesh->RefSkeleton(0).Name.ToString()));
			}
			bHideMissingTrackError = TRUE;
			FbxImporter->ReleaseScene();
			GWarn->EndSlowTask();
			return;
		}

		// Check for blend shape curves that are not skinned.  Unskinned geometry can still contain morph curves
		if( bImportMorphTracks )
		{
			TArray<FbxNode*> MeshNodes;
			FbxImporter->FillFbxMeshArray( FbxImporter->Scene->GetRootNode(), MeshNodes, FbxImporter );

			for( INT NodeIndex = 0; NodeIndex < MeshNodes.Num(); ++NodeIndex )
			{
				// Its possible the nodes already exist so make sure they are only added once
				FBXMeshNodeArray.AddUniqueItem( MeshNodes(NodeIndex) );
			}
		}

		TArray<FbxNode*> SortedLinks;
		FbxImporter->RecursiveBuildSkeleton(SkeletonRoot, SortedLinks);

		if(SortedLinks.Num() == 0)
		{
			appMsgf(AMT_OK, *LocalizeUnrealEd("Error_CouldNotBuildValidSkeleton"));
		}
		else
		{
			FbxImporter->ImportAnimSet( FillInMesh, SortedLinks, Filename.GetBaseFilename(), FBXMeshNodeArray, AnimSet );
		}
	}

	FbxImporter->ImportOptions->bImportMorph = bPrevImportMorph;
	FbxImporter->ReleaseScene();
	GWarn->EndSlowTask();
}


// The Unroll filter expects only rotation curves, we need to walk the scene and extract the
// rotation curves from the nodes property. This can become time consuming but we have no choice.
static void ApplyUnroll(FbxNode *pNode, FbxAnimLayer* pLayer, FbxAnimCurveFilterUnroll* pUnrollFilter)
{
	if (!pNode || !pLayer || !pUnrollFilter)
	{
		return;
	}

	FbxAnimCurveNode* lCN = pNode->LclRotation.GetCurveNode(pLayer);
	if (lCN)
	{
		FbxAnimCurve* lRCurve[3];
		lRCurve[0] = lCN->GetCurve(0);
		lRCurve[1] = lCN->GetCurve(1);
		lRCurve[2] = lCN->GetCurve(2);

		// Set bone rotation order
		FbxRotationOrderEnum RotationOrder = eEulerXYZ;
		pNode->GetRotationOrder(FbxNode::eSOURCE_SET, RotationOrder);
		pUnrollFilter->SetRotationOrder(RotationOrder*2);

		pUnrollFilter->Apply(lRCurve, 3);
	}

	for (INT i = 0; i < pNode->GetChildCount(); i++)
	{
		ApplyUnroll(pNode->GetChild(i), pLayer, pUnrollFilter);
	}
}

void UnFbx::CFbxImporter::MergeAllLayerAnimation(FbxAnimStack* AnimStack, INT ResampleRate)
{
	FbxTime lFramePeriod;
	lFramePeriod.SetSecondDouble(1.0 / ResampleRate);

	FbxTimeSpan lTimeSpan = AnimStack->GetLocalTimeSpan();
	AnimStack->BakeLayers(Scene->GetEvaluator(), lTimeSpan.GetStart(), lTimeSpan.GetStop(), lFramePeriod);

	// always apply unroll filter
	FbxAnimCurveFilterUnroll UnrollFilter;

	FbxAnimLayer* lLayer = dynamic_cast<FbxAnimLayer*>(AnimStack->GetMember(FBX_TYPE(FbxAnimLayer),0));
	UnrollFilter.Reset();
	ApplyUnroll(Scene->GetRootNode(), lLayer, &UnrollFilter);
}


//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
/**
* Add to the animation set, the animations contained within the FBX document, for the given skeletal mesh
*/
void UnFbx::CFbxImporter::ImportAnimSet(USkeletalMesh* SkeletalMesh, TArray<FbxNode*>& SortedLinks, FString Filename, TArray<FbxNode*>& NodeArray, UAnimSet* AnimSet /*=NULL*/ )
{
	INT ValidTakeCount = 0;

	// If there are no valid links, then we cannot import the anim set
	if(SortedLinks.Num() == 0)
	{
		return;
	}
	
	INT AnimStackCount = Scene->GetSrcObjectCount(FbxAnimStack::ClassId);

	INT AnimStackIndex;
	for (AnimStackIndex = 0; AnimStackIndex < AnimStackCount; AnimStackIndex++ )
	{
		FbxAnimStack* CurAnimStack = FbxCast<FbxAnimStack>(Scene->GetSrcObject(FbxAnimStack::ClassId, AnimStackIndex));
		// set current anim stack
		Scene->GetEvaluator()->SetContext(CurAnimStack);
		
		FbxTimeSpan AnimTimeSpan(FBXSDK_TIME_INFINITE,FBXSDK_TIME_MINUS_INFINITE);
		// we want even empty anim stack (take) because it can hold pose
		SortedLinks(0)->GetAnimationInterval(AnimTimeSpan);
		ValidTakeCount++;
		if (ImportOptions->bImportMorph) // check morph animation
		{
			UBOOL bBlendCurveFound = FALSE;
		
			for ( INT NodeIndex = 0; !bBlendCurveFound && NodeIndex < NodeArray.Num(); NodeIndex++ )
			{
				// consider blendshape animation curve
				FbxGeometry* Geometry = (FbxGeometry*)NodeArray(NodeIndex)->GetNodeAttribute();
				if (Geometry)
				{
					INT BlendShapeDeformerCount = Geometry->GetDeformerCount(FbxDeformer::eBLENDSHAPE);
					for(INT BlendShapeIndex = 0; BlendShapeIndex<BlendShapeDeformerCount; ++BlendShapeIndex)
					{
						FbxBlendShape* BlendShape = (FbxBlendShape*)Geometry->GetDeformer(BlendShapeIndex, FbxDeformer::eBLENDSHAPE);
						INT BlendShapeChannelCount = BlendShape->GetBlendShapeChannelCount();
						for(INT ChannelIndex = 0; ChannelIndex<BlendShapeChannelCount; ++ChannelIndex)
						{
							FbxBlendShapeChannel* Channel = BlendShape->GetBlendShapeChannel(ChannelIndex);
				
							if(Channel)
							{
								FbxAnimCurve* Curve = Geometry->GetShapeChannel(BlendShapeIndex, ChannelIndex, (FbxAnimLayer*)CurAnimStack->GetMember(0));
								if (Curve && Curve->KeyGetCount() > 0)
								{
									ValidTakeCount++;
									bBlendCurveFound = TRUE;
									break;
								}
							}
						}
					}
				}
			}
		}
	}

	if (ValidTakeCount == 0)
	{
		return;
	}
	
	TArray<FName> FbxRawBoneNames;

	INT TrackNum = SortedLinks.Num();

	for (INT BoneIndex = 0; BoneIndex < TrackNum; BoneIndex++)
	{
		FbxRawBoneNames.AddItem(FName(*FSkeletalMeshBinaryImport::FixupBoneName( (char*)MakeName(SortedLinks(BoneIndex)->GetName()) )));
	}

	// ensure there are no duplicated names
	for (INT I = 0; I < TrackNum; I++)
	{
		for ( INT J = I+1; J < TrackNum; J++ )
		{
			if (FbxRawBoneNames(I) == FbxRawBoneNames(J))
			{
				FString RawBoneName = FbxRawBoneNames(J).ToString();
				appMsgf( AMT_OK, LocalizeSecure( LocalizeUnrealEd("Error_DuplicateBoneName"), *Filename, *RawBoneName ) );
				return;
			}
		}
	}

	if (!AnimSet)
	{
		// TODO: check if the animset name existed
		
		// create a new AnimSet to be filled with animSequences
		FString SkeletonName = ANSI_TO_TCHAR(MakeName(SortedLinks(0)->GetName()));
		AnimSet = ConstructObject<UAnimSet>( UAnimSet::StaticClass(), Parent, *SkeletonName, RF_Public|RF_Standalone );
	}

	// Calculate track mapping from target tracks in AnimSet to the source among those we are loading.
	TArray<INT> TrackMap;
	TArray<UAnimSequence*> SequencesRequiringRecompression;
	// Flag to indicate that the AnimSet has had its TrackBoneNames array changed, and therefore its LinkupCache needs to be flushed.
	UBOOL bAnimSetTrackChanged = FALSE;
	
	if ( !GetTrackMapAndExtend(AnimSet, FbxRawBoneNames, SkeletalMesh, TrackMap, bAnimSetTrackChanged, SequencesRequiringRecompression, FALSE))
	{
		return;
	}
	
	INT ResampleRate = 30;
	if ( !ImportOptions->bResample )
	{
		// For FBX data, "Frame Rate" is just the speed at which the animation is played back.  It can change
		// arbitrarily, and the underlying data can stay the same.  What we really want here is the Sampling Rate,
		// ie: the number of animation keys per second.  These are the individual animation curve keys
		// on the FBX nodes of the skeleton.  So we loop through the nodes of the skeleton and find the maximum number 
		// of keys that any node has, then divide this by the total length (in seconds) of the animation to find the 
		// sampling rate of this set of data 

		// we want the maximum resample rate, so that we don't lose any precision of fast anims,
		// and don't mind creating lerped frames for slow anims
		INT MaxStackResampleRate = 0;

		for( INT AnimStackIndex = 0; AnimStackIndex < AnimStackCount; AnimStackIndex++ )
		{
			FbxAnimStack* CurAnimStack = FbxCast<FbxAnimStack>(Scene->GetSrcObject(FbxAnimStack::ClassId, AnimStackIndex));

			FbxTimeSpan AnimStackTimeSpan = CurAnimStack->GetLocalTimeSpan();
		
			DOUBLE AnimStackStart = AnimStackTimeSpan.GetStart().GetSecondDouble();
			DOUBLE AnimStackStop = AnimStackTimeSpan.GetStop().GetSecondDouble();

			FbxAnimLayer* AnimLayer = (FbxAnimLayer*)CurAnimStack->GetMember(0);

			for(INT LinkIndex = 0; LinkIndex < SortedLinks.Num(); ++LinkIndex)
			{
				FbxNode* CurrentLink = SortedLinks(LinkIndex);

				FbxAnimCurve* Curves[6];

				Curves[0] = CurrentLink->LclTranslation.GetCurve<FbxAnimCurve>(AnimLayer, FBXSDK_CURVENODE_COMPONENT_X, false);
				Curves[1] = CurrentLink->LclTranslation.GetCurve<FbxAnimCurve>(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Y, false);
				Curves[2] = CurrentLink->LclTranslation.GetCurve<FbxAnimCurve>(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Z, false);
				Curves[3] = CurrentLink->LclRotation.GetCurve<FbxAnimCurve>(AnimLayer, FBXSDK_CURVENODE_COMPONENT_X, false);
				Curves[4] = CurrentLink->LclRotation.GetCurve<FbxAnimCurve>(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Y, false);
				Curves[5] = CurrentLink->LclRotation.GetCurve<FbxAnimCurve>(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Z, false);

				for(INT CurveIndex = 0; CurveIndex < 6; ++CurveIndex)
				{
					FbxAnimCurve* CurrentCurve = Curves[CurveIndex];
					if(CurrentCurve)
					{
						INT KeyCount = CurrentCurve->KeyGetCount();

						FbxTimeSpan TimeInterval(FBXSDK_TIME_INFINITE,FBXSDK_TIME_MINUS_INFINITE);
						BOOL bValidTimeInterval = CurrentCurve->GetTimeInterval(TimeInterval);
						
						if(KeyCount > 1 && bValidTimeInterval)
						{
							// Ignore the first frame, it doesn't contribute to the sample rate
							KeyCount -= 1;

							DOUBLE KeyAnimLength = TimeInterval.GetDuration().GetSecondDouble();
							DOUBLE KeyAnimStart = TimeInterval.GetStart().GetSecondDouble();
							DOUBLE KeyAnimStop = TimeInterval.GetStop().GetSecondDouble();

							if(KeyAnimLength != 0.0)
							{
								INT NewRate = static_cast<INT>(KeyCount / KeyAnimLength);
								MaxStackResampleRate = Max(NewRate, MaxStackResampleRate);
							}
						}
					}
				}
			}
		}

		if(MaxStackResampleRate != 0)
		{
			ResampleRate = MaxStackResampleRate;
		}
	}

	UBOOL bAskAboutReplacing = TRUE;
	UBOOL bDoReplace = FALSE;
	for( AnimStackIndex = 0; AnimStackIndex < AnimStackCount; AnimStackIndex++ )
	{
		FbxAnimStack* CurAnimStack = FbxCast<FbxAnimStack>(Scene->GetSrcObject(FbxAnimStack::ClassId, AnimStackIndex));
		// set current anim stack
		Scene->GetEvaluator()->SetContext(CurAnimStack);
		
		warnf(NAME_Log,TEXT("Parsing AnimStack %s"),ANSI_TO_TCHAR(CurAnimStack->GetName()));

		// There are a FBX unroll filter bug, so don't bake animation layer at all
		MergeAllLayerAnimation(CurAnimStack, ResampleRate);
		
		UBOOL bValidAnimStack = FALSE;
				
		FbxTimeSpan AnimTimeSpan(FBXSDK_TIME_INFINITE,FBXSDK_TIME_MINUS_INFINITE);
		// we want even empty anim stack (take) because it can hold pose
		SortedLinks(0)->GetAnimationInterval(AnimTimeSpan);

		bValidAnimStack = TRUE;

		if (ImportOptions->bImportMorph)
		{
			for ( INT NodeIndex = 0; NodeIndex < NodeArray.Num(); NodeIndex++ )
			{
				// consider blendshape animation curve
				FbxGeometry* Geometry = (FbxGeometry*)NodeArray(NodeIndex)->GetNodeAttribute();
				if (Geometry)
				{
					INT BlendShapeDeformerCount = Geometry->GetDeformerCount(FbxDeformer::eBLENDSHAPE);
					for(INT BlendShapeIndex = 0; BlendShapeIndex<BlendShapeDeformerCount; ++BlendShapeIndex)
					{
						FbxBlendShape* BlendShape = (FbxBlendShape*)Geometry->GetDeformer(BlendShapeIndex, FbxDeformer::eBLENDSHAPE);

						INT BlendShapeChannelCount = BlendShape->GetBlendShapeChannelCount();
						for(INT ChannelIndex = 0; ChannelIndex<BlendShapeChannelCount; ++ChannelIndex)
						{
							FbxBlendShapeChannel* Channel = BlendShape->GetBlendShapeChannel(ChannelIndex);

							if(Channel)
							{
								// Get the percentage of influence of the shape.
								FbxAnimCurve* lFCurve = Geometry->GetShapeChannel(BlendShapeIndex, ChannelIndex, (FbxAnimLayer*)CurAnimStack->GetMember(0));
								if (lFCurve && lFCurve->KeyGetCount() > 0)
								{
									FbxTimeSpan TmpAnimSpan(FBXSDK_TIME_INFINITE,FBXSDK_TIME_MINUS_INFINITE);

									if (lFCurve->GetTimeInterval(TmpAnimSpan))
									{
										bValidAnimStack = TRUE;
										// update animation interval
										AnimTimeSpan = AnimTimeSpan.Intersect(TmpAnimSpan);
									}
								}
							}
						}
					}
				}
			}
		}
		
		// no animation
		if (!bValidAnimStack)
		{
			continue;
		}
		
		FString SequenceName = Filename;

		if (ValidTakeCount > 1)
		{
			SequenceName += "_";
			SequenceName += ANSI_TO_TCHAR(CurAnimStack->GetName());
		}

		// See if this sequence already exists.
		UAnimSequence* DestSeq = AnimSet->FindAnimSequence(FName(*SequenceName));

		// If we're overwriting an animation that had references to an additive, keep that information, so we can update the re-update the additive animations.
		TArray<AdditiveAnimRebuildInfo> AdditiveAnimRebuildList;

		// If not, create new one now.
		if(!DestSeq)
		{
			DestSeq = ConstructObject<UAnimSequence>( UAnimSequence::StaticClass(), AnimSet );

			AnimSet->Sequences.AddItem( DestSeq );
		}
		else
		{
			if (bAskAboutReplacing)
			{
				// if it's different animset, ask if they'd like to replace
				UINT MsgResult = appMsgf(AMT_YesNoYesAllNoAll, LocalizeSecure(LocalizeUnrealEd("Prompt_25"), *SequenceName));
				bDoReplace = MsgResult == ART_Yes || MsgResult == ART_YesAll;
				bAskAboutReplacing = MsgResult == ART_No || MsgResult == ART_Yes;
			}

			if(!bDoReplace)
			{
				continue; // Move on to next sequence...
			}

			// We're overriding an existing animation.
			// If that animation was involved in building an additive animation, get a list of them so we can auto rebuild them with the new data.
			FAnimationUtils::GetAdditiveAnimRebuildList(DestSeq, AdditiveAnimRebuildList);
			DestSeq->RecycleAnimSequence();
		}

		
		FbxTime SequenceLength = AnimTimeSpan.GetDuration();
		DOUBLE SequenceLengthDouble = SequenceLength.GetSecondDouble();

		DestSeq->SequenceName = *SequenceName;
		DestSeq->SequenceLength = SequenceLengthDouble;

		//RecursiveFillAnimSequence(SkeletonRoot,DestSeq,Start,End,TRUE,ResampleRate);

		DestSeq->RawAnimationData.AddZeroed( AnimSet->TrackBoneNames.Num() );

		//
		// shape animation START
		//
		// Clear before importing 
		DestSeq->CurveData.Empty();

		UBOOL bMarkDirty = FALSE;

		if (ImportOptions->bImportMorph)
		{
			for ( INT NodeIndex = 0; NodeIndex < NodeArray.Num(); NodeIndex++ )
			{
				// consider blendshape animation curve
				FbxGeometry* Geometry = (FbxGeometry*)NodeArray(NodeIndex)->GetNodeAttribute();
				if (Geometry)
				{
					INT BlendShapeDeformerCount = Geometry->GetDeformerCount(FbxDeformer::eBLENDSHAPE);
					for(INT BlendShapeIndex = 0; BlendShapeIndex<BlendShapeDeformerCount; ++BlendShapeIndex)
					{
						FbxBlendShape* BlendShape = (FbxBlendShape*)Geometry->GetDeformer(BlendShapeIndex, FbxDeformer::eBLENDSHAPE);
						const INT BlendShapeChannelCount = BlendShape->GetBlendShapeChannelCount();
						
						FString BlendShapeName = ANSI_TO_TCHAR(MakeName(BlendShape->GetName()));

						for(INT ChannelIndex = 0; ChannelIndex<BlendShapeChannelCount; ++ChannelIndex)
						{
							FbxBlendShapeChannel* Channel = BlendShape->GetBlendShapeChannel(ChannelIndex);

							if(Channel)
							{
								FString ChannelName = ANSI_TO_TCHAR(MakeName(Channel->GetName()));

								// Maya adds the name of the blendshape and an underscore to the front of the channel name, so remove it
								if(ChannelName.StartsWith(BlendShapeName))
								{
									ChannelName = ChannelName.Right(ChannelName.Len() - (BlendShapeName.Len()+1));
								}

								// Get the percentage of influence of the shape.						
								FbxAnimCurve* Curve = Geometry->GetShapeChannel(BlendShapeIndex, ChannelIndex, (FbxAnimLayer*)CurAnimStack->GetMember(0));
								if (Curve && Curve->KeyGetCount() > 0)
								{
									FCurveTrack CurveTrack;
									appMemzero(&CurveTrack, sizeof(CurveTrack));
									// FBX shape name is formatted as: "Blendshape$1.$2". 
									// $1 is the number of blendshape in FBX scene
									// $2 is the morph target name
									// Maybe we need to set the curve name as the morph target name as ActorX plug-in
			
									//FString name( ANSI_TO_TCHAR( MakeName(Channel->GetName() ) ) );
									//name += ANSI_TO_TCHAR( MakeName( Channel->GetTargetShape(0)->GetName() ) );
									//CurveTrack.CurveName = *name;

									// Using the channel name seems more correct in our testing
									CurveTrack.CurveName = *ChannelName;
									
									// get key values of weight
									TArray<FLOAT> Weights;
									for (FbxTime CurTime = AnimTimeSpan.GetStart(); CurTime <= AnimTimeSpan.GetStop(); CurTime += FbxLongLong(46186158000) / ResampleRate)
									{
										FLOAT KeyValue = Curve->Evaluate(CurTime);
										Weights.AddItem(KeyValue / 100.0);
									}
									CurveTrack.CurveWeights.Append(Weights);

									// Add one blend shape curve
									// make sure it has valid keys - if all 0, it doesn't have to be saved
									if (CurveTrack.IsValidCurveTrack())
									{
										bMarkDirty |= CurveTrack.CompressCurveWeights();
										DestSeq->CurveData.AddItem(CurveTrack);
									}
								}
							}
						}
					}
				}
			}
		}

		DestSeq->MarkPackageDirty(bMarkDirty);
		
		
		//
		//shape animation END
		//

		UBOOL NonIdentityScaleFound = FALSE;

		// Import each track.
		check( AnimSet->TrackBoneNames.Num() == DestSeq->RawAnimationData.Num() );
		

		for(INT TrackIdx = 0; TrackIdx < AnimSet->TrackBoneNames.Num(); TrackIdx++)
		{
			FRawAnimSequenceTrack& RawTrack = DestSeq->RawAnimationData(TrackIdx);

			// Find the source track for this one in the AnimSet
			INT SourceTrackIdx = TrackMap( TrackIdx );

			// If bone was not found in current skeleton, use SkeletalMesh to create the track.
			if(SourceTrackIdx == INDEX_NONE)
			{
				FName TrackName = AnimSet->TrackBoneNames(TrackIdx);
				INT PatchBoneIndex = SkeletalMesh->MatchRefBone(TrackName);
				check(PatchBoneIndex != INDEX_NONE); // Should have checked for this case above!

				// Create 1-frame animation from the reference pose of the skeletal mesh.
				// This is basically what the compression does, so should be fine.

				FVector RefPosition = SkeletalMesh->RefSkeleton(PatchBoneIndex).BonePos.Position;
				RawTrack.PosKeys.AddItem(RefPosition);

				FQuat RefOrientation = SkeletalMesh->RefSkeleton(PatchBoneIndex).BonePos.Orientation;
				// ???
				if(PatchBoneIndex == 0)
				{
					RefOrientation.W *= -1.f;
				}
				RawTrack.RotKeys.AddItem(RefOrientation);
			}
		}

		UINT NumFrames = 0;
		INT Index = 0;
		for (FbxTime CurTime = AnimTimeSpan.GetStart(); CurTime <= (AnimTimeSpan.GetStop()); CurTime += FbxLongLong(46186158000) / ResampleRate, Index++)
		{
			++NumFrames;
			for(INT TrackIdx = 0; TrackIdx < AnimSet->TrackBoneNames.Num(); TrackIdx++)
			{
				FRawAnimSequenceTrack& RawTrack = DestSeq->RawAnimationData(TrackIdx);

				//We do not want to clear these every time we loop.
				if(CurTime == AnimTimeSpan.GetStart())
				{
					RawTrack.PosKeys.Empty();
					RawTrack.RotKeys.Empty();
				}

				// Find the source track for this one in the AnimSet
				INT SourceTrackIdx = TrackMap( TrackIdx );


				// If bone was not found in current skeleton, use SkeletalMesh to create the track.
				if(SourceTrackIdx != INDEX_NONE)
				{
					check(SourceTrackIdx >= 0 && SourceTrackIdx < TrackNum);

					FbxNode* Link = SortedLinks(SourceTrackIdx);
		

					// Get node local transform returns "ParentGlobal.Inverse * Global"
					FbxAMatrix& LocalMatrix = Scene->GetEvaluator()->GetNodeLocalTransform(Link, CurTime);

					FbxAMatrix GlobalMatrix = Link->EvaluateGlobalTransform(CurTime);

					FbxAMatrix ParentGlobalMatrix = Link->GetParent()->EvaluateGlobalTransform(CurTime);
	
					FbxVector4 ParentGlobalLinkS = ParentGlobalMatrix.GetS();
					if (	(ParentGlobalLinkS[0] > 1.0 + SCALE_TOLERANCE || ParentGlobalLinkS[0] < 1.0 - SCALE_TOLERANCE) || 
							(ParentGlobalLinkS[1] > 1.0 + SCALE_TOLERANCE || ParentGlobalLinkS[1] < 1.0 - SCALE_TOLERANCE) || 
							(ParentGlobalLinkS[2] > 1.0 + SCALE_TOLERANCE || ParentGlobalLinkS[2] < 1.0 - SCALE_TOLERANCE) )
					{
						NonIdentityScaleFound = TRUE;

						if(TrackIdx)
						{
							FbxAMatrix OriginalGlobalScale;
							OriginalGlobalScale.SetS(GlobalMatrix.GetS());

							FbxAMatrix InvOriginalGlobalScale = OriginalGlobalScale.Inverse();

							// First, remove all scaling from the link's current global matrix
							FbxAMatrix GlobalMtx_NoScale = GlobalMatrix;
							GlobalMtx_NoScale = GlobalMtx_NoScale * InvOriginalGlobalScale;

							// Now use the scale-less version of the bone's world matrix to convert 
							// the parent's scale into the bone's local space
							FbxAMatrix ParentGlobalScale;
							ParentGlobalScale.SetS(ParentGlobalLinkS);

							FbxAMatrix	Matrix;
							Matrix = GlobalMtx_NoScale.Inverse() * ParentGlobalScale;

							FbxVector4 NewLocalS = Matrix.GetS();

							FbxVector4 LocalLinkT = LocalMatrix.GetT();

							LocalLinkT[0] *= NewLocalS[0];
							LocalLinkT[1] *= NewLocalS[1];
							LocalLinkT[2] *= NewLocalS[2];
							LocalMatrix.SetT(LocalLinkT);
						}
					}


					RawTrack.PosKeys.AddItem(Converter.ConvertPos(LocalMatrix.GetT()));
					RawTrack.RotKeys.AddItem(Converter.ConvertRotToAnimQuat(LocalMatrix.GetQ(),SourceTrackIdx));
				}
			}
		}

		DestSeq->NumFrames = NumFrames;

		if (NonIdentityScaleFound)
		{
			warnf( LocalizeSecure(LocalizeUnrealEd("Prompt_NoIdentityScaleForBone"), ANSI_TO_TCHAR(SortedLinks(0)->GetName())));
		}

		PostProcessSequence(DestSeq, AdditiveAnimRebuildList);
	}

	// If we need to, flush the LinkupCache.
	if (bAnimSetTrackChanged)
	{
		FlushLinkupCache(AnimSet, SequencesRequiringRecompression);
	}
}

#endif // WITH_FBX
