/*=============================================================================
	InterpEditorTools.cpp: Interpolation editing support tools
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by James Golding
=============================================================================*/

#include "UnrealEd.h"
#include "EngineSequenceClasses.h"
#include "EngineInterpolationClasses.h"
#include "UnLinkedObjDrawUtils.h"
#include "InterpEditor.h"

static const FColor ActiveCamColor(255, 255, 0);
static const INT	DuplicateKeyOffset(10);

///// UTILS

void WxInterpEd::TickInterp(FLOAT DeltaTime)
{
	if(Interp->bIsPlaying)
	{
		// Modify playback rate by desired speed.
		FLOAT TimeDilation = GEditor->Level->GetLevelInfo()->TimeDilation;
		Interp->StepInterp(DeltaTime * PlaybackSpeed * TimeDilation, true);
		
		// If we are looping the selected section, when we pass the end, place it back to the beginning 
		if(bLoopingSection)
		{
			if(Interp->Position >= IData->EdSectionEnd)
			{
				Interp->UpdateInterp(IData->EdSectionStart);
				Interp->Play();
			}
		}

		UpdateCameraToGroup();
		UpdateCamColours();
		CurveEd->SetPositionMarker(true, Interp->Position, PosMarkerColor );
	}
}

static UBOOL bIgnoreActorSelection = false;

void WxInterpEd::SetActiveTrack(UInterpGroup* InGroup, INT InTrackIndex)
{
	// Do nothing if already active.
	if( InGroup == ActiveGroup && InTrackIndex == ActiveTrackIndex)
		return;

	// If nothing selected.
	if( !InGroup )
	{
		check( InTrackIndex == INDEX_NONE );

		ActiveGroup = NULL;
		ActiveTrackIndex = INDEX_NONE;

		PropertyWindow->SetObject(NULL,0,0,0);
		GUnrealEd->SelectNone( GUnrealEd->Level, true);

		ClearKeySelection();

		return;
	}

	// We have selected a group

	ActiveGroup = InGroup;

	// If this track has an Actor, select it (if not already).
	UInterpGroupInst* GrInst = Interp->FindFirstGroupInst(ActiveGroup);
	check( GrInst ); // Should be at least 1 instance of every group.
	check( GrInst->TrackInst.Num() == ActiveGroup->InterpTracks.Num() );

	AActor* Actor = GrInst->GroupActor;
	if( Actor && !GSelectionTools.IsSelected( Actor ) )
	{
		bIgnoreActorSelection = true;
		GUnrealEd->SelectNone( GUnrealEd->Level, true);
		GUnrealEd->SelectActor( GUnrealEd->Level, Actor, 1, 1 );
		bIgnoreActorSelection = false;
	}

	// If a track is selected as well, put its properties in the Property Window
	if(InTrackIndex != INDEX_NONE)
	{
		check( InTrackIndex >= 0 && InTrackIndex < ActiveGroup->InterpTracks.Num() );
		ActiveTrackIndex = InTrackIndex;

		UInterpTrack* Track = ActiveGroup->InterpTracks(ActiveTrackIndex);

		// Set the property window to the selected track.
		PropertyWindow->SetObject( Track, 1,1,0 );
	}
	else
	{
		ActiveTrackIndex = INDEX_NONE;

		// If just selecting the group - show its properties
		PropertyWindow->SetObject( ActiveGroup, 1,1,0 );
	}
}

void WxInterpEd::ClearKeySelection()
{
	Opt->SelectedKeys.Empty();
	bAdjustingKeyframe = false;

	InterpEdVC->Viewport->Invalidate();
}

void WxInterpEd::AddKeyToSelection(UInterpGroup* InGroup, INT InTrackIndex, INT InKeyIndex, UBOOL bAutoWind)
{
	check(InGroup);

	UInterpTrack* Track = InGroup->InterpTracks(InTrackIndex);
	check(Track);

	check( InKeyIndex >= 0 && InKeyIndex < Track->GetNumKeyframes() );

	// If key is not already selected, add to selection set.
	if( !KeyIsInSelection(InGroup, InTrackIndex, InKeyIndex) )
	{
		// Add to array of selected keys.
		Opt->SelectedKeys.AddItem( FInterpEdSelKey(InGroup, InTrackIndex, InKeyIndex) );
	}

	// If this is the first and only keyframe selected, make track active and wind to it.
	if(Opt->SelectedKeys.Num() == 1 && bAutoWind)
	{
		SetActiveTrack( InGroup, InTrackIndex );

		FLOAT KeyTime = Track->GetKeyframeTime(InKeyIndex);
		SetInterpPosition(KeyTime);

		// When jumping to keyframe, update the pivot so the widget is in the right place.
		UInterpGroupInst* GrInst = Interp->FindFirstGroupInst(InGroup);
		if(GrInst && GrInst->GroupActor)
		{
			GEditor->SetPivot( GrInst->GroupActor->Location, 0, 0, 1 );
		}

		bAdjustingKeyframe = true;
	}

	if(Opt->SelectedKeys.Num() != 1)
	{
		bAdjustingKeyframe = false;
	}

	InterpEdVC->Viewport->Invalidate();
}

