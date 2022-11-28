/*=============================================================================
	InterpEditorMenus.cpp: Interpolation editing menus
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by James Golding
=============================================================================*/

#include "UnrealEd.h"
#include "EngineSequenceClasses.h"
#include "EngineInterpolationClasses.h"
#include "InterpEditor.h"
#include "UnLinkedObjDrawUtils.h"

///// MENU CALLBACKS

// Add a new keyframe on the selected track 
void WxInterpEd::OnMenuAddKey( wxCommandEvent& In )
{
	AddKey();
}

void WxInterpEd::OnContextNewGroup( wxCommandEvent& In )
{
	// Find out if we want to make a 'Director' group.
	UBOOL bDirGroup = ( In.GetId() == IDM_INTERP_NEW_DIRECTOR_GROUP );

	// This is temporary - need a unified way to associate tracks with components/actors etc... hmm..
	AActor* GroupActor = NULL;

	if(!bDirGroup)
	{
		for(INT i=0; i<GUnrealEd->Level->Actors.Num() && !GroupActor; i++)
		{
			AActor* Actor = GUnrealEd->Level->Actors(i);
			if( Actor && GSelectionTools.IsSelected( Actor ) )
				GroupActor = Actor;
		}

		if(GroupActor)
		{
			// Check we do not already have a group acting on this actor.
			for(INT i=0; i<Interp->GroupInst.Num(); i++)
			{
				if( GroupActor == Interp->GroupInst(i)->GroupActor )
				{
					appMsgf(0, TEXT("There is already a Group associated with this Actor."));
					return;
				}
			}
		}
	}

	FName NewGroupName;
	// If not a director group - ask for a name.
	if(!bDirGroup)
	{
		// Otherwise, pop up dialog to enter name.
		WxDlgGenericStringEntry dlg;
		INT Result = dlg.ShowModal( TEXT("Enter New Group Name"), TEXT("New Group Name:"), TEXT("InterpGroup") );
		if( Result != wxID_OK )
			return;		

		dlg.EnteredString.Replace(TEXT(" "),TEXT("_"));
		NewGroupName = FName( *dlg.EnteredString );
	}

	// Begin undo transaction
	InterpEdTrans->BeginSpecial( TEXT("NewGroup") );
	Interp->Modify();
	IData->Modify();

	// Create new InterpGroup.
	UInterpGroup* NewGroup = NULL;
	if(bDirGroup)
	{
		NewGroup = ConstructObject<UInterpGroupDirector>( UInterpGroupDirector::StaticClass(), IData, NAME_None, RF_Transactional );
	}
	else
	{
		NewGroup = ConstructObject<UInterpGroup>( UInterpGroup::StaticClass(), IData, NAME_None, RF_Transactional );
		NewGroup->GroupName = NewGroupName;
	}
	IData->InterpGroups.AddItem(NewGroup);


	// All groups must have a unique name.
	NewGroup->EnsureUniqueName();

	// Randomly generate a group colour for the new group.
	NewGroup->GroupColor = MakeRandomColor();


	NewGroup->Modify();

	// Create new InterpGroupInst
	UInterpGroupInst* NewGroupInst = NULL;
	if(bDirGroup)
	{
		NewGroupInst = ConstructObject<UInterpGroupInstDirector>( UInterpGroupInstDirector::StaticClass(), Interp, NAME_None, RF_Transactional );
		NewGroupInst->InitGroupInst(NewGroup, NULL);
	}
	else
	{
		NewGroupInst = ConstructObject<UInterpGroupInst>( UInterpGroupInst::StaticClass(), Interp, NAME_None, RF_Transactional );
		// Initialise group instance, saving ref to actor it works on.
		NewGroupInst->InitGroupInst(NewGroup, GroupActor);
	}

	INT NewGroupInstIndex = Interp->GroupInst.AddItem(NewGroupInst);

	NewGroupInst->Modify();

	// Don't need to save state here - no tracks!

	// If a director group, create a director track for it now.
	if(bDirGroup)
	{
		UInterpTrack* NewDirTrack = ConstructObject<UInterpTrackDirector>( UInterpTrackDirector::StaticClass(), NewGroup, NAME_None, RF_Transactional );
		NewGroup->InterpTracks.AddItem(NewDirTrack);

		UInterpTrackInst* NewDirTrackInst = ConstructObject<UInterpTrackInstDirector>( UInterpTrackInstDirector::StaticClass(), NewGroupInst, NAME_None, RF_Transactional );
		NewGroupInst->TrackInst.AddItem(NewDirTrackInst);

		NewDirTrackInst->InitTrackInst(NewDirTrack);
		NewDirTrackInst->SaveActorState();

		// Save for undo then redo.
		NewDirTrack->Modify();
		NewDirTrackInst->Modify();
	}
	// If regular track, create a new object variable connector, and variable containing selected actor if there is one.
	else
	{
		// Create a new variable connector on all Matinee's using this data.
		UpdateMatineeActionConnectors();

		// Find the newly created connector on this SeqAct_Interp. Should always have one now!
		INT NewLinkIndex = Interp->FindConnectorIndex( FString(*NewGroup->GroupName), LOC_VARIABLE );
		check(NewLinkIndex != INDEX_NONE);
		FSeqVarLink* NewLink = &(Interp->VariableLinks(NewLinkIndex));

		// Find the sequence that this SeqAct_Interp is in.
		USequence* Seq = Cast<USequence>( Interp->GetOuter() );
		check(Seq);

		if(GroupActor)
		{
			USeqVar_Object* NewObjVar = ConstructObject<USeqVar_Object>( USeqVar_Object::StaticClass(), Seq, NAME_None, RF_Transactional );
			NewObjVar->ObjPosX = Interp->ObjPosX + 50 * Interp->VariableLinks.Num();
			NewObjVar->ObjPosY = Interp->ObjPosY + 130;
			NewObjVar->ObjValue = GroupActor;

			Seq->SequenceObjects.AddItem(NewObjVar);

			// Set the new variable connector to point to the new variable.
			NewLink->LinkedVariables.AddItem(NewObjVar);
		}
	}

	InterpEdTrans->EndSpecial();

	// Update graphics to show new group.
	InterpEdVC->Viewport->Invalidate();

	// If adding a camera- make sure its frustum colour is updated.
	UpdateCamColours();
}



