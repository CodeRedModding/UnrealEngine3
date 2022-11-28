/*=============================================================================
	AnimSetViewerMain.cpp: AnimSet viewer main
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by James Golding
=============================================================================*/

#include "UnrealEd.h"
#include "EngineAnimClasses.h"
#include "AnimSetViewer.h"

void WxAnimSetViewer::SetSelectedSkelMesh(USkeletalMesh* InSkelMesh)
{
	check(InSkelMesh);

	SelectedSkelMesh = InSkelMesh;

	// Set combo box to reflect new selection.
	for(INT i=0; i<SkelMeshCombo->GetCount(); i++)
	{
		if( SkelMeshCombo->GetClientData(i) == SelectedSkelMesh )
		{
			SkelMeshCombo->SetSelection(i); // This won't cause a new IDM_ANIMSET_SKELMESHCOMBO event.
		}
	}

	PreviewSkelComp->SetSkeletalMesh(SelectedSkelMesh);

	MeshProps->SetObject( NULL, true, false, true );
	MeshProps->SetObject( SelectedSkelMesh, true, false, true );

	// Update the AnimSet combo box to only show AnimSets that can be played on the new skeletal mesh.
	UpdateAnimSetCombo();

	// Try and re-select the select AnimSet with the new mesh.
	SetSelectedAnimSet(SelectedAnimSet);

	StatusBar->UpdateStatusBar(this);
}

// If trying to set the AnimSet to one that cannot be played on selected SkelMesh, show message and just select first instead.
void WxAnimSetViewer::SetSelectedAnimSet(class UAnimSet* InAnimSet)
{
	// Only allow selection of compatible AnimSets. 
	// AnimSetCombo should contain all AnimSets that can played on SelectedSkelMesh.

	INT AnimSetIndex = INDEX_NONE;
	for(INT i=0; i<AnimSetCombo->GetCount(); i++)
	{
		if(AnimSetCombo->GetClientData(i) == InAnimSet)
		{
			AnimSetIndex = i;
		}
	}

	// If specified AnimSet is not compatible with current skeleton, show message and pick the first one.
	if(AnimSetIndex == INDEX_NONE)
	{
		if(InAnimSet)
			appMsgf( 0, TEXT("AnimSet (%s) cannot be played on Skeletal Mesh (%s)."), InAnimSet->GetName(), SelectedSkelMesh->GetName() );

		AnimSetIndex = 0;
	}

	// Handle case where combo is empty - select no AnimSet
	if(AnimSetCombo->GetCount() > 0)
	{
		AnimSetCombo->SetSelection(AnimSetIndex);

		// Assign selected AnimSet
		SelectedAnimSet = (UAnimSet*)AnimSetCombo->GetClientData(AnimSetIndex);

		// Add newly selected AnimSet to the AnimSets array of the preview skeletal mesh.
		PreviewSkelComp->AnimSets.Empty();
		PreviewSkelComp->AnimSets.AddItem(SelectedAnimSet);

		AnimSetProps->SetObject( NULL, true, false, true );
		AnimSetProps->SetObject( SelectedAnimSet, true, false, true );
	}
	else
	{
		SelectedAnimSet = NULL;

		PreviewSkelComp->AnimSets.Empty();

		AnimSetProps->SetObject( NULL, true, false, true );
	}

	// Refresh the animation sequence list.
	UpdateAnimSeqList();

	// Select the first sequence (if present) - or none at all.
	if( AnimSeqList->GetCount() > 0 )
		SetSelectedAnimSequence( (UAnimSequence*)(AnimSeqList->GetClientData(0)) );
	else
		SetSelectedAnimSequence( NULL );

	StatusBar->UpdateStatusBar(this);

	// The menu title bar displays the full path name of the selected AnimSet
	FString WinTitle;
	if(SelectedAnimSet)
	{
		WinTitle = FString::Printf( TEXT("AnimSet Editor: %s"), *SelectedAnimSet->GetFullName() );
	}
	else
	{
		WinTitle = FString::Printf( TEXT("AnimSet Editor") );
	}

	SetTitle( *WinTitle );
}

