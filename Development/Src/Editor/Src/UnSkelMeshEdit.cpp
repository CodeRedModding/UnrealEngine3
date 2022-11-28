/*=============================================================================
	UnSkelMeshEdit.cpp: Unreal editor skeletal mesh/anim support
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by James Golding
=============================================================================*/

#include "EditorPrivate.h"
#include "EngineAnimClasses.h"
#include "SkelImport.h"

// Open a PSA file with the given name, and import each sequence into the supplied AnimSet.
// This is only possible if each track expected in the AnimSet is found in the target file. If not the case, a warning is given and import is aborted.
// If AnimSet is empty (ie. TrackBoneNames is empty) then we use this PSA file to form the track names array.

void UEditorEngine::ImportPSAIntoAnimSet( UAnimSet* AnimSet, const TCHAR* Filename )
{
	// Open skeletal animation key file and read header.
	FArchive* AnimFile = GFileManager->CreateFileReader( Filename, 0, GLog );
	if( !AnimFile )
	{
		appMsgf( 0, TEXT("Error opening animation file %s"), Filename );
		return;
	}

	// Read main header
	VChunkHeader ChunkHeader;
	AnimFile->Serialize( &ChunkHeader, sizeof(VChunkHeader) );
	if( AnimFile->IsError() )
	{
		appMsgf( 0, TEXT("Error reading animation file %s"), Filename );
	}

	/////// Read the bone names - convert into array of FNames.
	AnimFile->Serialize( &ChunkHeader, sizeof(VChunkHeader) );

	INT NumPSATracks = ChunkHeader.DataCount; // Number of tracks of animation. One per bone.

	TArray<FNamedBoneBinary> RawBoneNamesBin;
	RawBoneNamesBin.Add(NumPSATracks);
	AnimFile->Serialize( &RawBoneNamesBin(0), sizeof(FNamedBoneBinary) * ChunkHeader.DataCount);

	TArray<FName> RawBoneNames;
	RawBoneNames.Add(NumPSATracks);
	for(INT i=0; i<NumPSATracks; i++)
	{
		appTrimSpaces( &RawBoneNamesBin(i).Name[0] );
		RawBoneNames(i) = FName( ANSI_TO_TCHAR(&RawBoneNamesBin(i).Name[0]) );
	}

	/////// Read the animation sequence infos
	AnimFile->Serialize( &ChunkHeader, sizeof(VChunkHeader) );

	INT NumPSASequences = ChunkHeader.DataCount;

	TArray<AnimInfoBinary> RawAnimSeqInfo; // Array containing per-sequence information (sequence name, key range etc)
	RawAnimSeqInfo.Add(NumPSASequences);
	AnimFile->Serialize( &RawAnimSeqInfo(0), sizeof(AnimInfoBinary) * ChunkHeader.DataCount);


	/////// Read the raw keyframe data (1 key per
	AnimFile->Serialize( &ChunkHeader, sizeof(VChunkHeader) );

	TArray<VQuatAnimKey> RawAnimKeys;
	RawAnimKeys.Add(ChunkHeader.DataCount);

	AnimFile->Serialize( &RawAnimKeys(0), sizeof(VQuatAnimKey) * ChunkHeader.DataCount);	

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


	// Calculate track mapping from target tracks in AnimSet to the source amoung those we are loading.
	TArray<INT> TrackMap;

	if(AnimSet->TrackBoneNames.Num() == 0)
	{
		AnimSet->TrackBoneNames.Add( NumPSATracks );
		TrackMap.Add( AnimSet->TrackBoneNames.Num() );

		for(INT i=0; i<AnimSet->TrackBoneNames.Num(); i++)
		{
			// If an empty AnimSet, we create the AnimSet track name table from this PSA file.
			AnimSet->TrackBoneNames(i) = RawBoneNames(i);

			TrackMap(i) = i;
		}
	}
	else
	{
		// Otherwise, ensure right track goes to right place.
		// If we are missing a track, we give a warning and refuse to import into this set.

		TrackMap.Add( AnimSet->TrackBoneNames.Num() );

		// For each track in the AnimSet, find its index in the imported data
		for(INT i=0; i<AnimSet->TrackBoneNames.Num(); i++)
		{
			FName TrackName = AnimSet->TrackBoneNames(i);

			TrackMap(i) = INDEX_NONE;

			for(INT j=0; j<NumPSATracks; j++)
			{
				FName TestName = RawBoneNames(j);
				if(TestName == TrackName)
				{	
					if( TrackMap(i) != INDEX_NONE )
					{
						debugf( TEXT(" DUPLICATE TRACK IN INCOMING DATA: %s"), *TrackName );
					}
					TrackMap(i) = j;
				}
			}

			// If we failed to find a track we need in the imported data - complain and give up.
			if(TrackMap(i) == INDEX_NONE)
			{
				appMsgf(0, TEXT("Could not find needed track (%s) for this AnimSet."), *TrackName);
				return;
			}
		}
	}

	// For each sequence in the PSA file, find the UAnimSequence we want to import into.
	for(INT SeqIdx=0; SeqIdx<NumPSASequences; SeqIdx++)
	{
		AnimInfoBinary& PSASeqInfo = RawAnimSeqInfo(SeqIdx);

		// Get sequence name as an FName
		appTrimSpaces( &PSASeqInfo.Name[0] );
		FName SeqName = FName( ANSI_TO_TCHAR(&PSASeqInfo.Name[0]) );

		// See if this sequence already exists.
		UAnimSequence* DestSeq = AnimSet->FindAnimSequence(SeqName);

		// If not, create new one now.
		if(!DestSeq)
		{
			DestSeq = ConstructObject<UAnimSequence>( UAnimSequence::StaticClass(), AnimSet );
			AnimSet->Sequences.AddItem( DestSeq );
		}
		else
		{
			UBOOL bDoReplace = appMsgf( 1, TEXT("This will replace the existing AnimSequence (%s).\nAre you sure?"), *SeqName );
			if(!bDoReplace)
			{
				continue; // Move on to next sequence...
			}

			DestSeq->RawAnimData.Load();
			DestSeq->RawAnimData.Empty();
		}

		DestSeq->SequenceName = SeqName;
		DestSeq->SequenceLength = PSASeqInfo.TrackTime / PSASeqInfo.AnimRate; // Time of animation if playback speed was 1.0
		DestSeq->NumFrames = PSASeqInfo.NumRawFrames;

		// Make sure data is zeroes
		DestSeq->RawAnimData.AddZeroed( AnimSet->TrackBoneNames.Num() );

		// Structure of data is this:
		// RawAnimKeys contains all keys. 
		// Sequence info FirstRawFrame and NumRawFrames indicate full-skel frames (NumPSATracks raw keys). Ie number of elements we need to copy from RawAnimKeys is NumRawFrames * NumPSATracks.

		// Import each track.
		for(INT TrackIdx = 0; TrackIdx < AnimSet->TrackBoneNames.Num(); TrackIdx++)
		{
			FRawAnimSequenceTrack& RawTrack = DestSeq->RawAnimData(TrackIdx);
			RawTrack.PosKeys.Add(DestSeq->NumFrames);
			RawTrack.RotKeys.Add(DestSeq->NumFrames);
			RawTrack.KeyTimes.Add(DestSeq->NumFrames);

			// Find the source track for this one in the AnimSet
			INT SourceTrackIdx = TrackMap( TrackIdx );
			check(SourceTrackIdx >= 0 && SourceTrackIdx < NumPSATracks);

			for(INT KeyIdx = 0; KeyIdx < DestSeq->NumFrames; KeyIdx++)
			{
				INT SrcKeyIdx = ((PSASeqInfo.FirstRawFrame + KeyIdx) * NumPSATracks) + SourceTrackIdx;
				check( SrcKeyIdx < RawAnimKeys.Num() );

				VQuatAnimKey& RawSrcKey = RawAnimKeys(SrcKeyIdx);

				RawTrack.PosKeys(KeyIdx) = RawSrcKey.Position;
				RawTrack.RotKeys(KeyIdx) = RawSrcKey.Orientation;
				RawTrack.KeyTimes(KeyIdx) = ((FLOAT)KeyIdx/(FLOAT)DestSeq->NumFrames) * DestSeq->SequenceLength;
			}
		}

		// Remove any redundant duplicate frames.
		DestSeq->CompressRawAnimData();

		// Detach loader - so any future calls to Load will not clobber the new data in the LazyArray
		DestSeq->RawAnimData.Detach();
	}
}