void WxInterpEd::OnContextNewTrack( wxCommandEvent& In )
{
	if( !ActiveGroup )
		return;

	UInterpGroupInst* GrInst = Interp->FindFirstGroupInst(ActiveGroup);
	check(GrInst);

	// Find the class of the new track we want to add.
	INT NewTrackClassIndex = In.GetId() - IDM_INTERP_NEW_TRACK_START;
	check( NewTrackClassIndex >= 0 && NewTrackClassIndex < InterpTrackClasses.Num() );

	UClass* NewInterpTrackClass = InterpTrackClasses(NewTrackClassIndex);
	check( NewInterpTrackClass->IsChildOf(UInterpTrack::StaticClass()) );

	UInterpTrack* TrackDef = (UInterpTrack*)NewInterpTrackClass->GetDefaultObject();

	// If bOnePerGrouop - check we don't already have a track of this type in the group.
	if(TrackDef->bOnePerGroup)
	{
		for(INT i=0; i<ActiveGroup->InterpTracks.Num(); i++)
		{
			if( ActiveGroup->InterpTracks(i)->GetClass() == NewInterpTrackClass )
			{
				appMsgf(0, TEXT("Only allowed one of this track type per group."));
				return;
			}
		}
	}

	// For FloatProp tracks - pop up a dialog to choose property name.
	FName FloatPropName = NAME_None;
	if( NewInterpTrackClass->IsChildOf(UInterpTrackFloatProp::StaticClass()) )
	{
		AActor* Actor = GrInst->GroupActor;
		if(!Actor)
			return;

		WxDlgGenericComboEntry dlg;

		TArray<FName> PropNames;
		Actor->GetInterpPropertyNames(PropNames);

		TArray<FString> PropStrings;
		PropStrings.AddZeroed( PropNames.Num() );
		for(INT i=0; i<PropNames.Num(); i++)
		{
			PropStrings(i) = FString( *PropNames(i) );
		}

		if( dlg.ShowModal( TEXT("Choose Property"), TEXT("Property Name:"), PropStrings ) != wxID_OK )
			return;

		FloatPropName = FName( *dlg.SelectedString );
		if(FloatPropName == NAME_None)
			return;

		// Check we don't already have a track controlling this property.
		for(INT i=0; i<ActiveGroup->InterpTracks.Num(); i++)
		{
			UInterpTrackFloatProp* TestPropTrack = Cast<UInterpTrackFloatProp>( ActiveGroup->InterpTracks(i) );
			if(TestPropTrack && TestPropTrack->PropertyName == FloatPropName)
			{
				appMsgf(0, TEXT("Already a FloatProp track controlling this property."));
				return;
			}
		}
	}

	InterpEdTrans->BeginSpecial( TEXT("NewTrack") );

	ActiveGroup->Modify();

	// Construct track and track instance objects.
	UInterpTrack* NewTrack = ConstructObject<UInterpTrack>( NewInterpTrackClass, ActiveGroup, NAME_None, RF_Transactional );
	INT NewTrackIndex = ActiveGroup->InterpTracks.AddItem(NewTrack);

	check( NewTrack->TrackInstClass );
	check( NewTrack->TrackInstClass->IsChildOf(UInterpTrackInst::StaticClass()) );

	UInterpTrackFloatProp* FloatPropTrack = Cast<UInterpTrackFloatProp>(NewTrack);
	if( FloatPropTrack )
	{
		FloatPropTrack->PropertyName = FloatPropName;

		// Set track title to property name.
		FloatPropTrack->TrackTitle = *FloatPropName;
	}

	NewTrack->SetTrackToSensibleDefault();

	NewTrack->Modify();

	// We need to create a InterpTrackInst in each instance of the active group (the one we are adding the track to).
	for(INT i=0; i<Interp->GroupInst.Num(); i++)
	{
		UInterpGroupInst* GrInst = Interp->GroupInst(i);
		if(GrInst->Group == ActiveGroup)
		{
			GrInst->Modify();

			UInterpTrackInst* NewTrackInst = ConstructObject<UInterpTrackInst>( NewTrack->TrackInstClass, GrInst, NAME_None, RF_Transactional );

			INT NewInstIndex = GrInst->TrackInst.AddItem(NewTrackInst);
			check(NewInstIndex == NewTrackIndex);

			// Initialise track, giving selected object.
			NewTrackInst->InitTrackInst(NewTrack);

			// Save state into new track before doing anything else (because we didn't do it on ed mode change).
			NewTrackInst->SaveActorState();

			NewTrackInst->Modify();
		}
	}

	InterpEdTrans->EndSpecial();

	// Select the new track
	ClearKeySelection();
	SetActiveTrack(ActiveGroup, NewTrackIndex);

	// Update graphics to show new track!
	InterpEdVC->Viewport->Invalidate();
}

void WxInterpEd::OnMenuPlay( wxCommandEvent& In )
{
	if(Interp->bIsPlaying)
		return;

	bAdjustingKeyframe = false;

	bLoopingSection = (In.GetId() == IDM_INTERP_PLAYLOOPSECTION);
	if(bLoopingSection)
	{
		// If looping - jump to start of looping section.
		Interp->UpdateInterp(IData->EdSectionStart);
	}

	Interp->bReversePlayback = false;
	Interp->bIsPlaying = true;

	InterpEdVC->Realtime = true; // So viewport client will get ticked.
}

void WxInterpEd::OnMenuStop( wxCommandEvent& In )
{
	// If stopped, 
	if(!Interp->bIsPlaying)
	{
		SetInterpPosition(0.f);
		return;
	}

	Interp->bIsPlaying = false;

	InterpEdVC->Realtime = false;
}

void WxInterpEd::OnChangePlaySpeed( wxCommandEvent& In )
{
	PlaybackSpeed = 1.f;

	switch( In.GetId() )
	{
	case IDM_INTERP_SPEED_1:
		PlaybackSpeed = 0.01f;
		break;
	case IDM_INTERP_SPEED_10:
		PlaybackSpeed = 0.1f;
		break;
	case IDM_INTERP_SPEED_25:
		PlaybackSpeed = 0.25f;
		break;
	case IDM_INTERP_SPEED_50:
		PlaybackSpeed = 0.5f;
		break;
	case IDM_INTERP_SPEED_100:
		PlaybackSpeed = 1.0f;
		break;
	}
}