void WxInterpEd::RemoveKeyFromSelection(UInterpGroup* InGroup, INT InTrackIndex, INT InKeyIndex)
{
	for(INT i=0; i<Opt->SelectedKeys.Num(); i++)
	{
		if( Opt->SelectedKeys(i).Group == InGroup && 
			Opt->SelectedKeys(i).TrackIndex == InTrackIndex && 
			Opt->SelectedKeys(i).KeyIndex == InKeyIndex )
		{
			Opt->SelectedKeys.Remove(i);

			bAdjustingKeyframe = false;

			InterpEdVC->Viewport->Invalidate();

			return;
		}
	}
}

UBOOL WxInterpEd::KeyIsInSelection(UInterpGroup* InGroup, INT InTrackIndex, INT InKeyIndex)
{
	for(INT i=0; i<Opt->SelectedKeys.Num(); i++)
	{
		if( Opt->SelectedKeys(i).Group == InGroup && 
			Opt->SelectedKeys(i).TrackIndex == InTrackIndex && 
			Opt->SelectedKeys(i).KeyIndex == InKeyIndex )
			return true;
	}

	return false;
}

/** Clear selection and then select all keys within the gree loop-section. */
void WxInterpEd::SelectKeysInLoopSection()
{
	ClearKeySelection();

	// Add keys that are within current section to selection
	for(INT i=0; i<IData->InterpGroups.Num(); i++)
	{
		UInterpGroup* Group = IData->InterpGroups(i);
		for(INT j=0; j<Group->InterpTracks.Num(); j++)
		{
			UInterpTrack* Track = Group->InterpTracks(j);
			Track->Modify();

			for(INT k=0; k<Track->GetNumKeyframes(); k++)
			{
				// Add keys in section to selection for deletion.
				FLOAT KeyTime = Track->GetKeyframeTime(k);
				if(KeyTime >= IData->EdSectionStart && KeyTime <= IData->EdSectionEnd)
				{
					// Add to selection for deletion.
					AddKeyToSelection(Group, j, k, false);
				}
			}
		}
	}
}

/** Calculate the start and end of the range of the selected keys. */
void WxInterpEd::CalcSelectedKeyRange(FLOAT& OutStartTime, FLOAT& OutEndTime)
{
	if(Opt->SelectedKeys.Num() == 0)
	{
		OutStartTime = 0.f;
		OutEndTime = 0.f;
	}
	else
	{
		OutStartTime = BIG_NUMBER;
		OutEndTime = -BIG_NUMBER;

		for(INT i=0; i<Opt->SelectedKeys.Num(); i++)
		{
			UInterpTrack* Track = Opt->SelectedKeys(i).Group->InterpTracks( Opt->SelectedKeys(i).TrackIndex );
			FLOAT KeyTime = Track->GetKeyframeTime( Opt->SelectedKeys(i).KeyIndex );

			OutStartTime = ::Min(KeyTime, OutStartTime);
			OutEndTime = ::Max(KeyTime, OutEndTime);
		}
	}
}

void WxInterpEd::DeleteSelectedKeys(UBOOL bDoTransaction)
{
	if(bDoTransaction)
	{
		InterpEdTrans->BeginSpecial( TEXT("Delete Selected Keys") );
		Interp->Modify();
		Opt->Modify();
	}

	TArray<UInterpTrack*> ModifiedTracks;

	UBOOL bRemovedEventKeys = false;
	for(INT i=0; i<Opt->SelectedKeys.Num(); i++)
	{
		FInterpEdSelKey& SelKey = Opt->SelectedKeys(i);
		UInterpTrack* Track = SelKey.Group->InterpTracks( SelKey.TrackIndex );

		check(Track);
		check(SelKey.KeyIndex >= 0 && SelKey.KeyIndex < Track->GetNumKeyframes());

		if(bDoTransaction)
		{
			// If not already done so, call Modify on this track now.
			if( !ModifiedTracks.ContainsItem(Track) )
			{
				Track->Modify();
				ModifiedTracks.AddItem(Track);
			}
		}

		// If this is an event key - we update the connectors later.
		if(Track->IsA(UInterpTrackEvent::StaticClass()))
		{
			bRemovedEventKeys = true;
		}
			
		Track->RemoveKeyframe(SelKey.KeyIndex);

		// If any other keys in the selection are on the same track but after the one we just deleted, decrement the index to correct it.
		for(INT j=i+1; j<Opt->SelectedKeys.Num(); j++)
		{
			if( Opt->SelectedKeys(j).Group == SelKey.Group &&
				Opt->SelectedKeys(j).TrackIndex == SelKey.TrackIndex &&
				Opt->SelectedKeys(j).KeyIndex > SelKey.KeyIndex )
			{
				Opt->SelectedKeys(j).KeyIndex--;
			}
		}
	}

	// If we removed some event keys - ensure all Matinee actions are up to date.
	if(bRemovedEventKeys)
	{
		UpdateMatineeActionConnectors();
	}

	// Select no keyframe.
	ClearKeySelection();

	if(bDoTransaction)
	{
		InterpEdTrans->EndSpecial();
	}
}