void WxAnimSetViewer::SetSelectedAnimSequence(UAnimSequence* InAnimSeq)
{
	if(!InAnimSeq)
	{
		PreviewAnimNode->SetAnim(NAME_None);
		SelectedAnimSeq = NULL;
		AnimSeqProps->SetObject( NULL, true, false, true );
	}
	else
	{
		INT AnimSeqIndex = INDEX_NONE;
		for(INT i=0; i<AnimSeqList->GetCount(); i++)
		{
			if( AnimSeqList->GetClientData(i) == InAnimSeq )
			{
				AnimSeqIndex = i;
			}
		}		

		if(AnimSeqIndex == INDEX_NONE)
		{
			PreviewAnimNode->SetAnim(NAME_None);
			SelectedAnimSeq = NULL;
			AnimSeqProps->SetObject( NULL, true, false, true );
		}
		else
		{
			AnimSeqList->SetSelection( AnimSeqIndex );
			PreviewAnimNode->SetAnim( InAnimSeq->SequenceName );
			SelectedAnimSeq = InAnimSeq;

			AnimSeqProps->SetObject( NULL, true, false, true );
			AnimSeqProps->SetObject( SelectedAnimSeq, true, false, true );
		}
	}

	StatusBar->UpdateStatusBar(this);
}


void WxAnimSetViewer::ImportPSA()
{
	if(!SelectedAnimSet)
	{
		appMsgf( 0, TEXT("Please select an AnimSet for importing a PSA file into.") );
		return;
	}

	wxFileDialog ImportFileDialog( this, 
		TEXT("Import PSA File"), 
		*(GApp->LastDir[LD_PSA]),
		TEXT(""),
		TEXT("PSA files|*.psa"),
		wxOPEN | wxFILE_MUST_EXIST | wxHIDE_READONLY | wxMULTIPLE,
		wxDefaultPosition);

	if( ImportFileDialog.ShowModal() == wxID_OK )
	{
		// Remember if this AnimSet was brand new
		UBOOL bSetWasNew = false;
		if( SelectedAnimSet->TrackBoneNames.Num() == 0 )
			bSetWasNew = true;

		wxArrayString ImportFilePaths;
		ImportFileDialog.GetPaths(ImportFilePaths);

		for(UINT FileIndex = 0; FileIndex < ImportFilePaths.Count(); FileIndex++)
		{
			FFilename filename = ImportFilePaths[FileIndex];
			GApp->LastDir[LD_PSA] = filename.GetPath(); // Save path as default for next time.

			UEditorEngine::ImportPSAIntoAnimSet( SelectedAnimSet, *filename );

			SelectedAnimSet->MarkPackageDirty();
		}

		// If this set was new - we have just created the bone-track-table.
		// So we now need to check if we can still play it on our selected SkeletalMesh.
		// If we can't, we look for a few one which can play it.
		// If we can't find any mesh to play this new set on, we fail the import and reset the AnimSet.
		if(bSetWasNew)
		{
			if( !SelectedAnimSet->CanPlayOnSkeletalMesh(SelectedSkelMesh) )
			{
				USkeletalMesh* NewSkelMesh = NULL;
				for(TObjectIterator<USkeletalMesh> It; It && !NewSkelMesh; ++It)
				{
					USkeletalMesh* TestSkelMesh = *It;
					if( SelectedAnimSet->CanPlayOnSkeletalMesh(TestSkelMesh) )
					{
						NewSkelMesh = TestSkelMesh;
					}
				}

				if(NewSkelMesh)
				{
					SetSelectedSkelMesh(NewSkelMesh);
				}
				else
				{
					appMsgf( 0, TEXT("This PSA could not be played on any loaded SkeletalMesh.\nImport failed.") );
					SelectedAnimSet->ResetAnimSet();
					return;
				}
			}
		}

		// Refresh AnimSet combo to show new number of sequences in this AnimSet.
		INT CurrentSelection = AnimSetCombo->GetSelection();
		UpdateAnimSetCombo();
		AnimSetCombo->SetSelection(CurrentSelection);
			
		// Refresh AnimSequence list box to show any new anims.
		UpdateAnimSeqList();

		// Reselect current animation sequence. If none selected, pick the first one in the box (if not empty).
		if(SelectedAnimSeq)
		{
			SetSelectedAnimSequence(SelectedAnimSeq);
		}
		else
		{
			if(AnimSeqList->GetCount() > 0)
			{
				SetSelectedAnimSequence( (UAnimSequence*)(AnimSeqList->GetClientData(0)) );
			}
		}

		SelectedAnimSet->MarkPackageDirty();
	}
}