void WxInterpEd::OnMenuStretchSection(wxCommandEvent& In)
{
	// Edit section markers should always be within sequence...

	FLOAT CurrentSectionLength = IData->EdSectionEnd - IData->EdSectionStart;
	if(CurrentSectionLength < 0.01f)
	{
		appMsgf( 0, TEXT("You must highlight a non-zero length section before stretching it.") );
		return;
	}


	FString CurrentLengthStr = FString::Printf( TEXT("%3.3f"), CurrentSectionLength );

	// Display dialog and let user enter new length for this section.
	WxDlgGenericStringEntry dlg;
	INT Result = dlg.ShowModal( TEXT("Stretch Section"), TEXT("New Length:"), *CurrentLengthStr);
	if( Result != wxID_OK )
		return;

	double dNewSectionLength;
	UBOOL bIsNumber = dlg.StringEntry->GetValue().ToDouble(&dNewSectionLength);
	if(!bIsNumber)
		return;

	FLOAT NewSectionLength = (FLOAT)dNewSectionLength;
	if(NewSectionLength <= 0.f)
		return;

	InterpEdTrans->BeginSpecial( TEXT("Stretch Section") );

	IData->Modify();
	Opt->Modify();

	FLOAT LengthDiff = NewSectionLength - CurrentSectionLength;
	FLOAT StretchRatio = NewSectionLength/CurrentSectionLength;

	// Iterate over all tracks.
	for(INT i=0; i<IData->InterpGroups.Num(); i++)
	{
		UInterpGroup* Group = IData->InterpGroups(i);
		for(INT j=0; j<Group->InterpTracks.Num(); j++)
		{
			UInterpTrack* Track = Group->InterpTracks(j);

			Track->Modify();

			for(INT k=0; k<Track->GetNumKeyframes(); k++)
			{
				FLOAT KeyTime = Track->GetKeyframeTime(k);

				// Key is before start of stretched section
				if(KeyTime < IData->EdSectionStart)
				{
					// Leave key as it is
				}
				// Key is in section being stretched
				else if(KeyTime < IData->EdSectionEnd)
				{
					// Calculate new key time.
					FLOAT FromSectionStart = KeyTime - IData->EdSectionStart;
					FLOAT NewKeyTime = IData->EdSectionStart + (StretchRatio * FromSectionStart);

					Track->SetKeyframeTime(k, NewKeyTime, false);
				}
				// Key is after stretched section
				else
				{
					// Move it on by the increase in sequence length.
					Track->SetKeyframeTime(k, KeyTime + LengthDiff, false);
				}
			}
		}
	}

	// Move the end of the interpolation to account for changing the length of this section.
	SetInterpEnd(IData->InterpLength + LengthDiff);

	// Move end marker of section to new, stretched position.
	MoveLoopMarker( IData->EdSectionEnd + LengthDiff, false );

	InterpEdTrans->EndSpecial();
}

/** Remove the currernt section, reducing the length of the sequence and moving any keys after the section earlier in time. */
void WxInterpEd::OnMenuDeleteSection(wxCommandEvent& In)
{
	FLOAT CurrentSectionLength = IData->EdSectionEnd - IData->EdSectionStart;
	if(CurrentSectionLength < 0.01f)
		return;

	InterpEdTrans->BeginSpecial( TEXT("Delete Section") );

	IData->Modify();
	Opt->Modify();

	// Add keys that are within current section to selection
	SelectKeysInLoopSection();

	// Delete current selection
	DeleteSelectedKeys(false);

	// Then move any keys after the current section back by the length of the section.
	for(INT i=0; i<IData->InterpGroups.Num(); i++)
	{
		UInterpGroup* Group = IData->InterpGroups(i);
		for(INT j=0; j<Group->InterpTracks.Num(); j++)
		{
			UInterpTrack* Track = Group->InterpTracks(j);
			Track->Modify();

			for(INT k=0; k<Track->GetNumKeyframes(); k++)
			{
				// Move keys after section backwards by length of the section
				FLOAT KeyTime = Track->GetKeyframeTime(k);
				if(KeyTime > IData->EdSectionEnd)
				{
					// Add to selection for deletion.
					Track->SetKeyframeTime(k, KeyTime - CurrentSectionLength, false);
				}
			}
		}
	}

	// Move the end of the interpolation to account for changing the length of this section.
	SetInterpEnd(IData->InterpLength - CurrentSectionLength);

	// Move section end marker on top of section start marker (section has vanished).
	MoveLoopMarker( IData->EdSectionStart, false );

	InterpEdTrans->EndSpecial();
}

/** Insert an amount of space (specified by user in dialog) at the current position in the sequence. */
void WxInterpEd::OnMenuInsertSpace( wxCommandEvent& In )
{
	WxDlgGenericStringEntry dlg;
	INT Result = dlg.ShowModal( TEXT("Insert Empty Space"), TEXT("Seconds:"), TEXT("1.0"));
	if( Result != wxID_OK )
		return;

	double dAddTime;
	UBOOL bIsNumber = dlg.StringEntry->GetValue().ToDouble(&dAddTime);
	if(!bIsNumber)
		return;

	FLOAT AddTime = (FLOAT)dAddTime;

	// Ignore if adding a negative amount of time!
	if(AddTime <= 0.f)
		return;

	InterpEdTrans->BeginSpecial( TEXT("Insert Space") );

	IData->Modify();
	Opt->Modify();

	// Move the end of the interpolation on by the amount we are adding.
	SetInterpEnd(IData->InterpLength + AddTime);

	// Iterate over all tracks.
	for(INT i=0; i<IData->InterpGroups.Num(); i++)
	{
		UInterpGroup* Group = IData->InterpGroups(i);
		for(INT j=0; j<Group->InterpTracks.Num(); j++)
		{
			UInterpTrack* Track = Group->InterpTracks(j);

			Track->Modify();

			for(INT k=0; k<Track->GetNumKeyframes(); k++)
			{
				FLOAT KeyTime = Track->GetKeyframeTime(k);
				if(KeyTime > Interp->Position)
				{
					Track->SetKeyframeTime(k, KeyTime + AddTime, false);
				}
			}
		}
	}

	InterpEdTrans->EndSpecial();
}

void WxInterpEd::OnMenuSelectInSection(wxCommandEvent& In)
{
	SelectKeysInLoopSection();
}

void WxInterpEd::OnMenuDuplicateSelectedKeys(wxCommandEvent& In)
{
	DuplicateSelectedKeys();
}

void WxInterpEd::OnSavePathTime( wxCommandEvent& In )
{
	IData->PathBuildTime = Interp->Position;
}

void WxInterpEd::OnJumpToPathTime( wxCommandEvent& In )
{
	SetInterpPosition(IData->PathBuildTime);
}