void WxInterpEd::DuplicateSelectedKeys()
{
	InterpEdTrans->BeginSpecial( TEXT("Duplicate Selected Keys") );
	Interp->Modify();
	Opt->Modify();

	TArray<UInterpTrack*> ModifiedTracks;

	for(INT i=0; i<Opt->SelectedKeys.Num(); i++)
	{
		FInterpEdSelKey& SelKey = Opt->SelectedKeys(i);
		UInterpTrack* Track = SelKey.Group->InterpTracks( SelKey.TrackIndex );

		check(Track);
		check(SelKey.KeyIndex >= 0 && SelKey.KeyIndex < Track->GetNumKeyframes());

		// If not already done so, call Modify on this track now.
		if( !ModifiedTracks.ContainsItem(Track) )
		{
			Track->Modify();
			ModifiedTracks.AddItem(Track);
		}
		
		FLOAT CurrentKeyTime = Track->GetKeyframeTime(SelKey.KeyIndex);
		FLOAT NewKeyTime = CurrentKeyTime + (FLOAT)DuplicateKeyOffset/InterpEdVC->PixelsPerSec;

		INT DupKeyIndex = Track->DuplicateKeyframe(SelKey.KeyIndex, NewKeyTime);

		// Change selection to select the new keyframe instead.
		SelKey.KeyIndex = DupKeyIndex;

		// If any other keys in the selection are on the same track but after the new key, increase the index to correct it.
		for(INT j=i+1; j<Opt->SelectedKeys.Num(); j++)
		{
			if( Opt->SelectedKeys(j).Group == SelKey.Group &&
				Opt->SelectedKeys(j).TrackIndex == SelKey.TrackIndex &&
				Opt->SelectedKeys(j).KeyIndex >= DupKeyIndex )
			{
				Opt->SelectedKeys(j).KeyIndex++;
			}
		}
	}

	InterpEdTrans->EndSpecial();
}

void WxInterpEd::SelectNextKey()
{
	if(ActiveTrackIndex == INDEX_NONE)
		return;

	check(ActiveGroup);
	UInterpTrack* Track = ActiveGroup->InterpTracks(ActiveTrackIndex);
	check(Track);

	INT NumKeys = Track->GetNumKeyframes();

	INT i;
	for(i=0; i < NumKeys-1 && Track->GetKeyframeTime(i) < (Interp->Position + KINDA_SMALL_NUMBER); i++);

	ClearKeySelection();
	AddKeyToSelection(ActiveGroup, ActiveTrackIndex, i, true);
}

void WxInterpEd::SelectPreviousKey()
{
	if(ActiveTrackIndex == INDEX_NONE)
		return;

	check(ActiveGroup);
	UInterpTrack* Track = ActiveGroup->InterpTracks(ActiveTrackIndex);
	check(Track);

	INT NumKeys = Track->GetNumKeyframes();

	INT i;
	for(i=NumKeys-1; i > 0 && Track->GetKeyframeTime(i) > (Interp->Position - KINDA_SMALL_NUMBER); i--);

	ClearKeySelection();
	AddKeyToSelection(ActiveGroup, ActiveTrackIndex, i, true);
}

/** Take the InTime and snap it to the current SnapAmount. Does nothing if bSnapEnabled is false */
FLOAT WxInterpEd::SnapTime(FLOAT InTime)
{
	if(bSnapEnabled)
	{
		return SnapAmount * appRound(InTime/SnapAmount);
	}
	else
	{
		return InTime;
	}
}

void WxInterpEd::BeginMoveMarker()
{
	if(InterpEdVC->GrabbedMarkerType == ISM_SeqEnd)
	{
		InterpEdVC->UnsnappedMarkerPos = IData->InterpLength;
		InterpEdTrans->BeginSpecial( TEXT("Move End Marker") );
		IData->Modify();
	}
	else if(InterpEdVC->GrabbedMarkerType == ISM_LoopStart)
	{
		InterpEdVC->UnsnappedMarkerPos = IData->EdSectionStart;
		InterpEdTrans->BeginSpecial( TEXT("Move Loop Start Marker") );
		IData->Modify();
	}
	else if(InterpEdVC->GrabbedMarkerType == ISM_LoopEnd)
	{
		InterpEdVC->UnsnappedMarkerPos = IData->EdSectionEnd;
		InterpEdTrans->BeginSpecial( TEXT("Move Loop End Marker") );
		IData->Modify();
	}
}

void WxInterpEd::EndMoveMarker()
{
	if(	InterpEdVC->GrabbedMarkerType == ISM_SeqEnd || 
		InterpEdVC->GrabbedMarkerType == ISM_LoopStart || 
		InterpEdVC->GrabbedMarkerType == ISM_LoopEnd)
	{
		InterpEdTrans->EndSpecial();
	}
}