void WxAnimSetViewer::CreateNewAnimSet()
{
	FString Package = TEXT("");
	FString Group = TEXT("");

	if(SelectedSkelMesh)
	{
		// Bit yucky this...
		check( SelectedSkelMesh->GetOuter() );

		// If there are 2 levels above this mesh - top is packages, then group
		if( SelectedSkelMesh->GetOuter()->GetOuter() )
		{
			Group = SelectedSkelMesh->GetOuter()->GetName();
			Package = SelectedSkelMesh->GetOuter()->GetOuter()->GetName();
		}
		else // Otherwise, just a package.
		{
			Group = TEXT("");
			Package = SelectedSkelMesh->GetOuter()->GetName();
		}
	}

	WxDlgRename RenameDialog;
	RenameDialog.SetTitle( TEXT("New AnimSet") );
	if( RenameDialog.ShowModal( Package, Group, TEXT("NewAnimSet") ) == wxID_OK )
	{
		if( RenameDialog.NewName.Len() == 0 || RenameDialog.NewPackage.Len() == 0 )
		{
			appMsgf( 0, TEXT("Must specify new AnimSet name and package.") );
			return;
		}

		UPackage* Pkg = GEngine->CreatePackage(NULL,*RenameDialog.NewPackage);
		if( RenameDialog.NewGroup.Len() )
		{
			Pkg = GEngine->CreatePackage(Pkg,*RenameDialog.NewGroup);
		}

		UAnimSet* NewAnimSet = ConstructObject<UAnimSet>( UAnimSet::StaticClass(), Pkg, FName( *RenameDialog.NewName ), RF_Public|RF_Standalone );

		// Will update AnimSet list, which will include new AnimSet.
		SetSelectedSkelMesh(SelectedSkelMesh);
		SetSelectedAnimSet(NewAnimSet);

		SelectedAnimSet->MarkPackageDirty();
	}
}

IMPLEMENT_COMPARE_POINTER( UAnimSequence, AnimSetViewer, { return appStricmp(*A->SequenceName,*B->SequenceName); } );

void WxAnimSetViewer::UpdateAnimSeqList()
{
	AnimSeqList->Clear();

	if(!SelectedAnimSet)
		return;

	Sort<USE_COMPARE_POINTER(UAnimSequence,AnimSetViewer)>(&SelectedAnimSet->Sequences(0),SelectedAnimSet->Sequences.Num());

	for(INT i=0; i<SelectedAnimSet->Sequences.Num(); i++)
	{
		UAnimSequence* Seq = SelectedAnimSet->Sequences(i);

		FString SeqString = FString::Printf( TEXT("%s [%d]"), *Seq->SequenceName, Seq->NumFrames );
		AnimSeqList->Append( *SeqString, Seq );
	}
}

// Note - this will clear the current selection in the combo- must manually select an AnimSet afterwards.
void WxAnimSetViewer::UpdateAnimSetCombo()
{
	AnimSetCombo->Clear();

	for(TObjectIterator<UAnimSet> It; It; ++It)
	{
		UAnimSet* ItAnimSet = *It;
		
		if( ItAnimSet->CanPlayOnSkeletalMesh(SelectedSkelMesh) )
		{
			FString AnimSetString = FString::Printf( TEXT("%s [%d]"), ItAnimSet->GetName(), ItAnimSet->Sequences.Num() );
			AnimSetCombo->Append( *AnimSetString, ItAnimSet );
		}
	}
}