void WxInterpEd::OnContextTrackRename( wxCommandEvent& In )
{
	if(ActiveTrackIndex == INDEX_NONE)
		return;

	check(ActiveGroup);

	UInterpTrack* Track = ActiveGroup->InterpTracks(ActiveTrackIndex);
	check(Track);

	WxDlgGenericStringEntry dlg;
	INT Result = dlg.ShowModal( TEXT("Rename Track"), TEXT("New Track Name:"), *Track->TrackTitle );
	if( Result != wxID_OK )
		return;

	Track->TrackTitle = dlg.EnteredString;
}

void WxInterpEd::OnContextTrackDelete( wxCommandEvent& In )
{
	if(ActiveTrackIndex == INDEX_NONE)
		return;

	check(ActiveGroup);

	InterpEdTrans->BeginSpecial( TEXT("TrackDelete") );
	Interp->Modify();
	IData->Modify();

	ActiveGroup->Modify();

	UInterpTrack* ActiveTrack = ActiveGroup->InterpTracks(ActiveTrackIndex);
	check(ActiveTrack);

	ActiveTrack->Modify();

	for(INT i=0; i<Interp->GroupInst.Num(); i++)
	{
		UInterpGroupInst* GrInst = Interp->GroupInst(i);
		if(GrInst->Group == ActiveGroup)
		{
			check( ActiveTrackIndex >= 0 && ActiveTrackIndex < GrInst->TrackInst.Num() );
			UInterpTrackInst* TrInst = GrInst->TrackInst(ActiveTrackIndex);

			GrInst->Modify();
			TrInst->Modify();

			// Before deleting this track - find each instance of it and restore state.
			TrInst->RestoreActorState();

			GrInst->TrackInst.Remove(ActiveTrackIndex);
		}
	}

	// Remove from the InterpGroup and free.
	ActiveGroup->InterpTracks.Remove(ActiveTrackIndex);

	// Remove from the Curve editor, if its there.
	IData->CurveEdSetup->RemoveCurve(ActiveTrack);
	CurveEd->CurveChanged();

	InterpEdTrans->EndSpecial();

	// Deselect current track and all keys as indices might be wrong.
	ClearKeySelection();
	SetActiveTrack(NULL, INDEX_NONE);
}

void WxInterpEd::OnContextTrackChangeFrame( wxCommandEvent& In  )
{
	check(ActiveGroup);

	UInterpTrack* Track = ActiveGroup->InterpTracks(ActiveTrackIndex);
	UInterpTrackMove* MoveTrack = Cast<UInterpTrackMove>(Track);
	if(!MoveTrack)
		return;

	// Find the frame we want to convert to.
	INT     Id              = In.GetId();
	BYTE    DesiredFrame    = 0;
	if(Id == IDM_INTERP_TRACK_FRAME_WORLD)
	{
		DesiredFrame = IMF_World;
	}
	else if(Id == IDM_INTERP_TRACK_FRAME_RELINITIAL)
	{
		DesiredFrame = IMF_RelativeToInitial;
	}
	else
	{
		check(0);
	}

	// Do nothing if already in desired frame
	if( DesiredFrame == MoveTrack->MoveFrame )
	{
		return;
	}
	
	// Find the first instance of this group. This is the one we are going to use to store the curve relative to.
	UInterpGroupInst* GrInst = Interp->FindFirstGroupInst(ActiveGroup);
	check(GrInst);

	AActor* Actor = GrInst->GroupActor;
	if(!Actor)
	{
		appMsgf(0, TEXT("No Actor for this group"));
		return;
	}

	// Get instance of movement track, for the initial TM.
	UInterpTrackInstMove* MoveTrackInst = CastChecked<UInterpTrackInstMove>( GrInst->TrackInst(ActiveTrackIndex) );

	// Find the frame to convert key-frame from.
	FMatrix FromFrameTM = MoveTrack->GetMoveRefFrame(MoveTrackInst);

	// Find the frame to convert the key-frame into.
	AActor* BaseActor = Actor->GetBase();

	FMatrix BaseTM = FMatrix::Identity;
	if(BaseActor)
	{
		BaseTM = FMatrix( BaseActor->Location, BaseActor->Rotation );
		BaseTM.RemoveScaling();
	}

	FMatrix ToFrameTM = FMatrix::Identity;
	if( DesiredFrame == IMF_World )
	{
		if(BaseActor)
		{
			ToFrameTM = BaseTM;
		}
		else
		{
			ToFrameTM = FMatrix::Identity;
		}
	}
	else if( DesiredFrame == IMF_RelativeToInitial )
	{
		if(BaseActor)
		{
			ToFrameTM = MoveTrackInst->InitialTM * BaseTM;
		}
		else
		{
			ToFrameTM = MoveTrackInst->InitialTM;
		}
	}
	FMatrix InvToFrameTM = ToFrameTM.Inverse();


	// Iterate over each keyframe. Convert key into world reference frame, then into new desired reference frame.
	check( MoveTrack->PosTrack.Points.Num() == MoveTrack->EulerTrack.Points.Num() );
	for(INT i=0; i<MoveTrack->PosTrack.Points.Num(); i++)
	{
		FQuat KeyQuat = FQuat::MakeFromEuler( MoveTrack->EulerTrack.Points(i).OutVal );
		FMatrix KeyTM = FMatrix( MoveTrack->PosTrack.Points(i).OutVal, KeyQuat );

		FMatrix WorldKeyTM = KeyTM * FromFrameTM;

		FVector WorldArriveTan = FromFrameTM.TransformNormal( MoveTrack->PosTrack.Points(i).ArriveTangent );
		FVector WorldLeaveTan = FromFrameTM.TransformNormal( MoveTrack->PosTrack.Points(i).LeaveTangent );

		FMatrix RelKeyTM = WorldKeyTM * InvToFrameTM;

		MoveTrack->PosTrack.Points(i).OutVal = RelKeyTM.GetOrigin();
		MoveTrack->PosTrack.Points(i).ArriveTangent = ToFrameTM.InverseTransformNormal( WorldArriveTan );
		MoveTrack->PosTrack.Points(i).LeaveTangent = ToFrameTM.InverseTransformNormal( WorldLeaveTan );

		MoveTrack->EulerTrack.Points(i).OutVal = FQuat(RelKeyTM).Euler();
	}

	MoveTrack->MoveFrame = DesiredFrame;

	//PropertyWindow->Refresh(); // Don't know why this doesn't work...

	PropertyWindow->SetObject(NULL, 1, 1, 0);
	PropertyWindow->SetObject(Track, 1, 1, 0);
}