void WxInterpEd::SetInterpEnd(FLOAT NewInterpLength)
{
	// Ensure non-negative end time.
	IData->InterpLength = ::Max(NewInterpLength, 0.f);
	
	CurveEd->SetEndMarker(true, IData->InterpLength);

	// Ensure the current position is always inside the valid sequence area.
	if(Interp->Position > IData->InterpLength)
	{
		SetInterpPosition(IData->InterpLength);
	}

	// Ensure loop points are inside sequence.
	IData->EdSectionStart = ::Clamp(IData->EdSectionStart, 0.f, IData->InterpLength);
	IData->EdSectionEnd = ::Clamp(IData->EdSectionEnd, 0.f, IData->InterpLength);
	CurveEd->SetRegionMarker(true, IData->EdSectionStart, IData->EdSectionEnd, RegionFillColor);
}

void WxInterpEd::MoveLoopMarker(FLOAT NewMarkerPos, UBOOL bIsStart)
{
	if(bIsStart)
	{
		IData->EdSectionStart = NewMarkerPos;
		IData->EdSectionEnd = ::Max(IData->EdSectionStart, IData->EdSectionEnd);				
	}
	else
	{
		IData->EdSectionEnd = NewMarkerPos;
		IData->EdSectionStart = ::Min(IData->EdSectionStart, IData->EdSectionEnd);
	}

	// Ensure loop points are inside sequence.
	IData->EdSectionStart = ::Clamp(IData->EdSectionStart, 0.f, IData->InterpLength);
	IData->EdSectionEnd = ::Clamp(IData->EdSectionEnd, 0.f, IData->InterpLength);

	CurveEd->SetRegionMarker(true, IData->EdSectionStart, IData->EdSectionEnd, RegionFillColor);
}

void WxInterpEd::BeginMoveSelectedKeys()
{
	InterpEdTrans->BeginSpecial( TEXT("MoveSelectedKeys") );
	Opt->Modify();

	TArray<UInterpTrack*> ModifiedTracks;
	for(INT i=0; i<Opt->SelectedKeys.Num(); i++)
	{
		FInterpEdSelKey& SelKey = Opt->SelectedKeys(i);

		UInterpTrack* Track = SelKey.Group->InterpTracks(SelKey.TrackIndex);
		check(Track);

		// If not already done so, call Modify on this track now.
		if( !ModifiedTracks.ContainsItem(Track) )
		{
			Track->Modify();
			ModifiedTracks.AddItem(Track);
		}

		SelKey.UnsnappedPosition = Track->GetKeyframeTime(SelKey.KeyIndex);
	}
}

void WxInterpEd::EndMoveSelectedKeys()
{
	InterpEdTrans->EndSpecial();
}

void WxInterpEd::MoveSelectedKeys(FLOAT DeltaTime)
{
	for(INT i=0; i<Opt->SelectedKeys.Num(); i++)
	{
		FInterpEdSelKey& SelKey = Opt->SelectedKeys(i);

		UInterpTrack* Track = SelKey.Group->InterpTracks(SelKey.TrackIndex);
		check(Track);

		SelKey.UnsnappedPosition += DeltaTime;
		FLOAT NewTime = SnapTime(SelKey.UnsnappedPosition);

		// Do nothing if already at target time.
		if( Track->GetKeyframeTime(SelKey.KeyIndex) != NewTime )
		{
			INT OldKeyIndex = SelKey.KeyIndex;
			INT NewKeyIndex = Track->SetKeyframeTime(SelKey.KeyIndex, NewTime);
			SelKey.KeyIndex = NewKeyIndex;

			// If the key changed index we need to search for any other selected keys on this track that may need their index adjusted because of this change.
			INT KeyMove = NewKeyIndex - OldKeyIndex;
			if(KeyMove > 0)
			{
				for(INT j=0; j<Opt->SelectedKeys.Num(); j++)
				{
					if( j == i ) // Don't look at one we just changed.
						continue;

					FInterpEdSelKey& TestKey = Opt->SelectedKeys(j);
					if( TestKey.TrackIndex == SelKey.TrackIndex && 
						TestKey.Group == SelKey.Group &&
						TestKey.KeyIndex > OldKeyIndex && 
						TestKey.KeyIndex <= NewKeyIndex)
					{
						TestKey.KeyIndex--;
					}
				}
			}
			else if(KeyMove < 0)
			{
				for(INT j=0; j<Opt->SelectedKeys.Num(); j++)
				{
					if( j == i )
						continue;

					FInterpEdSelKey& TestKey = Opt->SelectedKeys(j);
					if( TestKey.TrackIndex == SelKey.TrackIndex && 
						TestKey.Group == SelKey.Group &&
						TestKey.KeyIndex < OldKeyIndex && 
						TestKey.KeyIndex >= NewKeyIndex)
					{
						TestKey.KeyIndex++;
					}
				}
			}
		}

	} // FOR each selected key

	// Update positions at current time but with new keyframe times.
	SetInterpPosition(Interp->Position);

	CurveEd->UpdateDisplay();
}