// Update the UI to match the current animation state (position, playing, looping)
void WxAnimSetViewer::RefreshPlaybackUI()
{
	// Update scrub bar (can only do if we have an animation selected.
	if(SelectedAnimSeq)
	{
		check(PreviewAnimNode->AnimSeq == SelectedAnimSeq);

		FLOAT CurrentPos = PreviewAnimNode->CurrentTime / SelectedAnimSeq->SequenceLength;

		TimeSlider->SetValue( appRound(CurrentPos * (FLOAT)ASV_SCRUBRANGE) );
	}

	// Update Play/Stop button
	if(PreviewAnimNode->bPlaying)
	{
		PlayButton->SetBitmapLabel( StopB );
	}
	else
	{
		PlayButton->SetBitmapLabel( PlayB );
	}

	PlayButton->Refresh();

	// Update Loop toggle
	if(PreviewAnimNode->bLooping)
	{
		LoopButton->SetBitmapLabel( LoopB );
	}
	else
	{
		LoopButton->SetBitmapLabel( NoLoopB );
	}

	LoopButton->Refresh();

	if(PreviewAnimNode->bPlaying)
	{
		StatusBar->UpdateStatusBar(this);
	}
}

void WxAnimSetViewer::TickViewer(FLOAT DeltaSeconds)
{
	// Tick the preview level. Will tick the SkeletalMeshComponent of the preview actor, which will tick the AnimNodeSequence
	PreviewLevel->Tick(LEVELTICK_All, DeltaSeconds);

	// Move scrubber to reflect current animation position etc.
	RefreshPlaybackUI();
}

void WxAnimSetViewer::EmptySelectedSet()
{
	if(SelectedAnimSet)
	{
		UBOOL bDoEmpty = appMsgf( 1, TEXT("Are you sure you want to completely reset the selected AnimSet?") );
		if( bDoEmpty )
		{
			SelectedAnimSet->ResetAnimSet();
			UpdateAnimSeqList();
			SetSelectedAnimSequence(NULL);

			SelectedAnimSet->MarkPackageDirty();
		}
	}
}

void WxAnimSetViewer::RenameSelectedSeq()
{
	if(SelectedAnimSeq)
	{
		check(SelectedAnimSet);

		WxDlgGenericStringEntry dlg;
		INT Result = dlg.ShowModal( TEXT("Rename AnimSequence"), TEXT("New Sequence Name:"), *SelectedAnimSeq->SequenceName );
		if( Result == wxID_OK )
		{
			FString NewSeqString = dlg.EnteredString;
			FName NewSeqName =  FName( *NewSeqString );

			// If there is a sequence with that name already, see if we want to over-write it.
			UAnimSequence* FoundSeq = SelectedAnimSet->FindAnimSequence(NewSeqName);
			if(FoundSeq)
			{
				FString ConfirmMessage = FString::Printf( TEXT("An AnimSequence with that name (%s) already exists in this AnimSet.\n Do you want to replace it?"), *NewSeqName );
				UBOOL bDoDelete = appMsgf( 1, *ConfirmMessage );
				if(!bDoDelete)
					return;

				SelectedAnimSet->Sequences.RemoveItem(FoundSeq);
			}

			SelectedAnimSeq->SequenceName = NewSeqName;

			UpdateAnimSeqList();

			// This will reselect this sequence, update the AnimNodeSequence, status bar etc.
			SetSelectedAnimSequence(SelectedAnimSeq);


			SelectedAnimSet->MarkPackageDirty();
		}
	}
}