void WxInterpEd::OnContextGroupRename( wxCommandEvent& In )
{
	if(!ActiveGroup)
	{
		return;
	}

	WxDlgGenericStringEntry dlg;
	FName NewName = ActiveGroup->GroupName;
	UBOOL bValidName = false;

	while(!bValidName)
	{
		INT Result = dlg.ShowModal( TEXT("Rename Group"), TEXT("New Group Name:"), *NewName );
		if( Result != wxID_OK )
		{
			return;
		}

		NewName = FName(*dlg.EnteredString);
		bValidName = true;

		// Check this name does not already exist.
		for(INT i=0; i<IData->InterpGroups.Num() && bValidName; i++)
		{
			if(IData->InterpGroups(i)->GroupName == NewName)
			{
				bValidName = false;
			}
		}

		if(!bValidName)
		{
			appMsgf( 0, TEXT("Name already exists - please choose a unique name for this Group.") );
		}
	}
	
	// We also need to change the name of the variable connector on all SeqAct_Interps in this level using this InterpData
	USequence* RootSeq = Interp->GetRootSequence();
	check(RootSeq);

	TArray<USequenceObject*> MatineeActions;
	RootSeq->FindSeqObjectsByClass( USeqAct_Interp::StaticClass(), MatineeActions );

	for(INT i=0; i<MatineeActions.Num(); i++)
	{
		USeqAct_Interp* TestAction = CastChecked<USeqAct_Interp>( MatineeActions(i) );
		check(TestAction);
	
		UInterpData* TestData = TestAction->FindInterpDataFromVariable();
		if(TestData && TestData == IData)
		{
			INT VarIndex = TestAction->FindConnectorIndex( FString(*ActiveGroup->GroupName), LOC_VARIABLE );
			if(VarIndex != INDEX_NONE && VarIndex >= 1) // Ensure variable index is not the reserved first one.
			{
				TestAction->VariableLinks(VarIndex).LinkDesc = *NewName;
			}
		}
	}

	// Finally, change the name of the InterpGroup.
	ActiveGroup->GroupName = NewName;
}

void WxInterpEd::OnContextGroupDelete( wxCommandEvent& In )
{
	if(!ActiveGroup)
	{
		return;
	}

	// Check we REALLY want to do this.
	UBOOL bDoDestroy = appMsgf(1, TEXT("This will destroy InterpGroup (%s) and all Track data within it. Are you sure?"), *ActiveGroup->GroupName);
	if(!bDoDestroy)
	{
		return;
	}

	InterpEdTrans->BeginSpecial( TEXT("GroupDelete") );
	Interp->Modify();
	IData->Modify();

	// Mark InterpGroup and all InterpTracks as Modified.
	ActiveGroup->Modify();
	for(INT j=0; j<ActiveGroup->InterpTracks.Num(); j++)
	{
		ActiveGroup->InterpTracks(j)->Modify();
	}

	// First, destroy any instances of this group.
	INT i=0;
	while( i<Interp->GroupInst.Num() )
	{
		UInterpGroupInst* GrInst = Interp->GroupInst(i);
		if(GrInst->Group == ActiveGroup)
		{
			// Mark InterpGroupInst and all InterpTrackInsts as Modified.
			GrInst->Modify();
			for(INT j=0; j<GrInst->TrackInst.Num(); j++)
			{
				GrInst->TrackInst(j)->Modify();
			}

			// Restor all state in this group before exiting
			GrInst->RestoreGroupActorState();

			// Clean up GroupInst
			//GrInst->TermGroupInst();
			// Can't do this here as it actually deletes the TrackInsts, which means we can't undo this operation!

			// Remove from SeqAct_Interps list of GroupInsts
			Interp->GroupInst.Remove(i);
		}
		else
		{
			i++;
		}
	}

	// Then remove the group itself from the InterpData.
	IData->InterpGroups.RemoveItem(ActiveGroup);

	// Deselect everything.
	ClearKeySelection();

	// Also remove the variable connector that corresponded to this group in all SeqAct_Interps in the level.
	UpdateMatineeActionConnectors();	

	InterpEdTrans->EndSpecial();

	// Stop having the camera locked to this group if it currently is.
	if(CamViewGroup == ActiveGroup)
	{
		LockCamToGroup(NULL);
	}

	// Finally, deselect any groups.
	SetActiveTrack(NULL, INDEX_NONE);
}

void WxInterpEd::OnContextGroupSetParent( wxCommandEvent& In )
{
	SelectActiveGroupParent();
}

// Iterate over keys changing their interpolation mode and adjusting tangents appropriately.
void WxInterpEd::OnContextKeyInterpMode( wxCommandEvent& In )
{

	for(INT i=0; i<Opt->SelectedKeys.Num(); i++)
	{
		FInterpEdSelKey& SelKey = Opt->SelectedKeys(i);
		UInterpTrack* Track = SelKey.Group->InterpTracks(SelKey.TrackIndex);

		UInterpTrackMove* MoveTrack = Cast<UInterpTrackMove>(Track);
		if(MoveTrack)
		{
			if(In.GetId() == IDM_INTERP_KEYMODE_LINEAR)
			{
				MoveTrack->PosTrack.Points(SelKey.KeyIndex).InterpMode = CIM_Linear;
				MoveTrack->EulerTrack.Points(SelKey.KeyIndex).InterpMode = CIM_Linear;
			}
			else if(In.GetId() == IDM_INTERP_KEYMODE_CURVE)
			{
				MoveTrack->PosTrack.Points(SelKey.KeyIndex).InterpMode = CIM_CurveAuto;
				MoveTrack->EulerTrack.Points(SelKey.KeyIndex).InterpMode = CIM_CurveAuto;
			}
			else if(In.GetId() == IDM_INTERP_KEYMODE_CURVEBREAK)
			{
				MoveTrack->PosTrack.Points(SelKey.KeyIndex).InterpMode = CIM_CurveBreak;
				MoveTrack->EulerTrack.Points(SelKey.KeyIndex).InterpMode = CIM_CurveBreak;
			}
			else if(In.GetId() == IDM_INTERP_KEYMODE_CONSTANT)
			{
				MoveTrack->PosTrack.Points(SelKey.KeyIndex).InterpMode = CIM_Constant;
				MoveTrack->EulerTrack.Points(SelKey.KeyIndex).InterpMode = CIM_Constant;
			}

			MoveTrack->PosTrack.AutoSetTangents(MoveTrack->LinCurveTension);
			MoveTrack->EulerTrack.AutoSetTangents(MoveTrack->AngCurveTension);
		}

		UInterpTrackFloatBase* FloatTrack = Cast<UInterpTrackFloatBase>(Track);
		if(FloatTrack)
		{
			if(In.GetId() == IDM_INTERP_KEYMODE_LINEAR)
			{
				FloatTrack->FloatTrack.Points(SelKey.KeyIndex).InterpMode = CIM_Linear;
			}
			else if(In.GetId() == IDM_INTERP_KEYMODE_CURVE)
			{
				FloatTrack->FloatTrack.Points(SelKey.KeyIndex).InterpMode = CIM_CurveAuto;
			}
			else if(In.GetId() == IDM_INTERP_KEYMODE_CURVEBREAK)
			{
				FloatTrack->FloatTrack.Points(SelKey.KeyIndex).InterpMode = CIM_CurveBreak;
			}
			else if(In.GetId() == IDM_INTERP_KEYMODE_CONSTANT)
			{
				FloatTrack->FloatTrack.Points(SelKey.KeyIndex).InterpMode = CIM_Constant;
			}

			FloatTrack->FloatTrack.AutoSetTangents(FloatTrack->CurveTension);
		}
	}

	CurveEd->UpdateDisplay();
}