void WxInterpEd::BeginDrag3DHandle(UInterpGroup* Group, INT TrackIndex)
{
	if(TrackIndex < 0 || TrackIndex >= Group->InterpTracks.Num())
	{
		return;
	}

	UInterpTrackMove* MoveTrack = Cast<UInterpTrackMove>( Group->InterpTracks(TrackIndex) );
	if(MoveTrack)
	{
		InterpEdTrans->BeginSpecial( TEXT("Drag 3D Curve Handle") );
		MoveTrack->Modify();
		bDragging3DHandle = true;
	}
}

void WxInterpEd::Move3DHandle(UInterpGroup* Group, INT TrackIndex, INT KeyIndex, UBOOL bArriving, const FVector& Delta)
{
	if(!bDragging3DHandle)
	{
		return;
	}

	if(TrackIndex < 0 || TrackIndex >= Group->InterpTracks.Num())
	{
		return;
	}

	UInterpTrackMove* MoveTrack = Cast<UInterpTrackMove>( Group->InterpTracks(TrackIndex) );
	if(MoveTrack)
	{
		if(KeyIndex < 0 || KeyIndex >= MoveTrack->PosTrack.Points.Num())
		{
			return;
		}

		UInterpGroupInst* GrInst = Interp->FindFirstGroupInst(Group);
		check(GrInst);
		check(GrInst->TrackInst.Num() == Group->InterpTracks.Num());
		UInterpTrackInstMove* MoveInst = CastChecked<UInterpTrackInstMove>( GrInst->TrackInst(TrackIndex) );

		FMatrix InvRefTM = MoveTrack->GetMoveRefFrame( MoveInst ).Inverse();
		FVector LocalDelta = InvRefTM.TransformNormal(Delta);

		BYTE InterpMode = MoveTrack->PosTrack.Points(KeyIndex).InterpMode;

		if(bArriving)
		{
			MoveTrack->PosTrack.Points(KeyIndex).ArriveTangent -= LocalDelta;

			// If keeping tangents smooth, update the LeaveTangent
			if(InterpMode != CIM_CurveBreak)
			{
				MoveTrack->PosTrack.Points(KeyIndex).LeaveTangent = MoveTrack->PosTrack.Points(KeyIndex).ArriveTangent;
			}
		}
		else
		{
			MoveTrack->PosTrack.Points(KeyIndex).LeaveTangent += LocalDelta;

			// If keeping tangents smooth, update the ArriveTangent
			if(InterpMode != CIM_CurveBreak)
			{
				MoveTrack->PosTrack.Points(KeyIndex).ArriveTangent = MoveTrack->PosTrack.Points(KeyIndex).LeaveTangent;
			}
		}

		// If adjusting an 'Auto' keypoint, switch it to 'User'
		if(InterpMode == CIM_CurveAuto)
		{
			MoveTrack->PosTrack.Points(KeyIndex).InterpMode = CIM_CurveUser;
			MoveTrack->EulerTrack.Points(KeyIndex).InterpMode = CIM_CurveUser;
		}

		// Update the curve editor to see curves change.
		CurveEd->UpdateDisplay();
	}
}

void WxInterpEd::EndDrag3DHandle()
{
	if(bDragging3DHandle)
	{
		InterpEdTrans->EndSpecial();
	}
}


void WxInterpEd::AddKey()
{
	if(ActiveTrackIndex != INDEX_NONE)
	{
		check(ActiveGroup);
		UInterpTrack* Track = ActiveGroup->InterpTracks(ActiveTrackIndex);
		check(Track);

		UInterpGroupInst* GrInst = Interp->FindFirstGroupInst(ActiveGroup);
		check(GrInst);

		UInterpTrackInst* TrInst = GrInst->TrackInst(ActiveTrackIndex);
		check(TrInst);

		// If adding an event - add a connector to the action.
		FName NewEventName = NAME_None;
		UInterpTrackEvent* EventTrack = Cast<UInterpTrackEvent>(Track);
		if( EventTrack )
		{
			// Prompt user for name of new event.
			WxDlgGenericStringEntry dlg;
			INT Result = dlg.ShowModal( TEXT("New Event Key"), TEXT("New Event Name:"), TEXT("Event0") );
			if( Result == wxID_OK )
			{
				dlg.EnteredString.Replace(TEXT(" "),TEXT("_"));
				NewEventName = FName( *dlg.EnteredString );				
			}
			else
			{
				return;
			}
		}

		// If adding a cut, bring up combo to let user choose group to cut to.
		FName TargetGroupName = NAME_None;
		UInterpTrackDirector* DirTrack = Cast<UInterpTrackDirector>(Track);
		if( DirTrack )
		{
			// Make array of group names
			TArray<FString> GroupNames;
			for(INT i=0; i<IData->InterpGroups.Num(); i++)
			{
				GroupNames.AddItem( FString(*(IData->InterpGroups(i)->GroupName)) );
			}

			WxDlgGenericComboEntry dlg;
			INT Result = dlg.ShowModal( TEXT("New Cut"), TEXT("Cut To Group:"), GroupNames);
			if(Result == wxID_OK)
			{
				TargetGroupName = FName( *dlg.SelectedString );
			}
			else
			{
				return;
			}	
		}
	

		InterpEdTrans->BeginSpecial( TEXT("AddKey") );
		Track->Modify();
		Opt->Modify();

		// Add key at current time, snapped to the grid if its on.
		INT NewKeyIndex = Track->AddKeyframe( SnapTime(Interp->Position), TrInst );

		// If we failed to add the keyframe - bail out now.
		if(NewKeyIndex != INDEX_NONE) 
		{
			// Select the newly added keyframe.
			ClearKeySelection();
			AddKeyToSelection(ActiveGroup, ActiveTrackIndex, NewKeyIndex, true); // Probably don't need to auto-wind - should already be there!

			// If adding an event - add a connector to the action.
			if( EventTrack )
			{
				FEventTrackKey& NewEventKey = EventTrack->EventTrack(NewKeyIndex);
				NewEventKey.EventName = NewEventName;

				// Now ensure all Matinee actions (including 'Interp') are updated with a new connector to match the data.
				UpdateMatineeActionConnectors();				
			}

			// If adding a cut, bring up combo to let user choose group to cut to.
			if( DirTrack )
			{
				FDirectorTrackCut& NewDirCut = DirTrack->CutTrack(NewKeyIndex);
				NewDirCut.TargetCamGroup = TargetGroupName;
			}

			InterpEdVC->Viewport->Invalidate();
		}

		InterpEdTrans->EndSpecial();
	}
}