void WxAnimSetViewer::DeleteSelectedSequence()
{
	if(SelectedAnimSeq)
	{
		check( SelectedAnimSeq->GetOuter() == SelectedAnimSet );
		check( SelectedAnimSet->Sequences.ContainsItem(SelectedAnimSeq) );

		FString ConfirmMessage = FString::Printf( TEXT("Are you sure you want to delete the selected AnimSequence (%s)?"), *SelectedAnimSeq->SequenceName );

		UBOOL bDoDelete = appMsgf( 1, *ConfirmMessage );
		if(bDoDelete)
		{
			SelectedAnimSet->Sequences.RemoveItem( SelectedAnimSeq );

			// Refresh list
			UpdateAnimSeqList();

			// Select the first sequence (if present) - or none at all.
			if( AnimSeqList->GetCount() > 0 )
				SetSelectedAnimSequence( (UAnimSequence*)(AnimSeqList->GetClientData(0)) );
			else
				SetSelectedAnimSequence( NULL );
		}

		SelectedAnimSet->MarkPackageDirty();
	}
}

/**
 * Pop up a dialog asking for axis and angle (in debgrees), and apply that rotation to all keys in selected sequence.
 * Basically 
 */
void WxAnimSetViewer::SequenceApplyRotation()
{
	if(SelectedAnimSeq)
	{
		WxDlgRotateAnimSeq dlg;
		INT Result = dlg.ShowModal();
		if( Result == wxID_OK )
		{
			// Angle (in radians) to rotate AnimSequence by
			FLOAT RotAng = dlg.Degrees * (PI/180.f);

			// Axis to rotate AnimSequence about.
			FVector RotAxis;
			if( dlg.Axis == AXIS_X )
			{
				RotAxis = FVector(1.f, 0.f, 0.f);
			}
			else if( dlg.Axis == AXIS_Y )
			{
				RotAxis = FVector(0.f, 1.f, 0.0f);
			}
			else if( dlg.Axis == AXIS_Z )
			{
				RotAxis = FVector(0.f, 0.f, 1.0f);
			}
			else
			{
				check(0);
			}

			// Make transformation matrix out of rotation (via quaternion)
			FQuat RotQuat( RotAxis, RotAng );
			FMatrix RotTM( FVector(0.f), RotQuat );

			// Ensure all keyframe data is loaded.
			SelectedAnimSeq->RawAnimData.Load();

			// Hmm.. animations don't have any idea of heirarchy, so we just rotate the first track. Can we be sure track 0 is the root bone?
			FRawAnimSequenceTrack& RawTrack = SelectedAnimSeq->RawAnimData(0);
			for(INT i=0; i<RawTrack.RotKeys.Num(); i++)
			{
				FMatrix KeyTM( RawTrack.PosKeys(i), RawTrack.RotKeys(i) );
				FMatrix NewKeyTM = KeyTM * RotTM;

				RawTrack.PosKeys(i) = NewKeyTM.GetOrigin();
				RawTrack.RotKeys(i) = FQuat(NewKeyTM);
			}

			// This means that future Load calls won't clobber our data.
			SelectedAnimSeq->RawAnimData.Detach();


			SelectedAnimSeq->MarkPackageDirty();
		}
	}
}

void WxAnimSetViewer::SequenceReZeroToCurrent()
{
	if(SelectedAnimSeq)
	{
		// Find vector that would translate current root bone location onto origin.
		FVector ApplyTranslation = -1.f * PreviewSkelComp->SpaceBases(0).GetOrigin();

		// Convert into world space and eliminate 'z' translation. Don't want to move character into ground.
		FVector WorldApplyTranslation = PreviewSkelComp->LocalToWorld.TransformNormal(ApplyTranslation);
		WorldApplyTranslation.Z = 0.f;
		ApplyTranslation = PreviewSkelComp->LocalToWorld.InverseTransformNormal(WorldApplyTranslation);

		SelectedAnimSeq->RawAnimData.Load();

		// As above, animations don't have any idea of heirarchy, so we don't know for sure if track 0 is the root bone's track.
		FRawAnimSequenceTrack& RawTrack = SelectedAnimSeq->RawAnimData(0);
		for(INT i=0; i<RawTrack.RotKeys.Num(); i++)
		{
			RawTrack.PosKeys(i) += ApplyTranslation;
		}

		SelectedAnimSeq->RawAnimData.Detach();

		SelectedAnimSeq->MarkPackageDirty();
	}
}