/** Rename an event. Handle removing/adding connectors as appropriate. */
void WxInterpEd::OnContextRenameEventKey(wxCommandEvent& In)
{
	// Only works if one Event key is selected.
	if(Opt->SelectedKeys.Num() != 1)
	{
		return;
	}

	// Find the EventNames of selected key
	FName EventNameToChange;
	FInterpEdSelKey& SelKey = Opt->SelectedKeys(0);
	UInterpTrack* Track = SelKey.Group->InterpTracks(SelKey.TrackIndex);
	UInterpTrackEvent* EventTrack = Cast<UInterpTrackEvent>(Track);
	if(EventTrack)
	{
		EventNameToChange = EventTrack->EventTrack(SelKey.KeyIndex).EventName; 
	}
	else
	{
		return;
	}

	// Pop up dialog to ask for new name.
	WxDlgGenericStringEntry dlg;
	INT Result = dlg.ShowModal( TEXT("Enter New Event Name"), TEXT("New Event Name:"), *EventNameToChange );
	if( Result != wxID_OK )
		return;		

	dlg.EnteredString.Replace(TEXT(" "),TEXT("_"));
	FName NewEventName = FName( *dlg.EnteredString );

	// If this Event name is already in use- disallow it
	TArray<FName> CurrentEventNames;
	IData->GetAllEventNames(CurrentEventNames);
	if( CurrentEventNames.ContainsItem(NewEventName) )
	{
		appMsgf( 0, TEXT("Sorry - Event name already in use.") );
		return;
	}
	
	// Then go through all keys, changing those with this name to the new one.
	for(INT i=0; i<IData->InterpGroups.Num(); i++)
	{
		UInterpGroup* Group = IData->InterpGroups(i);
		for(INT j=0; j<Group->InterpTracks.Num(); j++)
		{
			UInterpTrackEvent* EventTrack = Cast<UInterpTrackEvent>( Group->InterpTracks(j) );
			if(EventTrack)
			{
				for(INT k=0; k<EventTrack->EventTrack.Num(); k++)
				{
					if(EventTrack->EventTrack(k).EventName == EventNameToChange)
					{
						EventTrack->EventTrack(k).EventName = NewEventName;
					}	
				}
			}			
		}
	}

	// We also need to change the name of the output connector on all SeqAct_Interps using this InterpData
	USequence* RootSeq = Interp->GetRootSequence();
	check(RootSeq);

	TArray<USequenceObject*> MatineeActions;
	RootSeq->FindSeqObjectsByClass( USeqAct_Interp::StaticClass(), MatineeActions );

	for(INT i=0; i<MatineeActions.Num(); i++)
	{
		USeqAct_Interp* TestAction = CastChecked<USeqAct_Interp>( MatineeActions(i) );
		check(TestAction);
	
		UInterpData* TestData = TestAction->FindInterpDataFromVariable();
		if(TestData && TestData == IData)
		{
			INT OutputIndex = TestAction->FindConnectorIndex( FString(*EventNameToChange), LOC_OUTPUT );
			if(OutputIndex != INDEX_NONE && OutputIndex >= 2) // Ensure Output index is not one of the reserved first 2.
			{
				TestAction->OutputLinks(OutputIndex).LinkDesc = *NewEventName;
			}
		}
	}
}


void WxInterpEd::OnMenuUndo(wxCommandEvent& In)
{
	InterpEdUndo();
}

void WxInterpEd::OnMenuRedo(wxCommandEvent& In)
{
	InterpEdRedo();
}

void WxInterpEd::OnToggleCurveEd(wxCommandEvent& In)
{
	UBOOL bShowCurveEd = In.IsChecked();

	// Show curve editor by splitting the window
	if(bShowCurveEd && !GraphSplitterWnd->IsSplit())
	{
		GraphSplitterWnd->SplitHorizontally( CurveEd, TrackWindow, GraphSplitPos );
		CurveEd->Show(true);
	}
	// Hide the graph editor 
	else if(!bShowCurveEd && GraphSplitterWnd->IsSplit())
	{
		GraphSplitterWnd->Unsplit( CurveEd );
	}
}

/** Called when sash position changes so we can remember the sash position. */
void WxInterpEd::OnGraphSplitChangePos(wxSplitterEvent& In)
{
	GraphSplitPos = GraphSplitterWnd->GetSashPosition();
}

/** Turn keyframe snap on/off. */
void WxInterpEd::OnToggleSnap(wxCommandEvent& In)
{
	bSnapEnabled = In.IsChecked();

	CurveEd->SetInSnap(bSnapEnabled, SnapAmount);
}

/** The snap resolution combo box was changed. */
void WxInterpEd::OnChangeSnapSize(wxCommandEvent& In)
{
	INT NewSelection = In.GetInt();
	check(NewSelection >= 0 && NewSelection < ARRAY_COUNT(InterpEdSnapSizes));
	SnapAmount = InterpEdSnapSizes[NewSelection];

	CurveEd->SetInSnap(bSnapEnabled, SnapAmount);
}

/** Adjust the view so the entire sequence fits into the viewport. */
void WxInterpEd::OnViewFitSequence(wxCommandEvent& In)
{
	InterpEdVC->ViewStartTime = 0.f;
	InterpEdVC->ViewEndTime = IData->InterpLength;

	SyncCurveEdView();
}