/** Go over all Matinee Action (SeqAct_Interp) making sure that their connectors (variables for group and outputs for event tracks) are up to date. */
void WxInterpEd::UpdateMatineeActionConnectors()
{
	USequence* RootSeq = Interp->GetRootSequence();
	check(RootSeq);
	RootSeq->UpdateInterpActionConnectors();
}


/** Jump the position of the interpolation to the current time, updating Actors. */
void WxInterpEd::SetInterpPosition(FLOAT NewPosition)
{
	// Move preview position in interpolation to where we want it, and update any properties
	Interp->UpdateInterp(NewPosition, true);

	// When playing/scrubbing, we release the current keyframe from editing
	bAdjustingKeyframe = false;

	// If we are locking the camera to a group, update it here
	UpdateCameraToGroup();

	// Set the camera frustum colours to show which is being viewed.
	UpdateCamColours();

	// Redraw viewport.
	InterpEdVC->Viewport->Invalidate();

	// Update the position of the marker in the curve view.
	CurveEd->SetPositionMarker(true, Interp->Position, PosMarkerColor );
}

/** Ensure the curve editor is synchronised with the track editor. */
void WxInterpEd::SyncCurveEdView()
{
	CurveEd->StartIn = InterpEdVC->ViewStartTime;
	CurveEd->EndIn = InterpEdVC->ViewEndTime;
	CurveEd->CurveEdVC->Viewport->Invalidate();
}

/** Add the property being controlled by this track to the graph editor. */
void WxInterpEd::AddTrackToCurveEd(class UInterpGroup* InGroup, INT InTrackIndex)
{
	UInterpTrack* InterpTrack = InGroup->InterpTracks(InTrackIndex);

	FString CurveName = FString::Printf( TEXT("%s_%s"), *InGroup->GroupName, *InterpTrack->TrackTitle);

	// Toggle whether this curve is edited in the Curve editor.
	if( IData->CurveEdSetup->ShowingCurve(InterpTrack) )
	{
		IData->CurveEdSetup->RemoveCurve(InterpTrack);
	}
	else
	{
		IData->CurveEdSetup->AddCurveToCurrentTab(InterpTrack, CurveName, InGroup->GroupColor);
	}

	CurveEd->CurveChanged();
}


/** 
 *	Get the actor that the camera should currently be viewed through.
 *	We look here to see if the viewed group has a Director Track, and if so, return that Group.
 */
AActor* WxInterpEd::GetViewedActor()
{
	if(!CamViewGroup)
	{
		return NULL;
	}

	UInterpGroupDirector* DirGroup = Cast<UInterpGroupDirector>(CamViewGroup);
	if(DirGroup)
	{
		return Interp->FindViewedActor();
	}
	else
	{
		UInterpGroupInst* GroupInst = Interp->FindFirstGroupInst(CamViewGroup);
		check(GroupInst);

		return GroupInst->GroupActor;
	}
}

/** Can input NULL to unlock camera from all group. */
void WxInterpEd::LockCamToGroup(class UInterpGroup* InGroup)
{
	// If different from current locked group - release current.
	if(CamViewGroup && (CamViewGroup != InGroup))
	{
		// Re-show the actor (if present)
		//UInterpGroupInst* GrInst = Interp->FindFirstGroupInst(ActiveGroup);
		//check(GrInst);
		//if(GrInst->GroupActor)
		//	GrInst->GroupActor->bHiddenEd = false;

		// Reset viewports (clear roll etc).
		for(INT i=0; i<4; i++)
		{
			WxLevelViewportWindow* LevelVC = GApp->EditorFrame->ViewportConfigData->Viewports[i].ViewportWindow;
			if(LevelVC && LevelVC->ViewportType == LVT_Perspective)
			{
				LevelVC->ViewRotation.Roll = 0;
				LevelVC->bConstrainAspectRatio = false;
				LevelVC->ViewFOV = GEditor->FOVAngle;
				LevelVC->bEnableFading = false;
			}
		}

		CamViewGroup = NULL;
	}

	// If non-null new group - switch to it now.
	if(InGroup)
	{
		// Hide the actor when viewing through it.
		//UInterpGroupInst* GrInst = Interp->FindFirstGroupInst(InGroup);
		//check(GrInst);
		//GrInst->GroupActor->bHiddenEd = true;

		CamViewGroup = InGroup;

		// Move camera to track now.
		UpdateCameraToGroup();
	}
}