/**
 * Crop a sequence either before or after the current position. This is made slightly more complicated due to the basic compression
 * we do where tracks which had all identical keyframes are reduced to just 1 frame.
 * 
 * @param bFromStart Should we remove the sequence before or after the selected position.
 */
void WxAnimSetViewer::SequenceCrop(UBOOL bFromStart)
{
	if(SelectedAnimSeq)
	{
		// Calculate new sequence length
		FLOAT CurrentTime = PreviewAnimNode->CurrentTime;
		FLOAT NewLength = bFromStart ? (SelectedAnimSeq->SequenceLength - CurrentTime) : CurrentTime;

		// This is just for information - may not be completely accurate...
		FLOAT NewNumKeysInfo = appFloor( (NewLength / SelectedAnimSeq->SequenceLength) * SelectedAnimSeq->NumFrames );


		SelectedAnimSeq->RawAnimData.Load();

		// Iterate over tracks removing keys from each one.
		for(INT i=0; i<SelectedAnimSeq->RawAnimData.Num(); i++)
		{
			FRawAnimSequenceTrack& RawTrack = SelectedAnimSeq->RawAnimData(i);
			INT NumKeys = Max3( RawTrack.KeyTimes.Num(), RawTrack.PosKeys.Num(), RawTrack.RotKeys.Num() );

			check(RawTrack.KeyTimes.Num() == NumKeys);
			check(RawTrack.PosKeys.Num() == 1 || RawTrack.PosKeys.Num() == NumKeys);
			check(RawTrack.RotKeys.Num() == 1 || RawTrack.RotKeys.Num() == NumKeys);

			if(NumKeys > 1)
			{
				// Find the right key to cut at.
				// This assumes that all keys are equally spaced (ie. won't work if we have dropped unimportant frames etc).
				FLOAT FrameInterval = RawTrack.KeyTimes(1) - RawTrack.KeyTimes(0);
				INT KeyIndex = appCeil( CurrentTime/FrameInterval );
				KeyIndex = Clamp<INT>(KeyIndex, 1, RawTrack.KeyTimes.Num()-1); // Ensure KeyIndex is in range.

				// KeyIndex should now be the very next frame after the desired Time.
				
				if(bFromStart)
				{
					if(RawTrack.PosKeys.Num() > 1)
					{
						RawTrack.PosKeys.Remove(0, KeyIndex);
					}

					if(RawTrack.RotKeys.Num() > 1)
					{
						RawTrack.RotKeys.Remove(0, KeyIndex);
					}

					RawTrack.KeyTimes.Remove(0, KeyIndex);
				}
				else
				{
					if(RawTrack.PosKeys.Num() > 1)
					{
						check(RawTrack.PosKeys.Num() == NumKeys);
						RawTrack.PosKeys.Remove(KeyIndex, NumKeys - KeyIndex);
					}

					if(RawTrack.RotKeys.Num() > 1)
					{
						check(RawTrack.RotKeys.Num() == NumKeys);
						RawTrack.RotKeys.Remove(KeyIndex, NumKeys - KeyIndex);
					}

					RawTrack.KeyTimes.Remove(KeyIndex, NumKeys - KeyIndex);
				}

				// Recalculate key times
				for(INT KeyIdx=0; KeyIdx < RawTrack.KeyTimes.Num(); KeyIdx++)
				{
					RawTrack.KeyTimes(KeyIdx) = ((FLOAT)KeyIdx/(FLOAT)RawTrack.KeyTimes.Num()) * NewLength;
				}
			}
		}

		SelectedAnimSeq->RawAnimData.Detach();

		SelectedAnimSeq->SequenceLength = NewLength;
		SelectedAnimSeq->NumFrames = NewNumKeysInfo;

		SelectedAnimSeq->MarkPackageDirty();

		if(bFromStart)
		{
			PreviewAnimNode->CurrentTime = 0.f;
		}
		else
		{		
			PreviewAnimNode->CurrentTime = SelectedAnimSeq->SequenceLength;
		}

		UpdateAnimSeqList();
		SetSelectedAnimSequence(SelectedAnimSeq);
	}
}