/** Adjust the view so the looped section fits into the viewport. */
void WxInterpEd::OnViewFitLoop(wxCommandEvent& In)
{
	// Do nothing if loop section is too small!
	FLOAT LoopRange = IData->EdSectionEnd - IData->EdSectionStart;
	if(LoopRange > 0.01f)
	{
		InterpEdVC->ViewStartTime = IData->EdSectionStart;
		InterpEdVC->ViewEndTime = IData->EdSectionEnd;

		SyncCurveEdView();
	}
}

/*-----------------------------------------------------------------------------
	WxInterpEdToolBar
-----------------------------------------------------------------------------*/

WxInterpEdToolBar::WxInterpEdToolBar( wxWindow* InParent, wxWindowID InID )
: wxToolBar( InParent, InID, wxDefaultPosition, wxDefaultSize, wxTB_HORIZONTAL | wxTB_3DBUTTONS )
{
	AddB.Load( TEXT("MAT_AddKey") );
	PlayB.Load( TEXT("MAT_Play") );
	LoopSectionB.Load( TEXT("MAT_PlayLoopSection") );
	StopB.Load( TEXT("MAT_Stop") );
	UndoB.Load( TEXT("MAT_Undo") );
	RedoB.Load( TEXT("MAT_Redo") );
	CurveEdB.Load( TEXT("MAT_CurveEd") );
	SnapB.Load( TEXT("MAT_ToggleSnap") );
	FitSequenceB.Load( TEXT("MAT_FitSequence") );
	FitLoopB.Load( TEXT("MAT_FitLoop") );

	Speed1B.Load(TEXT("CASC_Speed_1"));
	Speed10B.Load(TEXT("CASC_Speed_10"));
	Speed25B.Load(TEXT("CASC_Speed_25"));
	Speed50B.Load(TEXT("CASC_Speed_50"));
	Speed100B.Load(TEXT("CASC_Speed_100"));

	SetToolBitmapSize( wxSize( 18, 18 ) );

	AddTool( IDM_INTERP_ADDKEY, AddB, TEXT("Add Key") );

	AddSeparator();

	AddTool( IDM_INTERP_PLAY, PlayB, TEXT("Play") );
	AddTool( IDM_INTERP_PLAYLOOPSECTION, LoopSectionB, TEXT("Loop Section") );
	AddTool( IDM_INTERP_STOP, StopB, TEXT("Stop") );

	AddSeparator();

	AddRadioTool(IDM_INTERP_SPEED_100,	TEXT("Full Speed"), Speed100B);
	AddRadioTool(IDM_INTERP_SPEED_50,	TEXT("50% Speed"), Speed50B);
	AddRadioTool(IDM_INTERP_SPEED_25,	TEXT("25% Speed"), Speed25B);
	AddRadioTool(IDM_INTERP_SPEED_10,	TEXT("10% Speed"), Speed10B);
	AddRadioTool(IDM_INTERP_SPEED_1,	TEXT("1% Speed"), Speed1B);
	ToggleTool(IDM_INTERP_SPEED_100, true);

	AddSeparator();

	AddTool( IDM_INTERP_EDIT_UNDO, UndoB, TEXT("Undo") );
	AddTool( IDM_INTERP_EDIT_REDO, RedoB, TEXT("Redo") );

	AddSeparator();

	AddCheckTool( IDM_INTERP_TOGGLE_CURVEEDITOR, TEXT("Toggle Curve Editor"), CurveEdB );

	AddSeparator();

	AddCheckTool( IDM_INTERP_TOGGLE_SNAP, TEXT("Toggle Snap"), SnapB );
	
	// Create snap-size combo
	SnapCombo = new wxComboBox( this, IDM_INTERP_SNAPCOMBO, TEXT(""), wxDefaultPosition, wxSize(60, -1), 0, NULL, wxCB_READONLY );

	for(INT i=0; i<ARRAY_COUNT(InterpEdSnapSizes); i++)
	{
		FString SnapCaption = FString::Printf( TEXT("%1.2fs"), InterpEdSnapSizes[i] );
		SnapCombo->Append( *SnapCaption );
	}

	SnapCombo->SetSelection(2);

	AddControl(SnapCombo);

	AddSeparator();
	AddTool( IDM_INTERP_VIEW_FITSEQUENCE, TEXT("View Fit Sequence"), FitSequenceB );
	AddTool( IDM_INTERP_VIEW_FITLOOP, TEXT("View Fit Loop"), FitLoopB );

	Realize();
}

WxInterpEdToolBar::~WxInterpEdToolBar()
{
}


/*-----------------------------------------------------------------------------
	WxInterpEdMenuBar
-----------------------------------------------------------------------------*/

WxInterpEdMenuBar::WxInterpEdMenuBar()
{
	EditMenu = new wxMenu();
	Append( EditMenu, TEXT("&Edit") );

	EditMenu->Append( IDM_INTERP_EDIT_UNDO, TEXT("&Undo"), TEXT("") );
	EditMenu->Append( IDM_INTERP_EDIT_REDO, TEXT("&Redo"), TEXT("") );
	EditMenu->AppendSeparator();
	EditMenu->Append( IDM_INTERP_EDIT_INSERTSPACE, TEXT("Insert Space At Current"), TEXT("") );
	EditMenu->Append( IDM_INTERP_EDIT_STRETCHSECTION, TEXT("Stretch Section"), TEXT("") );
	EditMenu->Append( IDM_INTERP_EDIT_DELETESECTION, TEXT("Delete Section"), TEXT("") );
	EditMenu->Append( IDM_INTERP_EDIT_SELECTINSECTION, TEXT("Select Keys In Section"), TEXT("") );
	EditMenu->AppendSeparator();
	EditMenu->Append( IDM_INTERP_EDIT_DUPLICATEKEYS, TEXT("Duplicate Selected Keys"), TEXT("") );
	EditMenu->AppendSeparator();
	EditMenu->Append( IDM_INTERP_EDIT_SAVEPATHTIME, TEXT("Save As Path-Building Positions"), TEXT("") );
	EditMenu->Append( IDM_INTERP_EDIT_JUMPTOPATHTIME, TEXT("Jump To Path-Building Positions"), TEXT("") );
}

WxInterpEdMenuBar::~WxInterpEdMenuBar()
{

}

/*-----------------------------------------------------------------------------
	WxMBInterpEdGroupMenu
-----------------------------------------------------------------------------*/