/** Update the colours of any CameraActors we are manipulating to match their group colours, and indicate which is 'active'. */
void WxInterpEd::UpdateCamColours()
{
	AActor* ViewedActor = Interp->FindViewedActor();

	for(INT i=0; i<Interp->GroupInst.Num(); i++)
	{
		ACameraActor* Cam = Cast<ACameraActor>(Interp->GroupInst(i)->GroupActor);
		if(Cam && Cam->DrawFrustum)
		{
			if(Interp->GroupInst(i)->GroupActor == ViewedActor)
			{
				Cam->DrawFrustum->FrustumColor = ActiveCamColor;
			}
			else
			{
				Cam->DrawFrustum->FrustumColor = Interp->GroupInst(i)->Group->GroupColor;
			}
		}
	}
}

/** 
 *	If we are viewing through a particular group - move the camera to correspond. 
 */
void WxInterpEd::UpdateCameraToGroup()
{
	// If viewing through the director group, see if we have a fade track, and if so see how much fading we should do.
	FLOAT FadeAmount = 0.f;
	if(CamViewGroup)
	{
		UInterpGroupDirector* DirGroup = Cast<UInterpGroupDirector>(CamViewGroup);
		if(DirGroup)
		{
			UInterpTrackFade* FadeTrack = DirGroup->GetFadeTrack();
			if(FadeTrack)
			{
				FadeAmount = FadeTrack->GetFadeAmountAtTime(Interp->Position);
			}

			// Set TimeDilation in the LevelInfo based on what the Slomo track says (if there is one).
			UInterpTrackSlomo* SlomoTrack = DirGroup->GetSlomoTrack();
			if(SlomoTrack)
			{
				GEditor->Level->GetLevelInfo()->TimeDilation = SlomoTrack->GetSlomoFactorAtTime(Interp->Position);
			}
		}
	}

	AActor* ViewedActor = GetViewedActor();
	if(ViewedActor)
	{
		ACameraActor* Cam = Cast<ACameraActor>(ViewedActor);

		// Move any perspective viewports to coincide with moved actor.
		for(INT i=0; i<4; i++)
		{
			WxLevelViewportWindow* LevelVC = GApp->EditorFrame->ViewportConfigData->Viewports[i].ViewportWindow;
			if(LevelVC && LevelVC->ViewportType == LVT_Perspective)
			{
				LevelVC->ViewLocation = ViewedActor->Location;
				LevelVC->ViewRotation = ViewedActor->Rotation;				

				LevelVC->FadeAmount = FadeAmount;
				LevelVC->bEnableFading = true;

				// If viewing through a camera - enforce aspect ratio of camera.
				if(Cam)
				{
					LevelVC->bConstrainAspectRatio = Cam->bConstrainAspectRatio;
					LevelVC->AspectRatio = Cam->AspectRatio;
					LevelVC->ViewFOV = Cam->FOVAngle;
				}
				else
				{
					LevelVC->bConstrainAspectRatio = false;
					LevelVC->ViewFOV = GEditor->FOVAngle;
				}
			}
		}
	}
	// If not bound to anything at this point of the cinematic - leave viewports at default.
	else
	{
		for(INT i=0; i<4; i++)
		{
			WxLevelViewportWindow* LevelVC = GApp->EditorFrame->ViewportConfigData->Viewports[i].ViewportWindow;
			if(LevelVC && LevelVC->ViewportType == LVT_Perspective)
			{
				LevelVC->bConstrainAspectRatio = false;
				LevelVC->ViewFOV = GEditor->FOVAngle;

				LevelVC->FadeAmount = FadeAmount;
				LevelVC->bEnableFading = true;
			}
		}
	}
}

// Notification from the EdMode that a perspective camera has moves. 
// If we are locking the camera to a particular actor - we update its location to match.
void WxInterpEd::CamMoved(const FVector& NewCamLocation, const FRotator& NewCamRotation)
{
	// If cam not locked to something, do nothing.
	AActor* ViewedActor = GetViewedActor();
	if(ViewedActor)
	{
		// Update actors location/rotation from camera
		ViewedActor->Location = NewCamLocation;
		ViewedActor->Rotation = NewCamRotation;

		ViewedActor->InvalidateLightingCache();
		ViewedActor->UpdateComponents();

		// In case we were modifying a keyframe for this actor.
		ActorModified();
	}
}


void WxInterpEd::ActorModified()
{
	// We only care about this if we have a keyframe selected.
	if(!bAdjustingKeyframe)
		return;

	check(Opt->SelectedKeys.Num() == 1);
	FInterpEdSelKey& SelKey = Opt->SelectedKeys(0);

	// Find the actor controlled by the selected group.
	UInterpGroupInst* GrInst = Interp->FindFirstGroupInst(SelKey.Group);
	check(GrInst);

	if( !GrInst->GroupActor )
		return;

	// See if this is one of the actors that was just moved.
	UBOOL bTrackActorModified = false;
	for( INT i=0; i<GUnrealEd->Level->Actors.Num() && !bTrackActorModified; i++ )
	{
		AActor* Actor = GUnrealEd->Level->Actors(i);
		if( Actor && GSelectionTools.IsSelected( Actor ) && Actor == GrInst->GroupActor )
			bTrackActorModified = true;
	}

	// If so, update the selected keyframe on the selected track to reflect its new position.
	if(bTrackActorModified)
	{
		UInterpTrack* Track = SelKey.Group->InterpTracks(SelKey.TrackIndex);

		InterpEdTrans->BeginSpecial( TEXT("UpdateKeyframe") );
		Track->Modify();

		UInterpTrackInst* TrInst = GrInst->TrackInst(SelKey.TrackIndex);

		Track->UpdateKeyframe( SelKey.KeyIndex, TrInst);

		InterpEdTrans->EndSpecial();
	}
}

void WxInterpEd::ActorSelectionChange()
{
	// Ignore this selection notification if desired.
	if(bIgnoreActorSelection)
	{
		return;
	}

	// Look at currently selected actors. We only want one.
	AActor* SingleActor = NULL;
	for( INT i=0; i<GUnrealEd->Level->Actors.Num(); i++ )
	{
		AActor* Actor = GUnrealEd->Level->Actors(i);
		if( Actor && GSelectionTools.IsSelected( Actor ) )
		{
			if(!SingleActor)
			{
				SingleActor = Actor;
			}
			else
			{
				// If more than one thing selected - select no tracks.
				SetActiveTrack(NULL, INDEX_NONE);
				return;
			}
		}
	}

	if(!SingleActor)
	{
		SetActiveTrack(NULL, INDEX_NONE);
		return;
	}

	UInterpGroupInst* GrInst = Interp->FindGroupInst(SingleActor);
	if(GrInst)
	{
		check(GrInst->Group);
		SetActiveTrack(GrInst->Group, INDEX_NONE);
		return;
	}

	SetActiveTrack(NULL, INDEX_NONE);
}

UBOOL WxInterpEd::ProcessKeyPress(FName Key, UBOOL bCtrlDown)
{
	if(Key == KEY_Right)
	{
		SelectNextKey();
		return true;
	}
	else if(Key == KEY_Left)
	{
		SelectPreviousKey();
		return true;
	}
	else if(Key == KEY_Enter)
	{
		AddKey();
		return true;
	}
	else if( bCtrlDown && Key == KEY_Z )
	{
		InterpEdUndo();
		return true;
	}
	else if( bCtrlDown && Key == KEY_Y )
	{
		InterpEdRedo();
		return true;
	}
	else if( bCtrlDown && Key == KEY_W )
	{
		DuplicateSelectedKeys();
		return true;
	}

	return false;
}

void WxInterpEd::ZoomView(FLOAT ZoomAmount)
{
	// Proportion of interp we are currently viewing
	FLOAT CurrentZoomFactor = (InterpEdVC->ViewEndTime - InterpEdVC->ViewStartTime)/(InterpEdVC->TrackViewSizeX);

	FLOAT NewZoomFactor = Clamp<FLOAT>(CurrentZoomFactor * ZoomAmount, 0.0003f, 1.0f);
	FLOAT NewZoomWidth = NewZoomFactor * InterpEdVC->TrackViewSizeX;
	FLOAT ViewMidTime = InterpEdVC->ViewStartTime + 0.5f*(InterpEdVC->ViewEndTime - InterpEdVC->ViewStartTime);

	InterpEdVC->ViewStartTime = ViewMidTime - 0.5*NewZoomWidth;
	InterpEdVC->ViewEndTime = ViewMidTime + 0.5*NewZoomWidth;
	SyncCurveEdView();
}

void WxInterpEd::SelectActiveGroupParent()
{

	
}

void WxInterpEd::InterpEdUndo()
{
	GEditor->Trans->Undo();
	UpdateMatineeActionConnectors();
	CurveEd->SetRegionMarker(true, IData->EdSectionStart, IData->EdSectionEnd, RegionFillColor);
	CurveEd->SetEndMarker(true, IData->InterpLength);
}

void WxInterpEd::InterpEdRedo()
{
	GEditor->Trans->Redo();
	UpdateMatineeActionConnectors();
	CurveEd->SetRegionMarker(true, IData->EdSectionStart, IData->EdSectionEnd, RegionFillColor);
	CurveEd->SetEndMarker(true, IData->InterpLength);
}