WxMBInterpEdGroupMenu::WxMBInterpEdGroupMenu(WxInterpEd* InterpEd)
{
	// Nothing to do if no group selected
	if(!InterpEd->ActiveGroup)
		return;

	UBOOL bIsDirGroup = InterpEd->ActiveGroup->IsA(UInterpGroupDirector::StaticClass());

	for(INT i=0; i<InterpEd->InterpTrackClasses.Num(); i++)
	{
		UInterpTrack* DefTrack = (UInterpTrack*)InterpEd->InterpTrackClasses(i)->GetDefaultObject();
		if(!DefTrack->bDirGroupOnly)
		{
			FString NewTrackString = FString::Printf( TEXT("Add New %s"), InterpEd->InterpTrackClasses(i)->GetDescription() );
			Append( IDM_INTERP_NEW_TRACK_START+i, *NewTrackString, TEXT("") );
		}
	}

	// Add Director-group specific tracks to seperate menu underneath.
	if(bIsDirGroup)
	{
		AppendSeparator();

		for(INT i=0; i<InterpEd->InterpTrackClasses.Num(); i++)
		{
			UInterpTrack* DefTrack = (UInterpTrack*)InterpEd->InterpTrackClasses(i)->GetDefaultObject();
			if(DefTrack->bDirGroupOnly)
			{
				FString NewTrackString = FString::Printf( TEXT("Add New %s"), InterpEd->InterpTrackClasses(i)->GetDescription() );
				Append( IDM_INTERP_NEW_TRACK_START+i, *NewTrackString, TEXT("") );
			}
		}
	}

	AppendSeparator();

	Append( IDM_INTERP_GROUP_RENAME, TEXT("Rename Group"), TEXT("") );
	Append( IDM_INTERP_GROUP_DELETE, TEXT("Delete Group"), TEXT("") );
}

WxMBInterpEdGroupMenu::~WxMBInterpEdGroupMenu()
{

}

/*-----------------------------------------------------------------------------
	WxMBInterpEdTrackMenu
-----------------------------------------------------------------------------*/

WxMBInterpEdTrackMenu::WxMBInterpEdTrackMenu(WxInterpEd* InterpEd)
{
	if(InterpEd->ActiveTrackIndex == INDEX_NONE)
		return;

	Append( IDM_INTERP_TRACK_RENAME, TEXT("Rename Track"), TEXT("") );
	Append( IDM_INTERP_TRACK_DELETE, TEXT("Delete Track"), TEXT("") );

	check(InterpEd->ActiveGroup);
	UInterpTrack* Track = InterpEd->ActiveGroup->InterpTracks( InterpEd->ActiveTrackIndex );
	UInterpTrackMove* MoveTrack = Cast<UInterpTrackMove>(Track);

	if( MoveTrack )
	{
		AppendSeparator();
		AppendRadioItem( IDM_INTERP_TRACK_FRAME_WORLD, TEXT("World Frame"), TEXT("") );
		AppendRadioItem( IDM_INTERP_TRACK_FRAME_RELINITIAL, TEXT("Relative To Initial"), TEXT("") );

		// Check the currently the selected movement frame
		if( MoveTrack->MoveFrame == IMF_World )
		{
			Check(IDM_INTERP_TRACK_FRAME_WORLD, true);
		}
		else if( MoveTrack->MoveFrame == IMF_RelativeToInitial )
		{
			Check(IDM_INTERP_TRACK_FRAME_RELINITIAL, true);
		}
	}
}

WxMBInterpEdTrackMenu::~WxMBInterpEdTrackMenu()
{

}

/*-----------------------------------------------------------------------------
	WxMBInterpEdBkgMenu
-----------------------------------------------------------------------------*/

WxMBInterpEdBkgMenu::WxMBInterpEdBkgMenu(WxInterpEd* InterpEd)
{
	Append( IDM_INTERP_NEW_GROUP, TEXT("Add New Group"), TEXT("") );
	
	TArray<UInterpTrack*> Results;
	InterpEd->IData->FindTracksByClass( UInterpTrackDirector::StaticClass(), Results );
	if(Results.Num() == 0)
	{
		Append( IDM_INTERP_NEW_DIRECTOR_GROUP, TEXT("Add New Director Group"), TEXT("") );
	}
}

WxMBInterpEdBkgMenu::~WxMBInterpEdBkgMenu()
{

}

/*-----------------------------------------------------------------------------
	WxMBInterpEdKeyMenu
-----------------------------------------------------------------------------*/

WxMBInterpEdKeyMenu::WxMBInterpEdKeyMenu(WxInterpEd* InterpEd)
{
	UBOOL bHaveMoveKeys = false;
	UBOOL bHaveFloatKeys = false;
	UBOOL bHaveEventKeys = false;

	for(INT i=0; i<InterpEd->Opt->SelectedKeys.Num(); i++)
	{
		FInterpEdSelKey& SelKey = InterpEd->Opt->SelectedKeys(i);
		UInterpTrack* Track = SelKey.Group->InterpTracks(SelKey.TrackIndex);

		if( Track->IsA(UInterpTrackMove::StaticClass()) )
			bHaveMoveKeys = true;
		else if( Track->IsA(UInterpTrackFloatBase::StaticClass()) )
			bHaveFloatKeys = true;
		else if( Track->IsA(UInterpTrackEvent::StaticClass()) )
			bHaveEventKeys = true;
	}

	if(bHaveMoveKeys || bHaveFloatKeys)
	{
		wxMenu* MoveMenu = new wxMenu();
		MoveMenu->Append( IDM_INTERP_KEYMODE_CURVE, TEXT("Curve"), TEXT("") );
		MoveMenu->Append( IDM_INTERP_KEYMODE_CURVEBREAK, TEXT("Curve (Break)"), TEXT("") );
		MoveMenu->Append( IDM_INTERP_KEYMODE_LINEAR, TEXT("Linear"), TEXT("") );
		MoveMenu->Append( IDM_INTERP_KEYMODE_CONSTANT, TEXT("Constant"), TEXT("") );
		Append( IDM_INTERP_MOVEKEYMODEMENU, TEXT("Interp Mode"), MoveMenu );
	}

	if(bHaveEventKeys && InterpEd->Opt->SelectedKeys.Num() == 1)
	{
		Append( IDM_INTERP_EVENTKEY_RENAME, TEXT("Rename Event"), TEXT("") );
	}
}

WxMBInterpEdKeyMenu::~WxMBInterpEdKeyMenu()
{

}